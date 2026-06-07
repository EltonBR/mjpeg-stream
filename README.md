# MJPEG over IP in C

Projeto em C para capturar video de cameras V4L2 (`/dev/videoX`) e transmitir frames JPEG pela rede.

Documentacao detalhada por funcionalidade:

- [docs/README.md](docs/README.md)
- [Overlay e widgets HUD](docs/overlay-widgets.md)
- [Telemetria JSON Lines](docs/telemetria.md)
- [Demo TX/RX/telemetria](docs/demo.md)

## Binarios

- `mjpeg_tx`: transmissor. Captura da camera, prefere MJPEG nativo e transmite por TCP, UDP ou HTTP MJPEG.
- `mjpeg_rx`: receptor GTK. Recebe JPEG, exibe imagem, tem zoom e pode enviar eventos de teclado/mouse/joystick por TCP em JSON.
- `v4l2_discover`: discovery V4L2. Lista formatos, resolucoes e FPS suportados por uma camera.

## Dependencias

Debian/Ubuntu:

```sh
sudo apt-get install -y build-essential libjpeg-dev libgtk-3-dev libjson-c-dev
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
allow=::1
allow=2001:db8::/32
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

[overlay]
overlay=overlay.json
hud_color=green

[telemetry]
telemetry_enabled=true
telemetry_host=127.0.0.1
telemetry_port=7000
```

Quando `overlay=overlay.json` vem do `rx.ini` e o arquivo nao existe, o receptor segue sem overlay. Quando `--overlay overlay.json` e informado na CLI, arquivo ausente e erro. `hud_color` aceita `green`, `amber` ou `#rrggbb` e afeta labels/linhas.

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
http://[::1]:8080/
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

TCP via IPv6:

```sh
./mjpeg_tx --tcp --host :: --port 5000
./mjpeg_rx --tcp --host ::1 --port 5000
```

UDP:

```sh
./mjpeg_rx --udp --listen 0.0.0.0 --port 5000
./mjpeg_tx --udp --host 127.0.0.1 --port 5000
```

UDP via IPv6:

```sh
./mjpeg_rx --udp --listen :: --port 5000
./mjpeg_tx --udp --host ::1 --port 5000
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
OVERLAY=overlay.json TELEMETRY_ENABLED=true TELEMETRY_HOST=127.0.0.1 TELEMETRY_PORT=7000 ./start_receiver.sh
HUD_COLOR=amber ./start_overlay_demo.sh
```

## Controle de acesso

O transmissor aceita controle de acesso por IP para clientes TCP e HTTP.

Sem `allow`, todos sao permitidos. Com uma ou mais regras, apenas clientes que batem com alguma regra sao aceitos.

Formatos:

- `allow=all`
- `allow=192.168.0.10`
- `allow=192.168.0.20-192.168.0.30`
- `allow=10.0.0.0/24`
- `allow=::1`
- `allow=2001:db8::/32`
- `allow=2001:db8::1-2001:db8::ffff`

Via CLI:

```sh
./mjpeg_tx --http --port 8080 \
  --allow 192.168.0.10 \
  --allow 10.0.0.0/24 \
  --allow 2001:db8::/32
```

Via script:

```sh
ALLOW="192.168.0.10,10.0.0.0/24,2001:db8::/32" ./start_transmitter.sh
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

## Overlay/HUD do receptor

`mjpeg_rx` pode desenhar overlays sobre o video usando `GtkDrawingArea` e Cairo. Sem `overlay.json`, o receptor continua exibindo apenas o video.

CLI:

```sh
./mjpeg_rx --tcp --host 127.0.0.1 --port 5000 \
  --overlay overlay.json --hud-color amber \
  --telemetry-enabled --telemetry-host 127.0.0.1 --telemetry-port 7000
