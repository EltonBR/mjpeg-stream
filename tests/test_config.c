#include "../src/tx/config.h"
#include "../src/rx/config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

static int make_ini(char *path, size_t path_size, const char *contents)
{
    int fd;
    FILE *file;

    if (path_size < sizeof("/tmp/mjpeg-config-test-XXXXXX")) {
        return -1;
    }
    memcpy(path, "/tmp/mjpeg-config-test-XXXXXX",
           sizeof("/tmp/mjpeg-config-test-XXXXXX"));
    fd = mkstemp(path);
    if (fd < 0) {
        return -1;
    }
    file = fdopen(fd, "w");
    if (!file) {
        close(fd);
        unlink(path);
        return -1;
    }
    if (fputs(contents, file) == EOF) {
        fclose(file);
        unlink(path);
        return -1;
    }
    if (fclose(file) != 0) {
        unlink(path);
        return -1;
    }
    return 0;
}

static void test_tx_ini_and_cli_precedence(void)
{
    struct config cfg;
    char path[sizeof("/tmp/mjpeg-config-test-XXXXXX")];
    char *args[] = {
        "tx", "--config", path,
        "--device", "/cli/device", "--host", "cli-host", "--port", "9000"
    };

    CHECK("cria INI TX", make_ini(path, sizeof(path),
          "device=/ini/device\n"
          "host=first-host\n"
          "host=last-ini-host\n"
          "port=5001\n"
          "port=5002\n") == 0);
    CHECK("faz parse TX com substituições CLI",
          tx_parse_args((int)(sizeof(args) / sizeof(args[0])), args, &cfg) == 0);
    CHECK("CLI substitui device do INI", strcmp(cfg.device, "/cli/device") == 0);
    CHECK("CLI substitui host do INI", strcmp(cfg.host, "cli-host") == 0);
    CHECK("CLI substitui port do INI", strcmp(cfg.port, "9000") == 0);
    tx_config_destroy(&cfg);
    CHECK("destroy TX limpa campos", cfg.device == NULL && cfg.host == NULL &&
          cfg.port == NULL);
    tx_config_destroy(&cfg);
    unlink(path);
}

static void test_tx_ini_last_value_and_failures(void)
{
    struct config cfg;
    char path[sizeof("/tmp/mjpeg-config-test-XXXXXX")];
    char missing_path[] = "/tmp/mjpeg-config-missing-XXXXXX";
    char *args[] = {"tx", "--config", path};
    char *bad_ini_args[] = {"tx", "--config", path};
    char *bad_option_args[] = {"tx", "--config", path, "--unknown"};
    int missing_fd;

    CHECK("cria INI TX para repetição", make_ini(path, sizeof(path),
          "host=first-host\nhost=last-host\nport=5100\nport=5200\n") == 0);
    CHECK("faz parse TX somente com INI",
          tx_parse_args(3, args, &cfg) == 0);
    CHECK("último host INI prevalece", strcmp(cfg.host, "last-host") == 0);
    CHECK("última porta INI prevalece", strcmp(cfg.port, "5200") == 0);
    tx_config_destroy(&cfg);
    unlink(path);

    CHECK("cria INI TX inválido", make_ini(path, sizeof(path),
          "protocol=invalid\n") == 0);
    CHECK("protocolo INI inválido falha",
          tx_parse_args(3, bad_ini_args, &cfg) < 0);
    tx_config_destroy(&cfg);
    CHECK("destroy TX após falha INI é seguro",
          cfg.device == NULL && cfg.host == NULL && cfg.port == NULL);
    unlink(path);

    CHECK("cria INI TX para falha CLI", make_ini(path, sizeof(path),
          "host=ini-host\n") == 0);
    CHECK("opção CLI inválida falha",
          tx_parse_args(4, bad_option_args, &cfg) < 0);
    tx_config_destroy(&cfg);
    unlink(path);

    missing_fd = mkstemp(missing_path);
    CHECK("prepara caminho ausente", missing_fd >= 0);
    if (missing_fd >= 0) {
        close(missing_fd);
        unlink(missing_path);
        args[2] = missing_path;
        CHECK("arquivo INI ausente falha", tx_parse_args(3, args, &cfg) < 0);
        tx_config_destroy(&cfg);
    }
}

