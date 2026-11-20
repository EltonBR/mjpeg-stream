#include "telemetry.h"
#include "overlay.h"

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

struct telemetry_notify_state {
    gint references;
    gint cancelled;
    GMutex mutex;
    GSource *source;
    GMainContext *context;
    telemetry_notify_func notify;
    gpointer user_data;
};

static struct telemetry_notify_state *notify_state_new(
    GMainContext *context, telemetry_notify_func notify, gpointer user_data)
{
    struct telemetry_notify_state *state =
        g_new0(struct telemetry_notify_state, 1);
    state->references = 1;
    g_mutex_init(&state->mutex);
    state->context = g_main_context_ref(context ? context :
                                         g_main_context_default());
    state->notify = notify;
    state->user_data = user_data;
    return state;
}

static struct telemetry_notify_state *notify_state_ref(
    struct telemetry_notify_state *state)
{
    g_atomic_int_inc(&state->references);
    return state;
}

static void notify_state_unref(gpointer data)
{
    struct telemetry_notify_state *state = data;

    if (g_atomic_int_dec_and_test(&state->references)) {
        g_main_context_unref(state->context);
        g_mutex_clear(&state->mutex);
        g_free(state);
    }
}

static gboolean notify_idle(gpointer data)
{
    struct telemetry_notify_state *state = data;
    GSource *source;
    gboolean active;

    g_mutex_lock(&state->mutex);
    source = state->source;
    state->source = NULL;
    active = !g_atomic_int_get(&state->cancelled);
    g_mutex_unlock(&state->mutex);

    if (source) {
        g_source_unref(source);
    }
    if (active && state->notify) {
        state->notify(state->user_data);
    }
    return G_SOURCE_REMOVE;
}

static void schedule_notification(struct telemetry_client *client)
{
    struct telemetry_notify_state *state = client->notification;

    if (state) {
        g_mutex_lock(&state->mutex);
        if (!g_atomic_int_get(&state->cancelled) && !state->source) {
            state->source = g_idle_source_new();
            g_source_set_priority(state->source, G_PRIORITY_DEFAULT_IDLE);
            g_source_set_callback(state->source, notify_idle,
                                  notify_state_ref(state), notify_state_unref);
            g_source_attach(state->source, state->context);
        }
        g_mutex_unlock(&state->mutex);
    }
}

static gboolean wait_before_retry(struct telemetry_client *client)
{
    gint64 deadline = g_get_monotonic_time() + (gint64)client->reconnect_ms * 1000;
    gboolean keep_running;

    g_mutex_lock(&client->mutex);
    while (!g_atomic_int_get(&client->stopping) &&
           g_cond_wait_until(&client->condition, &client->mutex, deadline)) {
        /* Spurious wakeups do not shorten the reconnect delay. */
    }
    keep_running = !g_atomic_int_get(&client->stopping);
    g_mutex_unlock(&client->mutex);
    return keep_running;
}

static int telemetry_connect(struct telemetry_client *client)
{
    struct sockaddr_storage address;
    socklen_t address_len;
    char *end = NULL;
    long port;
    int fd;
    int flags;
    int rc;

    if (g_atomic_int_get(&client->stopping)) {
        errno = ECANCELED;
        return -1;
    }
    port = strtol(client->port, &end, 10);
    if (!client->port[0] || !end || *end != '\0' || port < 1 || port > 65535) {
        errno = EINVAL;
        return -1;
    }

    memset(&address, 0, sizeof(address));
    if (strcmp(client->host, "*") == 0) {
        struct sockaddr_in6 *address6 = (struct sockaddr_in6 *)&address;
        address6->sin6_family = AF_INET6;
        address6->sin6_port = htons((uint16_t)port);
        address6->sin6_addr = in6addr_any;
        address_len = sizeof(*address6);
    } else {
        struct sockaddr_in *address4 = (struct sockaddr_in *)&address;
        if (inet_pton(AF_INET, client->host, &address4->sin_addr) == 1) {
            address4->sin_family = AF_INET;
            address4->sin_port = htons((uint16_t)port);
            address_len = sizeof(*address4);
        } else {
            struct sockaddr_in6 *address6 = (struct sockaddr_in6 *)&address;
            if (inet_pton(AF_INET6, client->host, &address6->sin6_addr) != 1) {
                errno = EINVAL;
                return -1;
            }
            address6->sin6_family = AF_INET6;
            address6->sin6_port = htons((uint16_t)port);
            address_len = sizeof(*address6);
        }
    }

    fd = socket(address.ss_family, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }
    flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        int saved_errno = errno;
        close(fd);
        errno = saved_errno;
        return -1;
    }

    rc = connect(fd, (struct sockaddr *)&address, address_len);
    if (rc < 0 && errno != EINPROGRESS && errno != EWOULDBLOCK &&
        errno != EINTR) {
        int saved_errno = errno;
        close(fd);
        errno = saved_errno;
        return -1;
    }

    while (rc < 0) {
        struct pollfd pfd = { fd, POLLOUT, 0 };
        int ready;
        int socket_error = 0;
        socklen_t error_len = sizeof(socket_error);

        if (g_atomic_int_get(&client->stopping)) {
            close(fd);
            errno = ECANCELED;
            return -1;
        }
        ready = poll(&pfd, 1, 100);
        if (ready < 0) {
            if (errno == EINTR) {
                continue;
            }
            int saved_errno = errno;
            close(fd);
            errno = saved_errno;
            return -1;
        }
        if (ready == 0) {
            continue;
        }
        if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &socket_error, &error_len) < 0) {
            int saved_errno = errno;
            close(fd);
            errno = saved_errno;
            return -1;
        }
        if (socket_error != 0) {
            close(fd);
            errno = socket_error;
            return -1;
        }
        rc = 0;
    }

    if (g_atomic_int_get(&client->stopping) ||
        fcntl(fd, F_SETFL, flags) < 0) {
        int saved_errno = g_atomic_int_get(&client->stopping) ?
                          ECANCELED : errno;
        close(fd);
        errno = saved_errno;
        return -1;
    }
    return fd;
}

