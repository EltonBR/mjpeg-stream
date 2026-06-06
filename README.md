# MJPEG over IP in C

Projeto em C para capturar video de cameras V4L2 (`/dev/videoX`) e transmitir frames JPEG pela rede.

## Binarios

- `mjpeg_tx`: transmissor. Captura da camera, prefere MJPEG nativo e transmite por TCP, UDP ou HTTP MJPEG.
- `mjpeg_rx`: receptor GTK. Recebe JPEG, exibe imagem, tem zoom e pode enviar eventos de teclado/mouse/joystick por TCP em JSON.
- `v4l2_discover`: discovery V4L2. Lista formatos, resolucoes e FPS suportados por uma camera.

## Dependencias

Debian/Ubuntu:

```sh
sudo apt-get install -y build-essential libjpeg-dev libgtk-3-dev
```

Build:

```sh
make
```

Build estatico do transmissor:

```sh
make static-tx
```

`mjpeg_rx` usa GTK/GdkPixbuf e normalmente deve ser linkado dinamicamente.

## Configuracao INI

`mjpeg_tx` carrega `tx.ini` automaticamente se o arquivo existir. `mjpeg_rx` faz o mesmo com `rx.ini`.

Tambem e possivel informar outro arquivo:

```sh
./mjpeg_tx --config tx-lab.ini
./mjpeg_rx --config rx-lab.ini
```

Precedencia:

1. defaults internos
2. arquivo `.ini`
3. argumentos CLI

Se `--config` for informado e o arquivo nao existir, o programa falha.

### tx.ini

```ini
[stream]
device=/dev/video0
protocol=http
host=0.0.0.0
port=8080
width=640
height=480
fps=30
quality=80

# Opcional. Sem allow, todos os clientes TCP/HTTP sao permitidos.
allow=192.168.0.10
allow=10.0.0.0/24
```

### rx.ini

```ini
[stream]
protocol=tcp
host=127.0.0.1
listen_host=0.0.0.0
port=5000

[events]
events_enabled=true
event_host=127.0.0.1
event_port=6000
joystick_enabled=true
joystick=/dev/input/js0
```

## Uso

Descobrir formatos da camera:

```sh
./v4l2_discover /dev/video0
```

Transmissor HTTP MJPEG para navegador:

```sh
./mjpeg_tx --config tx.ini --http --port 8080
```

Abrir no navegador:

```text
http://127.0.0.1:8080/
```

Tambem ha uma pagina local:

```text
viewer.html
```

Ela permite configurar IP, porta, path e zoom.

TCP binario com receptor GTK:

```sh
./mjpeg_tx --tcp --host 0.0.0.0 --port 5000
./mjpeg_rx --tcp --host 127.0.0.1 --port 5000
```

UDP:

```sh
./mjpeg_rx --udp --listen 0.0.0.0 --port 5000
./mjpeg_tx --udp --host 127.0.0.1 --port 5000
```

Scripts:

```sh
./start_transmitter.sh
./start_receiver.sh
```

Os scripts usam `CONFIG=tx.ini` e `CONFIG=rx.ini` por padrao. Variaveis de ambiente so sobrescrevem o `.ini` quando forem definidas:

```sh
CONFIG=tx-lab.ini ./start_transmitter.sh
PORT=8080 PROTO=http ./start_transmitter.sh
EVENT_HOST=127.0.0.1 EVENT_PORT=6000 JOYSTICK=/dev/input/js0 ./start_receiver.sh
```

## Controle de acesso

O transmissor aceita controle de acesso por IP para clientes TCP e HTTP.

Sem `allow`, todos sao permitidos. Com uma ou mais regras, apenas clientes que batem com alguma regra sao aceitos.

Formatos:

- `allow=all`
- `allow=192.168.0.10`
- `allow=192.168.0.20-192.168.0.30`
- `allow=10.0.0.0/24`

Via CLI:

```sh
./mjpeg_tx --http --port 8080 \
  --allow 192.168.0.10 \
  --allow 10.0.0.0/24
```

Via script:

```sh
ALLOW="192.168.0.10,10.0.0.0/24" ./start_transmitter.sh
```

## Eventos do receptor

Quando configurado, o receptor envia eventos de teclado, mouse e joystick por TCP como JSON Lines.

CLI:

```sh
./mjpeg_rx --tcp --host 127.0.0.1 --port 5000 \
  --event-host 127.0.0.1 --event-port 6000 \
  --joystick /dev/input/js0
```

Exemplos:

```json
{"origin":"keyboard","type":"key_press","keyval":65361,"key":"Left","state":0}
{"origin":"mouse","type":"motion","x":215.00,"y":110.00,"state":0}
{"origin":"mouse","type":"button_press","button":1,"x":215.00,"y":110.00,"state":0}
{"origin":"joystick","type":"axis","number":0,"value":1200,"time":123456,"initial":false}
```

O socket de eventos permanece ativo enquanto o receptor estiver aberto. Se a conexao cair ou o servidor de controle ainda nao estiver disponivel, o receptor continua rodando e tenta reconectar automaticamente. Eventos gerados enquanto nao ha conexao ativa sao descartados.

## Protocolos

- TCP: o transmissor e servidor; o receptor e cliente. Cada frame usa prefixo de tamanho de 32 bits.
- HTTP: `multipart/x-mixed-replace`, compativel com navegador e `<img>`.
- UDP: fragmenta o JPEG em pacotes menores que o MTU. Perda de pacote descarta o frame.

## Observacoes

- Enderecos de rede devem ser IPv4 literal: `127.0.0.1`, `0.0.0.0`, `192.168.x.x`. Hostnames como `localhost` nao sao aceitos.
- O transmissor tenta `MJPEG` nativo primeiro e envia o JPEG da camera sem recompressao.
- Se `MJPEG` nao estiver disponivel, tenta `RGB24` e depois `YUYV`; nesses casos usa libjpeg.
- `quality` so afeta o fallback com codificacao via libjpeg. Em MJPEG nativo, a qualidade depende da camera/driver.
- O receptor GTK tem botoes `+` e `-` para zoom.
- Joystick usa a API Linux `/dev/input/jsX`.

## Sistemas embarcados

- `mjpeg_tx` e `v4l2_discover` sao os binarios mais adequados para embarcados.
- Cameras USB com MJPEG nativo reduzem bastante CPU e memoria, porque o transmissor nao recomprime frames.
- `mjpeg_rx` depende de GTK/GdkPixbuf e e mais adequado para desktop.
- A rede usa IPv4 literal e nao usa `getaddrinfo`, evitando dependencia de NSS/DNS da glibc no transmissor estatico.
- Para cross-compiling, sobrescreva `CC` conforme a toolchain:

```sh
make clean
make CC=arm-linux-gnueabihf-gcc mjpeg_tx
```

- Para remover a dependencia de libjpeg do transmissor, seria necessario compilar uma variante que exige MJPEG nativo e remove o fallback `RGB24`/`YUYV`.
