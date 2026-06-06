#ifndef MJPEG_RX_APP_H
#define MJPEG_RX_APP_H

#include "event_sender.h"

#include <gtk/gtk.h>
#include <stdint.h>

struct rx_app {
    GtkWidget *image;
    GtkWidget *zoom_label;
    GdkPixbuf *last_pixbuf;
    double zoom;
    int fd;
    int listen_fd;
    int use_udp;
    GByteArray *frame;
    uint32_t udp_frame_id;
    uint16_t udp_chunk_count;
    uint16_t udp_received;
    unsigned char *udp_seen;
    struct event_sender events;
    const char *joystick_device;
    GThread *joystick_thread;
    int joystick_enabled;
    volatile int stopping;
};

void rx_app_init(struct rx_app *app, int use_udp, const char *joystick_device,
                 int joystick_enabled);
void rx_app_cleanup(struct rx_app *app);

#endif