```

Exemplo `overlay.json`:

```json
{
  "assets_dir": "overlays",
  "widgets": [
    {"id":"compass","type":"compass","x":0.5,"y":12,"anchor":"top-center","w":0.46,"h":54,"azimuth":"{azimuth}","size":14,"z_index":40,"visible":true},
    {"id":"crosshair","type":"image","file":"crosshair.png","x":0.5,"y":0.5,"anchor":"center","w":72,"h":72,"alpha":0.9,"rotation":"{rotate}","z_index":20,"visible":true}
  ]
}
```

Widgets suportados:

- `compass`: bussola estilo HUD no topo ou em qualquer posicao; usa `azimuth`, por exemplo `{azimuth}`.
- `readout`: grupo de linhas de texto com `items`, cada item com `label` e `value`.
- `status`: texto simples com templates.
- `image`: PNG com alpha; aceita `rotation`, inclusive via template como `{rotate}`.

Elementos basicos continuam aceitos via `elements` para compatibilidade:

- `image`: `file`, `x`, `y`, `w`, `h`, `anchor`, `rotation`, `alpha`, `visible`, `z_index`.
- `label`: `text`, `x`, `y`, `size`, `color`, `alpha`, `visible`, `z_index`.
- `line`: `x1`, `y1`, `x2`, `y2`, `width`, `color`, `alpha`, `visible`, `z_index`.

Coordenadas entre `0.0` e `1.0` sao relativas ao frame desenhado; valores maiores que `1` sao pixels. Anchors aceitos: `top-left`, `top-center`, `top-right`, `bottom-left`, `bottom-right`, `center`. Imagens PNG sao carregadas de `assets_dir`; paths absolutos e paths com `..` sao rejeitados.

A cor global do HUD vem de `hud_color` no `rx.ini` ou de `--hud-color`. Labels e linhas usam essa cor. Imagens PNG sao carregadas como estao no arquivo; para pintar assets, use o binario separado `asset_colorize`.

Colorir um PNG:

```sh
make asset_colorize
./asset_colorize --color amber overlays/crosshair-original.png overlays/crosshair.png
./asset_colorize --color green overlays/crosshair-original.png overlays/crosshair.png
./asset_colorize --color '#00ccff' overlays/crosshair-original.png overlays/crosshair.png
```

O `asset_colorize` preserva alpha e usa os niveis de preto/branco do PNG para variar a intensidade da cor. Ele tambem tenta lidar com PNGs de linhas pretas ou brancas: quando o asset e quase todo escuro ou quase todo claro, a parte visivel recebe a cor cheia.

### Telemetria JSON Lines

O cliente de telemetria conecta por TCP e tenta reconectar se a conexao cair. Cada linha deve ser um JSON.

Atualizar variaveis usadas em templates:

```json
{"type":"telemetry","angle":32.5,"heading":180,"battery":11.8}
```

Alterar elemento:

```json
{"type":"set","id":"crosshair","visible":false}
{"type":"set","id":"direction_arrow","rotation":245}
```

Criar ou substituir elemento:

```json
{"type":"upsert","id":"target_1","element":{"type":"image","file":"target.png","x":0.72,"y":0.44,"anchor":"center","w":32,"h":32,"rotation":0,"alpha":1.0,"z_index":30,"visible":true}}
```

Remover elemento:

```json
{"type":"delete","id":"target_1"}
```

Teste simples com `nc`:

```sh
nc -l 7000
```

Depois envie linhas como:

```json
{"type":"telemetry","angle":32.5,"heading":180,"battery":11.8}
{"type":"set","id":"direction_arrow","rotation":245}
```

## Protocolos

- TCP: o transmissor e servidor; o receptor e cliente. Cada frame usa prefixo de tamanho de 32 bits.
- HTTP: `multipart/x-mixed-replace`, compativel com navegador e `<img>`.
- UDP: fragmenta o JPEG em pacotes menores que o MTU. Perda de pacote descarta o frame.

## Observacoes

- Enderecos de rede devem ser IP literal IPv4 ou IPv6: `127.0.0.1`, `0.0.0.0`, `::1`, `::`, `192.168.x.x`, `2001:db8::1`. Hostnames como `localhost` nao sao aceitos.
- Em URLs HTTP, IPv6 deve usar colchetes: `http://[::1]:8080/`.
- Em servidores TCP/HTTP/UDP, `--host ::` ou `--listen ::` tenta aceitar IPv6 e IPv4 no mesmo socket quando o sistema permite dual-stack.
- O transmissor tenta `MJPEG` nativo primeiro e envia o JPEG da camera sem recompressao.
- Se `MJPEG` nao estiver disponivel, tenta `RGB24` e depois `YUYV`; nesses casos usa libjpeg.
- `quality` so afeta o fallback com codificacao via libjpeg. Em MJPEG nativo, a qualidade depende da camera/driver.
- O receptor GTK tem botoes `+` e `-` para zoom.
- Joystick usa a API Linux `/dev/input/jsX`.

## Sistemas embarcados

- `mjpeg_tx` e `v4l2_discover` sao os binarios mais adequados para embarcados.
- Cameras USB com MJPEG nativo reduzem bastante CPU e memoria, porque o transmissor nao recomprime frames.
- `mjpeg_rx` depende de GTK/GdkPixbuf e e mais adequado para desktop.
- A rede usa IP literal IPv4/IPv6 e nao usa `getaddrinfo`, evitando dependencia de NSS/DNS da glibc no transmissor estatico.
- Para cross-compiling, sobrescreva `CC` conforme a toolchain:

```sh
make clean
make CC=arm-linux-gnueabihf-gcc mjpeg_tx
```

- Para remover a dependencia de libjpeg do transmissor, seria necessario compilar uma variante que exige MJPEG nativo e remove o fallback `RGB24`/`YUYV`.
