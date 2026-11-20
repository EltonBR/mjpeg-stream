#include "event_sender.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define EVENT_SENDER_STOP GINT_TO_POINTER(1)
#define EVENT_SENDER_POLL_INTERVAL_MS 100
#define EVENT_SENDER_CONNECT_TIMEOUT_US G_USEC_PER_SEC
#define EVENT_SENDER_RETRY_INTERVAL_US G_USEC_PER_SEC
#define EVENT_SENDER_QUEUE_POLL_US (100 * 1000)

static int event_is_coalescible(const char *json)
{
    return strstr(json, "\"type\":\"motion\"") != NULL ||
           strstr(json, "\"type\":\"axis\"") != NULL;
}

/* Caller holds sender->lock. Evict the oldest continuous event, retaining
 * the relative order of every item that remains in the queue. */
static int evict_oldest_coalescible(struct event_sender *sender)
{
    GPtrArray *items = g_ptr_array_new();
    gpointer item;
    int evicted = 0;

    g_async_queue_lock(sender->queue);
    while ((item = g_async_queue_try_pop_unlocked(sender->queue)) != NULL) {
        if (!evicted && event_is_coalescible(item)) {
            g_free(item);
            evicted = 1;
            continue;
        }
        g_ptr_array_add(items, item);
    }
    {
        guint i;

        for (i = 0; i < items->len; i++) {
            item = g_ptr_array_index(items, i);
            g_async_queue_push_unlocked(sender->queue, item);
        }
    }
    g_async_queue_unlock(sender->queue);
    g_ptr_array_free(items, TRUE);
    if (evicted) {
        sender->queued_events--;
    }
    return evicted;
}

static int sender_is_stopping(struct event_sender *sender)
{
    int stopping;

    g_mutex_lock(&sender->lock);
    stopping = sender->stopping;
    g_mutex_unlock(&sender->lock);
    return stopping;
}

static int parse_address(struct event_sender *sender,
                         struct sockaddr_storage *address,
                         socklen_t *address_len)
{
    char *end = NULL;
    long port = strtol(sender->port, &end, 10);

    if (!sender->port[0] || !end || *end != '\0' || port < 1 || port > 65535) {
        return -1;
    }

    memset(address, 0, sizeof(*address));
    if (strcmp(sender->host, "*") == 0) {
        struct sockaddr_in6 *address6 = (struct sockaddr_in6 *)address;
        address6->sin6_family = AF_INET6;
        address6->sin6_port = htons((uint16_t)port);
        address6->sin6_addr = in6addr_any;
        *address_len = sizeof(*address6);
        return 0;
    }

    {
        struct sockaddr_in *address4 = (struct sockaddr_in *)address;
        if (inet_pton(AF_INET, sender->host, &address4->sin_addr) == 1) {
            address4->sin_family = AF_INET;
            address4->sin_port = htons((uint16_t)port);
            *address_len = sizeof(*address4);
            return 0;
        }
    }

    {
        struct sockaddr_in6 *address6 = (struct sockaddr_in6 *)address;
        if (inet_pton(AF_INET6, sender->host, &address6->sin6_addr) == 1) {
            address6->sin6_family = AF_INET6;
            address6->sin6_port = htons((uint16_t)port);
            *address_len = sizeof(*address6);
            return 0;
        }
    }
    return -1;
}

/* Retorna 1 quando há atividade, 0 no prazo e -1 no cancelamento/erro. */
static int poll_until(struct event_sender *sender, int fd, short events,
                      gint64 deadline)
{
    struct pollfd pfd;

    pfd.fd = fd;
    pfd.events = events;
    pfd.revents = 0;
    for (;;) {
        gint64 remaining;
        int timeout;
        int result;

        if (sender_is_stopping(sender)) {
            errno = ECANCELED;
            return -1;
        }

        remaining = deadline - g_get_monotonic_time();
        if (remaining <= 0) {
            errno = ETIMEDOUT;
            return 0;
        }
        timeout = (int)MIN(EVENT_SENDER_POLL_INTERVAL_MS,
                           (remaining + 999) / 1000);
        result = poll(&pfd, 1, timeout);
        if (result > 0) {
            return 1;
        }
        if (result < 0 && errno != EINTR) {
            return -1;
        }
    }
}

static int connect_sender(struct event_sender *sender, int *fd)
{
    struct sockaddr_storage address;
    socklen_t address_len;
    gint64 deadline;
    int socket_fd;
    int flags;
    int result;

    if (sender_is_stopping(sender) ||
        parse_address(sender, &address, &address_len) < 0) {
        return -1;
    }

    socket_fd = socket(address.ss_family, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        return -1;
    }
    flags = fcntl(socket_fd, F_GETFL, 0);
    if (flags < 0 || fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        close(socket_fd);
        return -1;
    }

    result = connect(socket_fd, (struct sockaddr *)&address, address_len);
    if (result < 0 && errno != EINPROGRESS && errno != EWOULDBLOCK) {
        close(socket_fd);
        return -1;
    }
    if (result < 0) {
        int socket_error = 0;
        socklen_t error_len = sizeof(socket_error);

        deadline = g_get_monotonic_time() + EVENT_SENDER_CONNECT_TIMEOUT_US;
        result = poll_until(sender, socket_fd, POLLOUT, deadline);
        if (result <= 0 ||
            getsockopt(socket_fd, SOL_SOCKET, SO_ERROR,
                       &socket_error, &error_len) < 0 || socket_error != 0) {
            if (socket_error != 0) {
                errno = socket_error;
            }
            close(socket_fd);
            return -1;
        }
    }

    *fd = socket_fd;
    fprintf(stderr, "canal de eventos conectado em %s:%s\n",
            sender->host, sender->port);
    return 0;
}

