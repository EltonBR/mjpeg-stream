# Demo TX + RX + Telemetria + Eventos

O script `start_overlay_demo.sh` inicia:

1. servidor Node.js de telemetria e eventos
2. `mjpeg_tx` usando `tx.ini`
3. `mjpeg_rx` usando `rx.ini` e `overlay.ini`

## Uso

```sh
./start_overlay_demo.sh
```

## Usar outros INIs

```sh
TX_CONFIG=tx-lab.ini RX_CONFIG=rx-lab.ini OVERLAY_CONFIG=overlay-lab.ini ./start_overlay_demo.sh
```

## Configuracao do HUD

Cor, fonte, camada de leitura, overlay e telemetria ficam em `overlay.ini`; eventos ficam em `rx.ini`. Para o HUD dinamico atualizar, mantenha:

```ini
[overlay]
enabled=true
layout=overlay.json
assets_dir=overlays

[telemetry]
enabled=true
host=127.0.0.1
port=7000
```

## Colorir asset antes da demo

```sh
ASSET_COLOR=amber \
HUD_ASSET_SRC=overlays/crosshair-original.png \
HUD_ASSET_OUT=overlays/crosshair.png \
./start_overlay_demo.sh
```

## Portas

Defaults:

- stream TCP: configurado por `tx.ini` e `rx.ini`
- telemetria: `127.0.0.1:7000`
- eventos: `127.0.0.1:6000`

Para trocar portas de telemetria e eventos, edite `port` em `[telemetry]` de `overlay.ini` e `event_port` em `[events]` de `rx.ini`.

Eventos de mouse/teclado/joystick recebidos pelo servidor Node aparecem no console.
