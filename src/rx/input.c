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

static gboolean on_key_press(GtkWidget *widget, GdkEventKey *event,
                             gpointer data)
{
    struct rx_app *app = data;
    char json[256];
    (void)widget;

    snprintf(json, sizeof(json),
             "{\"origin\":\"keyboard\",\"type\":\"key_press\",\"keyval\":%u,"
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
             "{\"origin\":\"keyboard\",\"type\":\"key_release\",\"keyval\":%u,"
             "\"key\":\"%s\",\"state\":%u}",
             event->keyval, key_name(event->keyval), event->state);
    event_sender_send(&app->events, json);
    return FALSE;
}

static gboolean on_motion(GtkWidget *widget, GdkEventMotion *event,
                          gpointer data)
{
    struct rx_app *app = data;
    char json[256];
    (void)widget;

    snprintf(json, sizeof(json),
             "{\"origin\":\"mouse\",\"type\":\"motion\",\"x\":%.2f,"
             "\"y\":%.2f,\"state\":%u}",
             event->x, event->y, event->state);
    event_sender_send(&app->events, json);
    return FALSE;
}

static gboolean on_button_press(GtkWidget *widget, GdkEventButton *event,
                                gpointer data)
{
    struct rx_app *app = data;
    char json[256];
    (void)widget;

    snprintf(json, sizeof(json),
             "{\"origin\":\"mouse\",\"type\":\"button_press\",\"button\":%u,"
             "\"x\":%.2f,\"y\":%.2f,\"state\":%u}",
             event->button, event->x, event->y, event->state);
    event_sender_send(&app->events, json);
    return FALSE;
}

static gboolean on_button_release(GtkWidget *widget, GdkEventButton *event,
                                  gpointer data)
{
    struct rx_app *app = data;
    char json[256];
    (void)widget;

    snprintf(json, sizeof(json),
             "{\"origin\":\"mouse\",\"type\":\"button_release\",\"button\":%u,"
             "\"x\":%.2f,\"y\":%.2f,\"state\":%u}",
             event->button, event->x, event->y, event->state);
    event_sender_send(&app->events, json);
    return FALSE;
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
            const char *kind = type == JS_EVENT_AXIS ? "axis" :
                               type == JS_EVENT_BUTTON ? "button" : "unknown";
            snprintf(json, sizeof(json),
                     "{\"origin\":\"joystick\",\"type\":\"%s\","
                     "\"number\":%u,\"value\":%d,\"time\":%u,"
                     "\"initial\":%s}",
                     kind, event.number, event.value, event.time,
                     (event.type & JS_EVENT_INIT) ? "true" : "false");
            event_sender_send(&app->events, json);
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
    GtkWidget *mouse_widget = app->image ? app->image : window;

    gtk_widget_set_can_focus(window, TRUE);
    gtk_widget_add_events(window,
                          GDK_KEY_PRESS_MASK |
                          GDK_KEY_RELEASE_MASK);
    gtk_widget_add_events(mouse_widget,
                          GDK_POINTER_MOTION_MASK |
                          GDK_BUTTON_PRESS_MASK |
                          GDK_BUTTON_RELEASE_MASK);
    g_signal_connect(window, "key-press-event", G_CALLBACK(on_key_press), app);
    g_signal_connect(window, "key-release-event", G_CALLBACK(on_key_release), app);
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
