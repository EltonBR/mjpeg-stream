#include "net.h"

#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

static int parse_port(const char *port)
{
    char *end = NULL;
    long value = strtol(port, &end, 10);
    if (!port[0] || !end || *end != '\0' || value < 1 || value > 65535) {
        fprintf(stderr, "porta invalida: %s\n", port);
        return -1;
    }
    return (int)value;
}

static int fill_ipv4_addr(const char *host, const char *port,
                          struct sockaddr_in *addr)
{
    int parsed_port = parse_port(port);
    if (parsed_port < 0) {
        return -1;
    }

    memset(addr, 0, sizeof(*addr));
    addr->sin_family = AF_INET;
    addr->sin_port = htons((uint16_t)parsed_port);

    if (strcmp(host, "0.0.0.0") == 0 || strcmp(host, "*") == 0) {
        addr->sin_addr.s_addr = htonl(INADDR_ANY);
        return 0;
    }

    if (inet_pton(AF_INET, host, &addr->sin_addr) != 1) {
        fprintf(stderr, "IP IPv4 invalido: %s\n", host);
        return -1;
    }
    return 0;
}

int set_reuseaddr(int fd)
{
    int yes = 1;
    return setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
}

int tcp_connect(const char *host, const char *port)
{
    struct sockaddr_in addr;
    int fd;

    if (fill_ipv4_addr(host, port, &addr) < 0) {
        return -1;
    }

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket tcp");
        return -1;
    }
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect tcp");
        close(fd);
        return -1;
    }
    return fd;
}

int tcp_listen(const char *host, const char *port)
{
    struct sockaddr_in addr;
    int fd;

    if (fill_ipv4_addr(host, port, &addr) < 0) {
        return -1;
    }

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket tcp");
        return -1;
    }
    (void)set_reuseaddr(fd);
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind tcp");
        close(fd);
        return -1;
    }
    if (listen(fd, 1) < 0) {
        perror("listen");
        close(fd);
        return -1;
    }
    return fd;
}

int udp_connect(const char *host, const char *port)
{
    struct sockaddr_in addr;
    int fd;

    if (fill_ipv4_addr(host, port, &addr) < 0) {
        return -1;
    }

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        perror("socket udp");
        return -1;
    }
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect udp");
        close(fd);
        return -1;
    }
    return fd;
}

int udp_bind_socket(const char *host, const char *port)
{
    struct sockaddr_in addr;
    int fd;

    if (fill_ipv4_addr(host, port, &addr) < 0) {
        return -1;
    }

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        perror("socket udp");
        return -1;
    }
    (void)set_reuseaddr(fd);
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind udp");
        close(fd);
        return -1;
    }
    return fd;
}

int write_all(int fd, const void *buf, size_t len)
{
    const unsigned char *p = buf;
    while (len > 0) {
        ssize_t n = write(fd, p, len);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (n == 0) {
            return -1;
        }
        p += (size_t)n;
        len -= (size_t)n;
    }
    return 0;
}

int read_all(int fd, void *buf, size_t len)
{
    unsigned char *p = buf;
    while (len > 0) {
        ssize_t n = read(fd, p, len);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (n == 0) {
            return 0;
        }
        p += (size_t)n;
        len -= (size_t)n;
    }
    return 1;
}

ssize_t recv_intr(int fd, void *buf, size_t len, int flags)
{
    ssize_t n;
    do {
        n = recv(fd, buf, len, flags);
    } while (n < 0 && errno == EINTR);
    return n;
}
