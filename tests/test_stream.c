#include "stream.h"
#include "net.h"

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static size_t drain_available(int fd, unsigned char *buf, size_t capacity)
{
    size_t used = 0;
    while (used < capacity) {
        ssize_t n = recv(fd, buf + used, capacity - used, 0);
        if (n > 0) {
            used += (size_t)n;
            continue;
        }
        if (n < 0 && errno == EINTR) {
            continue;
        }
        assert(n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK));
        break;
    }
    return used;
}

static void set_nonblocking_test(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    assert(flags >= 0);
    assert(fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0);
}

static void test_tcp_eagain_recovery(void)
{
    enum { FRAME_LEN = 64 * 1024, WIRE_CAPACITY = 8 * 1024 * 1024 };
    struct stream_server server = {0};
    int pair[2];
    unsigned int frame_id = 0;
    unsigned char *first = malloc(FRAME_LEN);
    unsigned char *later = malloc(FRAME_LEN);
    unsigned char *wire = malloc(WIRE_CAPACITY);
    unsigned char scratch[65536];
    size_t used = 0;

    assert(first && later && wire);
    memset(first, 0x11, FRAME_LEN);
    memset(later, 0x22, FRAME_LEN);
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == 0);
    set_nonblocking_test(pair[0]);
    set_nonblocking_test(pair[1]);
    server.fd = -1;
    server.protocol = PROTO_TCP;
    server.client_count = 1;
    server.clients[0].fd = pair[0];

    for (int i = 0; i < 100 && !server.clients[0].pending; i++) {
        assert(stream_send_frame(&server, first, FRAME_LEN, &frame_id) == 0);
    }
    assert(server.client_count == 1);
    assert(server.clients[0].pending != NULL);
    for (int i = 0; i < 1000 && server.clients[0].pending; i++) {
        size_t n = drain_available(pair[1], scratch, sizeof(scratch));
        assert(used + n <= WIRE_CAPACITY);
        memcpy(wire + used, scratch, n);
        used += n;
        assert(stream_send_frame(&server, later, FRAME_LEN, &frame_id) == 0);
        assert(server.client_count == 1);
    }
    assert(server.clients[0].pending == NULL);
    for (;;) {
        size_t n = drain_available(pair[1], scratch, sizeof(scratch));
        if (!n) {
            break;
        }
        assert(used + n <= WIRE_CAPACITY);
        memcpy(wire + used, scratch, n);
        used += n;
    }
    assert(used >= FRAME_LEN + 4);
    assert(used % (FRAME_LEN + 4) == 0);
    for (size_t offset = 0; offset < used; offset += FRAME_LEN + 4) {
        uint32_t nlen;
        memcpy(&nlen, wire + offset, sizeof(nlen));
        assert(ntohl(nlen) == FRAME_LEN);
        assert(memcmp(wire + offset + 4, first, FRAME_LEN) == 0);
    }

    /* A fresh frame is accepted after the pending frame has drained. */
    assert(stream_send_frame(&server, later, FRAME_LEN, &frame_id) == 0);
    used = 0;
    for (int i = 0; i < 1000 && used < FRAME_LEN + 4; i++) {
        size_t n = drain_available(pair[1], wire + used,
                                   FRAME_LEN + 4 - used);
        used += n;
        if (server.clients[0].pending) {
            assert(stream_send_frame(&server, later, FRAME_LEN, &frame_id) == 0);
        }
    }
    assert(used == FRAME_LEN + 4);
    {
        uint32_t nlen;
        memcpy(&nlen, wire, sizeof(nlen));
        assert(ntohl(nlen) == FRAME_LEN);
        assert(memcmp(wire + 4, later, FRAME_LEN) == 0);
    }
    stream_close(&server);
    close(pair[1]);
    free(wire);
    free(later);
    free(first);
}

static void test_stalled_client_timeout(void)
{
    enum { FRAME_LEN = 64 * 1024 };
    struct stream_server server = {0};
    int pair[2];
    unsigned int frame_id = 0;
    unsigned char *frame = malloc(FRAME_LEN);

    assert(frame);
    memset(frame, 0x5a, FRAME_LEN);
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == 0);
    set_nonblocking_test(pair[0]);
    set_nonblocking_test(pair[1]);
    server.fd = -1;
    server.protocol = PROTO_TCP;
    server.client_count = 1;
    server.clients[0].fd = pair[0];

    for (int i = 0; i < 100 && !server.clients[0].pending; i++) {
        assert(stream_send_frame(&server, frame, FRAME_LEN, &frame_id) == 0);
    }
    assert(server.clients[0].pending != NULL);
    assert(server.pending_bytes > 0);

    /* Model a stalled pending write without sleeping for the production timeout. */
    server.clients[0].pending_progress_at.tv_sec =
        -(time_t)STREAM_PENDING_PROGRESS_TIMEOUT_SEC - 1;
    server.clients[0].pending_progress_at.tv_nsec = 0;
    stream_prune_stalled(&server);
    assert(server.client_count == 0);
    assert(server.pending_bytes == 0);

    close(pair[1]);
    free(frame);
}

static void test_http_header_and_frame(void)
{
    static const char expected[] =
        "HTTP/1.0 200 OK\r\n"
        "Server: mjpeg_tx\r\n"
        "Cache-Control: no-cache, no-store, must-revalidate\r\n"
        "Pragma: no-cache\r\n"
        "Expires: 0\r\n"
        "Connection: close\r\n"
        "Content-Type: multipart/x-mixed-replace; boundary=mjpegframe\r\n"
        "\r\n"
        "--mjpegframe\r\nContent-Type: image/jpeg\r\nContent-Length: 3\r\n\r\n"
        "abc\r\n";
    struct stream_server server = {0};
    struct access_control access;
    struct sockaddr_in addr = {0};
    socklen_t addr_len = sizeof(addr);
    int peer;
    int listener;
    unsigned int frame_id = 0;
    unsigned char actual[sizeof(expected) - 1];

    listener = socket(AF_INET, SOCK_STREAM, 0);
    assert(listener >= 0);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    assert(bind(listener, (struct sockaddr *)&addr, sizeof(addr)) == 0);
    assert(listen(listener, 1) == 0);
    assert(getsockname(listener, (struct sockaddr *)&addr, &addr_len) == 0);
    set_nonblocking_test(listener);
    peer = socket(AF_INET, SOCK_STREAM, 0);
    assert(peer >= 0);
    assert(connect(peer, (struct sockaddr *)&addr, sizeof(addr)) == 0);

    access_control_init(&access);
    server.fd = listener;
    server.protocol = PROTO_HTTP;
    server.access = &access;
    stream_accept_pending(&server);
    assert(server.client_count == 1);
    assert(server.clients[0].pending != NULL);
    assert(stream_send_frame(&server, (const unsigned char *)"abc", 3,
                             &frame_id) == 0);
    /* The first call advances the initial HTTP header; it never interleaves a frame. */
    assert(server.client_count == 1);
    assert(stream_send_frame(&server, (const unsigned char *)"abc", 3,
                             &frame_id) == 0);
    assert(read_all(peer, actual, sizeof(actual)) == 1);
    assert(memcmp(actual, expected, sizeof(actual)) == 0);
    stream_close(&server);
    close(peer);
}

int main(void)
{
    test_tcp_eagain_recovery();
    test_stalled_client_timeout();
    test_http_header_and_frame();
    puts("TCP and HTTP framing, EAGAIN recovery and stalled-client timeout passed");
    return 0;
}
