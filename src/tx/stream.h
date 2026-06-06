#ifndef MJPEG_TX_STREAM_H
#define MJPEG_TX_STREAM_H

#include "config.h"

#define MAX_STREAM_CLIENTS 32

struct client {
    int fd;
};

struct stream_server {
    int fd;
    enum protocol protocol;
    const struct access_control *access;
    struct client clients[MAX_STREAM_CLIENTS];
    int client_count;
};

int stream_open(struct stream_server *server, const struct config *cfg);
int stream_wait_fd(const struct stream_server *server);
void stream_accept_pending(struct stream_server *server);
int stream_send_frame(struct stream_server *server, const unsigned char *jpeg,
                      unsigned long jpeg_len, unsigned int *frame_id);
void stream_close(struct stream_server *server);

#endif
