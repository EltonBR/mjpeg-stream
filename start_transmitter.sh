#!/bin/sh
set -eu

CONFIG="${CONFIG:-tx.ini}"

if [ ! -f "$CONFIG" ]; then
    echo "config do transmissor nao encontrada: $CONFIG" >&2
    exit 1
fi

if [ ! -x ./mjpeg_tx ]; then
    make mjpeg_tx
fi

exec ./mjpeg_tx --config "$CONFIG"
