#ifndef MJPEG_RX_APP_H
#define MJPEG_RX_APP_H

#include "event_sender.h"
#include "overlay.h"
#include "telemetry.h"

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
    GByteArray *frame;
    uint32_t udp_frame_id;
    uint16_t udp_chunk_count;
    uint16_t udp_received;
    unsigned char *udp_seen;
    struct event_sender events;
    struct overlay_state overlay;
    struct telemetry_client telemetry;
    const char *joystick_device;
    GThread *joystick_thread;
    int joystick_enabled;
    volatile int stopping;
};

void rx_app_init(struct rx_app *app, int use_udp, const char *joystick_device,
                 int joystick_enabled, int lock_aspect,
                 const char *zoom_in_key, const char *zoom_out_key,
                 const char *dim_alpha_up_key,
                 const char *dim_alpha_down_key,
                 double dim_alpha_step);
void rx_app_cleanup(struct rx_app *app);

#endif
