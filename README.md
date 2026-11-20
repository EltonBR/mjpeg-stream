# MJPEG over IP in C

Projeto em C para capturar video de cameras V4L2 (`/dev/videoX`) e transmitir frames JPEG pela rede. O projeto inclui o **Overlay Designer**, um editor visual local para criar e editar o HUD do receptor.

## Distribuicao TX e RX

Os arquivos prontos desta maquina estao em `dist/tx/` e `dist/rx/`. Eles sao um snapshot das configuracoes e do layout; edite as copias dentro de cada diretorio conforme a maquina de destino.

```sh
cd dist/tx
./iniciar-tx.sh
```

Em outra maquina ou terminal, apos configurar `dist/rx/rx.ini` com o IP do transmissor:

```sh
cd dist/rx
./iniciar-rx.sh
# Em outro terminal, no mesmo diretorio:
./overlay-designer
```

O editor informa o endereco para abrir no navegador; nao precisa de Node nem `make` para executar. Cada pacote tem um `README.txt` com seu conteudo e dependencias. Estes binarios foram gerados para **Linux x86-64** e dependem de bibliotecas do sistema compativeis; nao sao executaveis para ARM.

Para gerar uma nova distribuicao a partir dos binarios e configuracoes atuais, execute `python3 tools/build_dist.py` na raiz. O script nao substitui uma pasta `dist` existente: preserve ou mova a distribuicao anterior antes de executar novamente. `dist/` e um artefato local ignorado pelo Git.

## Editar o overlay: Overlay Designer

O executavel Linux fica na raiz do projeto. Para iniciar, execute:

```sh
./overlay-designer
```

Ele inicia o servidor e mostra o endereco no terminal, normalmente **http://127.0.0.1:8081/**. Abra esse endereco no navegador e mantenha o terminal aberto; pressione `Ctrl+C` para encerrar. Nao e necessario usar `make`, instalar Node.js ou executar `npm install` para rodar o executavel.

Por padrao, o programa usa `overlay.ini` do diretorio atual ou, se nao houver, o que estiver ao lado do executavel. Para escolher outro arquivo:

```sh
./overlay-designer --overlay-config /caminho/overlay.ini
```

O arquivo INI, o layout e os assets continuam externos e devem estar acessiveis para leitura e gravacao. O codigo-fonte esta em [`tools/overlay-designer/`](tools/overlay-designer/); o editor e separado de `viewer.html`, que apenas visualiza streams HTTP MJPEG. O executavel gerado nesta maquina e para Linux x86-64.

No editor, selecione um widget na paleta ou em **Layers**, arraste e redimensione no canvas, ajuste suas propriedades e use **Salvar**. O arquivo padrao e `overlay.json`; imagens PNG sao armazenadas em `overlays/`. O painel de telemetria simula valores sem servidor. Para ver as alteracoes sobre video real, inicie o RX com o mesmo layout: ele monitora o arquivo e recarrega layouts validos sem interromper o video. Alteracoes no endpoint de telemetria ou na aparencia em `overlay.ini` exigem reiniciar o RX.

O preview Canvas e uma aproximacao do desenho Cairo. Veja o [guia do Overlay Designer](docs/overlay-designer.md) para configuracao, seguranca e testes.

Documentacao detalhada por funcionalidade:

- [docs/README.md](docs/README.md)
- [Overlay Designer](docs/overlay-designer.md)
- [Overlay e widgets HUD](docs/overlay-widgets.md)
- [API TCP do RX: telemetria, comandos e eventos](docs/api-telemetria-eventos-rx.md)
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

`mjpeg_tx` carrega `tx.ini` automaticamente se o arquivo existir. `mjpeg_rx` carrega `rx.ini` e procura `overlay.ini` no mesmo diretorio para as configuracoes de HUD e telemetria.

Tambem e possivel informar outro arquivo:

```sh
./mjpeg_tx --config tx-lab.ini
./mjpeg_rx --config rx-lab.ini --overlay-config overlay-lab.ini
```

Precedencia:

1. defaults internos
2. `rx.ini`, incluindo chaves legadas de overlay e telemetria
3. `overlay.ini`
4. argumentos CLI

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

[window]
lock_aspect=true
zoom_in_key=plus
zoom_out_key=minus
dim_alpha_up_key=
dim_alpha_down_key=
dim_alpha_step=0.05

[events]
events_enabled=true
event_host=127.0.0.1
event_port=6000
joystick_enabled=true
joystick=/dev/input/js0

```

### overlay.ini

```ini
[overlay]
enabled=true
layout=overlay.json
assets_dir=overlays

[appearance]
hud_color=#ffbf00
hud_font=Monospace
dim_color=#000000
dim_alpha=0.20

[telemetry]
enabled=true
host=127.0.0.1
port=7000
reconnect_ms=1000

