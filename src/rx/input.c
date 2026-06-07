#include "input.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/joystick.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static const char *key_name(guint keyval)
{
    const char *name = gdk_keyval_name(keyval);
    return name ? name : "unknown";
}

static int mouse_image_coords(GtkWidget *widget, double event_x, double event_y,
                              double *rel_x, double *rel_y,
                              int *image_w, int *image_h)
{
    int width = gtk_widget_get_allocated_width(widget);
    int height = gtk_widget_get_allocated_height(widget);

    if (width <= 0 || height <= 0 ||
        event_x < 0.0 || event_y < 0.0 ||
        event_x >= (double)width || event_y >= (double)height) {
        return 0;
    }

    *rel_x = event_x / (double)width;
    *rel_y = event_y / (double)height;
    *image_w = width;
    *image_h = height;
    return 1;
}

static gboolean on_key_press(GtkWidget *widget, GdkEventKey *event,
                             gpointer data)
{
    struct rx_app *app = data;
    char json[256];
    (void)widget;

    snprintf(json, sizeof(json),
             "{\"origin\":\"keyboard\",\"type\":\"keydown\",\"keyval\":%u,"
             "\"key\":\"%s\",\"state\":%u}",
             event->keyval, key_name(event->keyval), event->state);
    event_sender_send(&app->events, json);
    return FALSE;
}

static gboolean on_key_release(GtkWidget *widget, GdkEventKey *event,
                               gpointer data)
{
    struct rx_app *app = data;
    char json[256];
    (void)widget;

    snprintf(json, sizeof(json),
             "{\"origin\":\"keyboard\",\"type\":\"keyup\",\"keyval\":%u,"
             "\"key\":\"%s\",\"state\":%u}",
             event->keyval, key_name(event->keyval), event->state);
    event_sender_send(&app->events, json);

    snprintf(json, sizeof(json),
             "{\"origin\":\"keyboard\",\"type\":\"keypress\",\"keyval\":%u,"
             "\"key\":\"%s\",\"state\":%u}",
             event->keyval, key_name(event->keyval), event->state);
    event_sender_send(&app->events, json);
    return FALSE;
}

static gboolean on_motion(GtkWidget *widget, GdkEventMotion *event,
                          gpointer data)
{
    struct rx_app *app = data;
    char json[512];
    double x;
    double y;
    int image_w;
    int image_h;

    if (!mouse_image_coords(widget, event->x, event->y, &x, &y,
                            &image_w, &image_h)) {
        return FALSE;
    }

    snprintf(json, sizeof(json),
             "{\"origin\":\"mouse\",\"type\":\"motion\",\"x\":%.6f,"
             "\"y\":%.6f,\"pixel_x\":%.2f,\"pixel_y\":%.2f,"
             "\"image_w\":%d,\"image_h\":%d,\"state\":%u}",
             x, y, event->x, event->y, image_w, image_h, event->state);
    event_sender_send(&app->events, json);
    return TRUE;
}

static gboolean on_button_press(GtkWidget *widget, GdkEventButton *event,
                                gpointer data)
{
    struct rx_app *app = data;
    char json[512];
    double x;
    double y;
    int image_w;
    int image_h;

    if (!mouse_image_coords(widget, event->x, event->y, &x, &y,
                            &image_w, &image_h)) {
        return FALSE;
    }

    snprintf(json, sizeof(json),
             "{\"origin\":\"mouse\",\"type\":\"mousedown\",\"button\":%u,"
             "\"x\":%.6f,\"y\":%.6f,\"pixel_x\":%.2f,\"pixel_y\":%.2f,"
             "\"image_w\":%d,\"image_h\":%d,\"state\":%u}",
             event->button, x, y, event->x, event->y,
             image_w, image_h, event->state);
    event_sender_send(&app->events, json);
    return TRUE;
}

