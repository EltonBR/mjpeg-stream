#include "net.h"
#include "udp_reassembly.h"

#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static size_t make_packet(unsigned char *packet, uint32_t frame_id,
                          uint16_t chunk_id, uint16_t chunk_count,
                          uint32_t frame_size, const unsigned char *payload,
                          size_t payload_size)
{
    struct udp_frame_header hdr;

    hdr.magic = htonl(MJPEG_UDP_MAGIC);
    hdr.frame_id = htonl(frame_id);
    hdr.chunk_id = htons(chunk_id);
    hdr.chunk_count = htons(chunk_count);
    hdr.frame_size = htonl(frame_size);
    memcpy(packet, &hdr, sizeof(hdr));
    if (payload_size)
        memcpy(packet + sizeof(hdr), payload, payload_size);
    return sizeof(hdr) + payload_size;
}

static int push(struct udp_reassembly *state, unsigned char *packet,
                size_t packet_size, const unsigned char **frame,
                size_t *frame_size)
{
    return udp_reassembly_push(state, packet, packet_size, frame, frame_size);
}

static void test_reassembly_and_duplicate(void)
{
    struct udp_reassembly state;
    unsigned char packet[sizeof(struct udp_frame_header) + MJPEG_UDP_PAYLOAD];
    unsigned char first[MJPEG_UDP_PAYLOAD];
    unsigned char last[3] = { 7, 8, 9 };
    const unsigned char *frame = NULL;
    size_t frame_size = 0;
    size_t packet_size;

    memset(first, 0x5a, sizeof(first));
    udp_reassembly_init(&state);

    packet_size = make_packet(packet, 42, 1, 2, 1303, last, sizeof(last));
    assert(push(&state, packet, packet_size, &frame, &frame_size) == 0);
    assert(state.received == 1);
    assert(push(&state, packet, packet_size, &frame, &frame_size) == 0);
    assert(state.received == 1);

    packet_size = make_packet(packet, 42, 0, 2, 1303, first, sizeof(first));
    assert(push(&state, packet, packet_size, &frame, &frame_size) == 1);
    assert(frame_size == 1303);
    assert(memcmp(frame, first, sizeof(first)) == 0);
    assert(memcmp(frame + sizeof(first), last, sizeof(last)) == 0);
    udp_reassembly_destroy(&state);
}

static void test_invalid_packet_sizes_and_metadata(void)
{
    struct udp_reassembly state;
    unsigned char packet[sizeof(struct udp_frame_header) + MJPEG_UDP_PAYLOAD + 1];
    unsigned char chunk[MJPEG_UDP_PAYLOAD];
    const unsigned char *frame = NULL;
    size_t frame_size = 0;
    size_t packet_size;
    struct udp_frame_header hdr;

    memset(chunk, 0x31, sizeof(chunk));
    udp_reassembly_init(&state);

    packet_size = make_packet(packet, 10, 0, 2, 1, chunk, 1);
    assert(push(&state, packet, packet_size - 1, &frame, &frame_size) == 0);
    assert(!state.active);
    assert(push(&state, packet, packet_size, &frame, &frame_size) == 0);
    assert(!state.active); /* advertised chunk count must match frame size */

    packet_size = make_packet(packet, 10, 0, 2, 1301, chunk, 1300);
    assert(push(&state, packet, packet_size, &frame, &frame_size) == 0);
    assert(state.active && state.received == 1);

    /* Same frame ID cannot change its size/count metadata mid-frame. */
    packet_size = make_packet(packet, 10, 1, 2, 1302, chunk, 2);
    assert(push(&state, packet, packet_size, &frame, &frame_size) == 0);
    assert(state.active && state.frame_size == 1301 && state.received == 1);

    /* Exact payload length is required for every chunk. */
    packet_size = make_packet(packet, 10, 1, 2, 1301, chunk, 2);
    assert(push(&state, packet, packet_size, &frame, &frame_size) == 0);
    assert(state.received == 1);

    packet_size = make_packet(packet, 10, 1, 2, 1301, chunk, 1);
    assert(push(&state, packet, packet_size, &frame, &frame_size) == 1);
    assert(frame_size == 1301);
    udp_reassembly_destroy(&state);

    udp_reassembly_init(&state);
    packet_size = make_packet(packet, 11, 0, 1, 1, chunk, 1);
    memcpy(&hdr, packet, sizeof(hdr));
    hdr.magic = htonl(0);
    memcpy(packet, &hdr, sizeof(hdr));
    assert(push(&state, packet, packet_size, &frame, &frame_size) == 0);
    assert(!state.active);
    udp_reassembly_destroy(&state);
}

static void test_conflicting_duplicate_does_not_replace_data(void)
{
    struct udp_reassembly state;
    unsigned char packet[sizeof(struct udp_frame_header) + MJPEG_UDP_PAYLOAD];
    unsigned char original[MJPEG_UDP_PAYLOAD];
    unsigned char conflict[MJPEG_UDP_PAYLOAD];
    unsigned char last = 0xe7;
    const unsigned char *frame = NULL;
    size_t frame_size = 0;
    size_t packet_size;

    memset(original, 0x11, sizeof(original));
    memset(conflict, 0x22, sizeof(conflict));
    udp_reassembly_init(&state);

    packet_size = make_packet(packet, 7, 0, 2, 1301,
                              original, sizeof(original));
    assert(push(&state, packet, packet_size, &frame, &frame_size) == 0);
    packet_size = make_packet(packet, 7, 0, 2, 1301,
                              conflict, sizeof(conflict));
    assert(push(&state, packet, packet_size, &frame, &frame_size) == 0);
    assert(state.received == 1);

    packet_size = make_packet(packet, 7, 1, 2, 1301, &last, 1);
    assert(push(&state, packet, packet_size, &frame, &frame_size) == 1);
    assert(memcmp(frame, original, sizeof(original)) == 0);
    assert(frame[1300] == last);
    udp_reassembly_destroy(&state);
}

