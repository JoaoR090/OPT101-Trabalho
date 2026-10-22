#!/usr/bin/env python3
# Módulos da biblioteca padrão para JSON, expressões regulares, threads e HTTP.
import json
import re
import threading
from collections import deque
from datetime import datetime, timezone
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

# pyserial fornece a conexão com o Arduino e a lista de portas disponíveis.
import serial
from serial.tools import list_ports

# Configurações comuns do servidor e da conexão com o Arduino.
HOST = "127.0.0.1"
PORT = 8000
BAUD_RATE = 115200
MAX_EVENTS = 500

# Captura os campos na ordem em que printPacote() os escreve no Arduino.
# O campo "extra" permite guardar bytes adicionais após o valor Dados.
PACKET_PATTERN = re.compile(
    r"Rcvd\s+(?P<size>\d+)\s+Destino:\s+(?P<destination>\d+)"
    r"\s+Origem:\s+(?P<source>\d+)\s+Controle:\s+(?P<control>\d+)"
    r"\s+Dados:\s+(?P<data>\d+)\s*:\s*(?P<extra>.*)"
)


class MonitorError(Exception):
    """Representa erros esperados durante o gerenciamento da porta serial."""

    pass


def parse_serial_line(message):
    """Converte uma linha de pacote do Arduino para campos estruturados."""
    match = PACKET_PATTERN.search(message)
    if not match:
        # Linhas de confirmação e diagnóstico não seguem o formato de pacote.
        return None

    # Os quatro primeiros bytes de dados do pacote são exibidos separadamente.
    packet = {
        "size": int(match["size"]),
        "destination": int(match["destination"]),
        "source": int(match["source"]),
        "control": int(match["control"]),
        "data": int(match["data"]),
    }
    extra = match["extra"].strip()
    if extra:
        # Converte os bytes restantes de texto para uma lista de inteiros.
        packet["extra"] = [int(value) for value in extra.split()]
    return packet


class SerialMonitor:
    """Gerencia a porta serial e mantém um histórico limitado de eventos."""

    def __init__(self):
        # O lock protege o estado compartilhado entre a API e a thread serial.
        self._lock = threading.Lock()
        # A conexão e sua thread são None enquanto a porta estiver fechada.
        self._serial = None
        self._reader_thread = None
        # Mantém apenas os eventos mais recentes para limitar o uso de memória.
        self._events = deque(maxlen=MAX_EVENTS)
        # Os IDs crescentes permitem que o frontend peça somente eventos novos.
        self._next_event_id = 1
        self._packet_count = 0
        self._last_event_at = None

    def available_ports(self):
        """Lista portas seriais detectadas pelo pyserial."""
        # A interface precisa do identificador da porta e de um nome legível.
        return [
            {"device": port.device, "description": port.description}
            for port in list_ports.comports()
        ]

    def connect(self, device):
        """Abre a porta escolhida e inicia a thread que lê as mensagens."""
        if not isinstance(device, str) or not device.strip():
            raise ValueError("Informe uma porta serial válida.")

        with self._lock:
            if self._serial is not None:
                raise MonitorError("Já existe uma conexão serial ativa.")

        try:
            # O timeout permite que a thread verifique periodicamente se a
            # conexão ainda está ativa, mesmo quando a placa não envia dados.
            connection = serial.Serial(
                port=device.strip(),
                baudrate=BAUD_RATE,
                timeout=1,
            )
        except (serial.SerialException, OSError) as error:
            # Converte erros da biblioteca serial em erro tratado pela API.
            raise MonitorError(f"Não foi possível abrir {device}: {error}") from error

        with self._lock:
            # Revalida dentro do lock para evitar duas conexões concorrentes.
            if self._serial is not None:
                connection.close()
                raise MonitorError("Já existe uma conexão serial ativa.")
            self._serial = connection
            self._append_event_locked("log", f"Conectado a {device} ({BAUD_RATE} baud).")
            # Uma thread dedicada permite continuar atendendo requisições HTTP
            # enquanto aguarda linhas do Arduino.
            self._reader_thread = threading.Thread(
                target=self._read_serial, args=(connection,), daemon=True
            )
            self._reader_thread.start()

    def disconnect(self):
        """Fecha a porta atual e aguarda brevemente o fim da thread de leitura."""
        with self._lock:
            # Primeiro marca a conexão como encerrada para a thread parar.
            connection = self._serial
            reader_thread = self._reader_thread
            self._serial = None
            self._reader_thread = None
            if connection is not None:
                self._append_event_locked("log", "Conexão serial encerrada.")

        # Fecha fora do lock para desbloquear uma leitura serial em andamento.
        if connection is not None:
            connection.close()
        if reader_thread is not None:
            reader_thread.join(timeout=2)

    def status(self):
        """Devolve um retrato consistente do estado atual da conexão."""
        with self._lock:
            # O objeto retornado é serializável como JSON pela camada HTTP.
            return {
                "connected": self._serial is not None,
                "port": self._serial.port if self._serial is not None else None,
                "baudRate": BAUD_RATE,
                "packetCount": self._packet_count,
                "lastEventAt": self._last_event_at,
            }

    def events_after(self, event_id):
        """Retorna os eventos com identificador maior que o informado."""
        with self._lock:
            # O frontend guarda o último ID visto para evitar repetir mensagens.
            return [event for event in self._events if event["id"] > event_id]

    def _read_serial(self, connection):
        """Lê linhas da porta e as registra como pacotes, logs ou erros."""
        while True:
            with self._lock:
                # A comparação por identidade impede uma thread antiga de ler
                # depois que a porta foi desconectada ou substituída.
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
            # Tenta reconhecer o formato de pacote; tudo o mais é log textual.
            packet = parse_serial_line(message)
            event_type = "packet" if packet is not None else "log"
            with self._lock:
                # Ignora dados que cheguem depois de a conexão ser encerrada.
                if self._serial is not connection:
                    return
                if packet is not None:
                    self._packet_count += 1
                self._append_event_locked(event_type, message, packet)

    def _append_event_locked(self, event_type, message, packet=None):
        """Adiciona um evento; deve ser chamado enquanto o lock está adquirido."""
        # O horário UTC facilita comparação e conversão para o fuso do navegador.
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


