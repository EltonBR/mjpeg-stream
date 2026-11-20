#!/usr/bin/env python3
"""Servidor TCP JSON Lines de exemplo para telemetria do mjpeg_rx."""
import argparse
import datetime as dt
import json
import math
import socketserver
import time


def encode_message(message):
    # RX aceita no máximo 8191 bytes antes do '\\n'.
    line = json.dumps(message, ensure_ascii=False, separators=(',', ':'), allow_nan=False).encode('utf-8')
    if len(line) > 8191:
        raise ValueError('mensagem excede 8191 bytes')
    return line + b'\n'


class Handler(socketserver.BaseRequestHandler):
    def handle(self):
        self.request.settimeout(5)
        print(f'RX conectado: {self.client_address}', flush=True)
        try:
            if self.server.set_demo:
                self.request.sendall(encode_message({'type': 'set', 'id': self.server.set_demo, 'visible': True}))
            while True:
                now = dt.datetime.now().astimezone()
                angle = round(30 * math.sin(time.monotonic() / 3), 2)
                azimuth = round((time.monotonic() * 12) % 360, 2)
                message = {'type': 'telemetry', 'angle': angle, 'azimuth': azimuth,
                           'heading': azimuth, 'battery': 12.4, 'rotate': angle,
                           'date': now.strftime('%Y-%m-%d'), 'time': now.strftime('%H:%M:%S')}
                self.request.sendall(encode_message(message))
                time.sleep(self.server.interval)
        except (OSError, ConnectionError):
            pass
        finally:
            print(f'RX desconectado: {self.client_address}', flush=True)


class Server(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--host', default='127.0.0.1', help='endereço IPv4 local para bind')
    parser.add_argument('--port', type=int, default=7000)
    parser.add_argument('--interval', type=float, default=1.0, help='segundos entre mensagens')
    parser.add_argument('--set-demo', metavar='ID', help='envia set visible=true para este ID ao conectar')
    args = parser.parse_args()
    if not 1 <= args.port <= 65535 or not math.isfinite(args.interval) or args.interval <= 0:
        parser.error('porta deve ser 1..65535 e intervalo deve ser positivo e finito')
    with Server((args.host, args.port), Handler) as server:
        server.interval = args.interval
        server.set_demo = args.set_demo
        print(f'Telemetria TCP em {args.host}:{args.port}; Ctrl+C para sair', flush=True)
        try:
            server.serve_forever()
        except KeyboardInterrupt:
            pass


if __name__ == '__main__':
    main()
