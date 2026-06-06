#ifndef MJPEG_TX_CONFIG_H
#define MJPEG_TX_CONFIG_H

#include "access_control.h"

enum protocol {
    PROTO_TCP,
    PROTO_UDP,
    PROTO_HTTP
};

struct config {
    const char *device;
    const char *host;
    const char *port;
    enum protocol protocol;
    unsigned width;
    unsigned height;
    unsigned fps;
    int quality;
    struct access_control access;
};

void tx_usage(const char *argv0);
int tx_parse_args(int argc, char **argv, struct config *cfg);
const char *protocol_name(enum protocol protocol);

#endif
