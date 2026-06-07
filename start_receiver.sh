#!/bin/sh
set -eu

CONFIG="${CONFIG:-rx.ini}"

if [ ! -f "$CONFIG" ]; then
    echo "config do receptor nao encontrada: $CONFIG" >&2
    exit 1
fi

if [ ! -x ./mjpeg_rx ]; then
    make mjpeg_rx
fi

exec ./mjpeg_rx --config "$CONFIG"
