# Documentação de Input Events (Eventos de Entrada)

## Visão Geral

O sistema de input events do MJPEG Receiver (RX) captura eventos de teclado, mouse e joystick da interface GTK e os transmite via TCP como JSON Lines para um servidor remoto.

- **Transporte**: TCP
- **Formato**: JSON Lines (um JSON por linha)
- **Componentes**: Teclado, Mouse, Joystick

---

## Arquitetura

### Fluxo de Processamento

```
[Evento GTK/Linux] → [Handler Callback] → [Serializar JSON] → [EventSender] → [TCP]
                        ↓
                    [event_sender_send]
                        ↓
                  [try_connect_locked]
                        ↓
                    [write_all]
                        ↓
                   [Servidor TCP]
```

### Componentes Principais

| Componente | Arquivo | Responsabilidade |
|-----------|---------|------------------|
| **EventSender** | `src/rx/event_sender.c/h` | Conexão TCP, reconexão automática, envio de JSON |
| **InputHandler** | `src/rx/input.c/h` | Captura e serialização de eventos |
| **App State** | `src/rx/app.c/h` | Gerencia threads de joystick e estado |

### Componentes de Suporte

- `src/rx/main.c`: Ativa eventos conforme configuração
- `rx.ini`: Configuração de host, porta, joystick
- `src/common/net.c/h`: Funções de conexão TCP

---

## Eventos de Teclado

### Tipos de Eventos

| Tipo | Disparo | Campo Obrigatório | Notas |
|------|---------|-------------------|-------|
| `keydown` | Ao pressionar | `keyval`, `key` | Enviado imediatamente |
| `keyup` | Ao soltar | `keyval`, `key` | Enviado imediatamente |
| `keypress` | Ao soltar (após `keyup`) | `keyval`, `key` | Representa o ciclo completo pressão+liberação |

### Campos JSON

```json
{
  "origin": "keyboard",
  "type": "keydown|keyup|keypress",
  "keyval": 65361,
  "key": "Left",
  "state": 0
}
```

| Campo | Tipo | Descrição |
|-------|------|-----------|
| `origin` | string | Sempre `"keyboard"` |
| `type` | string | Tipo do evento: `keydown`, `keyup`, `keypress` |
| `keyval` | int | Código numérico da tecla (GDK keyval) |
| `key` | string | Nome da tecla em texto (ex: "Left", "space", "a") |
| `state` | int | Máscara de modificadores (Shift, Ctrl, Alt, etc.) |

### Exemplos

```json
{"origin":"keyboard","type":"keydown","keyval":65361,"key":"Left","state":0}
{"origin":"keyboard","type":"keyup","keyval":65361,"key":"Left","state":0}
{"origin":"keyboard","type":"keypress","keyval":65361,"key":"Left","state":0}
{"origin":"keyboard","type":"keydown","keyval":65507,"key":"Control_L","state":0}
{"origin":"keyboard","type":"keydown","keyval":97,"key":"a","state":4}
```

### Comportamento Especial

- **Atalhos Locais**: Teclas configuradas para zoom (`zoom_in_key`, `zoom_out_key`) e dim alpha (`dim_alpha_up_key`, `dim_alpha_down_key`) são **consumidas localmente** e NÃO são enviadas ao servidor.
- **Escape local**: A função `is_local_keybinding()` verifica se a tecla é um atalho local antes de enviar.

### Configuração

Os atalhos locais são configurados em `src/rx/app.h` como strings parseáveis (ex: "z,plus", "x,minus").

---

## Eventos de Mouse

### Tipos de Eventos

| Tipo | Disparo | Campo Obrigatório | Notas |
|------|---------|-------------------|-------|
| `motion` | Movimento | `x`, `y`, `pixel_x`, `pixel_y` | Contínuo enquanto mouse move |
| `mousedown` | Botão pressionado | `button`, `x`, `y` | Enviado imediatamente |
| `mouseup` | Botão solto | `button`, `x`, `y` | Enviado imediatamente |
| `mousepress` | Botão solto (após `mouseup`) | `button`, `x`, `y` | Representa o ciclo completo |
| `scroll` | Roda rolada | `direction`, `delta_x`, `delta_y` | Suporta scroll suave |

### Campos JSON

```json
{
  "origin": "mouse",
  "type": "motion|mousedown|mouseup|mousepress|scroll",
  "x": 0.500000,
  "y": 0.250000,
  "pixel_x": 640.00,
  "pixel_y": 180.00,
  "image_w": 1280,
  "image_h": 720,
  "button": 1,
  "state": 0
}
```

