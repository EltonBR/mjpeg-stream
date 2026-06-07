#!/bin/sh
set -eu

CONFIG="${CONFIG:-rx.ini}"

if [ ! -x ./mjpeg_rx ]; then
    make mjpeg_rx
fi

set -- ./mjpeg_rx --config "$CONFIG"

if [ "${PROTO+x}" ]; then
    case "$PROTO" in
        tcp|TCP)
            set -- "$@" --tcp
            ;;
        udp|UDP)
            set -- "$@" --udp
            ;;
        *)
            echo "PROTO deve ser tcp ou udp" >&2
            exit 2
            ;;
    esac
fi

if [ "${HOST+x}" ]; then
    set -- "$@" --host "$HOST"
fi
if [ "${LISTEN_HOST+x}" ]; then
    set -- "$@" --listen "$LISTEN_HOST"
fi
if [ "${PORT+x}" ]; then
    set -- "$@" --port "$PORT"
fi

if [ "${EVENT_HOST+x}" ] || [ "${EVENT_PORT+x}" ]; then
    if [ ! "${EVENT_HOST+x}" ] || [ ! "${EVENT_PORT+x}" ]; then
        echo "EVENT_HOST e EVENT_PORT devem ser definidos juntos" >&2
        exit 2
    fi
    set -- "$@" --event-host "$EVENT_HOST" --event-port "$EVENT_PORT"
fi

if [ "${JOYSTICK+x}" ]; then
    set -- "$@" --joystick "$JOYSTICK"
fi

if [ "${OVERLAY+x}" ]; then
    set -- "$@" --overlay "$OVERLAY"
fi
if [ "${HUD_COLOR+x}" ]; then
    set -- "$@" --hud-color "$HUD_COLOR"
fi

if [ "${TELEMETRY_ENABLED+x}" ]; then
    case "$TELEMETRY_ENABLED" in
        1|yes|true|on|YES|TRUE|ON)
            set -- "$@" --telemetry-enabled
            ;;
    esac
fi
if [ "${TELEMETRY_HOST+x}" ]; then
    set -- "$@" --telemetry-host "$TELEMETRY_HOST"
fi
if [ "${TELEMETRY_PORT+x}" ]; then
    set -- "$@" --telemetry-port "$TELEMETRY_PORT"
fi

exec "$@"
