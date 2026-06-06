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
    int events_enabled;
    int joystick_enabled;
};

void rx_usage(const char *argv0);
int rx_parse_args(int argc, char **argv, struct rx_config *cfg);

#endif
