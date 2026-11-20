#include "telemetry.h"
#include "telemetry_store.h"

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static GMutex applied_mutex;
static unsigned applied_count;
static char applied_line[128];
static GThread *test_thread;
static unsigned notification_count;
static gboolean notification_on_test_thread;

static void telemetry_notified(gpointer user_data)
{
    unsigned *count = user_data;

    (*count)++;
    notification_count++;
    notification_on_test_thread = g_thread_self() == test_thread;
}

/* The test exercises connection and shutdown without requiring overlay setup. */
void overlay_apply_json_line(struct overlay_state *overlay, const char *line)
{
    (void)overlay;
    g_mutex_lock(&applied_mutex);
    applied_count++;
    g_strlcpy(applied_line, line, sizeof(applied_line));
    g_mutex_unlock(&applied_mutex);
}

static int write_all(int fd, const void *data, size_t length)
{
    const char *bytes = data;

    while (length > 0) {
        ssize_t written = write(fd, bytes, length);
        if (written < 0 && errno == EINTR) {
            continue;
        }
        if (written <= 0) {
            return -1;
        }
        bytes += written;
        length -= (size_t)written;
    }
    return 0;
}

int main(void)
{
    struct sockaddr_in address;
    struct telemetry_client telemetry;
    TelemetryStore *store;
    socklen_t address_len = sizeof(address);
    char port[16];
    int listener;
    int accepted;
    int rc;
    unsigned notifications_seen = 0;
    char byte;
    char *long_line;
    size_t long_line_length = 9000;
    static const char prefix[] = "{\"type\":\"telemetry\",\"value\":\"";
    static const char suffix[] =
        "{\"type\":\"telemetry\",\"value\":37}\n{\"type\":\"ok\"}\n";
    unsigned i;

    g_mutex_init(&applied_mutex);

    listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0 && (errno == EPERM || errno == EACCES)) {
        puts("telemetry connect and shutdown skipped (network sockets unavailable)");
        return 0;
    }
    assert(listener >= 0);
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    rc = bind(listener, (struct sockaddr *)&address, sizeof(address));
    if (rc < 0 && (errno == EPERM || errno == EACCES)) {
        close(listener);
        puts("telemetry connect and shutdown skipped (network sockets unavailable)");
        return 0;
    }
    assert(rc == 0);
    assert(getsockname(listener, (struct sockaddr *)&address, &address_len) == 0);
    assert(listen(listener, 1) == 0);
    assert(snprintf(port, sizeof(port), "%u", ntohs(address.sin_port)) > 0);

    telemetry_init(&telemetry);
    store = telemetry_store_new();
    telemetry_start_with_store(&telemetry, "127.0.0.1", port, NULL, store,
                               g_main_context_default(), telemetry_notified,
                               &notifications_seen, 1000);
    test_thread = g_thread_self();
    accepted = accept(listener, NULL, NULL);
    assert(accepted >= 0);

    /* A truncated JSON prefix must be discarded through its newline. */
    long_line = malloc(long_line_length + 1 + sizeof(suffix));
    assert(long_line != NULL);
    memcpy(long_line, prefix, sizeof(prefix) - 1);
    for (i = sizeof(prefix) - 1; i < long_line_length - 2; i++) {
        long_line[i] = 'x';
    }
    long_line[long_line_length - 2] = '"';
    long_line[long_line_length - 1] = '}';
    long_line[long_line_length] = '\n';
    memcpy(long_line + long_line_length + 1, suffix, sizeof(suffix));
    assert(write_all(accepted, long_line,
                     long_line_length + 1 + sizeof(suffix) - 1) == 0);
    free(long_line);

    for (i = 0; i < 2000; i++) {
        unsigned count;
        g_mutex_lock(&applied_mutex);
        count = applied_count;
        g_mutex_unlock(&applied_mutex);
        if (count > 0) {
            break;
        }
        g_usleep(1000);
    }
    g_mutex_lock(&applied_mutex);
    assert(applied_count == 1);
    assert(strcmp(applied_line, "{\"type\":\"ok\"}") == 0);
    g_mutex_unlock(&applied_mutex);
    {
        GHashTable *snapshot = telemetry_store_snapshot(store);
        assert(strcmp(g_hash_table_lookup(snapshot, "value"), "37") == 0);
        g_hash_table_destroy(snapshot);
    }

    for (i = 0; i < 2000 && notification_count == 0; i++) {
        g_main_context_iteration(NULL, FALSE);
        g_usleep(1000);
    }
    assert(notification_count > 0);
    assert(notifications_seen > 0);
    assert(notification_on_test_thread);

    /* A notification queued just before close must be cancelled. */
    assert(write_all(accepted, "{\"type\":\"ok2\"}\n",
                     sizeof("{\"type\":\"ok2\"}\n") - 1) == 0);
    for (i = 0; i < 2000; i++) {
        unsigned count;
        g_mutex_lock(&applied_mutex);
        count = applied_count;
        g_mutex_unlock(&applied_mutex);
        if (count > 1) {
            break;
        }
        g_usleep(1000);
    }
    g_mutex_lock(&applied_mutex);
    assert(applied_count == 2);
    g_mutex_unlock(&applied_mutex);

    /* The worker is blocked reading again; close must wake it and join. */
    telemetry_close(&telemetry);
    while (g_main_context_iteration(NULL, FALSE)) {
    }
    assert(notification_count == 1);
    assert(read(accepted, &byte, sizeof(byte)) == 0);

    close(accepted);
    close(listener);
    telemetry_store_free(store);
    g_mutex_clear(&applied_mutex);
    puts("telemetry connect and shutdown passed");
    return 0;
}
