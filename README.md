# MJPEG over IP in C

Dois binarios:

- `mjpeg_tx`: captura `/dev/videoX` via V4L2, prefere MJPEG nativo da camera e transmite por TCP, UDP ou HTTP MJPEG. Se a camera nao entregar MJPEG, cai para RGB24/YUYV e codifica com libjpeg.
- `mjpeg_rx`: recebe frames JPEG e exibe em uma janela GTK.
- `v4l2_discover`: lista formatos, resolucoes e FPS suportados por uma camera V4L2.

## Dependencias

Debian/Ubuntu:

```sh
sudo apt-get install -y build-essential libjpeg-dev libgtk-3-dev
```

Build:

```sh
make
```

GTK quase sempre e distribuido para link dinamico. Se seu sistema tiver `libjpeg.a`, o transmissor pode ser compilado estaticamente:

```sh
make static-tx
```

## Arquivos INI

O transmissor e o receptor podem carregar configuracoes de arquivo `.ini`. A ordem de precedencia e:

1. valores padrao internos
2. arquivo `.ini`
3. argumentos de linha de comando

Arquivos padrao:

- `mjpeg_tx` tenta carregar `tx.ini` automaticamente se ele existir.
- `mjpeg_rx` tenta carregar `rx.ini` automaticamente se ele existir.
- Use `--config outro.ini` para escolher outro arquivo. Se `--config` for informado e o arquivo nao existir, o programa falha.

Exemplo TX:

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

allow=192.168.0.10
allow=10.0.0.0/24
```

Exemplo RX:

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

Os scripts `start_transmitter.sh` e `start_receiver.sh` usam `CONFIG=tx.ini`/`CONFIG=rx.ini` por padrao. Variaveis de ambiente so sobrescrevem o `.ini` quando forem definidas explicitamente:

```sh
CONFIG=tx-lab.ini ./start_transmitter.sh
PORT=8080 PROTO=http ./start_transmitter.sh
```

## Sistemas embarcados

Este projeto foi mantido simples, mas ha algumas limitacoes praticas ao compilar para sistemas embarcados:

- `mjpeg_tx` e `v4l2_discover` sao os binarios mais adequados para embarcados. Eles dependem basicamente de libc, headers Linux/V4L2 e, no caso do transmissor, libjpeg apenas para fallback quando a camera nao entrega MJPEG nativo.
- Se a camera USB ja entrega `MJPEG`, o transmissor nao recomprime frames. Isso reduz bastante uso de CPU e memoria.
- `mjpeg_rx` usa GTK/GdkPixbuf e costuma ser pesado para firmware minimo. Em embarcados, prefira visualizar pelo modo `--http` em um navegador ou crie um receptor especifico para o display/framebuffer do alvo.
- A rede usa apenas IPv4 literal, por exemplo `192.168.0.10` ou `0.0.0.0`. Hostnames/DNS e IPv6 nao sao suportados. Isso evita a dependencia de `getaddrinfo`/NSS da glibc no binario estatico.
- GTK estatico raramente e pratico: envolve muitas dependencias, loaders, temas, fontes e plugins. Trate o receptor GTK como ferramenta de desktop.
- Para cross-compiling, normalmente basta sobrescrever `CC` e apontar `PKG_CONFIG`/sysroot conforme sua toolchain. Exemplo para o transmissor:

```sh
make clean
make CC=arm-linux-gnueabihf-gcc mjpeg_tx
```

- Para evitar dependencias de libjpeg no transmissor, seria necessario criar uma variante sem fallback de codificacao e exigir `MJPEG` nativo da camera. O codigo atual ainda linka `-ljpeg` porque mantem o fallback `RGB24`/`YUYV`.
- O discovery usa ioctls V4L2 comuns, mas alguns drivers nao informam todos os intervalos de FPS/resolucoes. Nesse caso ele mostra que o dado nao foi informado pelo driver.

## Uso

Descobrir formatos e resolucoes da camera:

```sh
./v4l2_discover /dev/video0
```

Ou pelo script:

```sh
./discover_camera.sh /dev/video0
```

TCP, recomendado. O transmissor e o servidor; o receptor e o cliente. O transmissor aceita multiplos receptores e continua aguardando novos clientes quando algum fecha:

```sh
./mjpeg_tx --device /dev/video0 --host 0.0.0.0 --port 5000 --tcp --width 640 --height 480 --fps 30 --quality 80
./mjpeg_rx --host 127.0.0.1 --port 5000 --tcp
```

HTTP MJPEG para navegador:

```sh
./mjpeg_tx --device /dev/video0 --host 0.0.0.0 --port 8080 --http --width 640 --height 480 --fps 30
```

Depois abra:

```text
http://127.0.0.1:8080/
```

Ou abra `viewer.html` no navegador para configurar IP, porta e zoom pela interface.

Controle de acesso por IP no transmissor TCP/HTTP:

```sh
./mjpeg_tx --device /dev/video0 --host 0.0.0.0 --port 8080 --http \
  --allow 192.168.0.10 \
  --allow 192.168.0.20-192.168.0.30 \
  --allow 10.0.0.0/24
