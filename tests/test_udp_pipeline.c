#include "stream.h"
#include "udp_reassembly.h"
#include "net.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int main(void)
{
    struct stream_server server = {0};
    struct udp_reassembly state;
    int pair[2];
    unsigned int frame_id = 0;
    unsigned char input[3000], packet[sizeof(struct udp_frame_header) + MJPEG_UDP_PAYLOAD];
    const unsigned char *output = NULL;
    size_t output_len = 0;
    memset(input, 0x5a, sizeof(input));
    assert(socketpair(AF_UNIX, SOCK_DGRAM, 0, pair) == 0);
    server.fd = pair[0];
    server.protocol = PROTO_UDP;
    udp_reassembly_init(&state);
    assert(stream_send_frame(&server, input, sizeof(input), &frame_id) == 0);
    for (int i = 0; i < 3; i++) {
        ssize_t n = recv(pair[1], packet, sizeof(packet), 0);
        int complete;
        assert(n > 0);
        complete = udp_reassembly_push(&state, packet, (size_t)n, &output, &output_len);
        assert(complete == (i == 2));
    }
    assert(output_len == sizeof(input));
    assert(memcmp(output, input, sizeof(input)) == 0);
    udp_reassembly_destroy(&state);
    stream_close(&server);
    close(pair[1]);
    puts("UDP transmitter/reassembly integration passed");
    return 0;
}
