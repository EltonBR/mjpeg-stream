#include "net.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

static int fill_ip_addr(const char *host, const char *port,
                        struct sockaddr_storage *addr, socklen_t *addr_len)
{
    int parsed_port = parse_port(port);
    if (parsed_port < 0) {
        return -1;
    }

    memset(addr, 0, sizeof(*addr));
    if (strcmp(host, "*") == 0) {
        struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)addr;
        addr6->sin6_family = AF_INET6;
        addr6->sin6_port = htons((uint16_t)parsed_port);
        addr6->sin6_addr = in6addr_any;
        *addr_len = sizeof(*addr6);
        return 0;
    }

    {
        struct sockaddr_in *addr4 = (struct sockaddr_in *)addr;
        if (inet_pton(AF_INET, host, &addr4->sin_addr) == 1) {
            addr4->sin_family = AF_INET;
            addr4->sin_port = htons((uint16_t)parsed_port);
            *addr_len = sizeof(*addr4);
            return 0;
        }
    }

    {
        struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)addr;
        if (inet_pton(AF_INET6, host, &addr6->sin6_addr) == 1) {
            addr6->sin6_family = AF_INET6;
            addr6->sin6_port = htons((uint16_t)parsed_port);
            *addr_len = sizeof(*addr6);
            return 0;
        }
    }

    fprintf(stderr, "IP invalido: %s\n", host);
    return -1;
}

static void allow_dual_stack(int fd, int family)
{
    int no = 0;
    if (family == AF_INET6) {
        (void)setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &no, sizeof(no));
    }
}

static int socket_for(const struct sockaddr_storage *addr, int type)
{
    int fd = socket(addr->ss_family, type, 0);
    if (fd < 0) {
        perror(type == SOCK_STREAM ? "socket tcp" : "socket udp");
    }
    return fd;
}

static int bind_socket(const char *host, const char *port, int type,
                       const char *label)
{
    struct sockaddr_storage addr;
    socklen_t addr_len;
    int fd;

    if (fill_ip_addr(host, port, &addr, &addr_len) < 0) {
        return -1;
    }

    fd = socket_for(&addr, type);
    if (fd < 0) {
        return -1;
    }
    (void)set_reuseaddr(fd);
    allow_dual_stack(fd, addr.ss_family);
    if (bind(fd, (struct sockaddr *)&addr, addr_len) < 0) {
        perror(label);
        close(fd);
        return -1;
    }
    return fd;
}

static int connect_socket(const char *host, const char *port, int type,
                          const char *label)
{
    struct sockaddr_storage addr;
    socklen_t addr_len;
    int fd;

    if (fill_ip_addr(host, port, &addr, &addr_len) < 0) {
        return -1;
    }

    fd = socket_for(&addr, type);
    if (fd < 0) {
        return -1;
    }
    if (connect(fd, (struct sockaddr *)&addr, addr_len) < 0) {
        perror(label);
        close(fd);
        return -1;
    }
    return fd;
}

int set_reuseaddr(int fd)
{
    int yes = 1;
    return setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
}

int tcp_connect(const char *host, const char *port)
{
    return connect_socket(host, port, SOCK_STREAM, "connect tcp");
}

int tcp_listen(const char *host, const char *port)
{
    int fd = bind_socket(host, port, SOCK_STREAM, "bind tcp");
    if (fd < 0) {
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
    return connect_socket(host, port, SOCK_DGRAM, "connect udp");
}

int udp_bind_socket(const char *host, const char *port)
{
    return bind_socket(host, port, SOCK_DGRAM, "bind udp");
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
