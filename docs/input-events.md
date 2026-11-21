# Eventos de entrada do RX

O `mjpeg_rx` envia eventos de teclado, mouse e joystick por TCP como JSON Lines para um **servidor externo**. O RX inicia a conexão; o servidor recebe um objeto JSON por linha. Os eventos são separados do canal TCP que envia telemetria e comandos de overlay ao RX.

A especificação dos campos, tipos, coordenadas, fila, reconexão, perda e segurança está em [API de telemetria, comandos e eventos do RX](api-telemetria-eventos-rx.md). O guia também contém exemplos para implementar os dois servidores.

## Ativação

Em `config/rx.ini`:

```ini
[events]
events_enabled=true
event_host=127.0.0.1
event_port=6000
joystick_enabled=false
joystick=/dev/input/js0
```

Para observar eventos localmente:

```sh
python3 examples/rx_event_server.py --host 127.0.0.1 --port 6000
```

Inicie o RX com uma fonte de vídeo válida, mantenha a janela com foco para teclado e mova ou clique sobre a área de imagem para mouse. O servidor exemplo imprime os objetos recebidos. Para configurar telemetria, consulte `[telemetry]` em `config/overlay.ini` e a especificação vinculada acima.

A fila do RX é limitada e pode descartar eventos sob saturação ou em falha de envio; não há confirmação de processamento. Serviços que usam eventos para controle precisam lidar com desconexão e perda de eventos de liberação.
