#include "config.h"

#include "ini.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int replace_string(char **target, const char *value)
{
    char *copy = strdup(value);
    if (!copy) {
        return -1;
    }
    free(*target);
    *target = copy;
    return 0;
}

void tx_config_destroy(struct config *cfg)
{
    if (!cfg) {
        return;
    }
    free(cfg->device);
    free(cfg->host);
    free(cfg->port);
    cfg->device = NULL;
    cfg->host = NULL;
    cfg->port = NULL;
}

static int tx_defaults(struct config *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    cfg->protocol = PROTO_TCP;
    cfg->width = 640;
    cfg->height = 480;
    cfg->fps = 30;
    cfg->quality = 80;
    access_control_init(&cfg->access);
    if (replace_string(&cfg->device, "/dev/video0") < 0 ||
        replace_string(&cfg->host, "0.0.0.0") < 0 ||
        replace_string(&cfg->port, "5000") < 0) {
        tx_config_destroy(cfg);
        return -1;
    }
    return 0;
}

static int set_protocol(struct config *cfg, const char *value)
{
    if (strcmp(value, "tcp") == 0 || strcmp(value, "TCP") == 0) {
        cfg->protocol = PROTO_TCP;
    } else if (strcmp(value, "udp") == 0 || strcmp(value, "UDP") == 0) {
        cfg->protocol = PROTO_UDP;
    } else if (strcmp(value, "http") == 0 || strcmp(value, "HTTP") == 0) {
        cfg->protocol = PROTO_HTTP;
    } else {
        return -1;
    }
    return 0;
}

static int tx_ini_handler(void *user, const char *section,
                          const char *key, const char *value)
{
    struct config *cfg = user;
    (void)section;

    if (strcmp(key, "device") == 0) {
        return replace_string(&cfg->device, value);
    } else if (strcmp(key, "host") == 0 || strcmp(key, "listen_host") == 0) {
        return replace_string(&cfg->host, value);
    } else if (strcmp(key, "port") == 0) {
        return replace_string(&cfg->port, value);
    } else if (strcmp(key, "protocol") == 0) {
        return set_protocol(cfg, value);
    } else if (strcmp(key, "width") == 0) {
        cfg->width = (unsigned)atoi(value);
    } else if (strcmp(key, "height") == 0) {
        cfg->height = (unsigned)atoi(value);
    } else if (strcmp(key, "fps") == 0) {
        cfg->fps = (unsigned)atoi(value);
    } else if (strcmp(key, "quality") == 0) {
        cfg->quality = atoi(value);
    } else if (strcmp(key, "allow") == 0) {
        return access_control_add(&cfg->access, value);
    }
    return 0;
}

void tx_usage(const char *argv0)
{
    fprintf(stderr,
            "Uso: %s [--config tx.ini] --device /dev/video0 --host 0.0.0.0|:: --port 5000 [--tcp|--udp|--http]\n"
            "       [--width 640 --height 480 --fps 30 --quality 80]\n"
            "       [--allow all|IP|IP-IP|IP/CIDR] ... aceita IPv4 e IPv6 literais\n"
            "       --http expoe MJPEG compativel com navegador em http://host:port/\n"
            "       Tenta MJPEG nativo primeiro; quality so afeta fallback YUYV/RGB24.\n",
            argv0);
}

int tx_parse_args(int argc, char **argv, struct config *cfg)
{
    int i;
    const char *config_path = "tx.ini";
    int config_explicit = 0;

    if (tx_defaults(cfg) < 0) {
        return -1;
    }

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
            config_path = argv[++i];
            config_explicit = 1;
        }
    }

    if (access(config_path, R_OK) == 0) {
        if (ini_parse_file(config_path, tx_ini_handler, cfg) < 0) {
            tx_config_destroy(cfg);
            return -1;
        }
    } else if (config_explicit) {
        perror(config_path);
        tx_config_destroy(cfg);
        return -1;
    }

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
            i++;
        } else if (strcmp(argv[i], "--device") == 0 && i + 1 < argc) {
            if (replace_string(&cfg->device, argv[++i]) < 0) {
                tx_config_destroy(cfg);
                return -1;
            }
        } else if (strcmp(argv[i], "--host") == 0 && i + 1 < argc) {
            if (replace_string(&cfg->host, argv[++i]) < 0) {
                tx_config_destroy(cfg);
                return -1;
            }
        } else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            if (replace_string(&cfg->port, argv[++i]) < 0) {
                tx_config_destroy(cfg);
                return -1;
            }
        } else if (strcmp(argv[i], "--width") == 0 && i + 1 < argc) {
            cfg->width = (unsigned)atoi(argv[++i]);
        } else if (strcmp(argv[i], "--height") == 0 && i + 1 < argc) {
            cfg->height = (unsigned)atoi(argv[++i]);
        } else if (strcmp(argv[i], "--fps") == 0 && i + 1 < argc) {
            cfg->fps = (unsigned)atoi(argv[++i]);
        } else if (strcmp(argv[i], "--quality") == 0 && i + 1 < argc) {
            cfg->quality = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--udp") == 0) {
            (void)set_protocol(cfg, "udp");
        } else if (strcmp(argv[i], "--tcp") == 0) {
            (void)set_protocol(cfg, "tcp");
        } else if (strcmp(argv[i], "--http") == 0) {
            (void)set_protocol(cfg, "http");
        } else if (strcmp(argv[i], "--allow") == 0 && i + 1 < argc) {
            if (access_control_add(&cfg->access, argv[++i]) < 0) {
                tx_config_destroy(cfg);
                return -1;
            }
        } else {
            tx_config_destroy(cfg);
            return -1;
        }
    }

    if (cfg->width == 0 || cfg->height == 0 || cfg->fps == 0 ||
        cfg->quality < 1 || cfg->quality > 100) {
        tx_config_destroy(cfg);
        return -1;
    }
    return 0;
}

const char *protocol_name(enum protocol protocol)
{
    switch (protocol) {
    case PROTO_UDP:
        return "UDP";
    case PROTO_HTTP:
        return "HTTP";
    case PROTO_TCP:
    default:
        return "TCP";
    }
}
