#include "receiver.h"

#include "net.h"
#include "ui.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static gpointer tcp_thread(gpointer data)
{
    struct rx_app *app = data;
    while (1) {
        uint32_t nlen;
        uint32_t len;
        unsigned char *buf;
        int rc = read_all(app->fd, &nlen, sizeof(nlen));
        if (rc <= 0) {
            break;
        }
        len = ntohl(nlen);
        if (len == 0 || len > MJPEG_MAX_FRAME) {
            break;
        }
        buf = malloc(len);
        if (!buf) {
            break;
        }
        rc = read_all(app->fd, buf, len);
        if (rc <= 0) {
            free(buf);
            break;
        }
        rx_queue_frame(app, buf, len);
        free(buf);
    }
    g_idle_add((GSourceFunc)gtk_main_quit, NULL);
    return NULL;
}

static gpointer udp_thread(gpointer data)
{
    struct rx_app *app = data;
    unsigned char packet[sizeof(struct udp_frame_header) + MJPEG_UDP_PAYLOAD + 64u];

    while (1) {
        ssize_t n = recv_intr(app->fd, packet, sizeof(packet), 0);
        struct udp_frame_header hdr;
        uint32_t magic;
        uint32_t frame_id;
        uint16_t chunk_id;
        uint16_t chunk_count;
        uint32_t frame_size;
        size_t payload;
        size_t off;

        if (n <= (ssize_t)sizeof(hdr)) {
            if (n < 0) {
                perror("recv udp");
            }
            continue;
        }

        memcpy(&hdr, packet, sizeof(hdr));
        magic = ntohl(hdr.magic);
        frame_id = ntohl(hdr.frame_id);
        chunk_id = ntohs(hdr.chunk_id);
        chunk_count = ntohs(hdr.chunk_count);
        frame_size = ntohl(hdr.frame_size);
        payload = (size_t)n - sizeof(hdr);

        if (magic != MJPEG_UDP_MAGIC || frame_size == 0 ||
            frame_size > MJPEG_MAX_FRAME || chunk_count == 0 ||
            chunk_id >= chunk_count) {
            continue;
        }

        if (frame_id != app->udp_frame_id || app->frame->len != frame_size) {
            app->udp_frame_id = frame_id;
            app->udp_chunk_count = chunk_count;
            app->udp_received = 0;
            g_byte_array_set_size(app->frame, frame_size);
            free(app->udp_seen);
            app->udp_seen = calloc(chunk_count, 1);
            if (!app->udp_seen) {
                g_byte_array_set_size(app->frame, 0);
                continue;
            }
        }

        off = (size_t)chunk_id * MJPEG_UDP_PAYLOAD;
        if (off + payload > app->frame->len) {
            continue;
        }

        memcpy(app->frame->data + off, packet + sizeof(hdr), payload);
        if (!app->udp_seen[chunk_id]) {
            app->udp_seen[chunk_id] = 1;
            app->udp_received++;
        }

        if (app->udp_received == app->udp_chunk_count) {
            rx_queue_frame(app, app->frame->data, app->frame->len);
            g_byte_array_set_size(app->frame, 0);
        }
    }
    return NULL;
}

int rx_connect(struct rx_app *app, const struct rx_config *cfg)
{
    if (cfg->use_udp) {
        app->fd = udp_bind_socket(cfg->listen_host, cfg->port);
        if (app->fd < 0) {
            fprintf(stderr, "falha ao escutar UDP em %s:%s\n",
                    cfg->listen_host, cfg->port);
            return -1;
        }
    } else {
        fprintf(stderr, "conectando ao transmissor TCP em %s:%s...\n",
                cfg->host, cfg->port);
        app->fd = tcp_connect(cfg->host, cfg->port);
        if (app->fd < 0) {
            fprintf(stderr, "falha ao conectar em %s:%s\n",
                    cfg->host, cfg->port);
            return -1;
        }
    }
    return 0;
}

void rx_start_receiver(struct rx_app *app)
{
    if (app->use_udp) {
        g_thread_new("udp-recv", udp_thread, app);
    } else {
        g_thread_new("tcp-recv", tcp_thread, app);
    }
}
