#include "receiver.h"

#include "net.h"
#include "udp_reassembly.h"
#include "ui.h"

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
#include <glib-unix.h>

static void notify_receiver(struct rx_app *app)
{
    const unsigned char byte = 1;
    if (app->receiver_notify[1] >= 0) {
        ssize_t ignored = write(app->receiver_notify[1], &byte, sizeof(byte));
        (void)ignored;
    }
}

static gboolean receiver_notify_ready(gint fd, GIOCondition condition,
                                      gpointer data)
{
    struct rx_app *app = data;
    unsigned char bytes[32];
    (void)condition;

    while (read(fd, bytes, sizeof(bytes)) > 0) {
    }
    if (g_atomic_int_get(&app->receiver_done))
        gtk_main_quit();
    return G_SOURCE_CONTINUE;
}

static int make_pipe(int fds[2])
{
    int flags;
    if (pipe(fds) < 0)
        return -1;
    flags = fcntl(fds[0], F_GETFL, 0);
    if (flags < 0 || fcntl(fds[0], F_SETFL, flags | O_NONBLOCK) < 0)
        goto fail;
    flags = fcntl(fds[1], F_GETFL, 0);
    if (flags < 0 || fcntl(fds[1], F_SETFL, flags | O_NONBLOCK) < 0)
        goto fail;
    return 0;
fail:
    close(fds[0]);
    close(fds[1]);
    fds[0] = fds[1] = -1;
    return -1;
}

static void set_socket(struct rx_app *app, int fd)
{
    g_mutex_lock(&app->receiver_mutex);
    app->fd = fd;
    g_mutex_unlock(&app->receiver_mutex);
}

static int cancelled(struct rx_app *app)
{
    return g_atomic_int_get(&app->receiver_stopping);
}

static int tcp_connect_cancelable(struct rx_app *app)
{
    struct sockaddr_storage address;
    socklen_t address_len;
    struct sockaddr_in *addr4 = (struct sockaddr_in *)&address;
    struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)&address;
    char *end = NULL;
    long port;
    int fd, flags, result;

    port = strtol(app->receiver_port, &end, 10);
    if (!app->receiver_port[0] || !end || *end || port < 1 || port > 65535) {
        errno = EINVAL;
        return -1;
    }
    memset(&address, 0, sizeof(address));
    if (strcmp(app->receiver_host, "*") == 0) {
        addr6->sin6_family = AF_INET6;
        addr6->sin6_port = htons((uint16_t)port);
        addr6->sin6_addr = in6addr_any;
        address_len = sizeof(*addr6);
    } else if (inet_pton(AF_INET, app->receiver_host, &addr4->sin_addr) == 1) {
        addr4->sin_family = AF_INET;
        addr4->sin_port = htons((uint16_t)port);
        address_len = sizeof(*addr4);
    } else if (inet_pton(AF_INET6, app->receiver_host, &addr6->sin6_addr) == 1) {
        addr6->sin6_family = AF_INET6;
        addr6->sin6_port = htons((uint16_t)port);
        address_len = sizeof(*addr6);
    } else {
        errno = EINVAL;
        return -1;
    }

    fd = socket(address.ss_family, SOCK_STREAM, 0);
    if (fd < 0)
        return -1;
    set_socket(app, fd);
    flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
        goto fail;

    result = connect(fd, (struct sockaddr *)&address, address_len);
    if (result < 0 && errno != EINPROGRESS)
        goto fail;
    while (result < 0 && !cancelled(app)) {
        struct pollfd pfds[2] = {
            { fd, POLLOUT, 0 },
            { app->receiver_cancel[0], POLLIN, 0 }
        };
        int ready = poll(pfds, 2, -1);
        if (ready < 0) {
            if (errno == EINTR)
                continue;
            goto fail;
        }
        if (cancelled(app)) {
            errno = ECANCELED;
            goto fail;
        }
        if (pfds[1].revents & POLLIN) {
            errno = ECANCELED;
            goto fail;
        }
        if (pfds[0].revents & (POLLOUT | POLLERR | POLLHUP)) {
            int socket_error = 0;
            socklen_t error_len = sizeof(socket_error);
            if (getsockopt(fd, SOL_SOCKET, SO_ERROR,
                           &socket_error, &error_len) < 0)
                goto fail;
            if (socket_error) {
                errno = socket_error;
                goto fail;
            }
            result = 0;
        }
    }
    if (cancelled(app)) {
        errno = ECANCELED;
        goto fail;
    }
    if (fcntl(fd, F_SETFL, flags) < 0)
        goto fail;
    return fd;

fail:
    set_socket(app, -1);
    close(fd);
    return -1;
}

