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
