#!/bin/sh
set -eu

DEVICE="${DEVICE:-${1:-/dev/video0}}"

if [ ! -x ./v4l2_discover ]; then
    make v4l2_discover
fi

exec ./v4l2_discover "$DEVICE"
