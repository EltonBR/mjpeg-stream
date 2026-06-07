#!/bin/sh
set -eu

TX_CONFIG="${TX_CONFIG:-tx.ini}"
RX_CONFIG="${RX_CONFIG:-rx.ini}"
TELEMETRY_INTERVAL="${TELEMETRY_INTERVAL:-0.1}"

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

if [ ! -f "$TX_CONFIG" ]; then
    echo "config do transmissor nao encontrada: $TX_CONFIG" >&2
    exit 1
fi

if [ ! -f "$RX_CONFIG" ]; then
    echo "config do receptor nao encontrada: $RX_CONFIG" >&2
    exit 1
fi

ini_value() {
    awk -F= -v section="$1" -v key="$2" -v fallback="$3" '
        function trim(s) {
            sub(/^[ \t\r\n]+/, "", s)
            sub(/[ \t\r\n]+$/, "", s)
            return s
        }
        function strip_comment(s,    i, c, p) {
            for (i = 1; i <= length(s); i++) {
                c = substr(s, i, 1)
                p = i == 1 ? "" : substr(s, i - 1, 1)
                if ((c == "#" || c == ";") && (i == 1 || p ~ /[ \t]/)) {
                    return substr(s, 1, i - 1)
                }
            }
            return s
        }
        BEGIN { current = ""; found = fallback }
        {
            line = trim(strip_comment($0))
            if (line == "") next
            if (line ~ /^\[/) {
                current = line
                sub(/^\[/, "", current)
                sub(/\].*$/, "", current)
                current = trim(current)
                next
            }
            if (current == section) {
                k = trim($1)
                if (k == key) {
                    sub(/^[^=]*=/, "", line)
                    found = trim(line)
                }
            }
        }
        END { print found }
    ' "$RX_CONFIG"
}

TELEMETRY_HOST="$(ini_value telemetry telemetry_host 127.0.0.1)"
TELEMETRY_PORT="$(ini_value telemetry telemetry_port 7000)"
EVENT_HOST="$(ini_value events event_host 127.0.0.1)"
EVENT_PORT="$(ini_value events event_port 6000)"

if [ ! -f overlays/crosshair.png ]; then
    echo "asset nao encontrado: overlays/crosshair.png" >&2
    exit 1
fi

if [ ! -x ./telemetry_server.js ]; then
    echo "telemetry_server.js nao encontrado ou sem permissao de execucao" >&2
    exit 1
fi

make

if [ "${ASSET_COLOR+x}" ] && [ -n "$ASSET_COLOR" ]; then
    if [ ! -f "$HUD_ASSET_SRC" ]; then
        echo "asset de entrada nao encontrado: $HUD_ASSET_SRC" >&2
        exit 1
    fi
    echo "colorindo asset ${HUD_ASSET_SRC} -> ${HUD_ASSET_OUT} com ${ASSET_COLOR}"
    ./asset_colorize --color "$ASSET_COLOR" "$HUD_ASSET_SRC" "$HUD_ASSET_OUT"
fi

echo "iniciando telemetria em ${TELEMETRY_HOST}:${TELEMETRY_PORT} e eventos em ${EVENT_HOST}:${EVENT_PORT}"
./telemetry_server.js \
    --host "$TELEMETRY_HOST" \
    --port "$TELEMETRY_PORT" \
    --event-host "$EVENT_HOST" \
    --event-port "$EVENT_PORT" \
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

echo "abrindo receptor com config ${RX_CONFIG}"
./mjpeg_rx --config "$RX_CONFIG"
