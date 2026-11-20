#ifndef MJPEG_RX_UDP_REASSEMBLY_H
#define MJPEG_RX_UDP_REASSEMBLY_H

#include <stddef.h>
#include <stdint.h>

#define UDP_REASSEMBLY_IDLE_TIMEOUT_MS UINT64_C(2000)

struct udp_reassembly {
    unsigned char *data;
    unsigned char *seen;
    size_t frame_size;
    uint32_t frame_id;
    unsigned short chunk_count;
    unsigned short received;
    int active;
    uint64_t last_activity_ms;
};

void udp_reassembly_init(struct udp_reassembly *state);
void udp_reassembly_reset(struct udp_reassembly *state);
void udp_reassembly_destroy(struct udp_reassembly *state);

/* Returns 1 when a complete frame is available, otherwise 0. The returned
 * frame is owned by state and remains valid until the next push/reset. */
int udp_reassembly_push(struct udp_reassembly *state,
                        const unsigned char *packet, size_t packet_len,
                        const unsigned char **frame, size_t *frame_len);

/* Timestamped variant for deterministic timeout handling and tests. */
int udp_reassembly_push_at(struct udp_reassembly *state,
                           const unsigned char *packet, size_t packet_len,
                           const unsigned char **frame, size_t *frame_len,
                           uint64_t monotonic_ms);

#endif
