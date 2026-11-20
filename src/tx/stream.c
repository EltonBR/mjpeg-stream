#include "stream.h"

#include "net.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static int set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return -1;
    }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static void client_mark_progress(struct client *client)
{
    (void)clock_gettime(CLOCK_MONOTONIC, &client->pending_progress_at);
}

static int client_progress_expired(const struct client *client)
{
    struct timespec now;

    if (clock_gettime(CLOCK_MONOTONIC, &now) < 0 ||
        now.tv_sec < client->pending_progress_at.tv_sec) {
        return 0;
    }
    if (now.tv_sec - client->pending_progress_at.tv_sec >
        STREAM_PENDING_PROGRESS_TIMEOUT_SEC) {
        return 1;
    }
    return now.tv_sec - client->pending_progress_at.tv_sec ==
               STREAM_PENDING_PROGRESS_TIMEOUT_SEC &&
           now.tv_nsec >= client->pending_progress_at.tv_nsec;
}

static const char http_stream_header[] =
    "HTTP/1.0 200 OK\r\n"
    "Server: mjpeg_tx\r\n"
    "Cache-Control: no-cache, no-store, must-revalidate\r\n"
    "Pragma: no-cache\r\n"
    "Expires: 0\r\n"
    "Connection: close\r\n"
    "Content-Type: multipart/x-mixed-replace; boundary=mjpegframe\r\n"
    "\r\n";

static int client_queue_owned(struct stream_server *server,
                              struct client *client,
                              unsigned char *data, size_t len)
{
    if (server->pending_bytes > STREAM_MAX_PENDING_BYTES ||
        len > STREAM_MAX_PENDING_BYTES - server->pending_bytes) {
        return -1;
    }
    client->pending = data;
    client->pending_len = len;
    client->pending_offset = 0;
    client_mark_progress(client);
    server->pending_bytes += len;
    return 0;
}

static int client_queue(struct stream_server *server, struct client *client,
                        const void *data, size_t len)
{
    unsigned char *copy = malloc(len);
    if (!copy) {
        return -1;
    }
    memcpy(copy, data, len);
    if (client_queue_owned(server, client, copy, len) < 0) {
        free(copy);
        return -1;
    }
    return 0;
}

/* Returns 0 when drained, 1 when temporarily blocked, and -1 on fatal error. */
static int client_flush(struct client *client)
{
    while (client->pending && client->pending_offset < client->pending_len) {
        ssize_t n = send(client->fd, client->pending + client->pending_offset,
                         client->pending_len - client->pending_offset,
                         MSG_NOSIGNAL);
        if (n > 0) {
            client->pending_offset += (size_t)n;
            client_mark_progress(client);
            continue;
        }
        if (n < 0 && errno == EINTR) {
            continue;
        }
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            return 1;
        }
        return -1;
    }
    return 0;
}

static void client_clear_pending(struct stream_server *server,
                                 struct client *client)
{
    if (client->pending) {
        server->pending_bytes -= client->pending_len;
        free(client->pending);
        client->pending = NULL;
        client->pending_len = 0;
        client->pending_offset = 0;
        client->pending_progress_at.tv_sec = 0;
        client->pending_progress_at.tv_nsec = 0;
    }
}

static int serialize_frame(enum protocol protocol, const unsigned char *jpeg,
                           unsigned long len, unsigned char **out,
                           size_t *out_len)
{
    size_t prefix_len;
    size_t suffix_len = protocol == PROTO_HTTP ? 2u : 0u;
    char http_prefix[160];
    uint32_t tcp_len;
    unsigned char *buffer;

    if (len > MJPEG_MAX_FRAME) {
        return -1;
    }
    if (protocol == PROTO_HTTP) {
        int n = snprintf(http_prefix, sizeof(http_prefix),
                         "--mjpegframe\r\n"
                         "Content-Type: image/jpeg\r\n"
                         "Content-Length: %lu\r\n\r\n", len);
        if (n < 0 || (size_t)n >= sizeof(http_prefix)) {
            return -1;
        }
        prefix_len = (size_t)n;
    } else {
        prefix_len = sizeof(tcp_len);
    }
    *out_len = prefix_len + (size_t)len + suffix_len;
    buffer = malloc(*out_len);
    if (!buffer) {
        return -1;
    }
    if (protocol == PROTO_HTTP) {
        memcpy(buffer, http_prefix, prefix_len);
    } else {
        tcp_len = htonl((uint32_t)len);
        memcpy(buffer, &tcp_len, sizeof(tcp_len));
    }
    memcpy(buffer + prefix_len, jpeg, (size_t)len);
    if (suffix_len) {
        memcpy(buffer + prefix_len + (size_t)len, "\r\n", suffix_len);
    }
    *out = buffer;
    return 0;
}

