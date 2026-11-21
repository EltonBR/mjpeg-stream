#include "overlay_config.h"

#include "../common/ini.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct parse_context {
    struct overlay_config *cfg;
    int legacy_warned;
    int legacy_layout_set;
};

static int set_string(char **target, const char *value)
{
    char *copy = strdup(value);
    if (!copy) return -1;
    free(*target);
    *target = copy;
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

static int parse_double(const char *value, double *out)
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

static int parse_reconnect_ms(const char *value, int *out)
{
    char *end;
    long parsed;
    errno = 0;
    parsed = strtol(value, &end, 10);
    if (errno || end == value || *end || parsed < 1 || parsed > 600000) return -1;
    *out = (int)parsed;
    return 0;
}

static char *absolute_path(const char *path)
{
    char cwd[4096];
    char *result;
    if (path[0] == '/') return strdup(path);
    if (!getcwd(cwd, sizeof(cwd))) return NULL;
    if (asprintf(&result, "%s/%s", cwd, path) < 0) return NULL;
    return result;
}

static char *path_dirname(const char *path)
{
    const char *slash = strrchr(path, '/');
    if (!slash) return strdup(".");
    if (slash == path) return strdup("/");
    return strndup(path, (size_t)(slash - path));
}

static char *join_path(const char *dir, const char *path)
{
    char *result;
    if (path[0] == '/') return strdup(path);
    if (asprintf(&result, "%s/%s", dir, path) < 0) return NULL;
    return result;
}

static int parse_rx_legacy(void *user, const char *section,
                           const char *key, const char *value)
{
    struct parse_context *ctx = user;
    struct overlay_config *cfg = ctx->cfg;
    int *bool_target = NULL;
    if ((!strcmp(section, "overlay") &&
         (!strcmp(key, "overlay") || !strcmp(key, "hud_color") ||
          !strcmp(key, "hud_font") || !strcmp(key, "dim_color") ||
          !strcmp(key, "dim_alpha"))) ||
        (!strcmp(section, "telemetry") &&
         (!strcmp(key, "telemetry_enabled") || !strcmp(key, "telemetry_host") ||
          !strcmp(key, "telemetry_port")))) {
        if (!ctx->legacy_warned) {
            fprintf(stderr, "config: chaves legadas de overlay/telemetry em rx.ini; migre para overlay.ini\n");
            ctx->legacy_warned = 1;
        }
    }
    if (!strcmp(section, "overlay")) {
        if (!strcmp(key, "overlay")) {
            if (set_string(&cfg->layout_path, value) < 0) return -1;
            cfg->enabled = 1;
            ctx->legacy_layout_set = 1;
        } else if (!strcmp(key, "hud_color")) return set_string(&cfg->hud_color, value);
        else if (!strcmp(key, "hud_font")) return set_string(&cfg->hud_font, value);
        else if (!strcmp(key, "dim_color")) return set_string(&cfg->dim_color, value);
        else if (!strcmp(key, "dim_alpha")) return parse_double(value, &cfg->dim_alpha);
        else return 0;
    } else if (!strcmp(section, "telemetry")) {
        if (!strcmp(key, "telemetry_enabled")) bool_target = &cfg->telemetry_enabled;
        else if (!strcmp(key, "telemetry_host")) return set_string(&cfg->telemetry_host, value);
        else if (!strcmp(key, "telemetry_port")) return set_string(&cfg->telemetry_port, value);
        else return 0;
        return parse_bool(value, bool_target);
    } else {
        return 0;
    }
    if (cfg->dim_alpha < 0.0 || cfg->dim_alpha > 1.0) return -1;
    return 0;
}

static int overlay_ini_handler(void *user, const char *section,
                               const char *key, const char *value)
{
    struct overlay_config *cfg = user;
    if (!strcmp(section, "overlay")) {
        if (!strcmp(key, "enabled")) return parse_bool(value, &cfg->enabled);
        if (!strcmp(key, "layout")) return set_string(&cfg->layout_path, value);
        if (!strcmp(key, "assets_dir")) {
            cfg->assets_dir_explicit = 1;
            return set_string(&cfg->assets_dir, value);
        }
    } else if (!strcmp(section, "appearance")) {
        if (!strcmp(key, "hud_color")) return set_string(&cfg->hud_color, value);
        if (!strcmp(key, "hud_font")) return set_string(&cfg->hud_font, value);
        if (!strcmp(key, "dim_color")) return set_string(&cfg->dim_color, value);
        if (!strcmp(key, "dim_alpha")) {
            if (parse_double(value, &cfg->dim_alpha) < 0) return -1;
            return cfg->dim_alpha >= 0.0 && cfg->dim_alpha <= 1.0 ? 0 : -1;
        }
    } else if (!strcmp(section, "telemetry")) {
        if (!strcmp(key, "enabled")) return parse_bool(value, &cfg->telemetry_enabled);
        if (!strcmp(key, "host")) return set_string(&cfg->telemetry_host, value);
        if (!strcmp(key, "port")) return set_string(&cfg->telemetry_port, value);
        if (!strcmp(key, "reconnect_ms")) return parse_reconnect_ms(value, &cfg->reconnect_ms);
    }
    return 0;
}

static int apply_cli(struct overlay_config *cfg, int argc, char **argv)
{
    int i;
    for (i = 1; i < argc; i++) {
        const char *arg = argv[i];
        char **target = NULL;
        if (!strcmp(arg, "--config")) { i++; continue; }
        if (!strcmp(arg, "--overlay-config")) { i++; continue; }
        if (!strcmp(arg, "--overlay")) {
            if (i + 1 >= argc || set_string(&cfg->layout_path, argv[++i]) < 0) return -1;
            cfg->enabled = 1; cfg->layout_required = 1; continue;
        }
        if (!strcmp(arg, "--hud-color")) target = &cfg->hud_color;
        else if (!strcmp(arg, "--hud-font")) target = &cfg->hud_font;
        else if (!strcmp(arg, "--dim-color")) target = &cfg->dim_color;
        else if (!strcmp(arg, "--telemetry-host")) target = &cfg->telemetry_host;
        else if (!strcmp(arg, "--telemetry-port")) target = &cfg->telemetry_port;
        if (target) {
            if (i + 1 >= argc || set_string(target, argv[++i]) < 0) return -1;
            continue;
        }
        if (!strcmp(arg, "--dim-alpha")) {
            if (i + 1 >= argc || parse_double(argv[++i], &cfg->dim_alpha) < 0 ||
                cfg->dim_alpha < 0.0 || cfg->dim_alpha > 1.0) return -1;
        } else if (!strcmp(arg, "--telemetry-enabled")) cfg->telemetry_enabled = 1;
        else if (!strcmp(arg, "--udp") || !strcmp(arg, "--tcp") ||
                 !strcmp(arg, "--lock-aspect") || !strcmp(arg, "--help")) continue;
        else if (!strcmp(arg, "--host") || !strcmp(arg, "--listen") ||
                 !strcmp(arg, "--port") || !strcmp(arg, "--event-host") ||
                 !strcmp(arg, "--event-port") || !strcmp(arg, "--joystick") ||
                 !strcmp(arg, "--zoom-in-key") || !strcmp(arg, "--zoom-out-key") ||
                 !strcmp(arg, "--dim-alpha-up-key") || !strcmp(arg, "--dim-alpha-down-key") ||
                 !strcmp(arg, "--dim-alpha-step")) { i++; }
    }
    return 0;
}

void overlay_config_destroy(struct overlay_config *cfg)
{
    if (!cfg) return;
    free(cfg->layout_path); free(cfg->assets_dir); free(cfg->base_dir);
    free(cfg->hud_color); free(cfg->hud_font); free(cfg->dim_color);
    free(cfg->telemetry_host); free(cfg->telemetry_port);
    memset(cfg, 0, sizeof(*cfg));
}

int overlay_config_parse_args(int argc, char **argv,
                              struct overlay_config *cfg)
{
    const char *rx_path = "config/rx.ini";
    const char *overlay_path_cli = NULL;
    char *rx_abs = NULL, *base = NULL, *overlay_path = NULL, *resolved = NULL;
    int rx_explicit = 0, overlay_explicit = 0, i;
    struct parse_context legacy_ctx;
    memset(cfg, 0, sizeof(*cfg));
    cfg->enabled = 1;
    cfg->telemetry_enabled = 1;
    cfg->dim_alpha = 0.20;
    cfg->reconnect_ms = 1000;
    if (set_string(&cfg->layout_path, "overlay.json") < 0 ||
        set_string(&cfg->assets_dir, "overlays") < 0 ||
        set_string(&cfg->hud_color, "#ffbf00") < 0 ||
        set_string(&cfg->hud_font, "Monospace") < 0 ||
        set_string(&cfg->dim_color, "#000000") < 0 ||
        set_string(&cfg->telemetry_host, "127.0.0.1") < 0 ||
        set_string(&cfg->telemetry_port, "7000") < 0) goto fail;
    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--config") && i + 1 < argc) { rx_path = argv[++i]; rx_explicit = 1; }
        else if (!strcmp(argv[i], "--overlay-config") && i + 1 < argc) { overlay_path_cli = argv[++i]; overlay_explicit = 1; }
    }
    rx_abs = absolute_path(rx_path);
    if (!rx_abs) goto fail;
    base = path_dirname(rx_abs);
    if (!base) goto fail;
    legacy_ctx.cfg = cfg; legacy_ctx.legacy_warned = 0;
    legacy_ctx.legacy_layout_set = 0;
    if (access(rx_path, R_OK) == 0 && ini_parse_file(rx_path, parse_rx_legacy, &legacy_ctx) < 0) goto fail;
    if (rx_explicit && access(rx_path, R_OK) != 0) goto fail;
    if (legacy_ctx.legacy_layout_set && cfg->layout_path[0] != '/') {
        resolved = join_path(base, cfg->layout_path);
        if (!resolved) goto fail;
        free(cfg->layout_path); cfg->layout_path = resolved; resolved = NULL;
    }

    overlay_path = overlay_path_cli ? absolute_path(overlay_path_cli) : join_path(base, "overlay.ini");
    if (!overlay_path) goto fail;
    if (access(overlay_path, R_OK) == 0) {
        if (ini_parse_file(overlay_path, overlay_ini_handler, cfg) < 0) goto fail;
        free(base); base = path_dirname(overlay_path);
        if (!base) goto fail;
    } else if (overlay_explicit) {
        fprintf(stderr, "config: overlay config ausente: %s\n", overlay_path);
        goto fail;
    }
    free(cfg->base_dir); cfg->base_dir = strdup(base);
    if (!cfg->base_dir) goto fail;

    if (cfg->layout_path[0] != '/') {
        resolved = join_path(base, cfg->layout_path);
        if (!resolved) goto fail;
        free(cfg->layout_path); cfg->layout_path = resolved; resolved = NULL;
    }
    if (cfg->assets_dir[0] == '/' || strstr(cfg->assets_dir, "..")) {
        fprintf(stderr, "config: assets_dir invalido: %s\n", cfg->assets_dir); goto fail;
    }
    resolved = join_path(base, cfg->assets_dir);
    if (!resolved) goto fail;
    free(cfg->assets_dir); cfg->assets_dir = resolved; resolved = NULL;
    if (!valid_port(cfg->telemetry_port)) {
        fprintf(stderr, "config: telemetry port invalida: %s\n", cfg->telemetry_port); goto fail;
    }
    if (apply_cli(cfg, argc, argv) < 0) goto fail;
    if (!valid_port(cfg->telemetry_port)) goto fail;
    free(rx_abs); free(base); free(overlay_path);
    return 0;
fail:
    free(rx_abs); free(base); free(overlay_path); free(resolved);
    overlay_config_destroy(cfg);
    return -1;
}
