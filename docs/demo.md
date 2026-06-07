# Demo TX + RX + Telemetria + Eventos

O script `start_overlay_demo.sh` inicia:

1. servidor Node.js de telemetria e eventos
2. `mjpeg_tx` usando `tx.ini`
3. `mjpeg_rx` usando `rx.ini` com overlay, telemetria e eventos

## Uso

```sh
./start_overlay_demo.sh
```

## Usar outros INIs

```sh
TX_CONFIG=tx-lab.ini RX_CONFIG=rx-lab.ini ./start_overlay_demo.sh
```

## Cor do HUD

```sh
HUD_COLOR=amber ./start_overlay_demo.sh
```

## Colorir asset antes da demo

```sh
HUD_COLOR=amber \
HUD_ASSET_SRC=overlays/crosshair-original.png \
HUD_ASSET_OUT=overlays/crosshair.png \
./start_overlay_demo.sh
```

## Portas

Defaults:

- stream TCP: configurado por `tx.ini` e `rx.ini`
- telemetria: `127.0.0.1:7000`
- eventos: `127.0.0.1:6000`

Trocar porta de telemetria:

```sh
TELEMETRY_PORT=7001 ./start_overlay_demo.sh
```

Trocar porta de eventos:

```sh
EVENT_PORT=6001 ./start_overlay_demo.sh
```

Eventos de mouse/teclado/joystick recebidos pelo servidor Node aparecem no console.