static void remove_client(struct stream_server *server, int index)
{
    client_clear_pending(server, &server->clients[index]);
    close(server->clients[index].fd);
    if (index + 1 < server->client_count) {
        memmove(&server->clients[index], &server->clients[index + 1],
                (size_t)(server->client_count - index - 1) *
                    sizeof(server->clients[0]));
    }
    server->client_count--;
}

static const char *format_peer_ip(const struct sockaddr *addr, char *buf,
                                  size_t buf_len)
{
    const void *src = NULL;

    if (addr->sa_family == AF_INET) {
        const struct sockaddr_in *addr4 = (const struct sockaddr_in *)addr;
        src = &addr4->sin_addr;
    } else if (addr->sa_family == AF_INET6) {
        const struct sockaddr_in6 *addr6 = (const struct sockaddr_in6 *)addr;
        src = &addr6->sin6_addr;
    }

    if (!src || !inet_ntop(addr->sa_family, src, buf, (socklen_t)buf_len)) {
        strcpy(buf, "desconhecido");
    }
    return buf;
}

static int send_udp_frame(int fd, const unsigned char *jpeg, unsigned long len,
                          unsigned int frame_id)
{
    unsigned char packet[sizeof(struct udp_frame_header) + MJPEG_UDP_PAYLOAD];
    uint16_t chunks;
    uint16_t i;

    if (len > MJPEG_MAX_FRAME) {
        return -1;
    }

    chunks = (uint16_t)((len + MJPEG_UDP_PAYLOAD - 1u) / MJPEG_UDP_PAYLOAD);
    if (chunks == 0) {
        chunks = 1;
    }

    for (i = 0; i < chunks; i++) {
        struct udp_frame_header hdr;
        size_t off = (size_t)i * MJPEG_UDP_PAYLOAD;
        size_t left = (size_t)len - off;
        size_t payload = left < MJPEG_UDP_PAYLOAD ? left : MJPEG_UDP_PAYLOAD;

        hdr.magic = htonl(MJPEG_UDP_MAGIC);
        hdr.frame_id = htonl(frame_id);
        hdr.chunk_id = htons(i);
        hdr.chunk_count = htons(chunks);
        hdr.frame_size = htonl((uint32_t)len);

        memcpy(packet, &hdr, sizeof(hdr));
        memcpy(packet + sizeof(hdr), jpeg + off, payload);
        if (send(fd, packet, sizeof(hdr) + payload, 0) < 0) {
            perror("send udp");
            return -1;
        }
    }
    return 0;
}

int stream_open(struct stream_server *server, const struct config *cfg)
{
    memset(server, 0, sizeof(*server));
    server->fd = cfg->protocol == PROTO_UDP ? udp_connect(cfg->host, cfg->port)
                                            : tcp_listen(cfg->host, cfg->port);
    server->protocol = cfg->protocol;
    server->access = &cfg->access;

    if (server->fd < 0) {
        fprintf(stderr, "falha ao abrir %s em %s:%s\n",
                cfg->protocol == PROTO_UDP ? "UDP" :
                (cfg->protocol == PROTO_HTTP ? "servidor HTTP" : "servidor TCP"),
                cfg->host, cfg->port);
        return -1;
    }

    if (cfg->protocol != PROTO_UDP) {
        if (set_nonblocking(server->fd) < 0) {
            perror("fcntl listen");
            close(server->fd);
            server->fd = -1;
            return -1;
        }
        fprintf(stderr, "servidor %s aguardando clientes em %s:%s\n",
                protocol_name(cfg->protocol), cfg->host, cfg->port);
    }
    return 0;
}

