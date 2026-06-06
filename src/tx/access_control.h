#ifndef MJPEG_TX_ACCESS_CONTROL_H
#define MJPEG_TX_ACCESS_CONTROL_H

#include <stddef.h>
#include <stdint.h>
#include <sys/socket.h>

#define MAX_ACCESS_RULES 64

struct access_rule {
    int family;
    unsigned char start[16];
    unsigned char end[16];
    size_t addr_len;
};

struct access_control {
    int allow_all;
    struct access_rule rules[MAX_ACCESS_RULES];
    unsigned rule_count;
};

void access_control_init(struct access_control *access);
int access_control_add(struct access_control *access, const char *spec);
int access_control_allowed_sockaddr(const struct access_control *access,
                                    const struct sockaddr *addr);

#endif
