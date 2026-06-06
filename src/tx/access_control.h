#ifndef MJPEG_TX_ACCESS_CONTROL_H
#define MJPEG_TX_ACCESS_CONTROL_H

#include <stdint.h>

#define MAX_ACCESS_RULES 64

struct access_rule {
    uint32_t start;
    uint32_t end;
};

struct access_control {
    int allow_all;
    struct access_rule rules[MAX_ACCESS_RULES];
    unsigned rule_count;
};

void access_control_init(struct access_control *access);
int access_control_add(struct access_control *access, const char *spec);
int access_control_allowed(const struct access_control *access, uint32_t ip);

#endif