static void test_rx_ini_and_cli_precedence(void)
{
    struct rx_config cfg;
    char path[sizeof("/tmp/mjpeg-config-test-XXXXXX")];
    char *args[] = {
        "rx", "--config", path,
        "--host", "cli-host", "--hud-color", "amber"
    };

    CHECK("cria INI RX", make_ini(path, sizeof(path),
          "host=first-host\n"
          "host=last-ini-host\n"
          "hud_color=first-color\n"
          "hud_color=last-ini-color\n"
          "hud_font=INI Font\n") == 0);
    CHECK("faz parse RX com substituições CLI",
          rx_parse_args((int)(sizeof(args) / sizeof(args[0])), args, &cfg) == 0);
    CHECK("CLI substitui host do INI", strcmp(cfg.host, "cli-host") == 0);
    CHECK("CLI substitui cor do INI", strcmp(cfg.hud_color, "amber") == 0);
    CHECK("valor INI sem substituição permanece", strcmp(cfg.hud_font, "INI Font") == 0);
    rx_config_destroy(&cfg);
    CHECK("destroy RX limpa campos", cfg.host == NULL && cfg.hud_color == NULL &&
          cfg.hud_font == NULL && cfg.telemetry_port == NULL);
    rx_config_destroy(&cfg);
    unlink(path);
}

static void test_rx_ini_last_value_and_failures(void)
{
    struct rx_config cfg;
    char path[sizeof("/tmp/mjpeg-config-test-XXXXXX")];
    char *args[] = {"rx", "--config", path};
    char *bad_ini_args[] = {"rx", "--config", path};
    char *bad_option_args[] = {"rx", "--config", path, "--unknown"};
    char *missing_event_args[] = {"rx", "--config", path, "--event-host", "127.0.0.1"};

    CHECK("cria INI RX para repetição", make_ini(path, sizeof(path),
          "host=first-host\nhost=last-host\nport=5100\nport=5200\n") == 0);
    CHECK("faz parse RX somente com INI",
          rx_parse_args(3, args, &cfg) == 0);
    CHECK("último host INI prevalece", strcmp(cfg.host, "last-host") == 0);
    CHECK("última porta INI prevalece", strcmp(cfg.port, "5200") == 0);
    rx_config_destroy(&cfg);
    unlink(path);

    CHECK("cria INI RX inválido", make_ini(path, sizeof(path),
          "protocol=invalid\n") == 0);
    CHECK("protocolo INI inválido falha",
          rx_parse_args(3, bad_ini_args, &cfg) < 0);
    rx_config_destroy(&cfg);
    CHECK("destroy RX após falha INI é seguro", cfg.host == NULL && cfg.port == NULL);
    unlink(path);

    CHECK("cria INI RX para falhas CLI", make_ini(path, sizeof(path),
          "host=ini-host\n") == 0);
    CHECK("opção CLI inválida falha",
          rx_parse_args(4, bad_option_args, &cfg) < 0);
    rx_config_destroy(&cfg);
    CHECK("eventos sem porta falham",
          rx_parse_args(5, missing_event_args, &cfg) < 0);
    rx_config_destroy(&cfg);
    CHECK("destroy RX após falha CLI é seguro", cfg.host == NULL && cfg.port == NULL);
    unlink(path);
}

int main(void)
{
    test_tx_ini_and_cli_precedence();
    test_tx_ini_last_value_and_failures();
    test_rx_ini_and_cli_precedence();
    test_rx_ini_last_value_and_failures();

    if (failures != 0) {
        fprintf(stderr, "%u de %u verificações de config falharam\n",
                failures, checks);
        return 1;
    }
    printf("%u verificações de config TX/RX passaram\n", checks);
    return 0;
}
