#include "app.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void rx_app_init(struct rx_app *app, int use_udp, const char *joystick_device,
                 int joystick_enabled)
{
    memset(app, 0, sizeof(*app));
    app->fd = -1;
    app->listen_fd = -1;
    app->use_udp = use_udp;
    app->frame = g_byte_array_new();
    app->zoom = 1.0;
    app->joystick_device = joystick_device;
    app->joystick_enabled = joystick_enabled;
    event_sender_init(&app->events);
}

void rx_app_cleanup(struct rx_app *app)
{
    app->stopping = 1;
    if (app->joystick_thread) {
        g_thread_join(app->joystick_thread);
        app->joystick_thread = NULL;
    }
    event_sender_close(&app->events);

    free(app->udp_seen);
    app->udp_seen = NULL;

    if (app->last_pixbuf) {
        g_object_unref(app->last_pixbuf);
        app->last_pixbuf = NULL;
    }
    if (app->frame) {
        g_byte_array_unref(app->frame);
        app->frame = NULL;
    }
    if (app->fd >= 0) {
        close(app->fd);
        app->fd = -1;
    }
    if (app->listen_fd >= 0) {
        close(app->listen_fd);
        app->listen_fd = -1;
    }
}