| Campo | Tipo | Descrição |
|-------|------|-----------|
| `origin` | string | Sempre `"mouse"` |
| `type` | string | Tipo do evento |
| `x` | float | Coordenada X relativa normalizada (0.0 a 1.0) |
| `y` | float | Coordenada Y relativa normalizada (0.0 a 1.0) |
| `pixel_x` | float | Coordenada X em pixels |
| `pixel_y` | float | Coordenada Y em pixels |
| `image_w` | int | Largura da área da imagem em pixels |
| `image_h` | int | Altura da área da imagem em pixels |
| `button` | int | Número do botão (1=esquerdo, 2=meio, 3=direito) |
| `state` | int | Máscara de modificadores |
| `direction` | string | Apenas scroll: "up", "down", "left", "right", "smooth" |
| `delta_x` | float | Apenas scroll: deslocamento em X |
| `delta_y` | float | Apenas scroll: deslocamento em Y |

### Exemplos

```json
{"origin":"mouse","type":"motion","x":0.500000,"y":0.250000,"pixel_x":640.00,"pixel_y":180.00,"image_w":1280,"image_h":720,"state":0}
{"origin":"mouse","type":"mousedown","button":1,"x":0.500000,"y":0.250000,"pixel_x":640.00,"pixel_y":180.00,"image_w":1280,"image_h":720,"state":0}
{"origin":"mouse","type":"mouseup","button":1,"x":0.500000,"y":0.250000,"pixel_x":640.00,"pixel_y":180.00,"image_w":1280,"image_h":720,"state":0}
{"origin":"mouse","type":"mousepress","button":1,"x":0.500000,"y":0.250000,"pixel_x":640.00,"pixel_y":180.00,"image_w":1280,"image_h":720,"state":0}
{"origin":"mouse","type":"scroll","direction":"down","delta_x":0.000000,"delta_y":1.000000,"x":0.500000,"y":0.250000,"pixel_x":640.00,"pixel_y":180.00,"image_w":1280,"image_h":720,"state":0}
```

### Coordenadas Normalizadas

- **`x` e `y`**: Variam de **0.0 a 1.0**, onde:
  - `x=0.0` está na borda esquerda
  - `x=1.0` está na borda direita
  - `y=0.0` está na borda superior
  - `y=1.0` está na borda inferior
- **`pixel_x` e `pixel_y`**: Coordenadas exatas em pixels dentro da `GtkDrawingArea`
- **`image_w` e `image_h`**: Dimensões da área de desenho em pixels

### Restrição Espacial

- **Apenas na imagem**: Eventos de mouse são capturados **somente na `GtkDrawingArea`** (widget de imagem)
- **Toolbar e botões não enviam**: Cliques na toolbar ou em botões de zoom/controle são consumidos localmente e NÃO geram eventos de mouse para o servidor

### Scroll Suave

- `direction="smooth"`: Scroll suave com `delta_x` e `delta_y` precisos
- `direction="up|down|left|right"`: Scroll discreto (delta ±1.0)

---

## Eventos de Joystick

### Tipos de Eventos

| Tipo | Fonte | Campos Obrigatórios | Notas |
|------|-------|-------------------|-------|
| `axis` | Eixo analógico | `number`, `value`, `time` | Contínuo enquanto eixo se move |
| `buttondown` | Botão pressionado | `number`, `value=1` | Valor é sempre 1 |
| `buttonup` | Botão solto | `number`, `value=0` | Valor é sempre 0 |
| `buttonpress` | Botão solto (ciclo completo) | `number`, `value=0` | Gerado apenas quando value muda para 0 |

### Campos JSON

```json
{
  "origin": "joystick",
  "type": "axis|buttondown|buttonup|buttonpress|unknown",
  "number": 0,
  "value": 1200,
  "time": 123456,
  "initial": false
}
```

| Campo | Tipo | Descrição |
|-------|------|-----------|
| `origin` | string | Sempre `"joystick"` |
| `type` | string | Tipo do evento |
| `number` | int | Índice do eixo ou botão |
| `value` | int | Valor do eixo (-32768 a 32767) ou status do botão (0 ou 1) |
| `time` | int | Timestamp do evento em ms (desde abertura do device) |
| `initial` | bool | `true` se é evento inicial do kernel, `false` caso contrário |

