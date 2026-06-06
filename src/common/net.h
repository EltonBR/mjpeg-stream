#ifndef MJPEG_NET_H
#define MJPEG_NET_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#define MJPEG_UDP_MAGIC 0x4d4a5047u
#define MJPEG_UDP_PAYLOAD 1300u
#define MJPEG_MAX_FRAME (16u * 1024u * 1024u)

struct udp_frame_header {
    uint32_t magic;
    uint32_t frame_id;
    uint16_t chunk_id;
    uint16_t chunk_count;
    uint32_t frame_size;
};

int tcp_connect(const char *host, const char *port);
int tcp_listen(const char *host, const char *port);
int udp_connect(const char *host, const char *port);
int udp_bind_socket(const char *host, const char *port);
int set_reuseaddr(int fd);
int write_all(int fd, const void *buf, size_t len);
int read_all(int fd, void *buf, size_t len);
ssize_t recv_intr(int fd, void *buf, size_t len, int flags);

#endif