[designer]
host=127.0.0.1
port=8081
```

`layout` e `assets_dir` sao relativos ao diretorio de `overlay.ini`. Se o `overlay.ini` padrao estiver ausente, o RX aceita as configuracoes legadas do `rx.ini`; um arquivo passado explicitamente com `--overlay-config` deve existir. A opcao legada `--overlay arquivo.json` continua disponivel e exige que o arquivo exista. As opcoes de aparencia e telemetria pela CLI continuam tendo precedencia.

`zoom_in_key` e `zoom_out_key` usam nomes GDK e aceitam multiplas teclas separadas por virgula. As teclas de ajuste de dim ficam vazias por padrao; o slider da interface permanece disponivel.

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

Para manter a area da imagem sempre na proporcao do frame recebido:

```sh
./mjpeg_rx --tcp --host 127.0.0.1 --port 5000 --lock-aspect
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

Os scripts priorizam sempre o `.ini`. Use `CONFIG` apenas para escolher outro arquivo de configuracao; parametros de runtime devem ficar no INI.

```sh
CONFIG=tx-lab.ini ./start_transmitter.sh
CONFIG=rx-lab.ini ./start_receiver.sh
TX_CONFIG=tx-lab.ini RX_CONFIG=rx-lab.ini ./start_overlay_demo.sh
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

Via script, configure as regras `allow` no `tx.ini` e execute `./start_transmitter.sh`.

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
{"origin":"keyboard","type":"keydown","keyval":65361,"key":"Left","state":0}
{"origin":"keyboard","type":"keyup","keyval":65361,"key":"Left","state":0}
{"origin":"keyboard","type":"keypress","keyval":65361,"key":"Left","state":0}
{"origin":"mouse","type":"motion","x":0.500000,"y":0.250000,"pixel_x":640.00,"pixel_y":180.00,"image_w":1280,"image_h":720,"state":0}
{"origin":"mouse","type":"mousedown","button":1,"x":0.500000,"y":0.250000,"pixel_x":640.00,"pixel_y":180.00,"image_w":1280,"image_h":720,"state":0}
{"origin":"mouse","type":"mouseup","button":1,"x":0.500000,"y":0.250000,"pixel_x":640.00,"pixel_y":180.00,"image_w":1280,"image_h":720,"state":0}
{"origin":"mouse","type":"mousepress","button":1,"x":0.500000,"y":0.250000,"pixel_x":640.00,"pixel_y":180.00,"image_w":1280,"image_h":720,"state":0}
{"origin":"mouse","type":"scroll","direction":"down","delta_x":0.000000,"delta_y":1.000000,"x":0.500000,"y":0.250000,"pixel_x":640.00,"pixel_y":180.00,"image_w":1280,"image_h":720,"state":0}
{"origin":"joystick","type":"axis","number":0,"value":1200,"time":123456,"initial":false}
{"origin":"joystick","type":"buttondown","number":0,"value":1,"time":123456,"initial":false}
{"origin":"joystick","type":"buttonup","number":0,"value":0,"time":123500,"initial":false}
{"origin":"joystick","type":"buttonpress","number":0,"value":0,"time":123500,"initial":false}
```

`keypress` e `mousepress` sao emitidos apos os respectivos eventos de liberacao; `buttonpress` e emitido na liberacao nao inicial do joystick.

Eventos de mouse sao capturados somente sobre a area da imagem (`GtkDrawingArea`). Cliques na toolbar, como botoes de zoom, nao sao enviados. Em eventos de mouse, incluindo `scroll`, `x` e `y` sao relativos normalizados na area da imagem, de `0.0` a `1.0`; `pixel_x` e `pixel_y` trazem a posicao em pixels dentro dessa mesma area.

O socket de eventos permanece ativo enquanto o receptor estiver aberto. Se a conexao cair ou o servidor de controle ainda nao estiver disponivel, o receptor continua rodando e tenta reconectar automaticamente. Eventos pendentes usam uma fila limitada; sob saturacao, movimentos e eixos sao descartados primeiro, mas novos eventos discretos tambem podem ser descartados se a fila estiver cheia.

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
- `vertical_ruler` e `horizontal_ruler`: reguas configuraveis com limites, passos e templates.

Elementos basicos continuam aceitos via `elements` para compatibilidade:

- `image`: `file`, `x`, `y`, `w`, `h`, `anchor`, `rotation`, `alpha`, `visible`, `z_index`.
- `label`: `text`, `x`, `y`, `size`, `color`, `alpha`, `visible`, `z_index`.
- `line`: `x1`, `y1`, `x2`, `y2`, `width`, `color`, `alpha`, `visible`, `z_index`.

Sem unidade explicita, coordenadas entre `0.0` e `1.0` sao relativas a area visivel do HUD; os demais valores sao pixels. Use `x_unit`, `y_unit`, `w_unit` e `h_unit` com `relative` ou `px` para explicitar cada unidade. O receptor renderiza o video em modo cover, preenchendo a janela com proporcao preservada e crop central quando necessario. O zoom aumenta esse crop; tamanhos em pixels dos itens do HUD continuam fixos. Anchors aceitos: `top-left`, `top-center`, `top-right`, `bottom-left`, `bottom-right`, `center`. Imagens PNG sao carregadas de `assets_dir`; paths absolutos e paths com `..` sao rejeitados.

A cor global do HUD vem de `[appearance] hud_color` no `overlay.ini` ou de `--hud-color`. Labels e linhas usam essa cor. Imagens PNG sao carregadas como estao no arquivo; para pintar assets, use o binario separado `asset_colorize`.

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
