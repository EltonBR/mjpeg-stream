#ifndef MJPEG_RX_EVENT_SENDER_H
#define MJPEG_RX_EVENT_SENDER_H

#include <glib.h>

struct event_sender {
    int fd;
    int enabled;
    int stopping;
    const char *host;
    const char *port;
    GThread *thread;
    GMutex lock;
};

void event_sender_init(struct event_sender *sender);
void event_sender_start(struct event_sender *sender, const char *host,
                        const char *port);
void event_sender_close(struct event_sender *sender);
void event_sender_send(struct event_sender *sender, const char *json);

#endif