int stream_wait_fd(const struct stream_server *server)
{
    return server->fd;
}

void stream_accept_pending(struct stream_server *server)
{
    while (server->client_count < MAX_STREAM_CLIENTS) {
        struct sockaddr_storage addr;
        socklen_t addr_len = sizeof(addr);
        int fd = accept(server->fd, (struct sockaddr *)&addr, &addr_len);
        if (fd < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
                perror("accept");
            }
            return;
        }

        if (!access_control_allowed_sockaddr(server->access,
                                             (struct sockaddr *)&addr)) {
            char ip[INET6_ADDRSTRLEN];
            (void)format_peer_ip((struct sockaddr *)&addr, ip, sizeof(ip));
            fprintf(stderr, "cliente %s rejeitado por controle de acesso\n", ip);
            close(fd);
            continue;
        }

        /* Accepted sockets must never block frame capture. */
        if (set_nonblocking(fd) < 0) {
            close(fd);
            continue;
        }

        server->clients[server->client_count].fd = fd;
        server->clients[server->client_count].pending = NULL;
        server->clients[server->client_count].pending_len = 0;
        server->clients[server->client_count].pending_offset = 0;
        if (server->protocol == PROTO_HTTP &&
            client_queue(server, &server->clients[server->client_count],
                         http_stream_header, sizeof(http_stream_header) - 1u) < 0) {
            close(fd);
            continue;
        }
        server->client_count++;
        fprintf(stderr, "cliente %s conectado (%d/%d)\n",
                protocol_name(server->protocol), server->client_count,
                MAX_STREAM_CLIENTS);
    }
}

void stream_prune_stalled(struct stream_server *server)
{
    int i = 0;

    while (i < server->client_count) {
        struct client *client = &server->clients[i];

        if (client->pending && client_progress_expired(client)) {
            fprintf(stderr, "cliente %s sem progresso; desconectado\n",
                    protocol_name(server->protocol));
            remove_client(server, i);
            continue;
        }
        i++;
    }
}

int stream_send_frame(struct stream_server *server, const unsigned char *jpeg,
                      unsigned long jpeg_len, unsigned int *frame_id)
{
    int i = 0;

    if (server->protocol == PROTO_UDP) {
        return send_udp_frame(server->fd, jpeg, jpeg_len, (*frame_id)++);
    }

    stream_prune_stalled(server);
    while (i < server->client_count) {
        struct client *client = &server->clients[i];
        if (jpeg_len > MJPEG_MAX_FRAME) {
            remove_client(server, i);
            continue;
        }
        int had_pending = client->pending != NULL;
        int rc = client_flush(client);
        if (rc < 0) {
            fprintf(stderr, "cliente %s desconectado\n",
                    protocol_name(server->protocol));
            remove_client(server, i);
        } else {
            if (had_pending && rc == 0) {
                client_clear_pending(server, client);
            }
            if (!had_pending && rc == 0) {
                unsigned char *serialized = NULL;
                size_t serialized_len;
                size_t max_overhead = server->protocol == PROTO_HTTP ? 162u : 4u;
                if (server->pending_bytes > STREAM_MAX_PENDING_BYTES ||
                    (size_t)jpeg_len + max_overhead >
                        STREAM_MAX_PENDING_BYTES - server->pending_bytes) {
                    i++;
                    continue;
                }
                if (serialize_frame(server->protocol, jpeg, jpeg_len,
                                    &serialized, &serialized_len) < 0 ||
                    client_queue_owned(server, client, serialized,
                                       serialized_len) < 0) {
                    free(serialized);
                    /* Queue pressure drops this frame and leaves the client alive. */
                } else {
                    rc = client_flush(client);
                    if (rc < 0) {
                        fprintf(stderr, "cliente %s desconectado\n",
                                protocol_name(server->protocol));
                        remove_client(server, i);
                        continue;
                    }
                    if (rc == 0) {
                        client_clear_pending(server, client);
                    }
                }
            }
            i++;
        }
    }
    return 0;
}

void stream_close(struct stream_server *server)
{
    if (server->fd >= 0) {
        close(server->fd);
        server->fd = -1;
    }
    while (server->client_count > 0) {
        remove_client(server, 0);
    }
}
