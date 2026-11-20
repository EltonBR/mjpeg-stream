#ifndef MJPEG_RX_APP_H
#define MJPEG_RX_APP_H

#include "event_sender.h"
#include "overlay.h"
#include "telemetry.h"
#include "telemetry_store.h"

#include <gtk/gtk.h>
#include <stdint.h>

struct rx_app {
    GtkWidget *image;
    GtkWidget *drawing_area;
    GtkWidget *zoom_label;
    GtkWidget *dim_label;
    GtkWidget *dim_scale;
    GdkPixbuf *last_pixbuf;
    double zoom;
    const char *zoom_in_key;
    const char *zoom_out_key;
    const char *dim_alpha_up_key;
    const char *dim_alpha_down_key;
    double dim_alpha_step;
    int lock_aspect;
    int aspect_frame_w;
    int aspect_frame_h;
    int fd;
    int listen_fd;
    int use_udp;
    int overlay_enabled;
    GThread *receiver_thread;
    gint receiver_stopping;
    GMutex receiver_mutex;
    int receiver_cancel[2];
    int receiver_notify[2];
    guint receiver_source;
    gint receiver_done;
    const char *receiver_host;
    const char *receiver_port;
    gint receiver_initial_failed;
    GMutex frame_mutex;
    unsigned char *pending_frame;
    size_t pending_frame_len;
    guint frame_source;
    struct event_sender events;
    struct overlay_state overlay;
    TelemetryStore *telemetry_store;
    struct telemetry_client telemetry;
    const char *joystick_device;
    GThread *joystick_thread;
    int joystick_enabled;
    gint stopping;
};

void rx_app_init(struct rx_app *app, int use_udp, const char *joystick_device,
                 int joystick_enabled, int lock_aspect,
                 const char *zoom_in_key, const char *zoom_out_key,
                 const char *dim_alpha_up_key,
                 const char *dim_alpha_down_key,
                 double dim_alpha_step);
void rx_app_cleanup(struct rx_app *app);

#endif