static int read_line(int fd, char *buf, size_t len)
{
    size_t used = 0;
    int too_long = 0;

    for (;;) {
        char c;
        ssize_t n = read(fd, &c, 1);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (n == 0) {
            return 0;
        }
        if (c == '\n') {
            break;
        }
        if (c != '\r') {
            if (used + 1 < len) {
                buf[used++] = c;
            } else {
                too_long = 1;
            }
        }
    }
    if (too_long) {
        buf[0] = '\0';
        return 2;
    }
    buf[used] = '\0';
    return used > 0 ? 1 : 0;
}

static gpointer telemetry_thread(gpointer data)
{
    struct telemetry_client *client = data;
    char line[8192];

    while (!g_atomic_int_get(&client->stopping)) {
        int fd;

        fd = telemetry_connect(client);
        if (fd < 0) {
            if (!g_atomic_int_get(&client->stopping)) {
                perror("connect telemetria");
            }
            wait_before_retry(client);
            continue;
        }

        g_mutex_lock(&client->mutex);
        if (g_atomic_int_get(&client->stopping)) {
            g_mutex_unlock(&client->mutex);
            close(fd);
            break;
        }
        client->fd = fd;
        g_mutex_unlock(&client->mutex);
        fprintf(stderr, "telemetria conectada em %s:%s\n",
                client->host, client->port);

        while (!g_atomic_int_get(&client->stopping)) {
            int rc = read_line(fd, line, sizeof(line));
            if (rc < 0 || rc == 0) {
                break;
            }
            if (rc == 2) {
                continue;
            }
            if (!client->store ||
                !telemetry_store_update_json(client->store, line)) {
                overlay_apply_json_line(client->overlay, line);
            }
            schedule_notification(client);
        }

        g_mutex_lock(&client->mutex);
        if (client->fd == fd) {
            client->fd = -1;
        }
        g_mutex_unlock(&client->mutex);
        close(fd);
        if (!g_atomic_int_get(&client->stopping)) {
            fprintf(stderr, "telemetria desconectada, tentando reconectar\n");
            wait_before_retry(client);
        }
    }
    return NULL;
}

void telemetry_init(struct telemetry_client *client)
{
    memset(client, 0, sizeof(*client));
    client->fd = -1;
    g_mutex_init(&client->mutex);
    g_cond_init(&client->condition);
    client->synchronization_initialized = TRUE;
}

void telemetry_start(struct telemetry_client *client, const char *host,
                     const char *port, struct overlay_state *overlay,
                     GMainContext *context,
                     telemetry_notify_func notify, gpointer user_data,
                     unsigned reconnect_ms)
{
    telemetry_start_with_store(client, host, port, overlay, NULL,
                               context, notify, user_data, reconnect_ms);
}

void telemetry_start_with_store(struct telemetry_client *client,
                                const char *host, const char *port,
                                struct overlay_state *overlay,
                                TelemetryStore *store,
                                GMainContext *context,
                                telemetry_notify_func notify,
                                gpointer user_data,
                                unsigned reconnect_ms)
{
    client->reconnect_ms = reconnect_ms ? reconnect_ms : 1000;
    client->host = host;
    client->port = port;
    client->overlay = overlay;
    client->store = store;
    if (notify) {
        client->notification = notify_state_new(context, notify, user_data);
    }
    client->enabled = 1;
    client->thread = g_thread_new("telemetry", telemetry_thread, client);
}

void telemetry_close(struct telemetry_client *client)
{
    GSource *source = NULL;

    if (!client->synchronization_initialized) {
        return;
    }

    g_atomic_int_set(&client->stopping, 1);
    if (client->notification) {
        g_mutex_lock(&client->notification->mutex);
        g_atomic_int_set(&client->notification->cancelled, 1);
        source = client->notification->source;
        client->notification->source = NULL;
        g_mutex_unlock(&client->notification->mutex);
        if (source) {
            g_source_destroy(source);
            g_source_unref(source);
        }
    }
    g_mutex_lock(&client->mutex);
    if (client->fd >= 0) {
        shutdown(client->fd, SHUT_RDWR);
    }
    g_cond_broadcast(&client->condition);
    g_mutex_unlock(&client->mutex);
    if (client->thread) {
        GThread *thread = client->thread;
        client->thread = NULL;
        g_thread_join(thread);
    }
    client->enabled = 0;
    if (client->notification) {
        notify_state_unref(client->notification);
        client->notification = NULL;
    }
    g_cond_clear(&client->condition);
    g_mutex_clear(&client->mutex);
    client->synchronization_initialized = FALSE;
}
