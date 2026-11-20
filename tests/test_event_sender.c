#include "event_sender.h"

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#define IO_TIMEOUT_MS 5000

static int make_listener(char port[6])
{
    struct sockaddr_in address;
    socklen_t address_len = sizeof(address);
    int fd = socket(AF_INET, SOCK_STREAM, 0);

    assert(fd >= 0);
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    assert(bind(fd, (struct sockaddr *)&address, sizeof(address)) == 0);
    assert(listen(fd, 8) == 0);
    assert(getsockname(fd, (struct sockaddr *)&address, &address_len) == 0);
    assert(snprintf(port, 6, "%u", (unsigned int)ntohs(address.sin_port)) > 0);
    return fd;
}

static int listen_on_port(const char *port)
{
    struct sockaddr_in address;
    int fd = socket(AF_INET, SOCK_STREAM, 0);

    assert(fd >= 0);
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons((uint16_t)atoi(port));
    assert(bind(fd, (struct sockaddr *)&address, sizeof(address)) == 0);
    assert(listen(fd, 8) == 0);
    return fd;
}

static int accept_with_timeout(int listener, int timeout_ms)
{
    struct pollfd pfd;
    int result;

    pfd.fd = listener;
    pfd.events = POLLIN;
    pfd.revents = 0;
    do {
        result = poll(&pfd, 1, timeout_ms);
    } while (result < 0 && errno == EINTR);
    if (result <= 0) {
        return -1;
    }
    return accept(listener, NULL, NULL);
}

static int read_line(int fd, char *line, size_t capacity, int timeout_ms)
{
    size_t used = 0;
    gint64 deadline = g_get_monotonic_time() + (gint64)timeout_ms * 1000;

    assert(capacity > 1);
    while (used + 1 < capacity) {
        struct pollfd pfd;
        gint64 remaining = deadline - g_get_monotonic_time();
        int result;
        char byte;
        ssize_t received;

        if (remaining <= 0) {
            return -1;
        }
        pfd.fd = fd;
        pfd.events = POLLIN;
        pfd.revents = 0;
        do {
            result = poll(&pfd, 1, (int)((remaining + 999) / 1000));
        } while (result < 0 && errno == EINTR);
        if (result <= 0) {
            return -1;
        }
        received = recv(fd, &byte, 1, 0);
        if (received <= 0) {
            return -1;
        }
        if (byte == '\n') {
            line[used] = '\0';
            return (int)used;
        }
        line[used++] = byte;
    }
    return -1;
}

static void start_sender(struct event_sender *sender, const char *port)
{
    event_sender_init(sender);
    event_sender_start(sender, "127.0.0.1", port);
}

static void test_ordered_delivery(void)
{
    static const char *events[] = {
        "{\"sequence\":1}", "{\"sequence\":2}",
        "{\"sequence\":3}", "{\"sequence\":4}"
    };
    struct event_sender sender;
    char port[6];
    char line[128];
    int listener = make_listener(port);
    int peer;
    size_t i;

    start_sender(&sender, port);
    peer = accept_with_timeout(listener, IO_TIMEOUT_MS);
    assert(peer >= 0);
    for (i = 0; i < G_N_ELEMENTS(events); i++) {
        event_sender_send(&sender, events[i]);
    }
    for (i = 0; i < G_N_ELEMENTS(events); i++) {
        assert(read_line(peer, line, sizeof(line), IO_TIMEOUT_MS) > 0);
        assert(strcmp(line, events[i]) == 0);
    }

    event_sender_close(&sender);
    close(peer);
    close(listener);
}

static void test_reconnect_without_sigpipe(void)
{
    struct event_sender sender;
    struct sigaction action;
    struct sigaction old_action;
    char port[6];
    char line[128];
    int listener = make_listener(port);
    int first_peer;
    int attempt;
    int reconnected = 0;

    memset(&action, 0, sizeof(action));
    action.sa_handler = SIG_DFL;
    sigemptyset(&action.sa_mask);
    assert(sigaction(SIGPIPE, &action, &old_action) == 0);

    start_sender(&sender, port);
    first_peer = accept_with_timeout(listener, IO_TIMEOUT_MS);
    assert(first_peer >= 0);
    event_sender_send(&sender, "before-reset");
    assert(read_line(first_peer, line, sizeof(line), IO_TIMEOUT_MS) > 0);
    assert(strcmp(line, "before-reset") == 0);

    /* RST makes the sender discover the dead peer on its next write. */
    {
        struct linger reset = {1, 0};
        assert(setsockopt(first_peer, SOL_SOCKET, SO_LINGER,
                          &reset, sizeof(reset)) == 0);
    }
    close(first_peer);

    for (attempt = 0; attempt < 30 && !reconnected; attempt++) {
        event_sender_send(&sender, "after-reset");
        first_peer = accept_with_timeout(listener, 200);
        if (first_peer >= 0) {
            if (read_line(first_peer, line, sizeof(line), 1000) > 0 &&
                strcmp(line, "after-reset") == 0) {
                reconnected = 1;
            }
            close(first_peer);
        }
        g_usleep(50000);
    }
    assert(reconnected);
    event_sender_close(&sender);
    close(listener);
    assert(sigaction(SIGPIPE, &old_action, NULL) == 0);
}

