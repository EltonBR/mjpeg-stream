# Receptor `mjpeg_rx`

`mjpeg_rx` recebe JPEG por TCP ou UDP, desenha o video em GTK e pode aplicar HUD/overlay com telemetria.

## Configuracao

O receptor carrega `rx.ini` automaticamente se existir.

```sh
./mjpeg_rx --config rx.ini
```

## Uso TCP

```sh
./mjpeg_rx --tcp --host 127.0.0.1 --port 5000
```

## Janela

Use `--lock-aspect` para manter a area da imagem na proporcao do frame recebido:

```sh
./mjpeg_rx --tcp --host 127.0.0.1 --port 5000 --lock-aspect
```

No `rx.ini`:

```ini
[window]
lock_aspect=true
```

A proporcao e aplicada ao `GtkDrawingArea` por hints de geometria do GTK, compensando a toolbar da janela. Quando `lock_aspect` esta ativo, maximizar a janela e desabilitado porque a area de imagem deixaria de obedecer a proporcao do frame.

## Uso UDP

```sh
./mjpeg_rx --udp --listen 0.0.0.0 --port 5000
```

## Overlay

```sh
./mjpeg_rx --config rx.ini --overlay overlay.json
```

Com telemetria:

```sh
./mjpeg_rx --config rx.ini \
  --overlay overlay.json \
  --telemetry-enabled \
  --telemetry-host 127.0.0.1 \
  --telemetry-port 7000
```

## Eventos de entrada

O receptor pode enviar eventos de teclado, mouse e joystick para um servidor TCP em JSON Lines.

```sh
./mjpeg_rx --tcp --host 127.0.0.1 --port 5000 \
  --event-host 127.0.0.1 --event-port 6000 \
  --joystick /dev/input/js0
```

Eventos de mouse sao capturados somente sobre a area da imagem. Cliques na toolbar, incluindo zoom, nao sao enviados. `x` e `y` sao relativos normalizados de `0.0` a `1.0`; `pixel_x` e `pixel_y` informam a posicao em pixels dentro da area da imagem.

Eventos de pressionamento usam pares `down/up` e um evento `press` no release: teclado envia `keydown`, `keyup`, `keypress`; mouse envia `mousedown`, `mouseup`, `mousepress`; joystick button envia `buttondown`, `buttonup`, `buttonpress`.