```

Por padrao, todos os clientes sao permitidos. Ao informar uma ou mais regras `--allow`, o transmissor passa a aceitar apenas clientes que batem com alguma regra. Formatos aceitos:

- `--allow all` ou `--allow "*"`: permite todos.
- `--allow 192.168.0.10`: permite um IP especifico.
- `--allow 192.168.0.20-192.168.0.30`: permite range inclusivo.
- `--allow 10.0.0.0/24`: permite CIDR IPv4.

Pelo script:

```sh
PROTO=http PORT=8080 ALLOW="192.168.0.10,10.0.0.0/24" ./start_transmitter.sh
```

UDP:

```sh
./mjpeg_rx --listen 0.0.0.0 --port 5000 --udp
./mjpeg_tx --device /dev/video0 --host 127.0.0.1 --port 5000 --udp
```

Eventos de entrada do receptor:

```sh
./mjpeg_rx --host 127.0.0.1 --port 5000 --tcp \
  --event-host 127.0.0.1 --event-port 6000 \
  --joystick /dev/input/js0
```

O receptor conecta via TCP no IP/porta de eventos e envia JSON Lines, uma linha por evento. Exemplos:

```json
{"origin":"keyboard","type":"key_press","keyval":65361,"key":"Left","state":0}
{"origin":"mouse","type":"motion","x":215.00,"y":110.00,"state":0}
{"origin":"mouse","type":"button_press","button":1,"x":215.00,"y":110.00,"state":0}
{"origin":"joystick","type":"axis","number":0,"value":1200,"time":123456,"initial":false}
```

Pelo script:

```sh
EVENT_HOST=127.0.0.1 EVENT_PORT=6000 JOYSTICK=/dev/input/js0 ./start_receiver.sh
```

O socket de eventos permanece ativo enquanto o receptor estiver aberto. Se a conexao cair ou o servidor de controle ainda nao estiver disponivel, o receptor continua rodando e tenta reconectar automaticamente.

Notas:

- Enderecos de rede devem ser IPv4 literal; use `127.0.0.1`, `0.0.0.0` ou o IP da maquina. Nomes como `localhost` nao sao aceitos.
- TCP usa prefixo de tamanho de 32 bits por frame.
- HTTP usa `multipart/x-mixed-replace`, compativel com navegador e tags `<img>`.
- Controle de acesso por IP se aplica a clientes TCP e HTTP. UDP continua usando o destino configurado em `--host`.
- UDP divide o JPEG em fragmentos menores que o MTU. Perda de pacote descarta o frame.
- O receptor tem botoes `+` e `-` para zoom da imagem recebida.
- Eventos de teclado, mouse e joystick so sao enviados quando `--event-host` e `--event-port` forem informados. Joystick usa a API Linux `/dev/input/jsX`. O canal TCP de eventos tenta reconectar automaticamente em caso de perda de conexao.
- O transmissor tenta usar V4L2 `MJPEG` primeiro e envia o JPEG da camera direto, sem recompressao.
- Se `MJPEG` nao estiver disponivel, tenta `RGB24` e depois `YUYV`; nesses casos usa libjpeg para gerar cada frame JPEG.
- `--quality` so afeta o fallback com codificacao via libjpeg. Em MJPEG nativo, a qualidade depende da propria camera/driver.