static int send_all_nonblocking(struct event_sender *sender, int fd,
                                const char *data, size_t len)
{
    while (len > 0) {
        ssize_t sent;

        if (sender_is_stopping(sender)) {
            errno = ECANCELED;
            return -1;
        }
        sent = send(fd, data, len, MSG_NOSIGNAL | MSG_DONTWAIT);
        if (sent > 0) {
            data += (size_t)sent;
            len -= (size_t)sent;
            continue;
        }
        if (sent < 0 && errno == EINTR) {
            continue;
        }
        if (sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            gint64 deadline = g_get_monotonic_time() +
                              EVENT_SENDER_POLL_INTERVAL_MS * 1000;
            int result = poll_until(sender, fd, POLLOUT, deadline);
            if (result > 0) {
                continue;
            }
            if (result == 0) {
                continue;
            }
        }
        return -1;
    }
    return 0;
}

static int send_event(struct event_sender *sender, int *fd, char *json)
{
    size_t len = strlen(json);
    char *message;
    int result;

    if (len > G_MAXSIZE - 2) {
        errno = EOVERFLOW;
        return -1;
    }
    if (*fd < 0 && connect_sender(sender, fd) < 0) {
        return -1;
    }

    message = g_malloc(len + 2);
    memcpy(message, json, len);
    message[len] = '\n';
    message[len + 1] = '\0';
    result = send_all_nonblocking(sender, *fd, message, len + 1);
    g_free(message);
    if (result < 0) {
        close(*fd);
        *fd = -1;
        if (errno != ECANCELED) {
            fprintf(stderr, "canal de eventos desconectado, tentando reconectar\n");
        }
        return -1;
    }
    return 0;
}

static gpointer event_thread(gpointer data)
{
    struct event_sender *sender = data;
    gint64 next_retry = g_get_monotonic_time();
    int fd = -1;

    for (;;) {
        gpointer item;

        if (sender_is_stopping(sender)) {
            break;
        }
        if (fd < 0) {
            gint64 now = g_get_monotonic_time();

            if (now >= next_retry) {
                if (connect_sender(sender, &fd) < 0) {
                    next_retry = g_get_monotonic_time() +
                                 EVENT_SENDER_RETRY_INTERVAL_US;
                }
                continue;
            }
            g_usleep((gulong)MIN(EVENT_SENDER_QUEUE_POLL_US,
                                 next_retry - now));
            continue;
        }

        item = g_async_queue_timeout_pop(sender->queue,
                                         EVENT_SENDER_QUEUE_POLL_US);
        if (item == EVENT_SENDER_STOP) {
            break;
        }
        if (item) {
            char *json = item;

            g_mutex_lock(&sender->lock);
            if (sender->queued_events > 0) {
                sender->queued_events--;
            }
            g_mutex_unlock(&sender->lock);
            if (!sender_is_stopping(sender)) {
                (void)send_event(sender, &fd, json);
            }
            g_free(json);
        }
        if (fd < 0) {
            next_retry = g_get_monotonic_time() +
                         EVENT_SENDER_RETRY_INTERVAL_US;
        }
    }

    if (fd >= 0) {
        close(fd);
    }
    return NULL;
}

void event_sender_init(struct event_sender *sender)
{
    sender->enabled = 0;
    sender->stopping = 0;
    sender->queued_events = 0;
    sender->host = NULL;
    sender->port = NULL;
    sender->thread = NULL;
    sender->queue = g_async_queue_new();
    g_mutex_init(&sender->lock);
}

void event_sender_start(struct event_sender *sender, const char *host,
                        const char *port)
{
    g_mutex_lock(&sender->lock);
    if (!sender->enabled && !sender->stopping) {
        sender->host = g_strdup(host);
        sender->port = g_strdup(port);
        sender->enabled = 1;
        sender->thread = g_thread_new("event-sender", event_thread, sender);
    }
    g_mutex_unlock(&sender->lock);
}

void event_sender_close(struct event_sender *sender)
{
    GThread *thread;
    gpointer item;

    g_mutex_lock(&sender->lock);
    sender->enabled = 0;
    sender->stopping = 1;
    thread = sender->thread;
    sender->thread = NULL;
    if (thread) {
        g_async_queue_push(sender->queue, EVENT_SENDER_STOP);
    }
    g_mutex_unlock(&sender->lock);

    if (thread) {
        g_thread_join(thread);
    }

    while ((item = g_async_queue_try_pop(sender->queue)) != NULL) {
        if (item != EVENT_SENDER_STOP) {
            g_free(item);
        }
    }
    g_async_queue_unref(sender->queue);
    sender->queue = NULL;
    g_free(sender->host);
    g_free(sender->port);
    sender->host = NULL;
    sender->port = NULL;
    g_mutex_clear(&sender->lock);
}

void event_sender_send(struct event_sender *sender, const char *json)
{
    if (!sender || !json) {
        return;
    }

    g_mutex_lock(&sender->lock);
    if (sender->enabled && !sender->stopping) {
        if (sender->queued_events >= EVENT_SENDER_QUEUE_LIMIT &&
            !event_is_coalescible(json)) {
            (void)evict_oldest_coalescible(sender);
        }
        if (sender->queued_events < EVENT_SENDER_QUEUE_LIMIT) {
        g_async_queue_push(sender->queue, g_strdup(json));
        sender->queued_events++;
        }
    }
    g_mutex_unlock(&sender->lock);
}
