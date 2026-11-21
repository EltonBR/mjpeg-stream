#!/bin/sh
set -eu

DEVICE="${DEVICE:-${1:-/dev/video0}}"

if [ ! -x ./dist/v4l2_discover ]; then
    make v4l2_discover
fi

exec ./dist/v4l2_discover "$DEVICE"