### Exemplos

```json
{"origin":"joystick","type":"axis","number":0,"value":1200,"time":123456,"initial":false}
{"origin":"joystick","type":"axis","number":1,"value":-800,"time":123457,"initial":false}
{"origin":"joystick","type":"buttondown","number":0,"value":1,"time":123500,"initial":false}
{"origin":"joystick","type":"buttonup","number":0,"value":0,"time":123550,"initial":false}
{"origin":"joystick","type":"buttonpress","number":0,"value":0,"time":123550,"initial":false}
```

### Valores de Eixo

- **Range**: -32768 (máximo negativo) a 32767 (máximo positivo)
- **Centro**: Aproximadamente 0
- **Exemplos**:
  - Stick X: `number=0`, value de -32768 a 32767 (esquerda/direita)
  - Stick Y: `number=1`, value de -32768 a 32767 (cima/baixo)
  - Triggers analógicos: `number=2,3,...`, ajuste conforme seu joystick

### Comportamento de Botões

- **`buttondown`**: Emitido quando `value` muda para 1 (pressionado)
- **`buttonup`**: Emitido quando `value` muda para 0 (solto)
- **`buttonpress`**: Emitido APENAS quando o botão volta para `value=0` E não for evento inicial

### Comportamento Inicial

- Quando o joystick é aberto, o kernel envia eventos "iniciais" com flag `JS_EVENT_INIT`
- Estes eventos marcam `"initial":true` e representam o estado atual do device
- **Não use eventos iniciais para ações** — apenas para sincronizar estado

### Requisitos de Habilitação

O joystick é habilitado quando:
1. `events_enabled=1` em `rx.ini`
2. `joystick_enabled=1` em `rx.ini`
3. Device é especificado: `joystick=/dev/input/jsX`
4. `rx_input_start_joystick()` é chamada após inicializar o sender

### Thread de Leitura

- Roda em thread separada (`joystick_thread`)
- Lê `/dev/input/jsX` em modo não-bloqueante
- Dorme 10ms quando não há eventos
- Encerra quando `app->stopping` é setado

---

## Configuração

### Arquivo `rx.ini`

```ini
[events]
events_enabled = 1
event_host = 127.0.0.1
event_port = 6000

[joystick]
joystick_enabled = 1
joystick = /dev/input/js0
```

| Chave | Padrão | Descrição |
|-------|--------|-----------|
| `events_enabled` | 0 | Ativa captura e envio de eventos (teclado, mouse, joystick) |
| `event_host` | localhost | Host do servidor TCP de eventos |
| `event_port` | 6000 | Porta do servidor TCP de eventos |
| `joystick_enabled` | 0 | Ativa captura de joystick |
| `joystick` | `/dev/input/js0` | Path do device do joystick |

### Variáveis de Aplicação

Em `src/rx/app.h`:

```c
struct rx_app {
    struct event_sender events;
    int joystick_enabled;
    const char *joystick_device;
    GThread *joystick_thread;
    
    // Atalhos locais
    const char *zoom_in_key;
    const char *zoom_out_key;
    const char *dim_alpha_up_key;
    const char *dim_alpha_down_key;
    ...
}
```

---

## EventSender: Gerenciamento de Conexão

### Inicialização

```c
struct event_sender sender;
event_sender_init(&sender);
event_sender_start(&sender, "127.0.0.1", "6000");
```

### Fluxo de Reconexão

1. **`event_sender_start()`**: Guarda host/porta, habilita o sender, cria thread de reconexão
2. **Thread de Reconexão**: Tenta conectar a cada 1 segundo (`g_usleep(1000000)`)
3. **`try_connect_locked()`**: Testa conexão TCP; se bem-sucedida, guarda `fd`
4. **`event_sender_send()`**: Se não há `fd`, tenta conectar; se falha, descarta evento
5. **Reconexão em Erro**: Se `write_all()` falha, fecha `fd` e reconexão é retentada

### Sincronização

- Usa `GMutex` para proteger:
  - `fd` (file descriptor)
  - `enabled` (flag de ativação)
  - Estado de conexão
- **Chamadas thread-safe**: Sim, `event_sender_send()` é segura para múltiplas threads

### Desligamento

```c
event_sender_close(&sender);
```

- Seta `stopping=1`
- Aguarda término da thread de reconexão
- Fecha o socket
- Limpa o mutex

---