static void test_new_frame_replaces_partial_frame(void)
{
    struct udp_reassembly state;
    unsigned char packet[sizeof(struct udp_frame_header) + MJPEG_UDP_PAYLOAD];
    unsigned char payload[MJPEG_UDP_PAYLOAD];
    const unsigned char *frame = NULL;
    size_t frame_size = 0;
    size_t packet_size;

    memset(payload, 0x43, sizeof(payload));
    udp_reassembly_init(&state);
    packet_size = make_packet(packet, 1, 0, 2, 1301,
                              payload, MJPEG_UDP_PAYLOAD);
    assert(push(&state, packet, packet_size, &frame, &frame_size) == 0);
    packet_size = make_packet(packet, 2, 0, 1, 2, payload, 2);
    assert(push(&state, packet, packet_size, &frame, &frame_size) == 1);
    assert(state.frame_id == 2 && frame_size == 2);
    assert(memcmp(frame, payload, 2) == 0);
    udp_reassembly_destroy(&state);
}

static void test_delayed_previous_frame_is_ignored(void)
{
    struct udp_reassembly state;
    unsigned char packet[sizeof(struct udp_frame_header) + 2];
    unsigned char previous[2] = { 1, 2 };
    unsigned char latest[2] = { 8, 9 };
    const unsigned char *frame = NULL;
    size_t frame_size = 0;
    size_t packet_size;

    udp_reassembly_init(&state);
    packet_size = make_packet(packet, 100, 0, 1, 2,
                              latest, sizeof(latest));
    assert(push(&state, packet, packet_size, &frame, &frame_size) == 1);
    assert(state.frame_id == 100 && memcmp(frame, latest, sizeof(latest)) == 0);

    packet_size = make_packet(packet, 99, 0, 1, 2,
                              previous, sizeof(previous));
    assert(push(&state, packet, packet_size, &frame, &frame_size) == 0);
    assert(state.frame_id == 100 && frame_size == 0);

    /* Replaying the completed frame also must not emit it again. */
    packet_size = make_packet(packet, 100, 0, 1, 2,
                              latest, sizeof(latest));
    assert(push(&state, packet, packet_size, &frame, &frame_size) == 0);
    assert(state.frame_id == 100 && frame_size == 0);
    udp_reassembly_destroy(&state);
}

static void test_frame_id_serial_wrap(void)
{
    struct udp_reassembly state;
    unsigned char packet[sizeof(struct udp_frame_header) + 1];
    unsigned char payload = 0x72;
    const unsigned char *frame = NULL;
    size_t frame_size = 0;
    size_t packet_size;

    udp_reassembly_init(&state);
    packet_size = make_packet(packet, UINT32_MAX, 0, 1, 1,
                              &payload, sizeof(payload));
    assert(push(&state, packet, packet_size, &frame, &frame_size) == 1);
    assert(state.frame_id == UINT32_MAX);

    packet_size = make_packet(packet, 0, 0, 1, 1, &payload, sizeof(payload));
    assert(push(&state, packet, packet_size, &frame, &frame_size) == 1);
    assert(state.frame_id == 0);

    packet_size = make_packet(packet, UINT32_MAX, 0, 1, 1,
                              &payload, sizeof(payload));
    assert(push(&state, packet, packet_size, &frame, &frame_size) == 0);
    assert(state.frame_id == 0 && frame_size == 0);
    udp_reassembly_destroy(&state);
}

static void test_transmitter_restart_after_idle_timeout(void)
{
    struct udp_reassembly state;
    unsigned char packet[sizeof(struct udp_frame_header) + 1];
    unsigned char payload = 0x5c;
    const unsigned char *frame = NULL;
    size_t frame_size = 0;
    size_t packet_size;

    udp_reassembly_init(&state);
    packet_size = make_packet(packet, UINT32_C(0x70000000), 0, 1, 1,
                              &payload, sizeof(payload));
    assert(udp_reassembly_push_at(&state, packet, packet_size,
                                  &frame, &frame_size, 1000) == 1);
    assert(state.frame_id == UINT32_C(0x70000000));

    packet_size = make_packet(packet, 0, 0, 1, 1, &payload, sizeof(payload));
    assert(udp_reassembly_push_at(&state, packet, packet_size,
                                  &frame, &frame_size,
                                  1000 + UDP_REASSEMBLY_IDLE_TIMEOUT_MS - 1) == 0);
    assert(state.frame_id == UINT32_C(0x70000000));

    assert(udp_reassembly_push_at(&state, packet, packet_size,
                                  &frame, &frame_size,
                                  1000 + UDP_REASSEMBLY_IDLE_TIMEOUT_MS) == 1);
    assert(state.frame_id == 0 && frame_size == 1 && frame[0] == payload);
    udp_reassembly_destroy(&state);
}

int main(void)
{
    test_reassembly_and_duplicate();
    test_invalid_packet_sizes_and_metadata();
    test_conflicting_duplicate_does_not_replace_data();
    test_new_frame_replaces_partial_frame();
    test_delayed_previous_frame_is_ignored();
    test_frame_id_serial_wrap();
    test_transmitter_restart_after_idle_timeout();
    puts("UDP reassembly tests passed");
    return 0;
}
