#!/bin/sh
set -eu

CONFIG="${CONFIG:-tx.ini}"

if [ ! -x ./mjpeg_tx ]; then
    make mjpeg_tx
fi

set -- ./mjpeg_tx --config "$CONFIG"

if [ "${DEVICE+x}" ]; then
    set -- "$@" --device "$DEVICE"
fi
if [ "${PORT+x}" ]; then
    set -- "$@" --port "$PORT"
fi
if [ "${WIDTH+x}" ]; then
    set -- "$@" --width "$WIDTH"
fi
if [ "${HEIGHT+x}" ]; then
    set -- "$@" --height "$HEIGHT"
fi
if [ "${FPS+x}" ]; then
    set -- "$@" --fps "$FPS"
fi
if [ "${QUALITY+x}" ]; then
    set -- "$@" --quality "$QUALITY"
fi

if [ "${PROTO+x}" ]; then
    case "$PROTO" in
        tcp|TCP)
            set -- "$@" --tcp
            if [ "${LISTEN_HOST+x}" ]; then
                set -- "$@" --host "$LISTEN_HOST"
            fi
            ;;
        http|HTTP)
            set -- "$@" --http
            if [ "${LISTEN_HOST+x}" ]; then
                set -- "$@" --host "$LISTEN_HOST"
            fi
            ;;
        udp|UDP)
            set -- "$@" --udp
            if [ "${HOST+x}" ]; then
                set -- "$@" --host "$HOST"
            fi
            ;;
        *)
            echo "PROTO deve ser tcp, udp ou http" >&2
            exit 2
            ;;
    esac
else
    if [ "${LISTEN_HOST+x}" ]; then
        set -- "$@" --host "$LISTEN_HOST"
    elif [ "${HOST+x}" ]; then
        set -- "$@" --host "$HOST"
    fi
fi

if [ "${ALLOW+x}" ] && [ -n "$ALLOW" ]; then
    OLD_IFS="$IFS"
    IFS=", "
    for rule in $ALLOW; do
        if [ -n "$rule" ]; then
            set -- "$@" --allow "$rule"
        fi
    done
    IFS="$OLD_IFS"
fi

exec "$@"