# A instância única compartilha a conexão e o histórico entre as requisições.
monitor = SerialMonitor()


class RequestHandler(BaseHTTPRequestHandler):
    """Implementa as rotas HTTP consumidas pela página web."""

    def do_OPTIONS(self):
        """Permite as verificações CORS do navegador."""
        # O navegador envia OPTIONS antes de certas requisições cross-origin.
        self.send_response(204)
        self._add_cors_headers()
        self.end_headers()

    def do_GET(self):
        """Atende consultas de portas, estado e eventos."""
        # Cada rota expõe uma consulta independente para o frontend.
        if self.path == "/api/ports":
            self._send_json(200, {"ports": monitor.available_ports()})
        elif self.path == "/api/status":
            self._send_json(200, monitor.status())
        elif self.path.startswith("/api/events"):
            self._get_events()
        else:
            self._send_json(404, {"error": "Endpoint não encontrado."})

    def do_POST(self):
        """Atende solicitações para abrir ou fechar a conexão serial."""
        try:
            # A porta serial é enviada como JSON pela tela de conexão.
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
            # Erros de entrada são respostas 400; conflitos seriais são 409.
            self._send_json(400, {"error": str(error)})
        except MonitorError as error:
            self._send_json(409, {"error": str(error)})

    def _get_events(self):
        """Lê o identificador de paginação e devolve os eventos seguintes."""
        from urllib.parse import parse_qs, urlparse

        # Lê "?after=ID" para devolver apenas eventos não vistos pelo navegador.
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
        """Lê um objeto JSON pequeno e rejeita entradas inválidas."""
        try:
            # Limita o corpo para evitar leituras grandes desnecessárias.
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
        """Serializa a resposta JSON e inclui os cabeçalhos necessários."""
        # Codifica em UTF-8 para preservar mensagens em português.
        encoded = json.dumps(payload, ensure_ascii=False).encode("utf-8")
        self.send_response(status_code)
        self._add_cors_headers()
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(encoded)))
        self.end_headers()
        self.wfile.write(encoded)

    def _add_cors_headers(self):
        """Autoriza o frontend local a consultar a API."""
        # Necessário pois o frontend é servido em outra porta HTTP.
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")

    def log_message(self, format_string, *args):
        """Mantém no terminal os registros de acesso do servidor HTTP."""
        print(f"[HTTP] {self.address_string()} - {format_string % args}")


def main():
    """Inicia o servidor e libera recursos ao receber Ctrl+C."""
    # ThreadingHTTPServer mantém consultas disponíveis durante a leitura serial.
    server = ThreadingHTTPServer((HOST, PORT), RequestHandler)
    print(f"Backend serial disponível em http://{HOST}:{PORT}")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nEncerrando backend...")
    finally:
        # Fecha a porta do Arduino e o socket HTTP ao encerrar o processo.
        monitor.disconnect()
        server.server_close()


if __name__ == "__main__":
    main()
