#ifndef MJPEG_RX_OVERLAY_CONFIG_H
#define MJPEG_RX_OVERLAY_CONFIG_H

struct overlay_config {
    char *layout_path;
    char *assets_dir;
    char *base_dir;
    char *hud_color;
    char *hud_font;
    char *dim_color;
    double dim_alpha;
    char *telemetry_host;
    char *telemetry_port;
    int reconnect_ms;
    int enabled;
    int telemetry_enabled;
    int layout_required;
    int assets_dir_explicit;
};

int overlay_config_parse_args(int argc, char **argv,
                              struct overlay_config *cfg);
void overlay_config_destroy(struct overlay_config *cfg);

#endif