static gboolean on_button_release(GtkWidget *widget, GdkEventButton *event,
                                  gpointer data)
{
    struct rx_app *app = data;
    char json[512];
    double x;
    double y;
    int image_w;
    int image_h;

    if (!mouse_image_coords(widget, event->x, event->y, &x, &y,
                            &image_w, &image_h)) {
        return FALSE;
    }

    snprintf(json, sizeof(json),
             "{\"origin\":\"mouse\",\"type\":\"mouseup\",\"button\":%u,"
             "\"x\":%.6f,\"y\":%.6f,\"pixel_x\":%.2f,\"pixel_y\":%.2f,"
             "\"image_w\":%d,\"image_h\":%d,\"state\":%u}",
             event->button, x, y, event->x, event->y,
             image_w, image_h, event->state);
    event_sender_send(&app->events, json);

    snprintf(json, sizeof(json),
             "{\"origin\":\"mouse\",\"type\":\"mousepress\",\"button\":%u,"
             "\"x\":%.6f,\"y\":%.6f,\"pixel_x\":%.2f,\"pixel_y\":%.2f,"
             "\"image_w\":%d,\"image_h\":%d,\"state\":%u}",
             event->button, x, y, event->x, event->y,
             image_w, image_h, event->state);
    event_sender_send(&app->events, json);
    return TRUE;
}

static gpointer joystick_thread(gpointer data)
{
    struct rx_app *app = data;
    int fd = open(app->joystick_device, O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        perror(app->joystick_device);
        return NULL;
    }

    while (!app->stopping) {
        struct js_event event;
        ssize_t n = read(fd, &event, sizeof(event));
        if (n == (ssize_t)sizeof(event)) {
            char json[256];
            unsigned type = event.type & ~JS_EVENT_INIT;
            const int initial = (event.type & JS_EVENT_INIT) ? 1 : 0;
            if (type == JS_EVENT_AXIS) {
                snprintf(json, sizeof(json),
                         "{\"origin\":\"joystick\",\"type\":\"axis\","
                         "\"number\":%u,\"value\":%d,\"time\":%u,"
                         "\"initial\":%s}",
                         event.number, event.value, event.time,
                         initial ? "true" : "false");
                event_sender_send(&app->events, json);
            } else if (type == JS_EVENT_BUTTON) {
                const char *kind = event.value ? "buttondown" : "buttonup";
                snprintf(json, sizeof(json),
                         "{\"origin\":\"joystick\",\"type\":\"%s\","
                         "\"number\":%u,\"value\":%d,\"time\":%u,"
                         "\"initial\":%s}",
                         kind, event.number, event.value, event.time,
                         initial ? "true" : "false");
                event_sender_send(&app->events, json);

                if (!initial && event.value == 0) {
                    snprintf(json, sizeof(json),
                             "{\"origin\":\"joystick\",\"type\":\"buttonpress\","
                             "\"number\":%u,\"value\":%d,\"time\":%u,"
                             "\"initial\":false}",
                             event.number, event.value, event.time);
                    event_sender_send(&app->events, json);
                }
            } else {
                snprintf(json, sizeof(json),
                         "{\"origin\":\"joystick\",\"type\":\"unknown\","
                         "\"number\":%u,\"value\":%d,\"time\":%u,"
                         "\"initial\":%s}",
                         event.number, event.value, event.time,
                         initial ? "true" : "false");
                event_sender_send(&app->events, json);
            }
            continue;
        }

        if (n < 0 && errno != EAGAIN && errno != EINTR) {
            perror("read joystick");
            break;
        }
        g_usleep(10000);
    }

    close(fd);
    return NULL;
}

void rx_input_attach(GtkWidget *window, struct rx_app *app)
{
    GtkWidget *mouse_widget = app->drawing_area;

    gtk_widget_set_can_focus(window, TRUE);
    gtk_widget_add_events(window,
                          GDK_KEY_PRESS_MASK |
                          GDK_KEY_RELEASE_MASK);
    g_signal_connect(window, "key-press-event", G_CALLBACK(on_key_press), app);
    g_signal_connect(window, "key-release-event", G_CALLBACK(on_key_release), app);

    if (!mouse_widget) {
        return;
    }

    gtk_widget_add_events(mouse_widget,
                          GDK_POINTER_MOTION_MASK |
                          GDK_BUTTON_PRESS_MASK |
                          GDK_BUTTON_RELEASE_MASK);
    g_signal_connect(mouse_widget, "motion-notify-event", G_CALLBACK(on_motion), app);
    g_signal_connect(mouse_widget, "button-press-event", G_CALLBACK(on_button_press), app);
    g_signal_connect(mouse_widget, "button-release-event", G_CALLBACK(on_button_release), app);
}

void rx_input_start_joystick(struct rx_app *app)
{
    if (app->joystick_enabled && app->events.enabled) {
        app->joystick_thread = g_thread_new("joystick-input", joystick_thread, app);
    }
}
