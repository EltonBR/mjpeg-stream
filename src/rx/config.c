#include "config.h"

#include "ini.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int replace_string(char **target, const char *value)
{
    char *copy = value ? strdup(value) : NULL;
    if (value && !copy) return -1;
    free(*target);
    *target = copy;
    return 0;
}

void rx_config_destroy(struct rx_config *cfg)
{
    if (!cfg) return;
    free(cfg->host); free(cfg->listen_host); free(cfg->port);
    free(cfg->event_host); free(cfg->event_port); free(cfg->joystick_device);
    free(cfg->zoom_in_key); free(cfg->zoom_out_key);
    free(cfg->dim_alpha_up_key); free(cfg->dim_alpha_down_key);
    memset(cfg, 0, sizeof(*cfg));
}

static int rx_defaults(struct rx_config *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    cfg->dim_alpha_step = 0.05;
    if (replace_string(&cfg->host, "127.0.0.1") < 0 ||
        replace_string(&cfg->listen_host, "0.0.0.0") < 0 ||
        replace_string(&cfg->port, "5000") < 0 ||
        replace_string(&cfg->joystick_device, "/dev/input/js0") < 0 ||
        replace_string(&cfg->zoom_in_key, "plus") < 0 ||
        replace_string(&cfg->zoom_out_key, "minus") < 0 ||
        replace_string(&cfg->dim_alpha_up_key, "") < 0 ||
        replace_string(&cfg->dim_alpha_down_key, "") < 0) {
        rx_config_destroy(cfg);
        return -1;
    }
    return 0;
}

static int parse_bool(const char *value, int *out)
{
    if (!strcmp(value, "1") || !strcmp(value, "yes") ||
        !strcmp(value, "true") || !strcmp(value, "on")) *out = 1;
    else if (!strcmp(value, "0") || !strcmp(value, "no") ||
             !strcmp(value, "false") || !strcmp(value, "off")) *out = 0;
    else return -1;
    return 0;
}

static int parse_number(const char *value, double *out)
{
    char *end;
    errno = 0;
    *out = strtod(value, &end);
    return errno || end == value || *end || !isfinite(*out) ? -1 : 0;
}

static int valid_port(const char *value)
{
    char *end;
    long port;
    errno = 0;
    port = strtol(value, &end, 10);
    return !errno && end != value && !*end && port >= 1 && port <= 65535;
}

static int rx_ini_handler(void *user, const char *section,
                          const char *key, const char *value)
{
    struct rx_config *cfg = user;
    if (!strcmp(section, "stream")) {
        if (!strcmp(key, "host")) return replace_string(&cfg->host, value);
        if (!strcmp(key, "listen_host")) return replace_string(&cfg->listen_host, value);
        if (!strcmp(key, "port")) return replace_string(&cfg->port, value);
        if (!strcmp(key, "protocol")) {
            if (!strcmp(value, "udp") || !strcmp(value, "UDP")) cfg->use_udp = 1;
            else if (!strcmp(value, "tcp") || !strcmp(value, "TCP")) cfg->use_udp = 0;
            else return -1;
        }
    } else if (!strcmp(section, "events")) {
        if (!strcmp(key, "event_host")) return replace_string(&cfg->event_host, value);
        if (!strcmp(key, "event_port")) return replace_string(&cfg->event_port, value);
        if (!strcmp(key, "events_enabled")) return parse_bool(value, &cfg->events_enabled);
        if (!strcmp(key, "joystick") || !strcmp(key, "joystick_device"))
            return replace_string(&cfg->joystick_device, value);
        if (!strcmp(key, "joystick_enabled")) return parse_bool(value, &cfg->joystick_enabled);
    } else if (!strcmp(section, "window")) {
        if (!strcmp(key, "zoom_in_key")) return replace_string(&cfg->zoom_in_key, value);
        if (!strcmp(key, "zoom_out_key")) return replace_string(&cfg->zoom_out_key, value);
        if (!strcmp(key, "dim_alpha_up_key")) return replace_string(&cfg->dim_alpha_up_key, value);
        if (!strcmp(key, "dim_alpha_down_key")) return replace_string(&cfg->dim_alpha_down_key, value);
        if (!strcmp(key, "dim_alpha_step")) {
            if (parse_number(value, &cfg->dim_alpha_step) < 0 || cfg->dim_alpha_step <= 0)
                return -1;
        }
        if (!strcmp(key, "lock_aspect")) return parse_bool(value, &cfg->lock_aspect);
    }
    return 0;
}

void rx_usage(const char *argv0)
{
    fprintf(stderr,
            "Uso TCP: %s [--config config/rx.ini] --host 127.0.0.1|::1 --port 5000 --tcp\n"
            "Uso UDP: %s --listen 0.0.0.0|:: --port 5000 --udp\n"
            "Eventos: --event-host 127.0.0.1|::1 --event-port 6000 [--joystick /dev/input/js0]\n"
            "Overlay: [--overlay-config config/overlay.ini] [--overlay config/overlay.json] [--hud-color #rrggbb]\n"
            "Telemetria: [--telemetry-enabled --telemetry-host 127.0.0.1 --telemetry-port 7000]\n"
            "Janela: [--lock-aspect] [--zoom-in-key plus --zoom-out-key minus]\n", argv0, argv0);
}

