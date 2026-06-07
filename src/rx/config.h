#ifndef MJPEG_RX_CONFIG_H
#define MJPEG_RX_CONFIG_H

struct rx_config {
    const char *host;
    const char *listen_host;
    const char *port;
    int use_udp;
    const char *event_host;
    const char *event_port;
    const char *joystick_device;
    const char *overlay_path;
    const char *hud_color;
    const char *hud_font;
    const char *dim_color;
    double dim_alpha;
    const char *telemetry_host;
    const char *telemetry_port;
    int events_enabled;
    int joystick_enabled;
    int telemetry_enabled;
    int overlay_required;
    int lock_aspect;
};

void rx_usage(const char *argv0);
int rx_parse_args(int argc, char **argv, struct rx_config *cfg);

#endif
