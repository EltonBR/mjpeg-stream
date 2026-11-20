#include "udp_reassembly.h"

#include "net.h"

#include <arpa/inet.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int frame_id_is_newer(uint32_t candidate, uint32_t current)
{
    uint32_t distance = candidate - current;
    return distance != 0 && distance < UINT32_C(0x80000000);
}

void udp_reassembly_init(struct udp_reassembly *state)
{
    memset(state, 0, sizeof(*state));
}

void udp_reassembly_reset(struct udp_reassembly *state)
{
    free(state->data);
    free(state->seen);
    state->data = NULL;
    state->seen = NULL;
    state->frame_size = 0;
    state->frame_id = 0;
    state->chunk_count = 0;
    state->received = 0;
    state->active = 0;
    state->last_activity_ms = 0;
}

void udp_reassembly_destroy(struct udp_reassembly *state)
{
    udp_reassembly_reset(state);
}

int udp_reassembly_push_at(struct udp_reassembly *state,
                           const unsigned char *packet, size_t packet_len,
                           const unsigned char **frame, size_t *frame_len,
                           uint64_t monotonic_ms)
{
    struct udp_frame_header hdr;
    uint32_t frame_id;
    unsigned short chunk_id;
    unsigned short chunk_count;
    size_t frame_size;
    size_t payload_len;
    size_t chunk_offset;
    size_t expected_chunks;
    size_t expected_payload;

    if (frame) *frame = NULL;
    if (frame_len) *frame_len = 0;
    if (!state || !packet || packet_len < sizeof(hdr))
        return 0;

    memcpy(&hdr, packet, sizeof(hdr));
    frame_id = ntohl(hdr.frame_id);
    chunk_id = ntohs(hdr.chunk_id);
    chunk_count = ntohs(hdr.chunk_count);
    frame_size = ntohl(hdr.frame_size);
    payload_len = packet_len - sizeof(hdr);

    if (ntohl(hdr.magic) != MJPEG_UDP_MAGIC || frame_size == 0 ||
        frame_size > MJPEG_MAX_FRAME || chunk_count == 0 ||
        chunk_id >= chunk_count)
        return 0;

    expected_chunks = (frame_size + MJPEG_UDP_PAYLOAD - 1u) /
                      MJPEG_UDP_PAYLOAD;
    if (expected_chunks != chunk_count)
        return 0;

    chunk_offset = (size_t)chunk_id * MJPEG_UDP_PAYLOAD;
    expected_payload = frame_size - chunk_offset;
    if (expected_payload > MJPEG_UDP_PAYLOAD)
        expected_payload = MJPEG_UDP_PAYLOAD;
    if (payload_len != expected_payload)
        return 0;

    if (state->active && monotonic_ms >= state->last_activity_ms &&
        monotonic_ms - state->last_activity_ms >=
            UDP_REASSEMBLY_IDLE_TIMEOUT_MS)
        udp_reassembly_reset(state);

    if (state->active && state->frame_id != frame_id &&
        !frame_id_is_newer(frame_id, state->frame_id))
        return 0;

    if (state->active && state->frame_id == frame_id &&
        (state->frame_size != frame_size ||
         state->chunk_count != chunk_count)) {
        return 0;
    }

    if (!state->active || state->frame_id != frame_id) {
        unsigned char *new_data = malloc(frame_size);
        unsigned char *new_seen = calloc(chunk_count, 1);
        if (!new_data || !new_seen) {
            free(new_data);
            free(new_seen);
            udp_reassembly_reset(state);
            return 0;
        }
        udp_reassembly_reset(state);
        state->data = new_data;
        state->seen = new_seen;
        state->frame_size = frame_size;
        state->frame_id = frame_id;
        state->chunk_count = chunk_count;
        state->active = 1;
    }

    state->last_activity_ms = monotonic_ms;
    if (state->seen[chunk_id]) {
        if (memcmp(state->data + chunk_offset, packet + sizeof(hdr),
                   payload_len) != 0)
            return 0;
        return 0;
    }

    memcpy(state->data + chunk_offset, packet + sizeof(hdr), payload_len);
    state->seen[chunk_id] = 1;
    state->received++;
    if (state->received != state->chunk_count)
        return 0;

    if (frame) *frame = state->data;
    if (frame_len) *frame_len = state->frame_size;
    return 1;
}

int udp_reassembly_push(struct udp_reassembly *state,
                        const unsigned char *packet, size_t packet_len,
                        const unsigned char **frame, size_t *frame_len)
{
    struct timespec now;
    uint64_t monotonic_ms = 0;

    if (clock_gettime(CLOCK_MONOTONIC, &now) == 0 && now.tv_sec >= 0)
        monotonic_ms = (uint64_t)now.tv_sec * UINT64_C(1000) +
                       (uint64_t)now.tv_nsec / UINT64_C(1000000);
    return udp_reassembly_push_at(state, packet, packet_len, frame, frame_len,
                                  monotonic_ms);
}
