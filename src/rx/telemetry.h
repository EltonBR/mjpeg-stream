#ifndef MJPEG_RX_TELEMETRY_H
#define MJPEG_RX_TELEMETRY_H

#include "overlay.h"

#include <gtk/gtk.h>

struct telemetry_client {
    int enabled;
    int stopping;
    const char *host;
    const char *port;
    int fd;
    GThread *thread;
    struct overlay_state *overlay;
    GtkWidget *drawing_area;
};

void telemetry_init(struct telemetry_client *client);
void telemetry_start(struct telemetry_client *client, const char *host,
                     const char *port, struct overlay_state *overlay,
                     GtkWidget *drawing_area);
void telemetry_close(struct telemetry_client *client);

#endif
