# Telemetria JSON Lines

O receptor pode abrir um cliente TCP de telemetria. Cada linha recebida deve ser um JSON. Para especificação completa dos dois canais e servidores de exemplo, consulte [API de telemetria, comandos e eventos do RX](api-telemetria-eventos-rx.md).

## Ativar no receptor

```sh
./mjpeg_rx --config rx.ini \
  --overlay overlay.json \
  --telemetry-enabled \
  --telemetry-host 127.0.0.1 \
  --telemetry-port 7000
```

## Mensagem `telemetry`

Atualiza variaveis usadas em templates.

```json
{"type":"telemetry","angle":32.50,"azimuth":123.45,"heading":180,"battery":11.80}
```

Variaveis comuns:

- `angle`
- `azimuth`
- `heading`
- `battery`
- `date`
- `time`
- `rotate`

`angle` e `azimuth` aceitam numeros JSON; a exibicao depende do widget.

## Mensagem `set`

Altera campos simples de elemento/widget por `id`.

```json
{"type":"set","id":"crosshair","visible":false}
{"type":"set","id":"crosshair","rotation":"245"}
```

## Mensagem `upsert`

Cria ou substitui um elemento basico.

```json
{"type":"upsert","id":"target_1","element":{"type":"image","file":"target.png","x":0.72,"y":0.44,"anchor":"center","w":32,"h":32,"rotation":0,"alpha":1.0,"z_index":30,"visible":true}}
```

## Mensagem `delete`

Remove elemento basico por `id`.

```json
{"type":"delete","id":"target_1"}
```

## Servidor de teste Node.js

```sh
./telemetry_server.js \
  --host 127.0.0.1 --port 7000 \
  --event-host 127.0.0.1 --event-port 6000 \
  --interval 1
```

Ele envia `telemetry` em loop com data, hora, angulo, azimuth, heading, bateria e rotacao. O mesmo processo tambem abre um servidor TCP de eventos JSON Lines e imprime no console os eventos enviados pelo `mjpeg_rx`.

A configuração independente fica em `overlay.ini`, seção `[telemetry]`. `reconnect_ms` controla o intervalo de reconexão. Mudanças de endpoint requerem reiniciar o RX. Um `set` pode ajustar propriedades comuns de widgets modernos pelo seu ID; os comandos legados continuam aceitos.
