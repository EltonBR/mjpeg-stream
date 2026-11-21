#!/bin/sh
set -eu

CONFIG="${CONFIG:-config/rx.ini}"

if [ ! -f "$CONFIG" ]; then
    echo "config do receptor nao encontrada: $CONFIG" >&2
    exit 1
fi

if [ ! -x ./dist/mjpeg_rx ]; then
    make dist/mjpeg_rx
fi

exec ./dist/mjpeg_rx --config "$CONFIG"
