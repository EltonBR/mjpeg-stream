#include "config.h"

#include "ini.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void rx_defaults(struct rx_config *cfg)
{
    cfg->host = "127.0.0.1";
    cfg->listen_host = "0.0.0.0";
    cfg->port = "5000";
    cfg->use_udp = 0;
    cfg->event_host = NULL;
    cfg->event_port = NULL;
    cfg->joystick_device = "/dev/input/js0";
    cfg->overlay_path = NULL;
    cfg->hud_color = "green";
    cfg->hud_font = "Monospace";
    cfg->dim_color = "#000000";
    cfg->dim_alpha = 0.20;
    cfg->zoom_in_key = "plus";
    cfg->zoom_out_key = "minus";
    cfg->dim_alpha_up_key = "";
    cfg->dim_alpha_down_key = "";
    cfg->dim_alpha_step = 0.05;
    cfg->telemetry_host = "127.0.0.1";
    cfg->telemetry_port = "7000";
    cfg->events_enabled = 0;
    cfg->joystick_enabled = 0;
    cfg->telemetry_enabled = 0;
    cfg->overlay_required = 0;
    cfg->lock_aspect = 0;
}

static int parse_bool(const char *value)
{
    return strcmp(value, "1") == 0 ||
           strcmp(value, "yes") == 0 ||
           strcmp(value, "true") == 0 ||
           strcmp(value, "on") == 0;
}

static int rx_ini_handler(void *user, const char *section,
                          const char *key, const char *value)
{
    struct rx_config *cfg = user;
    (void)section;

    if (strcmp(key, "host") == 0) {
        cfg->host = strdup(value);
    } else if (strcmp(key, "listen_host") == 0) {
        cfg->listen_host = strdup(value);
    } else if (strcmp(key, "port") == 0) {
        cfg->port = strdup(value);
    } else if (strcmp(key, "protocol") == 0) {
        if (strcmp(value, "udp") == 0 || strcmp(value, "UDP") == 0) {
            cfg->use_udp = 1;
        } else if (strcmp(value, "tcp") == 0 || strcmp(value, "TCP") == 0) {
            cfg->use_udp = 0;
        } else {
            return -1;
        }
    } else if (strcmp(key, "event_host") == 0) {
        cfg->event_host = strdup(value);
    } else if (strcmp(key, "event_port") == 0) {
        cfg->event_port = strdup(value);
    } else if (strcmp(key, "events_enabled") == 0) {
        cfg->events_enabled = parse_bool(value);
    } else if (strcmp(key, "joystick") == 0 ||
               strcmp(key, "joystick_device") == 0) {
        cfg->joystick_device = strdup(value);
    } else if (strcmp(key, "joystick_enabled") == 0) {
        cfg->joystick_enabled = parse_bool(value);
    } else if (strcmp(key, "overlay") == 0) {
        cfg->overlay_path = strdup(value);
    } else if (strcmp(key, "hud_color") == 0) {
        cfg->hud_color = strdup(value);
    } else if (strcmp(key, "hud_font") == 0) {
        cfg->hud_font = strdup(value);
    } else if (strcmp(key, "dim_color") == 0) {
        cfg->dim_color = strdup(value);
    } else if (strcmp(key, "dim_alpha") == 0) {
        cfg->dim_alpha = strtod(value, NULL);
    } else if (strcmp(key, "zoom_in_key") == 0) {
        cfg->zoom_in_key = strdup(value);
    } else if (strcmp(key, "zoom_out_key") == 0) {
        cfg->zoom_out_key = strdup(value);
    } else if (strcmp(key, "dim_alpha_up_key") == 0) {
        cfg->dim_alpha_up_key = strdup(value);
    } else if (strcmp(key, "dim_alpha_down_key") == 0) {
        cfg->dim_alpha_down_key = strdup(value);
    } else if (strcmp(key, "dim_alpha_step") == 0) {
        cfg->dim_alpha_step = strtod(value, NULL);
    } else if (strcmp(key, "telemetry_enabled") == 0) {
        cfg->telemetry_enabled = parse_bool(value);
    } else if (strcmp(key, "telemetry_host") == 0) {
        cfg->telemetry_host = strdup(value);
    } else if (strcmp(key, "telemetry_port") == 0) {
        cfg->telemetry_port = strdup(value);
    } else if (strcmp(key, "lock_aspect") == 0) {
        cfg->lock_aspect = parse_bool(value);
    }
    return 0;
}

