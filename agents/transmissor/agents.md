# Agente: Transmissor MJPEG

## Escopo

Responsavel pelo binario `mjpeg_tx`, que captura frames de uma camera V4L2 e transmite JPEG por TCP, UDP ou HTTP MJPEG.

## Arquivos principais

- `src/tx/main.c`: loop principal, sinais, espera por camera/clientes e envio de frames.
- `src/tx/config.c` e `src/tx/config.h`: defaults, INI, CLI e nome do protocolo.
- `src/tx/camera.c` e `src/tx/camera.h`: abertura V4L2, formato, mmap, dequeue/requeue.
- `src/tx/jpeg.c` e `src/tx/jpeg.h`: conversao YUYV para RGB e encode JPEG via libjpeg.
- `src/tx/stream.c` e `src/tx/stream.h`: TCP, UDP, HTTP MJPEG, clientes e fragmentacao UDP.
- `src/tx/access_control.c` e `src/tx/access_control.h`: regras `allow` para TCP/HTTP.
- `tx.ini`: configuracao padrao do transmissor.
- `start_transmitter.sh`: wrapper de inicializacao que usa `tx.ini` e nao sobrescreve parametros de runtime por CLI.

## Fluxo de execucao

1. `tx_parse_args` aplica defaults, le `tx.ini` se existir e depois aplica argumentos CLI.
2. `camera_open` tenta configurar MJPEG nativo; se nao der, tenta RGB24 e YUYV.
3. `stream_open` abre o destino UDP ou o servidor TCP/HTTP.
4. O loop principal usa `select` para camera e socket de escuta.
5. Cada frame vira JPEG diretamente ou por conversao/encode.
6. `stream_send_frame` envia para clientes TCP/HTTP ou fragmenta em UDP.
7. O buffer V4L2 e recolocado na fila com `camera_requeue_frame`.

## Decisoes importantes

- MJPEG nativo e preferido porque evita conversao e encode.
- `quality` so afeta fallback RGB24/YUYV; em MJPEG nativo a qualidade vem da camera/driver.
- TCP envia cada frame com prefixo de tamanho de 32 bits em network byte order.
- HTTP usa `multipart/x-mixed-replace` com boundary `mjpegframe`.
- UDP usa `udp_frame_header` e descarta frames incompletos no receptor.
- TCP/HTTP aceitam varios clientes ate `MAX_STREAM_CLIENTS`; UDP usa um destino conectado.
- Controle de acesso so vale para clientes TCP/HTTP, nao para UDP.

## Pontos de cuidado

- Sempre refileirar o frame V4L2 mesmo quando houver erro de envio, exceto quando encerrar de fato.
- `camera_open` atualiza `cfg->width` e `cfg->height` para os valores retornados pelo driver.
- O loop ignora timeouts de `select`; nao trate `rc == 0` como erro.
- Nao liberar `jpeg` quando o frame ja veio em MJPEG nativo; nesse caso o ponteiro e do mmap da camera.
- Alteracoes no limite de frame precisam considerar `MJPEG_MAX_FRAME` em transmissor e receptor.

## Verificacao recomendada

```sh
make clean
make
./v4l2_discover /dev/video0
./mjpeg_tx --config tx.ini --http --port 8080
```

Para validar TCP local:

```sh
./mjpeg_tx --tcp --host 0.0.0.0 --port 5000
./mjpeg_rx --tcp --host 127.0.0.1 --port 5000
```
