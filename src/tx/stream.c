#include "stream.h"

#include "net.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdint.h>
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

static int send_all_nosigpipe(int fd, const void *buf, size_t len)
{
    const unsigned char *p = buf;
    while (len > 0) {
        ssize_t n = send(fd, p, len, MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (n == 0) {
            return -1;
        }
        p += (size_t)n;
        len -= (size_t)n;
    }
    return 0;
}

static int send_tcp_frame_to_client(int fd, const unsigned char *jpeg,
                                    unsigned long len)
{
    uint32_t nlen;
    if (len > MJPEG_MAX_FRAME) {
        return -1;
    }
    nlen = htonl((uint32_t)len);
    if (send_all_nosigpipe(fd, &nlen, sizeof(nlen)) < 0 ||
        send_all_nosigpipe(fd, jpeg, (size_t)len) < 0) {
        return -1;
    }
    return 0;
}

static int send_http_stream_header(int fd)
{
    static const char header[] =
        "HTTP/1.0 200 OK\r\n"
        "Server: mjpeg_tx\r\n"
        "Cache-Control: no-cache, no-store, must-revalidate\r\n"
        "Pragma: no-cache\r\n"
        "Expires: 0\r\n"
        "Connection: close\r\n"
        "Content-Type: multipart/x-mixed-replace; boundary=mjpegframe\r\n"
        "\r\n";
    return send_all_nosigpipe(fd, header, sizeof(header) - 1u);
}

static int send_http_frame_to_client(int fd, const unsigned char *jpeg,
                                     unsigned long len)
{
    char header[160];
    int header_len;
    if (len > MJPEG_MAX_FRAME) {
        return -1;
    }

    header_len = snprintf(header, sizeof(header),
                          "--mjpegframe\r\n"
                          "Content-Type: image/jpeg\r\n"
                          "Content-Length: %lu\r\n"
                          "\r\n",
                          len);
    if (header_len < 0 || (size_t)header_len >= sizeof(header)) {
        return -1;
    }

    if (send_all_nosigpipe(fd, header, (size_t)header_len) < 0 ||
        send_all_nosigpipe(fd, jpeg, (size_t)len) < 0 ||
        send_all_nosigpipe(fd, "\r\n", 2) < 0) {
        return -1;
    }
    return 0;
}

static void remove_client(struct stream_server *server, int index)
{
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

        if (server->protocol == PROTO_HTTP &&
            send_http_stream_header(fd) < 0) {
            close(fd);
            continue;
        }

        server->clients[server->client_count].fd = fd;
        server->client_count++;
        fprintf(stderr, "cliente %s conectado (%d/%d)\n",
                protocol_name(server->protocol), server->client_count,
                MAX_STREAM_CLIENTS);
    }
}

int stream_send_frame(struct stream_server *server, const unsigned char *jpeg,
                      unsigned long jpeg_len, unsigned int *frame_id)
{
    int i = 0;

    if (server->protocol == PROTO_UDP) {
        return send_udp_frame(server->fd, jpeg, jpeg_len, (*frame_id)++);
    }

    while (i < server->client_count) {
        int rc = server->protocol == PROTO_HTTP
                     ? send_http_frame_to_client(server->clients[i].fd,
                                                 jpeg, jpeg_len)
                     : send_tcp_frame_to_client(server->clients[i].fd,
                                                jpeg, jpeg_len);
        if (rc < 0) {
            fprintf(stderr, "cliente %s desconectado\n",
                    protocol_name(server->protocol));
            remove_client(server, i);
        } else {
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