static void test_shutdown_with_nonreading_peer(void)
{
    struct event_sender sender;
    char port[6];
    char *large_event;
    int listener = make_listener(port);
    int peer;
    int receive_buffer = 1024;
    gint64 start;
    gint64 elapsed;
    const size_t event_size = 8 * 1024 * 1024;

    start_sender(&sender, port);
    peer = accept_with_timeout(listener, IO_TIMEOUT_MS);
    assert(peer >= 0);
    assert(setsockopt(peer, SOL_SOCKET, SO_RCVBUF,
                      &receive_buffer, sizeof(receive_buffer)) == 0);

    large_event = g_malloc(event_size + 1);
    memset(large_event, 'x', event_size);
    large_event[event_size] = '\0';
    event_sender_send(&sender, large_event);
    g_free(large_event);
    g_usleep(100000);

    start = g_get_monotonic_time();
    event_sender_close(&sender);
    elapsed = g_get_monotonic_time() - start;
    assert(elapsed < G_USEC_PER_SEC);

    close(peer);
    close(listener);
}

static void test_discrete_events_survive_saturation(void)
{
    struct event_sender sender;
    char event[64];
    gpointer item;
    guint i;

    event_sender_init(&sender);
    sender.enabled = 1;
    for (i = 0; i < EVENT_SENDER_QUEUE_LIMIT; i++) {
        snprintf(event, sizeof(event), "{\"type\":\"motion\",\"n\":%u}", i);
        event_sender_send(&sender, event);
    }
    event_sender_send(&sender, "{\"type\":\"mousedown\",\"n\":999}");
    assert(sender.queued_events == EVENT_SENDER_QUEUE_LIMIT);

    item = g_async_queue_try_pop(sender.queue);
    assert(item != NULL);
    assert(strcmp(item, "{\"type\":\"motion\",\"n\":1}") == 0);
    g_free(item);
    for (i = 2; i < EVENT_SENDER_QUEUE_LIMIT; i++) {
        char expected[64];

        item = g_async_queue_try_pop(sender.queue);
        assert(item != NULL);
        snprintf(expected, sizeof(expected),
                 "{\"type\":\"motion\",\"n\":%u}", i);
        assert(strcmp(item, expected) == 0);
        g_free(item);
    }
    item = g_async_queue_try_pop(sender.queue);
    assert(item != NULL);
    assert(strcmp(item, "{\"type\":\"mousedown\",\"n\":999}") == 0);
    g_free(item);
    sender.queued_events = 0;

    event_sender_close(&sender);
}

static void test_queue_waits_for_rate_limited_reconnect(void)
{
    struct event_sender sender;
    char port[6];
    char line[128];
    int temporary_listener = make_listener(port);
    int listener;
    int peer;
    guint queued;

    close(temporary_listener);
    start_sender(&sender, port);
    event_sender_send(&sender, "queued-1");
    event_sender_send(&sender, "queued-2");
    g_usleep(250000);
    g_mutex_lock(&sender.lock);
    queued = sender.queued_events;
    g_mutex_unlock(&sender.lock);
    assert(queued == 2);

    listener = listen_on_port(port);
    peer = accept_with_timeout(listener, 3000);
    assert(peer >= 0);
    assert(read_line(peer, line, sizeof(line), IO_TIMEOUT_MS) > 0);
    assert(strcmp(line, "queued-1") == 0);
    assert(read_line(peer, line, sizeof(line), IO_TIMEOUT_MS) > 0);
    assert(strcmp(line, "queued-2") == 0);

    event_sender_close(&sender);
    close(peer);
    close(listener);
}

int main(void)
{
    test_ordered_delivery();
    test_reconnect_without_sigpipe();
    test_shutdown_with_nonreading_peer();
    test_discrete_events_survive_saturation();
    test_queue_waits_for_rate_limited_reconnect();
    puts("event sender ordering, reconnect, saturation, shutdown and SIGPIPE passed");
    return 0;
}
