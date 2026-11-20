#ifndef MJPEG_RX_CONFIG_H
#define MJPEG_RX_CONFIG_H

struct rx_config {
    /* All non-NULL string fields are owned by this config. */
    char *host;
    char *listen_host;
    char *port;
    int use_udp;
    char *event_host;
    char *event_port;
    char *joystick_device;
    char *overlay_path;
    char *hud_color;
    char *hud_font;
    char *dim_color;
    double dim_alpha;
    char *zoom_in_key;
    char *zoom_out_key;
    char *dim_alpha_up_key;
    char *dim_alpha_down_key;
    double dim_alpha_step;
    char *telemetry_host;
    char *telemetry_port;
    int events_enabled;
    int joystick_enabled;
    int telemetry_enabled;
    int overlay_required;
    int lock_aspect;
};

void rx_usage(const char *argv0);
int rx_parse_args(int argc, char **argv, struct rx_config *cfg);
void rx_config_destroy(struct rx_config *cfg);

#endif
