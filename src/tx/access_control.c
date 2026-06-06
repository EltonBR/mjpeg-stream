#include "access_control.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int parse_ip(const char *text, int *family, unsigned char *addr,
                    size_t *addr_len)
{
    struct in_addr addr4;
    struct in6_addr addr6;

    if (inet_pton(AF_INET, text, &addr4) == 1) {
        *family = AF_INET;
        memcpy(addr, &addr4, sizeof(addr4));
        *addr_len = sizeof(addr4);
        return 0;
    }
    if (inet_pton(AF_INET6, text, &addr6) == 1) {
        *family = AF_INET6;
        memcpy(addr, &addr6, sizeof(addr6));
        *addr_len = sizeof(addr6);
        return 0;
    }
    return -1;
}

static int compare_bytes(const unsigned char *a, const unsigned char *b,
                         size_t len)
{
    return memcmp(a, b, len);
}

static int add_range(struct access_control *access, int family,
                     const unsigned char *start, const unsigned char *end,
                     size_t addr_len)
{
    struct access_rule *rule;

    if (access->rule_count >= MAX_ACCESS_RULES) {
        fprintf(stderr, "limite de regras de acesso excedido (%d)\n",
                MAX_ACCESS_RULES);
        return -1;
    }

    rule = &access->rules[access->rule_count];
    memset(rule, 0, sizeof(*rule));
    rule->family = family;
    rule->addr_len = addr_len;
    if (compare_bytes(start, end, addr_len) <= 0) {
        memcpy(rule->start, start, addr_len);
        memcpy(rule->end, end, addr_len);
    } else {
        memcpy(rule->start, end, addr_len);
        memcpy(rule->end, start, addr_len);
    }
    access->rule_count++;
    return 0;
}

static int add_exact(struct access_control *access, const char *spec)
{
    int family;
    unsigned char addr[16];
    size_t addr_len;

    if (parse_ip(spec, &family, addr, &addr_len) < 0) {
        return -1;
    }
    return add_range(access, family, addr, addr, addr_len);
}

static void make_cidr_range(const unsigned char *addr, unsigned char *start,
                            unsigned char *end, size_t addr_len, long prefix)
{
    size_t i;
    long bits_left = prefix;

    memset(start, 0, addr_len);
    memset(end, 0xff, addr_len);
    for (i = 0; i < addr_len; i++) {
        if (bits_left >= 8) {
            start[i] = addr[i];
            end[i] = addr[i];
            bits_left -= 8;
        } else if (bits_left > 0) {
            unsigned char mask = (unsigned char)(0xffu << (8 - bits_left));
            start[i] = addr[i] & mask;
            end[i] = start[i] | (unsigned char)~mask;
            bits_left = 0;
        } else {
            break;
        }
    }
}

static int parse_cidr(const char *spec, struct access_control *access)
{
    char ip_text[128];
    char *slash;
    char *endptr = NULL;
    long prefix;
    long max_prefix;
    int family;
    unsigned char addr[16];
    unsigned char start[16];
    unsigned char end[16];
    size_t addr_len;

    if (strlen(spec) >= sizeof(ip_text)) {
        return -1;
    }
    strcpy(ip_text, spec);
    slash = strchr(ip_text, '/');
    if (!slash) {
        return -1;
    }
    *slash = '\0';

    prefix = strtol(slash + 1, &endptr, 10);
    if (!slash[1] || !endptr || *endptr != '\0') {
        return -1;
    }
    if (parse_ip(ip_text, &family, addr, &addr_len) < 0) {
        return -1;
    }

    max_prefix = family == AF_INET ? 32 : 128;
    if (prefix < 0 || prefix > max_prefix) {
        return -1;
    }

    make_cidr_range(addr, start, end, addr_len, prefix);
    return add_range(access, family, start, end, addr_len);
}

static int parse_dash_range(const char *spec, struct access_control *access)
{
    char text[256];
    char *dash;
    int start_family;
    int end_family;
    unsigned char start[16];
    unsigned char end[16];
    size_t start_len;
    size_t end_len;

    if (strlen(spec) >= sizeof(text)) {
        return -1;
    }
    strcpy(text, spec);
    dash = strchr(text, '-');
    if (!dash) {
        return -1;
    }
    *dash = '\0';
    if (parse_ip(text, &start_family, start, &start_len) < 0 ||
        parse_ip(dash + 1, &end_family, end, &end_len) < 0 ||
        start_family != end_family || start_len != end_len) {
        return -1;
    }
    return add_range(access, start_family, start, end, start_len);
}

static int sockaddr_ip(const struct sockaddr *addr, int *family,
                       unsigned char *ip, size_t *addr_len)
{
    if (addr->sa_family == AF_INET) {
        const struct sockaddr_in *addr4 = (const struct sockaddr_in *)addr;
        *family = AF_INET;
        memcpy(ip, &addr4->sin_addr, sizeof(addr4->sin_addr));
        *addr_len = sizeof(addr4->sin_addr);
        return 0;
    }

    if (addr->sa_family == AF_INET6) {
        const struct sockaddr_in6 *addr6 = (const struct sockaddr_in6 *)addr;
        const unsigned char *bytes = addr6->sin6_addr.s6_addr;
        static const unsigned char mapped_prefix[12] = {
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff
        };

        if (memcmp(bytes, mapped_prefix, sizeof(mapped_prefix)) == 0) {
            *family = AF_INET;
            memcpy(ip, bytes + 12, 4);
            *addr_len = 4;
        } else {
            *family = AF_INET6;
            memcpy(ip, bytes, 16);
            *addr_len = 16;
        }
        return 0;
    }

    return -1;
}

void access_control_init(struct access_control *access)
{
    memset(access, 0, sizeof(*access));
    access->allow_all = 1;
}

int access_control_add(struct access_control *access, const char *spec)
{
    if (strcmp(spec, "all") == 0 || strcmp(spec, "*") == 0) {
        access->allow_all = 1;
        access->rule_count = 0;
        return 0;
    }

    if (access->allow_all) {
        access->allow_all = 0;
    }

    if (strchr(spec, '/')) {
        if (parse_cidr(spec, access) < 0) {
            fprintf(stderr, "regra CIDR invalida: %s\n", spec);
            return -1;
        }
        return 0;
    }

    if (strchr(spec, '-')) {
        if (parse_dash_range(spec, access) < 0) {
            fprintf(stderr, "range de IP invalido: %s\n", spec);
            return -1;
        }
        return 0;
    }

    if (add_exact(access, spec) < 0) {
        fprintf(stderr, "IP de acesso invalido: %s\n", spec);
        return -1;
    }
    return 0;
}

int access_control_allowed_sockaddr(const struct access_control *access,
                                    const struct sockaddr *addr)
{
    unsigned i;
    int family;
    unsigned char ip[16];
    size_t addr_len;

    if (access->allow_all) {
        return 1;
    }
    if (sockaddr_ip(addr, &family, ip, &addr_len) < 0) {
        return 0;
    }

    for (i = 0; i < access->rule_count; i++) {
        const struct access_rule *rule = &access->rules[i];
        if (rule->family == family && rule->addr_len == addr_len &&
            compare_bytes(ip, rule->start, addr_len) >= 0 &&
            compare_bytes(ip, rule->end, addr_len) <= 0) {
            return 1;
        }
    }
    return 0;
}
