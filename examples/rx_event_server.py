#!/usr/bin/env python3
"""Servidor TCP JSON Lines de exemplo para eventos enviados pelo mjpeg_rx."""
import argparse
import json
import socketserver

MAX_LINE_BYTES = 65536


class Handler(socketserver.StreamRequestHandler):
    def handle(self):
        print(f'RX conectado: {self.client_address}', flush=True)
        while True:
            line = self.rfile.readline(MAX_LINE_BYTES + 1)
            if not line:
                break
            if len(line) > MAX_LINE_BYTES or not line.endswith(b'\n'):
                print('Linha excessiva ou incompleta: encerrando conexao', flush=True)
                break
            try:
                event = json.loads(line.decode('utf-8'))
                if not isinstance(event, dict):
                    raise ValueError('evento precisa ser objeto JSON')
            except (UnicodeDecodeError, ValueError) as exc:
                print(f'Linha invalida ignorada: {exc}', flush=True)
                continue
            # Substitua pelo despacho da aplicacao; nao execute comandos arbitrarios.
            print(json.dumps(event, ensure_ascii=False), flush=True)
        print(f'RX desconectado: {self.client_address}', flush=True)


class Server(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--host', default='127.0.0.1', help='endereço IPv4 local para bind')
    parser.add_argument('--port', type=int, default=6000)
    args = parser.parse_args()
    if not 1 <= args.port <= 65535:
        parser.error('porta deve ser 1..65535')
    with Server((args.host, args.port), Handler) as server:
        print(f'Eventos TCP em {args.host}:{args.port}; Ctrl+C para sair', flush=True)
        try:
            server.serve_forever()
        except KeyboardInterrupt:
            pass


if __name__ == '__main__':
    main()
