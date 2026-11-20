#ifndef MJPEG_RX_EVENT_SENDER_H
#define MJPEG_RX_EVENT_SENDER_H

#include <glib.h>

#define EVENT_SENDER_QUEUE_LIMIT 256

struct event_sender {
    int enabled;
    int stopping;
    guint queued_events;
    char *host;
    char *port;
    GThread *thread;
    GAsyncQueue *queue;
    GMutex lock;
};

void event_sender_init(struct event_sender *sender);
void event_sender_start(struct event_sender *sender, const char *host,
                        const char *port);
/* The caller must stop and join all producers before closing the sender.
 * No call to event_sender_send may overlap or follow event_sender_close. */
void event_sender_close(struct event_sender *sender);
/* A fila mantém até 256 eventos pendentes. Sem conexão, eventos ficam na fila
 * e novas tentativas ocorrem no máximo uma vez por segundo. Sob saturação,
 * eventos contínuos novos (motion/axis) são descartados; um evento discreto
 * remove o contínuo mais antigo para abrir espaço, se houver, e é descartado
 * caso contrário. A ordem relativa dos eventos mantidos é preservada. */
void event_sender_send(struct event_sender *sender, const char *json);

#endif
