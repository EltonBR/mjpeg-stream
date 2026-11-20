#include "access_control.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

static unsigned checks;
static unsigned failures;

#define CHECK(description, expression) \
    do { \
        checks++; \
        if (!(expression)) { \
            fprintf(stderr, "FAIL: %s (linha %d)\n", description, __LINE__); \
            failures++; \
        } \
    } while (0)

static int allowed_ip(const struct access_control *access, int family,
                      const char *text)
{
    if (family == AF_INET) {
        struct sockaddr_in addr;

        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        if (inet_pton(AF_INET, text, &addr.sin_addr) != 1) {
            return -1;
        }
        return access_control_allowed_sockaddr(
            access, (const struct sockaddr *)&addr);
    }

    if (family == AF_INET6) {
        struct sockaddr_in6 addr;

        memset(&addr, 0, sizeof(addr));
        addr.sin6_family = AF_INET6;
        if (inet_pton(AF_INET6, text, &addr.sin6_addr) != 1) {
            return -1;
        }
        return access_control_allowed_sockaddr(
            access, (const struct sockaddr *)&addr);
    }

    return -1;
}

static void test_default_and_reset(void)
{
    struct access_control access;

    access_control_init(&access);
    CHECK("ACL sem regras permite IPv4", allowed_ip(&access, AF_INET, "192.0.2.1") == 1);
    CHECK("ACL sem regras permite IPv6", allowed_ip(&access, AF_INET6, "2001:db8::1") == 1);
    CHECK("regra all valida", access_control_add(&access, "all") == 0);
    CHECK("regra IPv4 adicionada", access_control_add(&access, "192.0.2.1") == 0);
    CHECK("regra all remove restrições anteriores",
          access_control_add(&access, "*") == 0 &&
              allowed_ip(&access, AF_INET6, "2001:db8::1") == 1);
}

static void test_ipv4_rules(void)
{
    struct access_control access;

    access_control_init(&access);
    CHECK("adiciona IPv4 exato", access_control_add(&access, "192.0.2.10") == 0);
    CHECK("IPv4 exato permitido", allowed_ip(&access, AF_INET, "192.0.2.10") == 1);
    CHECK("outro IPv4 negado", allowed_ip(&access, AF_INET, "192.0.2.11") == 0);
    CHECK("IPv4 mapeado em sockaddr IPv6 corresponde à regra IPv4",
          allowed_ip(&access, AF_INET6, "::ffff:192.0.2.10") == 1);

    access_control_init(&access);
    CHECK("adiciona CIDR IPv4", access_control_add(&access, "192.0.2.64/26") == 0);
    CHECK("primeiro endereço do CIDR permitido",
          allowed_ip(&access, AF_INET, "192.0.2.64") == 1);
    CHECK("último endereço do CIDR permitido",
          allowed_ip(&access, AF_INET, "192.0.2.127") == 1);
    CHECK("endereço anterior ao CIDR negado",
          allowed_ip(&access, AF_INET, "192.0.2.63") == 0);
    CHECK("endereço posterior ao CIDR negado",
          allowed_ip(&access, AF_INET, "192.0.2.128") == 0);

    access_control_init(&access);
    CHECK("adiciona faixa IPv4 invertida",
          access_control_add(&access, "198.51.100.20-198.51.100.10") == 0);
    CHECK("limite inferior da faixa permitido",
          allowed_ip(&access, AF_INET, "198.51.100.10") == 1);
    CHECK("limite superior da faixa permitido",
          allowed_ip(&access, AF_INET, "198.51.100.20") == 1);
    CHECK("fora da faixa IPv4 negado",
          allowed_ip(&access, AF_INET, "198.51.100.21") == 0);
}

static void test_ipv6_rules(void)
{
    struct access_control access;

    access_control_init(&access);
    CHECK("adiciona IPv6 exato", access_control_add(&access, "2001:db8::5") == 0);
    CHECK("IPv6 exato permitido", allowed_ip(&access, AF_INET6, "2001:db8::5") == 1);
    CHECK("outro IPv6 negado", allowed_ip(&access, AF_INET6, "2001:db8::6") == 0);
    CHECK("IPv4 não corresponde à regra IPv6",
          allowed_ip(&access, AF_INET, "192.0.2.5") == 0);

    access_control_init(&access);
    CHECK("adiciona CIDR IPv6", access_control_add(&access, "2001:db8:1234::/48") == 0);
    CHECK("início do CIDR IPv6 permitido",
          allowed_ip(&access, AF_INET6, "2001:db8:1234::") == 1);
    CHECK("fim do CIDR IPv6 permitido",
          allowed_ip(&access, AF_INET6, "2001:db8:1234:ffff:ffff:ffff:ffff:ffff") == 1);
    CHECK("endereço fora do CIDR IPv6 negado",
          allowed_ip(&access, AF_INET6, "2001:db8:1235::") == 0);

    access_control_init(&access);
    CHECK("adiciona faixa IPv6 invertida",
          access_control_add(&access, "2001:db8::20-2001:db8::10") == 0);
    CHECK("limite inferior da faixa IPv6 permitido",
          allowed_ip(&access, AF_INET6, "2001:db8::10") == 1);
    CHECK("limite superior da faixa IPv6 permitido",
          allowed_ip(&access, AF_INET6, "2001:db8::20") == 1);
    CHECK("fora da faixa IPv6 negado",
          allowed_ip(&access, AF_INET6, "2001:db8::21") == 0);
}

static void test_mapped_and_invalid_addresses(void)
{
    struct access_control access;
    struct sockaddr unsupported;

    access_control_init(&access);
    CHECK("adiciona faixa IPv4 para endereço mapeado",
          access_control_add(&access, "192.0.2.0/24") == 0);
    CHECK("IPv4 mapeado em sockaddr IPv6 segue regra IPv4",
          allowed_ip(&access, AF_INET6, "::ffff:192.0.2.9") == 1);
    CHECK("IPv4 mapeado fora da faixa é negado",
          allowed_ip(&access, AF_INET6, "::ffff:198.51.100.9") == 0);

    access_control_init(&access);
    CHECK("rejeita CIDR IPv4 com prefixo inválido",
          access_control_add(&access, "192.0.2.0/33") < 0);
    CHECK("rejeita IPv6 inválido", access_control_add(&access, "2001:db8::gg") < 0);

    memset(&unsupported, 0, sizeof(unsupported));
    unsupported.sa_family = AF_UNIX;
    CHECK("família de socket não IP é negada",
          access_control_allowed_sockaddr(&access, &unsupported) == 0);
}

int main(void)
{
    test_default_and_reset();
    test_ipv4_rules();
    test_ipv6_rules();
    test_mapped_and_invalid_addresses();

    if (failures != 0) {
        fprintf(stderr, "%u de %u verificações falharam\n", failures, checks);
        return 1;
    }
    printf("%u verificações ACL IPv4/IPv6 passaram\n", checks);
    return 0;
}
