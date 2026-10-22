import unittest

from server import parse_serial_line


class ParseSerialLineTests(unittest.TestCase):
    def test_parses_arduino_packet_output(self):
        packet = parse_serial_line(
            "Rcvd 5 Destino: 37 Origem: 48 Controle: 0 Dados: 12 : "
        )

        self.assertEqual(
            packet,
            {
                "size": 5,
                "destination": 37,
                "source": 48,
                "control": 0,
                "data": 12,
            },
        )

    def test_parses_additional_payload_bytes(self):
        packet = parse_serial_line(
            "Rcvd 6 Destino: 48 Origem: 37 Controle: 0 Dados: 9 : 10 "
        )

        self.assertEqual(packet["extra"], [10])

    def test_ignores_regular_diagnostic_lines(self):
        self.assertIsNone(parse_serial_line("Confirmacao recebida"))


if __name__ == "__main__":
    unittest.main()
