#!/bin/sh
set -eu

DEVICE="${DEVICE:-/dev/video0}"
LISTEN_HOST="${LISTEN_HOST:-0.0.0.0}"
HOST="${HOST:-127.0.0.1}"
PORT="${PORT:-5000}"
PROTO="${PROTO:-tcp}"
WIDTH="${WIDTH:-640}"
HEIGHT="${HEIGHT:-480}"
FPS="${FPS:-30}"
QUALITY="${QUALITY:-80}"

case "$PROTO" in
    tcp|TCP)
        PROTO_FLAG="--tcp"
        TARGET_HOST="$LISTEN_HOST"
        ;;
    http|HTTP)
        PROTO_FLAG="--http"
        TARGET_HOST="$LISTEN_HOST"
        ;;
    udp|UDP)
        PROTO_FLAG="--udp"
        TARGET_HOST="$HOST"
        ;;
    *)
        echo "PROTO deve ser tcp, udp ou http" >&2
        exit 2
        ;;
esac

if [ ! -x ./mjpeg_tx ]; then
    make mjpeg_tx
fi

exec ./mjpeg_tx \
    --device "$DEVICE" \
    --host "$TARGET_HOST" \
    --port "$PORT" \
    "$PROTO_FLAG" \
    --width "$WIDTH" \
    --height "$HEIGHT" \
    --fps "$FPS" \
    --quality "$QUALITY"
