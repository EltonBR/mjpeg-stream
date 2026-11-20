#include "receiver.h"

#include <assert.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

void rx_queue_frame(struct rx_app *app, const unsigned char *data, size_t len)
{
    (void)app;
    (void)data;
    (void)len;
}

static void init_test_app(struct rx_app *app)
{
    memset(app, 0, sizeof(*app));
    app->fd = -1;
    app->receiver_cancel[0] = app->receiver_cancel[1] = -1;
    app->receiver_notify[0] = app->receiver_notify[1] = -1;
    g_mutex_init(&app->receiver_mutex);
}

static void wait_done(struct rx_app *app)
{
    gint64 deadline = g_get_monotonic_time() + G_TIME_SPAN_SECOND;
    while (!g_atomic_int_get(&app->receiver_done) &&
           g_get_monotonic_time() < deadline)
        g_usleep(1000);
    assert(g_atomic_int_get(&app->receiver_done));
}

static void test_cancel_receiver(void)
{
    struct rx_app app;
    int sockets[2];
    gint64 start;
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);
    init_test_app(&app);
    app.fd = sockets[0];
    assert(rx_start_receiver(&app) == 0);
    g_usleep(20000);
    start = g_get_monotonic_time();
    rx_stop_receiver(&app);
    assert(g_get_monotonic_time() - start < G_TIME_SPAN_SECOND);
    assert(app.receiver_thread == NULL);
    close(sockets[0]);
    close(sockets[1]);
    g_mutex_clear(&app.receiver_mutex);
}

static void test_initial_failure(void)
{
    struct rx_app app;
    init_test_app(&app);
    app.receiver_host = "invalid-address";
    app.receiver_port = "5000";
    assert(rx_start_receiver(&app) == 0);
    wait_done(&app);
    assert(g_atomic_int_get(&app.receiver_initial_failed));
    rx_stop_receiver(&app);
    g_mutex_clear(&app.receiver_mutex);
}

static void test_udp_cancel(void)
{
    struct rx_app app;
    struct sockaddr_in address = {0};
    socklen_t len = sizeof(address);
    char port[16];
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    assert(fd >= 0);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    assert(bind(fd, (struct sockaddr *)&address, sizeof(address)) == 0);
    assert(getsockname(fd, (struct sockaddr *)&address, &len) == 0);
    snprintf(port, sizeof(port), "%u", ntohs(address.sin_port));
    close(fd);
    init_test_app(&app);
    app.use_udp = 1;
    app.receiver_host = "127.0.0.1";
    app.receiver_port = port;
    assert(rx_start_receiver(&app) == 0);
    g_usleep(20000);
    rx_stop_receiver(&app);
    assert(!g_atomic_int_get(&app.receiver_initial_failed));
    if (app.fd >= 0) close(app.fd);
    g_mutex_clear(&app.receiver_mutex);
}

int main(void)
{
    test_cancel_receiver();
    test_initial_failure();
    test_udp_cancel();
    puts("receiver cancellation/failure passed");
    return 0;
}
