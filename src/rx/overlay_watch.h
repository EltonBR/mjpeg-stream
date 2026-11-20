#ifndef MJPEG_RX_OVERLAY_WATCH_H
#define MJPEG_RX_OVERLAY_WATCH_H
#include "overlay.h"
#include <gio/gio.h>

struct overlay_watch {
    GFileMonitor *monitor;
    char *path;
    struct overlay_state *overlay;
    GtkWidget *widget;
    guint timeout;
};
int overlay_watch_start(struct overlay_watch *watch, struct overlay_state *overlay,
                        GtkWidget *widget, const char *path);
void overlay_watch_stop(struct overlay_watch *watch);
#endif
