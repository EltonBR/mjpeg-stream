#!/bin/sh
set -eu

LISTEN_HOST="${LISTEN_HOST:-0.0.0.0}"
HOST="${HOST:-127.0.0.1}"
PORT="${PORT:-5000}"
PROTO="${PROTO:-tcp}"

case "$PROTO" in
    tcp|TCP)
        PROTO_FLAG="--tcp"
        ;;
    udp|UDP)
        PROTO_FLAG="--udp"
        ;;
    *)
        echo "PROTO deve ser tcp ou udp" >&2
        exit 2
        ;;
esac

if [ ! -x ./mjpeg_rx ]; then
    make mjpeg_rx
fi

if [ "$PROTO" = "tcp" ] || [ "$PROTO" = "TCP" ]; then
    exec ./mjpeg_rx --host "$HOST" --port "$PORT" "$PROTO_FLAG"
fi

exec ./mjpeg_rx --listen "$LISTEN_HOST" --port "$PORT" "$PROTO_FLAG"
