#ifndef MJPEG_RX_TELEMETRY_H
#define MJPEG_RX_TELEMETRY_H

#include "telemetry_store.h"

#include <glib.h>

struct overlay_state;
struct telemetry_notify_state;

typedef void (*telemetry_notify_func)(gpointer user_data);

/* user_data must remain valid until telemetry_close() returns. */

struct telemetry_client {
    int enabled;
    int stopping;
    unsigned reconnect_ms;
    const char *host;
    const char *port;
    int fd;
    GThread *thread;
    struct overlay_state *overlay;
    TelemetryStore *store;
    GMutex mutex;
    GCond condition;
    gboolean synchronization_initialized;
    struct telemetry_notify_state *notification;
};

void telemetry_init(struct telemetry_client *client);
void telemetry_start(struct telemetry_client *client, const char *host,
                     const char *port, struct overlay_state *overlay,
                     GMainContext *context,
                     telemetry_notify_func notify, gpointer user_data,
                     unsigned reconnect_ms);
void telemetry_start_with_store(struct telemetry_client *client,
                                const char *host, const char *port,
                                struct overlay_state *overlay,
                                TelemetryStore *store,
                                GMainContext *context,
                                telemetry_notify_func notify,
                                gpointer user_data,
                                unsigned reconnect_ms);
void telemetry_close(struct telemetry_client *client);

#endif
