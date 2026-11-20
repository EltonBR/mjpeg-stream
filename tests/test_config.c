#include "../src/tx/config.h"
#include "../src/rx/config.h"
#include "../src/rx/overlay_config.h"

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
        "--host", "cli-host"
    };

    CHECK("cria INI RX", make_ini(path, sizeof(path),
          "[stream]\nhost=first-host\n"
          "host=last-ini-host\n[window]\nzoom_in_key=Zoom\n"
          "[overlay]\nhud_color=legacy-color\n") == 0);
    CHECK("faz parse RX com substituições CLI",
          rx_parse_args((int)(sizeof(args) / sizeof(args[0])), args, &cfg) == 0);
    CHECK("CLI substitui host do INI", strcmp(cfg.host, "cli-host") == 0);
    CHECK("RX lê somente seção de janela", strcmp(cfg.zoom_in_key, "Zoom") == 0);
    rx_config_destroy(&cfg);
    CHECK("destroy RX limpa campos", cfg.host == NULL && cfg.event_host == NULL);
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
          "[stream]\nhost=first-host\nhost=last-host\nport=5100\nport=5200\n") == 0);
    CHECK("faz parse RX somente com INI",
          rx_parse_args(3, args, &cfg) == 0);
    CHECK("último host INI prevalece", strcmp(cfg.host, "last-host") == 0);
    CHECK("última porta INI prevalece", strcmp(cfg.port, "5200") == 0);
    rx_config_destroy(&cfg);
    unlink(path);

    CHECK("cria INI RX inválido", make_ini(path, sizeof(path),
          "[stream]\nprotocol=invalid\n") == 0);
    CHECK("protocolo INI inválido falha",
          rx_parse_args(3, bad_ini_args, &cfg) < 0);
    rx_config_destroy(&cfg);
    CHECK("destroy RX após falha INI é seguro", cfg.host == NULL && cfg.port == NULL);
    unlink(path);

    CHECK("cria INI RX para falhas CLI", make_ini(path, sizeof(path),
          "[stream]\nhost=ini-host\n") == 0);
    CHECK("opção CLI inválida falha",
          rx_parse_args(4, bad_option_args, &cfg) < 0);
    rx_config_destroy(&cfg);
    CHECK("eventos sem porta falham",
          rx_parse_args(5, missing_event_args, &cfg) < 0);
    rx_config_destroy(&cfg);
    CHECK("destroy RX após falha CLI é seguro", cfg.host == NULL && cfg.port == NULL);
    unlink(path);
}

static void test_overlay_config_precedence_and_paths(void)
{
    struct overlay_config cfg;
    char rx_path[sizeof("/tmp/mjpeg-config-test-XXXXXX")];
    char overlay_path[sizeof("/tmp/mjpeg-config-test-XXXXXX")];
    char *args[] = {"rx", "--config", rx_path, "--overlay-config", overlay_path,
                    "--hud-color", "amber", "--telemetry-port", "7123"};
    CHECK("cria RX INI legado", make_ini(rx_path, sizeof(rx_path),
          "[overlay]\noverlay=legacy.json\nhud_color=green\ndim_alpha=0.4\n"
          "[telemetry]\ntelemetry_enabled=false\ntelemetry_port=7001\n") == 0);
    CHECK("cria overlay.ini", make_ini(overlay_path, sizeof(overlay_path),
          "[overlay]\nenabled=true\nlayout=layout.json\nassets_dir=assets\n"
          "[appearance]\nhud_color=#123456\ndim_alpha=0.25\n"
          "[telemetry]\nenabled=true\nhost=127.0.0.2\nport=7002\nreconnect_ms=2345\n") == 0);
    CHECK("parse overlay com precedência", overlay_config_parse_args(9, args, &cfg) == 0);
    CHECK("CLI supera overlay.ini", strcmp(cfg.hud_color, "amber") == 0);
    CHECK("overlay.ini supera rx legado", cfg.dim_alpha == 0.25);
    CHECK("CLI telemetry port prevalece", strcmp(cfg.telemetry_port, "7123") == 0);
    CHECK("overlay.ini prevalece na telemetria", cfg.telemetry_enabled &&
          strcmp(cfg.telemetry_host, "127.0.0.2") == 0);
    CHECK("reconnect_ms é lido", cfg.reconnect_ms == 2345);
    CHECK("layout resolvido ao diretório do overlay.ini",
          strstr(cfg.layout_path, "/layout.json") != NULL);
    CHECK("assets relativos ao overlay.ini", cfg.assets_dir_explicit &&
          strstr(cfg.assets_dir, "/assets") != NULL);
    overlay_config_destroy(&cfg);
    {
        char *legacy_args[] = {"rx", "--config", rx_path,
                               "--overlay-config", overlay_path};
        CHECK("parse legado sem flags de aparência",
              overlay_config_parse_args(5, legacy_args, &cfg) == 0);
        CHECK("overlay.ini supera cor legada", strcmp(cfg.hud_color, "#123456") == 0);
        CHECK("overlay.ini supera alpha legado", cfg.dim_alpha == 0.25);
        overlay_config_destroy(&cfg);
    }
    unlink(rx_path); unlink(overlay_path);
}

static void test_overlay_config_validation_and_missing_file(void)
{
    struct overlay_config cfg;
    char path[sizeof("/tmp/mjpeg-config-test-XXXXXX")];
    char absent[] = "/tmp/mjpeg-overlay-config-missing.ini";
    char *args[] = {"rx", "--overlay-config", path};
    CHECK("cria overlay.ini com alpha inválido", make_ini(path, sizeof(path),
          "[appearance]\ndim_alpha=1.5\n") == 0);
    CHECK("alpha fora de faixa falha", overlay_config_parse_args(3, args, &cfg) < 0);
    overlay_config_destroy(&cfg);
    unlink(path);
    args[2] = absent;
    CHECK("overlay config explícito ausente falha",
          overlay_config_parse_args(3, args, &cfg) < 0);
    overlay_config_destroy(&cfg);
    CHECK("cria overlay.ini com booleano inválido", make_ini(path, sizeof(path),
          "[overlay]\nenabled=perhaps\n") == 0);
    args[2] = path;
    CHECK("booleano inválido falha", overlay_config_parse_args(3, args, &cfg) < 0);
    overlay_config_destroy(&cfg);
    unlink(path);
    CHECK("cria overlay.ini desabilitado", make_ini(path, sizeof(path),
          "[overlay]\nenabled=false\n[telemetry]\nreconnect_ms=0\n") == 0);
    CHECK("reconnect_ms não positivo falha", overlay_config_parse_args(3, args, &cfg) < 0);
    overlay_config_destroy(&cfg);
    unlink(path);
    CHECK("cria overlay.ini desabilitado sem erros", make_ini(path, sizeof(path),
          "[overlay]\nenabled=false\n") == 0);
    CHECK("enabled=false é aceito", overlay_config_parse_args(3, args, &cfg) == 0);
    CHECK("overlay disabled preservado", cfg.enabled == 0);
    overlay_config_destroy(&cfg);
    unlink(path);
}

int main(void)
{
    test_tx_ini_and_cli_precedence();
    test_tx_ini_last_value_and_failures();
    test_rx_ini_and_cli_precedence();
    test_rx_ini_last_value_and_failures();
    test_overlay_config_precedence_and_paths();
    test_overlay_config_validation_and_missing_file();

    if (failures != 0) {
        fprintf(stderr, "%u de %u verificações de config falharam\n",
                failures, checks);
        return 1;
    }
    printf("%u verificações de config TX/RX passaram\n", checks);
    return 0;
}
