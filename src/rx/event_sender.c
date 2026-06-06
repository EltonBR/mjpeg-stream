#include "event_sender.h"

#include "net.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int try_connect_locked(struct event_sender *sender)
{
    int fd;

    if (!sender->enabled || sender->fd >= 0 || sender->stopping) {
        return 0;
    }

    fd = tcp_connect(sender->host, sender->port);
    if (fd < 0) {
        return -1;
    }

    sender->fd = fd;
    fprintf(stderr, "canal de eventos conectado em %s:%s\n",
            sender->host, sender->port);
    return 0;
}

static gpointer reconnect_thread(gpointer data)
{
    struct event_sender *sender = data;

    while (!sender->stopping) {
        g_mutex_lock(&sender->lock);
        (void)try_connect_locked(sender);
        g_mutex_unlock(&sender->lock);
        g_usleep(1000000);
    }
    return NULL;
}

void event_sender_init(struct event_sender *sender)
{
    sender->fd = -1;
    sender->enabled = 0;
    sender->stopping = 0;
    sender->host = NULL;
    sender->port = NULL;
    sender->thread = NULL;
    g_mutex_init(&sender->lock);
}

void event_sender_start(struct event_sender *sender, const char *host,
                        const char *port)
{
    sender->host = host;
    sender->port = port;
    sender->enabled = 1;
    sender->thread = g_thread_new("event-reconnect", reconnect_thread, sender);
}

void event_sender_close(struct event_sender *sender)
{
    sender->stopping = 1;
    if (sender->thread) {
        g_thread_join(sender->thread);
        sender->thread = NULL;
    }

    g_mutex_lock(&sender->lock);
    sender->enabled = 0;
    if (sender->fd >= 0) {
        close(sender->fd);
        sender->fd = -1;
    }
    g_mutex_unlock(&sender->lock);
    g_mutex_clear(&sender->lock);
}

void event_sender_send(struct event_sender *sender, const char *json)
{
    size_t len;

    if (!sender->enabled) {
        return;
    }

    len = strlen(json);
    g_mutex_lock(&sender->lock);
    if (sender->fd < 0) {
        (void)try_connect_locked(sender);
    }
    if (sender->enabled && sender->fd >= 0) {
        if (write_all(sender->fd, json, len) < 0 ||
            write_all(sender->fd, "\n", 1) < 0) {
            close(sender->fd);
            sender->fd = -1;
            fprintf(stderr, "canal de eventos desconectado, tentando reconectar\n");
        }
    }
    g_mutex_unlock(&sender->lock);
}
