#!/bin/sh
set -eu

CONFIG="${CONFIG:-config/tx.ini}"

if [ ! -f "$CONFIG" ]; then
    echo "config do transmissor nao encontrada: $CONFIG" >&2
    exit 1
fi

if [ ! -x ./dist/mjpeg_tx ]; then
    make dist/mjpeg_tx
fi

exec ./dist/mjpeg_tx --config "$CONFIG"
