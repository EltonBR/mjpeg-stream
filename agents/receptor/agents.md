# Agente: Receptor GTK

## Escopo

Responsavel pelo binario `mjpeg_rx`, que recebe frames JPEG por TCP ou UDP, exibe a imagem em GTK e opcionalmente envia eventos de entrada como JSON Lines.

## Arquivos principais

- `src/rx/main.c`: inicializacao do app, conexao, janela, eventos e thread de recepcao.
- `src/rx/config.c` e `src/rx/config.h`: defaults, INI e CLI do receptor.
- `src/rx/app.c` e `src/rx/app.h`: estado compartilhado do receptor e cleanup.
- `src/rx/receiver.c` e `src/rx/receiver.h`: threads TCP/UDP e remontagem de frames.
- `src/rx/ui.c` e `src/rx/ui.h`: janela GTK, zoom e exibicao de JPEG.
- `src/rx/event_sender.c` e `src/rx/event_sender.h`: canal TCP de eventos e reconexao.
- `src/rx/input.c` e `src/rx/input.h`: teclado, mouse e joystick Linux.
- `rx.ini`: configuracao padrao do receptor.
- `start_receiver.sh`: wrapper de inicializacao com variaveis de ambiente.

## Fluxo de execucao

1. `rx_parse_args` aplica defaults, le `rx.ini` se existir e depois aplica CLI.
2. `rx_app_init` inicializa estado, `GByteArray`, zoom e canal de eventos.
3. `rx_connect` conecta no transmissor TCP ou faz bind UDP.
4. Se eventos estiverem habilitados, `event_sender_start` inicia thread de reconexao.
5. `rx_create_window` monta a UI GTK.
6. `rx_input_attach` e `rx_input_start_joystick` sao ativados quando eventos estao ligados.
7. `rx_start_receiver` inicia thread TCP ou UDP.
8. Frames recebidos sao enfileirados com `g_idle_add` e renderizados na main thread.

## Decisoes importantes

- TCP espera frames com prefixo de tamanho de 32 bits em network byte order.
- UDP remonta chunks por `frame_id`, `chunk_id`, `chunk_count` e `frame_size`.
- A UI nunca deve ser atualizada direto da thread de recepcao; use `rx_queue_frame`.
- Eventos de teclado, mouse e joystick sao enviados como uma linha JSON por evento.
- O canal de eventos tenta reconectar automaticamente; eventos sem conexao ativa sao descartados.

## Pontos de cuidado

- `rx_app_cleanup` faz join da thread de joystick; garanta que `app->stopping` seja setado antes.
- `event_sender_close` limpa o mutex; nao use o sender depois do cleanup.
- `udp_thread` reutiliza `app->frame` para remontagem; alteracoes precisam preservar consistencia entre tamanho, chunks vistos e contador.
- `show_frame` toma posse de `msg->data` e `msg`; nao compartilhe ponteiros sem copiar.
- Coordenadas de mouse sao do widget que recebe os eventos, hoje preferencialmente `app->image`.

## Verificacao recomendada

```sh
make mjpeg_rx
./mjpeg_rx --config rx.ini --tcp --host 127.0.0.1 --port 5000
```

Para eventos:

```sh
nc -l 6000
./mjpeg_rx --tcp --host 127.0.0.1 --port 5000 --event-host 127.0.0.1 --event-port 6000
```