## Tratamento de Erros

### Cenários de Falha

| Cenário | Comportamento |
|---------|---------------|
| Servidor não disponível | Descarta evento, tenta reconectar a cada 1s |
| Conexão cai durante envio | Fecha socket, marca para reconexão |
| JSON mal formado | Não deve ocorrer, mas seria rejeitado pelo servidor |
| Locale com vírgula decimal | **JSON inválido**: deve usar `LC_NUMERIC=C` |
| Joystick não encontrado | Abre arquivo falha, thread encerra com `perror()` |
| Buffer de JSON muito pequeno | Truncagem silenciosa via `snprintf()` (risco) |

### Verificação Recomendada

Para testar se os eventos estão sendo enviados:

```bash
# Terminal 1: Ouve na porta 6000
nc -l 6000

# Terminal 2: Inicia o receptor
./mjpeg_rx --tcp --host 127.0.0.1 --port 5000 \
           --event-host 127.0.0.1 --event-port 6000

# Terminal 3: Testa com joystick
./mjpeg_rx --tcp --host 127.0.0.1 --port 5000 \
           --event-host 127.0.0.1 --event-port 6000 \
           --joystick /dev/input/js0
```

---

## Pontos Críticos de Implementação

### 1. Locale (LC_NUMERIC)

**⚠️ CRÍTICO**: O RX deve rodar com `LC_NUMERIC=C`:

```bash
export LC_NUMERIC=C
./mjpeg_rx ...
```

Reason: `snprintf()` usa locale para decimais. Com locale pt_BR, gera `0,5` em vez de `0.5`, quebrando JSON.

### 2. Escaping JSON

Atualmente **NÃO há escaping** completo de caracteres especiais em nomes de tecla. Se uma tecla tiver aspas ou barras, o JSON pode ficar inválido.

**Solução**: Implementar função de escaping JSON ou usar biblioteca externa.

### 3. Cleanup de Joystick

O `app->stopping` **DEVE ser setado antes** de `g_thread_join()` na thread de joystick, senão ela não encerra:

```c
app->stopping = 1;
if (app->joystick_thread) {
    g_thread_join(app->joystick_thread);
}
```

### 4. Sincronização de EventSender

Múltiplas threads podem chamar `event_sender_send()` simultaneamente:
- Teclado/mouse: Main thread GTK
- Joystick: Thread separada
- **Mutex garante atomicidade** da operação de escrita

### 5. Portabilidade

- **Joystick**: API Linux (`<linux/joystick.h>`), não portável para macOS/Windows sem adaptação
- **GTK**: Portável, mas eventos podem variar entre plataformas

---

## Exemplo de Servidor TCP Recebedor

```c
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = htonl(INADDR_ANY),
        .sin_port = htons(6000)
    };
    
    bind(sock, (struct sockaddr *)&addr, sizeof(addr));
    listen(sock, 1);
    
    int client = accept(sock, NULL, NULL);
    char buf[512];
    while (fgets(buf, sizeof(buf), fdopen(client, "r"))) {
        printf("Evento: %s", buf);
    }
    
    close(client);
    close(sock);
    return 0;
}
```

---

## Resumo de Campos por Tipo de Evento

### Teclado

```
origin (keyboard)
├── type (keydown | keyup | keypress)
├── keyval (int)
├── key (string)
└── state (int)
```

### Mouse

```
origin (mouse)
├── type (motion | mousedown | mouseup | mousepress | scroll)
├── x (float 0-1)
├── y (float 0-1)
├── pixel_x (float)
├── pixel_y (float)
├── image_w (int)
├── image_h (int)
├── button (int, apenas em *down/*up/*press)
├── state (int)
├── direction (string, apenas em scroll)
├── delta_x (float, apenas em scroll)
└── delta_y (float, apenas em scroll)
```

### Joystick

```
origin (joystick)
├── type (axis | buttondown | buttonup | buttonpress | unknown)
├── number (int)
├── value (int)
├── time (int)
└── initial (bool)
```

---

## Referências

- [GDK Key Symbols](https://developer.gnome.org/gdk3/stable/gdk3-Keyboard-Handling.html)
- [Linux Joystick API](https://www.kernel.org/doc/html/latest/input/joystick.html)
- [GTK Signal Reference](https://developer.gnome.org/gtk3/stable/GtkWidget.html#GtkWidget-key-press-event)
- [JSON Lines Format](https://jsonlines.org/)