static int set_cli_string(char **p, const char *v) { return replace_string(p, v); }

int rx_parse_args(int argc, char **argv, struct rx_config *cfg)
{
    const char *config_path = "config/rx.ini";
    int explicit_config = 0;
    int i;
    if (rx_defaults(cfg) < 0) return -1;
    for (i = 1; i < argc; i++) if (!strcmp(argv[i], "--config") && i + 1 < argc) {
        config_path = argv[++i]; explicit_config = 1;
    }
    if (access(config_path, R_OK) == 0) {
        if (ini_parse_file(config_path, rx_ini_handler, cfg) < 0) goto fail;
    } else if (explicit_config) { perror(config_path); goto fail; }

    for (i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if ((!strcmp(arg, "--config") || !strcmp(arg, "--overlay-config") ||
             !strcmp(arg, "--overlay") || !strcmp(arg, "--hud-color") ||
             !strcmp(arg, "--hud-font") || !strcmp(arg, "--dim-color") ||
             !strcmp(arg, "--dim-alpha") || !strcmp(arg, "--telemetry-host") ||
             !strcmp(arg, "--telemetry-port") || !strcmp(arg, "--host") ||
             !strcmp(arg, "--listen") || !strcmp(arg, "--port") ||
             !strcmp(arg, "--event-host") || !strcmp(arg, "--event-port") ||
             !strcmp(arg, "--joystick") || !strcmp(arg, "--zoom-in-key") ||
             !strcmp(arg, "--zoom-out-key") || !strcmp(arg, "--dim-alpha-up-key") ||
             !strcmp(arg, "--dim-alpha-down-key") || !strcmp(arg, "--dim-alpha-step")) &&
            i + 1 >= argc) goto fail;
        if (!strcmp(arg, "--config")) { i++; continue; }
        if (!strcmp(arg, "--overlay-config") || !strcmp(arg, "--overlay") ||
            !strcmp(arg, "--hud-color") || !strcmp(arg, "--hud-font") ||
            !strcmp(arg, "--dim-color") || !strcmp(arg, "--dim-alpha") ||
            !strcmp(arg, "--telemetry-host") || !strcmp(arg, "--telemetry-port")) { i++; continue; }
        if (!strcmp(arg, "--host")) { if (set_cli_string(&cfg->host, argv[++i]) < 0) goto fail; }
        else if (!strcmp(arg, "--listen")) { if (set_cli_string(&cfg->listen_host, argv[++i]) < 0) goto fail; }
        else if (!strcmp(arg, "--port")) { if (set_cli_string(&cfg->port, argv[++i]) < 0) goto fail; }
        else if (!strcmp(arg, "--tcp")) cfg->use_udp = 0;
        else if (!strcmp(arg, "--udp")) cfg->use_udp = 1;
        else if (!strcmp(arg, "--event-host")) { if (set_cli_string(&cfg->event_host, argv[++i]) < 0) goto fail; cfg->events_enabled = 1; }
        else if (!strcmp(arg, "--event-port")) { if (set_cli_string(&cfg->event_port, argv[++i]) < 0) goto fail; cfg->events_enabled = 1; }
        else if (!strcmp(arg, "--joystick")) { if (set_cli_string(&cfg->joystick_device, argv[++i]) < 0) goto fail; cfg->joystick_enabled = 1; }
        else if (!strcmp(arg, "--zoom-in-key")) { if (set_cli_string(&cfg->zoom_in_key, argv[++i]) < 0) goto fail; }
        else if (!strcmp(arg, "--zoom-out-key")) { if (set_cli_string(&cfg->zoom_out_key, argv[++i]) < 0) goto fail; }
        else if (!strcmp(arg, "--dim-alpha-up-key")) { if (set_cli_string(&cfg->dim_alpha_up_key, argv[++i]) < 0) goto fail; }
        else if (!strcmp(arg, "--dim-alpha-down-key")) { if (set_cli_string(&cfg->dim_alpha_down_key, argv[++i]) < 0) goto fail; }
        else if (!strcmp(arg, "--dim-alpha-step")) {
            double parsed;
            if (i + 1 >= argc || parse_number(argv[++i], &parsed) < 0 || parsed <= 0.0) goto fail;
            cfg->dim_alpha_step = parsed;
        }
        else if (!strcmp(arg, "--lock-aspect")) cfg->lock_aspect = 1;
        else if (!strcmp(arg, "--telemetry-enabled")) continue;
        else if (!strcmp(arg, "--help") || arg[0] == '-') goto fail;
    }
    if (!valid_port(cfg->port) ||
        (cfg->event_port && !valid_port(cfg->event_port)) ||
        (cfg->events_enabled && (!cfg->event_host || !cfg->event_port))) goto fail;
    return 0;
fail:
    rx_config_destroy(cfg);
    return -1;
}