static gpointer tcp_thread(gpointer data)
{
    struct rx_app *app = data;
    int fd = app->fd;
    if (fd < 0) {
        fprintf(stderr, "conectando ao transmissor TCP em %s:%s...\n",
                app->receiver_host, app->receiver_port);
        fd = tcp_connect_cancelable(app);
        if (fd < 0) {
            if (!cancelled(app)) {
                fprintf(stderr, "falha ao conectar em %s:%s\n",
                        app->receiver_host, app->receiver_port);
                g_atomic_int_set(&app->receiver_initial_failed, 1);
                g_atomic_int_set(&app->receiver_done, 1);
                notify_receiver(app);
            }
            return NULL;
        }
    }

    while (!cancelled(app)) {
        uint32_t nlen;
        uint32_t len;
        unsigned char *buf;
        int rc = read_all(fd, &nlen, sizeof(nlen));
        if (rc <= 0)
            break;
        len = ntohl(nlen);
        if (len == 0 || len > MJPEG_MAX_FRAME)
            break;
        buf = malloc(len);
        if (!buf)
            break;
        rc = read_all(fd, buf, len);
        if (rc <= 0) {
            free(buf);
            break;
        }
        rx_queue_frame(app, buf, len);
        free(buf);
    }
    if (!cancelled(app)) {
        g_atomic_int_set(&app->receiver_done, 1);
        notify_receiver(app);
    }
    return NULL;
}

static gpointer udp_thread(gpointer data)
{
    struct rx_app *app = data;
    struct udp_reassembly reassembly;
    unsigned char packet[sizeof(struct udp_frame_header) + MJPEG_UDP_PAYLOAD + 64u];

    {
        int fd = udp_bind_socket(app->receiver_host, app->receiver_port);
        set_socket(app, fd);
    }
    if (app->fd < 0) {
        if (!cancelled(app)) {
            fprintf(stderr, "falha ao escutar UDP em %s:%s\n",
                    app->receiver_host, app->receiver_port);
            g_atomic_int_set(&app->receiver_initial_failed, 1);
            g_atomic_int_set(&app->receiver_done, 1);
            notify_receiver(app);
        }
        return NULL;
    }

    udp_reassembly_init(&reassembly);
    while (!cancelled(app)) {
        struct pollfd pfds[2] = {
            { app->fd, POLLIN, 0 },
            { app->receiver_cancel[0], POLLIN, 0 }
        };
        int ready = poll(pfds, 2, -1);
        ssize_t n;
        const unsigned char *frame;
        size_t frame_size;

        if (ready < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (cancelled(app) || (pfds[1].revents & POLLIN))
            break;
        if (!(pfds[0].revents & POLLIN))
            break;
        n = recv_intr(app->fd, packet, sizeof(packet), 0);
        if (n < 0) {
            perror("recv udp");
            break;
        }
        if ((size_t)n < sizeof(struct udp_frame_header))
            continue;
        if (udp_reassembly_push(&reassembly, packet, (size_t)n,
                                &frame, &frame_size))
            rx_queue_frame(app, frame, frame_size);
    }
    udp_reassembly_destroy(&reassembly);
    if (!cancelled(app)) {
        g_atomic_int_set(&app->receiver_done, 1);
        notify_receiver(app);
    }
    return NULL;
}

int rx_connect(struct rx_app *app, const struct rx_config *cfg)
{
    app->receiver_host = cfg->use_udp ? cfg->listen_host : cfg->host;
    app->receiver_port = cfg->port;
    app->use_udp = cfg->use_udp;
    return 0;
}

int rx_start_receiver(struct rx_app *app)
{
    if (make_pipe(app->receiver_cancel) < 0 ||
        make_pipe(app->receiver_notify) < 0) {
        perror("pipe receiver");
        if (app->receiver_cancel[0] >= 0) {
            close(app->receiver_cancel[0]);
            close(app->receiver_cancel[1]);
            app->receiver_cancel[0] = app->receiver_cancel[1] = -1;
        }
        return -1;
    }
    app->receiver_source = g_unix_fd_add(app->receiver_notify[0],
                                         G_IO_IN | G_IO_HUP | G_IO_ERR,
                                         receiver_notify_ready, app);
    if (!app->receiver_source)
        return -1;
    g_atomic_int_set(&app->receiver_stopping, 0);
    g_atomic_int_set(&app->receiver_done, 0);
    app->receiver_thread = g_thread_new(app->use_udp ? "udp-recv" : "tcp-recv",
                                        app->use_udp ? udp_thread : tcp_thread,
                                        app);
    if (!app->receiver_thread) {
        g_source_remove(app->receiver_source);
        app->receiver_source = 0;
        return -1;
    }
    return 0;
}

void rx_stop_receiver(struct rx_app *app)
{
    const unsigned char byte = 1;
    g_atomic_int_set(&app->receiver_stopping, 1);
    if (app->receiver_cancel[1] >= 0) {
        ssize_t ignored = write(app->receiver_cancel[1], &byte, sizeof(byte));
        (void)ignored;
    }
    g_mutex_lock(&app->receiver_mutex);
    if (!app->use_udp && app->fd >= 0)
        shutdown(app->fd, SHUT_RDWR);
    g_mutex_unlock(&app->receiver_mutex);
    if (app->receiver_thread) {
        g_thread_join(app->receiver_thread);
        app->receiver_thread = NULL;
    }
    if (app->receiver_source) {
        g_source_remove(app->receiver_source);
        app->receiver_source = 0;
    }
    if (app->receiver_cancel[0] >= 0) close(app->receiver_cancel[0]);
    if (app->receiver_cancel[1] >= 0) close(app->receiver_cancel[1]);
    if (app->receiver_notify[0] >= 0) close(app->receiver_notify[0]);
    if (app->receiver_notify[1] >= 0) close(app->receiver_notify[1]);
    app->receiver_cancel[0] = app->receiver_cancel[1] = -1;
    app->receiver_notify[0] = app->receiver_notify[1] = -1;
}
