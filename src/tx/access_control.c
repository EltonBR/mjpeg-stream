#include "access_control.h"

#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int parse_ipv4(const char *text, uint32_t *out)
{
    struct in_addr addr;
    if (inet_pton(AF_INET, text, &addr) != 1) {
        return -1;
    }
    *out = ntohl(addr.s_addr);
    return 0;
}

static int add_range(struct access_control *access, uint32_t start, uint32_t end)
{
    if (access->rule_count >= MAX_ACCESS_RULES) {
        fprintf(stderr, "limite de regras de acesso excedido (%d)\n",
                MAX_ACCESS_RULES);
        return -1;
    }
    if (start > end) {
        uint32_t tmp = start;
        start = end;
        end = tmp;
    }
    access->rules[access->rule_count].start = start;
    access->rules[access->rule_count].end = end;
    access->rule_count++;
    return 0;
}

static int parse_cidr(const char *spec, struct access_control *access)
{
    char ip_text[64];
    char *slash;
    char *end = NULL;
    long prefix;
    uint32_t ip;
    uint32_t mask;

    if (strlen(spec) >= sizeof(ip_text)) {
        return -1;
    }
    strcpy(ip_text, spec);
    slash = strchr(ip_text, '/');
    if (!slash) {
        return -1;
    }
    *slash = '\0';
    prefix = strtol(slash + 1, &end, 10);
    if (!slash[1] || !end || *end != '\0' || prefix < 0 || prefix > 32) {
        return -1;
    }
    if (parse_ipv4(ip_text, &ip) < 0) {
        return -1;
    }

    mask = prefix == 0 ? 0u : (0xffffffffu << (32 - (unsigned)prefix));
    return add_range(access, ip & mask, (ip & mask) | ~mask);
}

static int parse_dash_range(const char *spec, struct access_control *access)
{
    char text[96];
    char *dash;
    uint32_t start;
    uint32_t end;

    if (strlen(spec) >= sizeof(text)) {
        return -1;
    }
    strcpy(text, spec);
    dash = strchr(text, '-');
    if (!dash) {
        return -1;
    }
    *dash = '\0';
    if (parse_ipv4(text, &start) < 0 || parse_ipv4(dash + 1, &end) < 0) {
        return -1;
    }
    return add_range(access, start, end);
}

void access_control_init(struct access_control *access)
{
    memset(access, 0, sizeof(*access));
    access->allow_all = 1;
}

int access_control_add(struct access_control *access, const char *spec)
{
    uint32_t ip;

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

    if (parse_ipv4(spec, &ip) < 0) {
        fprintf(stderr, "IP de acesso invalido: %s\n", spec);
        return -1;
    }
    return add_range(access, ip, ip);
}

int access_control_allowed(const struct access_control *access, uint32_t ip)
{
    unsigned i;

    if (access->allow_all) {
        return 1;
    }

    for (i = 0; i < access->rule_count; i++) {
        if (ip >= access->rules[i].start && ip <= access->rules[i].end) {
            return 1;
        }
    }
    return 0;
}
