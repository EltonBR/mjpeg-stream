#include "event_sender.h"

#include "net.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int try_connect_locked(struct event_sender *sender)
{
    int fd;

    /* SEGURANÇA: Esta função DEVE ser chamada dentro de g_mutex_lock()
     * (daí o sufixo "_locked")
     * 
     * Validações:
     * - sender->enabled: Se desabilitado, não tenta conectar
     * - sender->fd >= 0: Se já conectado, não tenta novamente
     * - sender->stopping: Se encerrando, não tenta conectar */
    if (!sender->enabled || sender->fd >= 0 || sender->stopping) {
        return 0;
    }

    fd = tcp_connect(sender->host, sender->port);
    if (fd < 0) {
        return -1;
    }

    sender->fd = fd;  /* Atribuição thread-safe (dentro do lock) */
    fprintf(stderr, "canal de eventos conectado em %s:%s\n",
            sender->host, sender->port);
    return 0;
}

static gpointer reconnect_thread(gpointer data)
{
    struct event_sender *sender = data;

    /* PONTO CRÍTICO: Sincronização de Reconexão
     * Esta thread funciona de forma separada de event_sender_send():
     * - Tenta conectar a cada 1 segundo
     * - Usa mesmo mutex para proteger acesso ao fd
     * - Encerra quando sender->stopping=1 (setado em event_sender_close())
     *
     * Sincronização: Cada iteração locks/unlocks, sem race condition:
     * ┌─ event_sender_send()        ┌─ reconnect_thread()
     * │  g_mutex_lock()             │  g_mutex_lock()        ← Bloqueia até liberar
     * │  write_all(fd)              │  g_usleep(1s)
     * │  g_mutex_unlock()           │  try_connect()
     * │                             │  g_mutex_unlock()
     * └─────────────────────────────┴─ while(!stopping) */

    if (!sender) {
        return NULL;  /* Validação: evita NULL pointer */
    }

    while (!sender->stopping) {
        g_mutex_lock(&sender->lock);
        (void)try_connect_locked(sender);
        g_mutex_unlock(&sender->lock);
        g_usleep(1000000);  /* Dorme 1s antes da próxima tentativa */
    }
    return NULL;
}

void event_sender_init(struct event_sender *sender)
{
    sender->fd = -1;
    sender->enabled = 0;
    sender->stopping = 0;
    sender->host = NULL;
    sender->port = NULL;
    sender->thread = NULL;
    g_mutex_init(&sender->lock);
}

void event_sender_start(struct event_sender *sender, const char *host,
                        const char *port)
{
    sender->host = host;
    sender->port = port;
    sender->enabled = 1;
    sender->thread = g_thread_new("event-reconnect", reconnect_thread, sender);
}

void event_sender_close(struct event_sender *sender)
{
    sender->stopping = 1;
    if (sender->thread) {
        g_thread_join(sender->thread);
        sender->thread = NULL;
    }

    g_mutex_lock(&sender->lock);
    sender->enabled = 0;
    if (sender->fd >= 0) {
        close(sender->fd);
        sender->fd = -1;
    }
    g_mutex_unlock(&sender->lock);
    g_mutex_clear(&sender->lock);
}

void event_sender_send(struct event_sender *sender, const char *json)
{
    size_t len;

    /* PONTO CRÍTICO: Sincronização Multi-Thread
     * Válido chamar de múltiplas threads:
     * - Thread principal GTK (teclado, mouse)
     * - Thread de joystick
     * O mutex garante que writes são atômicas e não intercalam */

    if (!sender || !json || !sender->enabled) {
        return;  /* Validação: evita NULL pointer dereference */
    }

    len = strlen(json);
    
    /* LOCK: Protege fd, enabled, e as operações de I/O */
    g_mutex_lock(&sender->lock);
    
    /* Tenta conectar se necessário (dentro do lock) */
    if (sender->fd < 0) {
        (void)try_connect_locked(sender);
    }
    
    /* Verifica novamente state antes de escrever:
     * sender->enabled pode ter mudado durante try_connect_locked() */
    if (sender->enabled && sender->fd >= 0) {
        /* Escreve JSON + newline como unidade atômica (não intercalável)
         * Se qualquer write falha, fecha fd e marca para reconexão */
        if (write_all(sender->fd, json, len) < 0 ||
            write_all(sender->fd, "\n", 1) < 0) {
            close(sender->fd);
            sender->fd = -1;
            fprintf(stderr, "canal de eventos desconectado, tentando reconectar\n");
        }
    }
    
    /* UNLOCK: Libera mutex para outras threads */
    g_mutex_unlock(&sender->lock);
}
