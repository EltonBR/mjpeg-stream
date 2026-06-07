# Agente: Protocolos e Rede

## Escopo

Responsavel pelos contratos de rede usados por transmissor e receptor: TCP binario, UDP fragmentado, HTTP MJPEG, IPv4/IPv6 e controle de tamanho de frames.

## Arquivos principais

- `src/common/net.c` e `src/common/net.h`: sockets TCP/UDP, bind/connect, read/write completos e parsing de IP/porta.
- `src/tx/stream.c` e `src/tx/stream.h`: envio TCP/UDP/HTTP e gerenciamento de clientes.
- `src/rx/receiver.c` e `src/rx/receiver.h`: leitura TCP e remontagem UDP.
- `src/tx/access_control.c` e `src/tx/access_control.h`: autorizacao de peers TCP/HTTP.

## Contratos

### TCP

- Transmissor atua como servidor.
- Receptor atua como cliente.
- Cada frame e enviado como:
  - `uint32_t` com tamanho em network byte order.
  - payload JPEG com exatamente esse tamanho.
- Tamanho zero ou acima de `MJPEG_MAX_FRAME` encerra a recepcao.

### HTTP MJPEG

- Transmissor atua como servidor HTTP simples.
- Resposta usa `multipart/x-mixed-replace`.
- Boundary atual: `mjpegframe`.
- Cada parte envia `Content-Type: image/jpeg` e `Content-Length`.
- Nao ha parser HTTP completo; ao conectar, o transmissor envia direto o stream.

### UDP

- Transmissor usa socket UDP conectado para um destino.
- JPEG e dividido em chunks de ate `MJPEG_UDP_PAYLOAD`.
- Cada datagrama contem `udp_frame_header`:
  - `magic`
  - `frame_id`
  - `chunk_id`
  - `chunk_count`
  - `frame_size`
- Receptor aceita apenas headers validos e descarta frames incompletos por troca de `frame_id`.

## IPv4 e IPv6

- `net.c` aceita IP literal IPv4 ou IPv6.
- Host `*` faz bind em IPv6 any.
- Para sockets IPv6 de bind, tenta desativar `IPV6_V6ONLY` para permitir dual-stack quando o sistema permite.
- Controle de acesso converte IPv4-mapped IPv6 para IPv4 antes de comparar regras.

## Pontos de cuidado

- Qualquer mudanca no header UDP precisa ser refletida nos dois lados.
- `MJPEG_MAX_FRAME`, `MJPEG_UDP_MAGIC` e `MJPEG_UDP_PAYLOAD` devem permanecer sincronizados entre headers de TX/RX.
- `tcp_listen` hoje usa backlog 1; aumentar clientes aceitos pode exigir revisar backlog e `MAX_STREAM_CLIENTS`.
- `send_all_nosigpipe` usa `MSG_NOSIGNAL`; mantenha isso para evitar SIGPIPE em clientes desconectados.
- UDP nao garante entrega nem ordem; nao introduza suposicoes de confiabilidade sem um protocolo novo.

## Verificacao recomendada

```sh
make
./mjpeg_tx --http --host 0.0.0.0 --port 8080
./mjpeg_tx --tcp --host 0.0.0.0 --port 5000
./mjpeg_rx --tcp --host 127.0.0.1 --port 5000
./mjpeg_rx --udp --listen 0.0.0.0 --port 5000
./mjpeg_tx --udp --host 127.0.0.1 --port 5000
```
