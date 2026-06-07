#!/bin/sh
set -eu

TX_CONFIG="${TX_CONFIG:-tx.ini}"
RX_CONFIG="${RX_CONFIG:-rx.ini}"
TELEMETRY_HOST="${TELEMETRY_HOST:-127.0.0.1}"
TELEMETRY_PORT="${TELEMETRY_PORT:-7000}"
TELEMETRY_INTERVAL="${TELEMETRY_INTERVAL:-1}"

OVERLAY="${OVERLAY:-overlay.json}"
HUD_COLOR="${HUD_COLOR:-}"
HUD_ASSET_SRC="${HUD_ASSET_SRC:-overlays/crosshair.png}"
HUD_ASSET_OUT="${HUD_ASSET_OUT:-overlays/crosshair.png}"

telemetry_pid=""
tx_pid=""

cleanup() {
    if [ -n "$tx_pid" ] && kill -0 "$tx_pid" 2>/dev/null; then
        kill "$tx_pid" 2>/dev/null || true
        wait "$tx_pid" 2>/dev/null || true
    fi
    if [ -n "$telemetry_pid" ] && kill -0 "$telemetry_pid" 2>/dev/null; then
        kill "$telemetry_pid" 2>/dev/null || true
        wait "$telemetry_pid" 2>/dev/null || true
    fi
}

trap cleanup INT TERM EXIT

if [ ! -f "$OVERLAY" ]; then
    echo "overlay nao encontrado: $OVERLAY" >&2
    exit 1
fi

if [ ! -f "$TX_CONFIG" ]; then
    echo "config do transmissor nao encontrada: $TX_CONFIG" >&2
    exit 1
fi

if [ ! -f "$RX_CONFIG" ]; then
    echo "config do receptor nao encontrada: $RX_CONFIG" >&2
    exit 1
fi

if [ ! -f overlays/crosshair.png ]; then
    echo "asset nao encontrado: overlays/crosshair.png" >&2
    exit 1
fi

if [ ! -x ./telemetry_server.js ]; then
    echo "telemetry_server.js nao encontrado ou sem permissao de execucao" >&2
    exit 1
fi

make

if [ -n "$HUD_COLOR" ]; then
    if [ ! -f "$HUD_ASSET_SRC" ]; then
        echo "asset de entrada nao encontrado: $HUD_ASSET_SRC" >&2
        exit 1
    fi
    echo "colorindo asset ${HUD_ASSET_SRC} -> ${HUD_ASSET_OUT} com ${HUD_COLOR}"
    ./asset_colorize --color "$HUD_COLOR" "$HUD_ASSET_SRC" "$HUD_ASSET_OUT"
fi

echo "iniciando telemetria em ${TELEMETRY_HOST}:${TELEMETRY_PORT}"
./telemetry_server.js \
    --host "$TELEMETRY_HOST" \
    --port "$TELEMETRY_PORT" \
    --interval "$TELEMETRY_INTERVAL" &
telemetry_pid="$!"

sleep 1

echo "iniciando transmissor com config ${TX_CONFIG}"
./mjpeg_tx --config "$TX_CONFIG" &
tx_pid="$!"

sleep 2

if ! kill -0 "$tx_pid" 2>/dev/null; then
    echo "mjpeg_tx encerrou antes do receptor. Verifique ${TX_CONFIG} e a camera." >&2
    exit 1
fi

echo "abrindo receptor com config ${RX_CONFIG} e overlay ${OVERLAY}"
set -- ./mjpeg_rx \
    --config "$RX_CONFIG" \
    --overlay "$OVERLAY" \
    --telemetry-enabled \
    --telemetry-host "$TELEMETRY_HOST" \
    --telemetry-port "$TELEMETRY_PORT"

if [ -n "$HUD_COLOR" ]; then
    set -- "$@" --hud-color "$HUD_COLOR"
fi

"$@"