void rx_usage(const char *argv0)
{
    fprintf(stderr,
            "Uso TCP: %s [--config rx.ini] --host 127.0.0.1|::1 --port 5000 --tcp\n"
            "Uso UDP: %s --listen 0.0.0.0|:: --port 5000 --udp\n"
            "Eventos: --event-host 127.0.0.1|::1 --event-port 6000 [--joystick /dev/input/js0]\n"
            "Overlay: --overlay overlay.json [--hud-color green|amber|#rrggbb] [--hud-font Monospace] [--dim-color #000000 --dim-alpha 0.20]\n"
            "Telemetria: [--telemetry-enabled --telemetry-host 127.0.0.1 --telemetry-port 7000]\n"
            "Janela: [--lock-aspect] [--zoom-in-key plus --zoom-out-key minus] [--dim-alpha-up-key Page_Up --dim-alpha-down-key Page_Down --dim-alpha-step 0.05]\n",
            argv0, argv0);
}

int rx_parse_args(int argc, char **argv, struct rx_config *cfg)
{
    int i;
    const char *config_path = "rx.ini";
    int config_explicit = 0;

    rx_defaults(cfg);

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
            config_path = argv[++i];
            config_explicit = 1;
        }
    }

    if (access(config_path, R_OK) == 0) {
        if (ini_parse_file(config_path, rx_ini_handler, cfg) < 0) {
            return -1;
        }
    } else if (config_explicit) {
        perror(config_path);
        return -1;
    }

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
            i++;
        } else if (strcmp(argv[i], "--host") == 0 && i + 1 < argc) {
            cfg->host = argv[++i];
        } else if (strcmp(argv[i], "--listen") == 0 && i + 1 < argc) {
            cfg->listen_host = argv[++i];
        } else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            cfg->port = argv[++i];
        } else if (strcmp(argv[i], "--udp") == 0) {
            cfg->use_udp = 1;
        } else if (strcmp(argv[i], "--tcp") == 0) {
            cfg->use_udp = 0;
        } else if (strcmp(argv[i], "--event-host") == 0 && i + 1 < argc) {
            cfg->event_host = argv[++i];
            cfg->events_enabled = 1;
        } else if (strcmp(argv[i], "--event-port") == 0 && i + 1 < argc) {
            cfg->event_port = argv[++i];
            cfg->events_enabled = 1;
        } else if (strcmp(argv[i], "--joystick") == 0 && i + 1 < argc) {
            cfg->joystick_device = argv[++i];
            cfg->joystick_enabled = 1;
        } else if (strcmp(argv[i], "--overlay") == 0 && i + 1 < argc) {
            cfg->overlay_path = argv[++i];
            cfg->overlay_required = 1;
        } else if (strcmp(argv[i], "--hud-color") == 0 && i + 1 < argc) {
            cfg->hud_color = argv[++i];
        } else if (strcmp(argv[i], "--hud-font") == 0 && i + 1 < argc) {
            cfg->hud_font = argv[++i];
        } else if (strcmp(argv[i], "--dim-color") == 0 && i + 1 < argc) {
            cfg->dim_color = argv[++i];
        } else if (strcmp(argv[i], "--dim-alpha") == 0 && i + 1 < argc) {
            cfg->dim_alpha = strtod(argv[++i], NULL);
        } else if (strcmp(argv[i], "--zoom-in-key") == 0 && i + 1 < argc) {
            cfg->zoom_in_key = argv[++i];
        } else if (strcmp(argv[i], "--zoom-out-key") == 0 && i + 1 < argc) {
            cfg->zoom_out_key = argv[++i];
        } else if (strcmp(argv[i], "--dim-alpha-up-key") == 0 && i + 1 < argc) {
            cfg->dim_alpha_up_key = argv[++i];
        } else if (strcmp(argv[i], "--dim-alpha-down-key") == 0 && i + 1 < argc) {
            cfg->dim_alpha_down_key = argv[++i];
        } else if (strcmp(argv[i], "--dim-alpha-step") == 0 && i + 1 < argc) {
            cfg->dim_alpha_step = strtod(argv[++i], NULL);
        } else if (strcmp(argv[i], "--telemetry-enabled") == 0) {
            cfg->telemetry_enabled = 1;
        } else if (strcmp(argv[i], "--telemetry-host") == 0 && i + 1 < argc) {
            cfg->telemetry_host = argv[++i];
        } else if (strcmp(argv[i], "--telemetry-port") == 0 && i + 1 < argc) {
            cfg->telemetry_port = argv[++i];
        } else if (strcmp(argv[i], "--lock-aspect") == 0) {
            cfg->lock_aspect = 1;
        } else {
            return -1;
        }
    }
    if (cfg->events_enabled && (!cfg->event_host || !cfg->event_port)) {
        return -1;
    }
    if (cfg->telemetry_enabled && (!cfg->telemetry_host || !cfg->telemetry_port)) {
        return -1;
    }
    return 0;
}
