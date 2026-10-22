#!/usr/bin/env python3
import json
import re
import threading
from collections import deque
from datetime import datetime, timezone
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

import serial
from serial.tools import list_ports


HOST = "127.0.0.1"
PORT = 8000
BAUD_RATE = 115200
MAX_EVENTS = 500

PACKET_PATTERN = re.compile(
    r"Rcvd\s+(?P<size>\d+)\s+Destino:\s+(?P<destination>\d+)"
    r"\s+Origem:\s+(?P<source>\d+)\s+Controle:\s+(?P<control>\d+)"
    r"\s+Dados:\s+(?P<data>\d+)\s*:\s*(?P<extra>.*)"
)


class MonitorError(Exception):
    pass


def parse_serial_line(message):
    match = PACKET_PATTERN.search(message)
    if not match:
        return None

    packet = {
        "size": int(match["size"]),
        "destination": int(match["destination"]),
        "source": int(match["source"]),
        "control": int(match["control"]),
        "data": int(match["data"]),
    }
    extra = match["extra"].strip()
    if extra:
        packet["extra"] = [int(value) for value in extra.split()]
    return packet


class SerialMonitor:
    def __init__(self):
        self._lock = threading.Lock()
        self._serial = None
        self._reader_thread = None
        self._events = deque(maxlen=MAX_EVENTS)
        self._next_event_id = 1
        self._packet_count = 0
        self._last_event_at = None

    def available_ports(self):
        return [
            {"device": port.device, "description": port.description}
            for port in list_ports.comports()
        ]

    def connect(self, device):
        if not isinstance(device, str) or not device.strip():
            raise ValueError("Informe uma porta serial válida.")

        with self._lock:
            if self._serial is not None:
                raise MonitorError("Já existe uma conexão serial ativa.")

        try:
            connection = serial.Serial(
                port=device.strip(),
                baudrate=BAUD_RATE,
                timeout=1,
            )
        except (serial.SerialException, OSError) as error:
            raise MonitorError(f"Não foi possível abrir {device}: {error}") from error

        with self._lock:
            if self._serial is not None:
                connection.close()
                raise MonitorError("Já existe uma conexão serial ativa.")
            self._serial = connection
            self._append_event_locked("log", f"Conectado a {device} ({BAUD_RATE} baud).")
            self._reader_thread = threading.Thread(
                target=self._read_serial, args=(connection,), daemon=True
            )
            self._reader_thread.start()

    def disconnect(self):
        with self._lock:
            connection = self._serial
            reader_thread = self._reader_thread
            self._serial = None
            self._reader_thread = None
            if connection is not None:
                self._append_event_locked("log", "Conexão serial encerrada.")

        if connection is not None:
            connection.close()
        if reader_thread is not None:
            reader_thread.join(timeout=2)

    def status(self):
        with self._lock:
            return {
                "connected": self._serial is not None,
                "port": self._serial.port if self._serial is not None else None,
                "baudRate": BAUD_RATE,
                "packetCount": self._packet_count,
                "lastEventAt": self._last_event_at,
            }

    def events_after(self, event_id):
        with self._lock:
            return [event for event in self._events if event["id"] > event_id]

    def _read_serial(self, connection):
        while True:
            with self._lock:
                if self._serial is not connection:
                    return
            try:
                raw_line = connection.readline()
            except (serial.SerialException, OSError) as error:
                with self._lock:
                    if self._serial is connection:
                        self._serial = None
                        self._reader_thread = None
                        self._append_event_locked("error", f"Falha na leitura serial: {error}")
                return

            if not raw_line:
                continue

            message = raw_line.decode("utf-8", errors="replace").strip()
            if not message:
                continue
            packet = parse_serial_line(message)
            event_type = "packet" if packet is not None else "log"
            with self._lock:
                if self._serial is not connection:
                    return
                if packet is not None:
                    self._packet_count += 1
                self._append_event_locked(event_type, message, packet)

    def _append_event_locked(self, event_type, message, packet=None):
        timestamp = datetime.now(timezone.utc).isoformat(timespec="seconds")
        event = {
            "id": self._next_event_id,
            "timestamp": timestamp,
            "type": event_type,
            "message": message,
            "packet": packet,
        }
        self._next_event_id += 1
        self._events.append(event)
        self._last_event_at = timestamp


monitor = SerialMonitor()


class RequestHandler(BaseHTTPRequestHandler):
    def do_OPTIONS(self):
        self.send_response(204)
        self._add_cors_headers()
        self.end_headers()

    def do_GET(self):
        if self.path == "/api/ports":
            self._send_json(200, {"ports": monitor.available_ports()})
        elif self.path == "/api/status":
            self._send_json(200, monitor.status())
        elif self.path.startswith("/api/events"):
            self._get_events()
        else:
            self._send_json(404, {"error": "Endpoint não encontrado."})

    def do_POST(self):
        try:
            body = self._read_json_body()
            if self.path == "/api/connect":
                monitor.connect(body.get("port"))
                self._send_json(200, monitor.status())
            elif self.path == "/api/disconnect":
                monitor.disconnect()
                self._send_json(200, monitor.status())
            else:
                self._send_json(404, {"error": "Endpoint não encontrado."})
        except ValueError as error:
            self._send_json(400, {"error": str(error)})
        except MonitorError as error:
            self._send_json(409, {"error": str(error)})

    def _get_events(self):
        from urllib.parse import parse_qs, urlparse

        query = parse_qs(urlparse(self.path).query)
        try:
            after = int(query.get("after", ["0"])[0])
            if after < 0:
                raise ValueError
        except ValueError:
            self._send_json(400, {"error": "O parâmetro after deve ser um inteiro não negativo."})
            return
        self._send_json(200, {"events": monitor.events_after(after)})

    def _read_json_body(self):
        try:
            length = int(self.headers.get("Content-Length", "0"))
            if length < 0 or length > 4096:
                raise ValueError
            body = json.loads(self.rfile.read(length) or b"{}")
        except (ValueError, json.JSONDecodeError) as error:
            raise ValueError("Corpo JSON inválido.") from error
        if not isinstance(body, dict):
            raise ValueError("O corpo da requisição deve ser um objeto JSON.")
        return body

    def _send_json(self, status_code, payload):
        encoded = json.dumps(payload, ensure_ascii=False).encode("utf-8")
        self.send_response(status_code)
        self._add_cors_headers()
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(encoded)))
        self.end_headers()
        self.wfile.write(encoded)

    def _add_cors_headers(self):
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")

    def log_message(self, format_string, *args):
        print(f"[HTTP] {self.address_string()} - {format_string % args}")


def main():
    server = ThreadingHTTPServer((HOST, PORT), RequestHandler)
    print(f"Backend serial disponível em http://{HOST}:{PORT}")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nEncerrando backend...")
    finally:
        monitor.disconnect()
        server.server_close()


if __name__ == "__main__":
    main()
