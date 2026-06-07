# Agente: Eventos de Entrada

## Escopo

Responsavel por capturar teclado, mouse e joystick no receptor GTK e enviar eventos por TCP como JSON Lines.

## Arquivos principais

- `src/rx/event_sender.c` e `src/rx/event_sender.h`: conexao TCP, reconexao e envio.
- `src/rx/input.c` e `src/rx/input.h`: captura de teclado, mouse e joystick.
- `src/rx/main.c`: ativa eventos conforme configuracao.
- `src/rx/app.c` e `src/rx/app.h`: estado do sender, thread de joystick e cleanup.
- `rx.ini`: configuracao de `events_enabled`, `event_host`, `event_port`, `joystick_enabled` e `joystick`.
- `start_receiver.sh`: variaveis `EVENT_HOST`, `EVENT_PORT` e `JOYSTICK`.

## Contrato de eventos

- Transporte: TCP.
- Formato: JSON Lines, um JSON por linha.
- Quando a conexao cai, o receptor continua aberto.
- A thread de reconexao tenta conectar periodicamente.
- Eventos emitidos sem conexao ativa podem ser descartados.
- Teclado usa `keydown`, `keyup` e `keypress`; `keypress` e emitido no release, depois do ciclo pressionar+soltar.
- Mouse usa `mousedown`, `mouseup` e `mousepress`; `mousepress` e emitido no release.
- Joystick button usa `buttondown`, `buttonup` e `buttonpress`; `buttonpress` e emitido quando o botao volta para `value=0`. Joystick axis continua usando `axis`.
- Eventos de mouse sao capturados somente no `GtkDrawingArea`; toolbar e botoes de zoom nao devem enviar mouse ao servidor.
- Em eventos de mouse, `x` e `y` sao coordenadas relativas normalizadas da area da imagem (`0.0` a `1.0`). `pixel_x` e `pixel_y` mantem a posicao em pixels dentro dessa area.

## Exemplos

```json
{"origin":"keyboard","type":"keydown","keyval":65361,"key":"Left","state":0}
{"origin":"keyboard","type":"keyup","keyval":65361,"key":"Left","state":0}
{"origin":"keyboard","type":"keypress","keyval":65361,"key":"Left","state":0}
{"origin":"mouse","type":"motion","x":0.500000,"y":0.250000,"pixel_x":640.00,"pixel_y":180.00,"image_w":1280,"image_h":720,"state":0}
{"origin":"mouse","type":"mousedown","button":1,"x":0.500000,"y":0.250000,"pixel_x":640.00,"pixel_y":180.00,"image_w":1280,"image_h":720,"state":0}
{"origin":"mouse","type":"mouseup","button":1,"x":0.500000,"y":0.250000,"pixel_x":640.00,"pixel_y":180.00,"image_w":1280,"image_h":720,"state":0}
{"origin":"mouse","type":"mousepress","button":1,"x":0.500000,"y":0.250000,"pixel_x":640.00,"pixel_y":180.00,"image_w":1280,"image_h":720,"state":0}
{"origin":"joystick","type":"axis","number":0,"value":1200,"time":123456,"initial":false}
{"origin":"joystick","type":"buttondown","number":0,"value":1,"time":123456,"initial":false}
{"origin":"joystick","type":"buttonup","number":0,"value":0,"time":123500,"initial":false}
{"origin":"joystick","type":"buttonpress","number":0,"value":0,"time":123500,"initial":false}
```

## Fluxo

1. `event_sender_start` guarda host/porta, habilita o sender e cria a thread de reconexao.
2. `rx_input_attach` registra callbacks GTK para teclado e mouse.
3. `rx_input_start_joystick` cria a thread de leitura de `/dev/input/jsX` se habilitado.
4. Cada callback monta uma string JSON e chama `event_sender_send`.
5. `event_sender_send` tenta conectar se necessario, escreve JSON e `\n`, ou fecha o fd em erro.

## Pontos de cuidado

- O JSON e montado com `snprintf`; se nomes de tecla tiverem aspas ou barras, hoje nao ha escaping completo.
- Joystick usa API Linux `linux/joystick.h`; nao e portavel para outros sistemas sem adaptacao.
- A leitura de joystick e non-blocking e dorme 10 ms quando nao ha evento.
- O cleanup precisa setar `app->stopping` antes de join da thread de joystick.
- `event_sender_send` usa mutex; mantenha o lock ao alterar fd/estado.

## Verificacao recomendada

```sh
nc -l 6000
./mjpeg_rx --tcp --host 127.0.0.1 --port 5000 --event-host 127.0.0.1 --event-port 6000
```

Com joystick:

```sh
./mjpeg_rx --tcp --host 127.0.0.1 --port 5000 --event-host 127.0.0.1 --event-port 6000 --joystick /dev/input/js0
```
