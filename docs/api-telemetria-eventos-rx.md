# API TCP do RX: telemetria, comandos de overlay e eventos de entrada

Este documento descreve o comportamento implementado pelo `mjpeg_rx` neste repositório. Ele é destinado a quem vai criar serviços externos que alimentam o HUD ou recebem os eventos de teclado, mouse e joystick. **O RX é cliente TCP nos dois canais**: cada servidor externo abre uma porta e aceita conexões do RX.

```text
Servidor de telemetria e comandos                   Receptor
 TCP 127.0.0.1:7000  <──── conexão iniciada ─────  mjpeg_rx
                     ──── JSON Lines ────────────> store/HUD

Servidor de eventos                                  Receptor
 TCP 127.0.0.1:6000  <──── conexão iniciada ─────  mjpeg_rx
                     <──── JSON Lines ──────────── teclado/mouse/joystick
```

As portas e hosts são configuráveis; 7000 e 6000 são exemplos padrão. Os canais são independentes entre si e do TCP/UDP/HTTP de vídeo. Um programa pode hospedar os dois servidores, desde que abra listeners distintos. O protocolo de eventos não envia comandos de overlay ao RX: **`set`, `upsert` e `delete` entram pelo canal de telemetria**. Eventos enviados pelo RX são dados de entrada; o servidor receptor decide como tratá-los.

## Transporte e enquadramento

- Transporte: TCP de longa duração, sem HTTP, WebSocket, prefixo de comprimento ou cabeçalho de handshake.
- Formato: JSON Lines. Cada mensagem é um objeto JSON codificado em UTF-8, seguido de `\n` (byte `0x0a`). O RX tolera `\r\n` na entrada; prefira `\n`.
- TCP é um fluxo: uma chamada `recv`/`read` pode retornar parte de uma linha ou várias linhas juntas. Acumule bytes por conexão e separe no `\n`; nunca suponha que um pacote ou uma chamada corresponda a uma mensagem.
- Use um serializador JSON que escape aspas, barras e quebras de linha dentro de strings. Números JSON usam ponto decimal e devem ser finitos; não envie `NaN`/`Infinity`.
- No canal **servidor → RX**, cada linha admite até **8191 bytes de conteúdo**, sem contar o `\n`. O RX descarta uma linha maior até encontrar o próximo `\n` e continua com a próxima mensagem. Não conte caracteres Unicode como bytes. Não envie linhas vazias: uma linha vazia faz o RX encerrar aquela leitura e reconectar.
- Não há versão de protocolo negociada, autenticação, criptografia, identificação de cliente, número de sequência, timestamp comum, heartbeat obrigatório nem confirmação de processamento. Uma escrita TCP bem-sucedida não confirma que o RX aceitou ou desenhou um comando. Respostas enviadas pelo servidor de eventos não fazem parte deste protocolo.

O RX aceita IP literal IPv4 ou IPv6 para os endpoints, como `127.0.0.1`, `192.168.1.10` ou `::1`; não resolve hostnames como `localhost` nesses clientes. Os programas Python abaixo implementam listeners IPv4 por simplicidade; para IPv6, adapte o servidor para `socket.AF_INET6`.

## Configuração do RX

Em `overlay.ini`, configure o **servidor externo** que receberá a conexão do cliente de telemetria do RX:

```ini
[overlay]
enabled=true
layout=overlay.json
assets_dir=overlays

[telemetry]
enabled=true
host=127.0.0.1
port=7000
reconnect_ms=1000
```

Em `rx.ini`, configure o **outro servidor externo**, que receberá eventos do RX:

```ini
[events]
events_enabled=true
event_host=127.0.0.1
event_port=6000
joystick_enabled=false
joystick=/dev/input/js0
```

Os endereços acima indicam a própria máquina do RX. Se o serviço estiver em outro computador, use o IP **desse computador** e permita a porta na rede. `joystick_enabled=true` só é necessário para ler joystick Linux; eventos de teclado e mouse não dependem disso. As opções legadas de telemetria `--telemetry-enabled`, `--telemetry-host` e `--telemetry-port` continuam aceitas, assim como `--event-host`, `--event-port` e `--joystick`. `--config` seleciona `rx.ini`; `--overlay-config` seleciona `overlay.ini`. Veja [a configuração do receptor](receptor.md).

`reconnect_ms` configura o intervalo de nova tentativa do canal de telemetria, em milissegundos; o padrão é 1000. Alterações no endpoint de `overlay.ini` exigem reiniciar o RX. Quando o layout JSON é salvo, o RX tenta recarregá-lo sem reiniciar o vídeo. Isso não reconecta a telemetria nem modifica `overlay.ini`.

## Canal servidor → RX: telemetria e comandos

O servidor deve fazer `bind`, `listen` e `accept`; após aceitar, escreve linhas na conexão. Para mais de um RX, mantenha conexões independentes e envie a cada cliente os dados desejados. Não compartilhe buffers ou estado de escrita sem sincronização. O servidor pode iniciar antes ou depois do RX; o RX tenta reconectar quando o serviço está indisponível.

### `telemetry`: atualizar variáveis

```json
{"type":"telemetry","angle":32.5,"azimuth":123.45,"heading":180,"battery":11.8,"rotate":15,"date":"2026-11-20","time":"18:30:00"}
```

Esses nomes são exemplos, não uma lista fixa. Qualquer campo plano com valor JSON numérico, string ou booleano pode ser utilizado em um template `{nome}`. Valores são armazenados como texto; booleanos tornam-se `true`/`false`. Campos `null`, objetos e arrays são ignorados. Enviar uma variável substitui seu valor anterior; campos omitidos mantêm seus valores. Uma chave desconhecida no template resulta em texto vazio. O nome reservado `type` seleciona a operação, não é armazenado como variável.

Exemplos de bindings em `overlay.json`:

```json
{"id":"az","type":"compass","x":0.5,"y":12,"azimuth":"{azimuth}"}
{"id":"status","type":"status","x":16,"y":32,"text":"BAT {battery} V — {date} {time}"}
{"id":"angulo","type":"vertical_ruler","x":0.1,"y":0.5,"value":"{angle}","min":-90,"max":90,"window":40,"major_step":10,"minor_step":1}
```

Cada objeto acima é um item do array `widgets`, não uma mensagem avulsa para o canal TCP. A unidade de ângulo é determinada pelo widget e pela aplicação; bússola e rotação usam graus. O servidor deve usar nomes coerentes com os templates do layout. A precisão enviada é decisão do servidor; a apresentação numérica depende do widget.

### `set`: alterar um ID existente

Aplique `set` a um elemento legado ou a um widget moderno já carregado. `id` diferencia maiúsculas e minúsculas e deve identificar um único item. ID ausente ou desconhecido não produz resposta ao servidor.

```json
{"type":"set","id":"crosshair","visible":false}
{"type":"set","id":"crosshair","x":0.5,"x_unit":"relative","y":30,"y_unit":"px","rotation":"{rotate}"}
{"type":"set","id":"compass","azimuth":"{heading}","alpha":0.9,"labels":["N","NE","L","SE","S","SO","O","NO"]}
{"type":"set","id":"angle_ruler","min":-90,"max":90,"window":40,"major_step":10,"minor_step":1,"flip_horizontal":true}
{"type":"set","id":"readout","items":[{"label":"BAT: ","value":"{battery} V"},{"label":"ANG: ","value":"{angle}"}]}
```

Os IDs acima são exemplos; confira os IDs do seu `overlay.json`. `rotation` de **widget moderno** é string, inclusive para valor fixo como `"45"`; em **elemento legado** também pode ser número como `45`. Para trocar apenas valores exibidos por templates, prefira `telemetry`, sem enviar um `set` por frame.

| Propriedades de widget moderno | Tipo/validação |
|---|---|
| `x`, `y` | números finitos; a interpretação usa a unidade correspondente |
| `w`, `h` | números finitos não negativos |
| `x_unit`, `y_unit`, `w_unit`, `h_unit` | `"relative"` ou `"px"` |
| `visible`, `flip_horizontal`, `flip_vertical` | booleanos JSON |
| `alpha` | número entre 0 e 1 |
| `size`, `width`, `window`, `major_step`, `minor_step` | números finitos positivos |
| `min`, `max` | números finitos; para réguas, `min < max` após a atualização |
| `z_index` | inteiro JSON |
| `anchor` | `top-left`, `top-center`, `top-right`, `bottom-left`, `bottom-right` ou `center` |
| `file`, `text`, `value`, `azimuth`, `rotation`, `label`, `suffix` | strings; o arquivo de imagem deve ser relativo e seguro |
| `items` | apenas `readout`: array de objetos com strings `label` e `value` |
| `labels` | apenas `compass`: exatamente oito strings |

Para widget moderno, a atualização é transacional: se uma propriedade reconhecida for inválida, o conjunto da mensagem é rejeitado e o widget anterior é preservado. Elementos legados aceitam somente `visible`, `x`, `y`, `alpha` e `rotation` pelo `set`; não assuma que os demais campos serão alterados. Campos não suportados não têm confirmação. A coordenada `relative` refere-se à área visível do HUD; sem unidade explícita, o layout mantém a regra legada de valores entre 0 e 1 relativos e outros valores em pixels.

### `upsert`: criar ou substituir elemento legado

```json
{"type":"upsert","id":"target_1","element":{"type":"image","file":"target.png","x":0.72,"y":0.44,"anchor":"center","w":32,"h":32,"rotation":0,"alpha":1,"z_index":30,"visible":true}}
```

`id` externo e `element` são obrigatórios. `element.type` pode ser `image`, `label` ou `line` legados; o `id` externo determina o ID final. Para imagem, o arquivo deve estar sob o diretório de assets configurado; não se aceita caminho absoluto nem `..`. Um ID já pertencente a um widget moderno não é substituído por `upsert`. Esse comando **não cria nem substitui widgets modernos**: para eles, edite o arquivo de layout e deixe o hot reload aplicar a alteração.

Outros exemplos:

```json
{"type":"upsert","id":"mensagem","element":{"type":"label","text":"ALT {altitude}","x":20,"y":80,"size":18,"visible":true}}
{"type":"upsert","id":"referencia","element":{"type":"line","x1":0.4,"y1":0.6,"x2":0.6,"y2":0.6,"width":2,"visible":true}}
```

### `delete`: remover elemento legado

```json
{"type":"delete","id":"target_1"}
```

Remove somente elemento legado. Um ID ausente, inexistente ou de widget moderno não remove um widget e não gera confirmação. Um hot reload posterior pode substituir alterações temporárias de layout feitas por comandos; o arquivo JSON continua sendo a fonte de layout persistente. Os valores de telemetria são mantidos ao recarregar o layout.

### Validação e recuperação

Cada mensagem exige um objeto com `type`. JSON malformado, mensagens desconhecidas, IDs inexistentes ou alterações inválidas podem ser ignorados; informações de diagnóstico podem aparecer em `stderr` do RX. **Não existe ACK/NACK**. Para estado confiável, mantenha a fonte da verdade no servidor e reenvie um snapshot de telemetria quando um RX conectar. `set`/`upsert`/`delete` não oferecem confirmação, sequência ou deduplicação: projetar protocolo confiável adicional exigiria alteração coordenada dos dois lados.

## Canal RX → servidor: eventos de entrada

O servidor abre uma porta TCP, aceita a conexão, lê linhas, valida JSON e faz dispatch por `origin` e `type`. Não é necessário responder. Considere tipos e campos desconhecidos para permitir evolução, trate entrada inválida, imponha um limite de tamanho de linha e feche conexões encerradas. Nunca use uma string recebida diretamente como comando de shell.

### Teclado

```json
{"origin":"keyboard","type":"keydown","keyval":65361,"key":"Left","state":0}
{"origin":"keyboard","type":"keyup","keyval":65361,"key":"Left","state":0}
{"origin":"keyboard","type":"keypress","keyval":65361,"key":"Left","state":0}
```

`keyval` é o código numérico GDK; `key` é o nome retornado pelo GDK ou `unknown`; `state` é uma máscara numérica de modificadores do GDK. `keydown` vem do pressionamento GTK. Na liberação, o código envia `keyup` e depois `keypress`: não depende de um `keydown` ter sido observado pelo seu servidor. Autorrepetição da plataforma pode gerar mais de um pressionamento. Atalhos locais configurados para zoom e dim são consumidos pelo RX e não são enviados.

### Mouse e scroll

```json
{"origin":"mouse","type":"motion","x":0.5,"y":0.25,"pixel_x":640.0,"pixel_y":180.0,"image_w":1280,"image_h":720,"state":0}
{"origin":"mouse","type":"mousedown","button":1,"x":0.5,"y":0.25,"pixel_x":640.0,"pixel_y":180.0,"image_w":1280,"image_h":720,"state":0}
{"origin":"mouse","type":"mouseup","button":1,"x":0.5,"y":0.25,"pixel_x":640.0,"pixel_y":180.0,"image_w":1280,"image_h":720,"state":0}
{"origin":"mouse","type":"mousepress","button":1,"x":0.5,"y":0.25,"pixel_x":640.0,"pixel_y":180.0,"image_w":1280,"image_h":720,"state":0}
{"origin":"mouse","type":"scroll","direction":"down","delta_x":0.0,"delta_y":1.0,"x":0.5,"y":0.25,"pixel_x":640.0,"pixel_y":180.0,"image_w":1280,"image_h":720,"state":0}
```

`x` e `y` são coordenadas normalizadas pela largura e altura alocadas da **área GTK visível**, não pixels do JPEG original. `pixel_x`/`pixel_y` são as coordenadas nessa área; `image_w`/`image_h` são as dimensões da área de desenho. Eventos fora dela não são enviados. `state` é máscara GDK; `button` aparece apenas nos eventos de botão. Na liberação dentro da área, `mouseup` é seguido de `mousepress`.

No scroll discreto, `up` usa `delta_y=-1`, `down` `+1`, `left` usa `delta_x=-1` e `right` `+1`; `smooth` usa os deltas fornecidos pelo GTK. A implementação também prevê `unknown` para direção não reconhecida.

### Joystick

```json
{"origin":"joystick","type":"axis","number":0,"value":1200,"time":123456,"initial":false}
{"origin":"joystick","type":"buttondown","number":0,"value":1,"time":123500,"initial":false}
{"origin":"joystick","type":"buttonup","number":0,"value":0,"time":123550,"initial":false}
{"origin":"joystick","type":"buttonpress","number":0,"value":0,"time":123550,"initial":false}
```

`number` é o índice do eixo ou botão; `value` é o valor informado pela API de joystick Linux, normalmente `-32768..32767` para eixo e `0/1` para botão. `time` é o timestamp em milissegundos fornecido pelo kernel, **não** um horário UTC nem uma referência comum entre dispositivos. `initial` indica `JS_EVENT_INIT`; eventos iniciais também podem ser emitidos. `buttonpress` é emitido após `buttonup` numa liberação não inicial. `unknown` é o tipo de fallback para evento não reconhecido.

### Reconexão, ordem e perda

O RX reconecta o canal de eventos automaticamente; após falha, tenta novamente no intervalo de aproximadamente um segundo. Há uma fila de até **256 eventos pendentes**, além de um que pode estar em envio. Enquanto desconectado, os pendentes aguardam reconexão. Quando a fila está cheia, novos eventos contínuos `motion`/`axis` podem ser descartados; para um evento discreto, o RX tenta remover o contínuo mais antigo. Se não houver contínuo para remover, o novo evento discreto também pode ser descartado. A ordem relativa dos eventos mantidos é preservada.

Se a conexão falhar durante a escrita, o evento retirado da fila não é reenfileirado. Em nenhum caso há garantia de entrega, de ordem entre conexões, de processamento ou de exatamente uma vez. Projetos que controlem equipamento devem lidar com desconexão, liberar estados pressionados por timeout e não depender de receber todos os releases. O JSON dos eventos é montado pelo RX em buffers fixos; strings de nome de tecla não passam por escape JSON completo. O servidor deve tratar linhas inválidas e não confiar no conteúdo recebido.

## Dois servidores de exemplo em Python

Os arquivos abaixo usam somente a biblioteca padrão do Python 3, são exemplos independentes e escutam em loopback por padrão:

- [Servidor que envia telemetria](../examples/rx_telemetry_server.py): aceita vários RX, envia um snapshot por segundo e opcionalmente envia um `set` ao conectar.
- [Servidor que recebe eventos](../examples/rx_event_server.py): aceita vários RX, lê por linha com limite de 64 KiB definido **pelo exemplo**, valida JSON e imprime os objetos. Esse limite não é uma garantia do protocolo do RX.

Em terminais separados, a partir da raiz do repositório:

```sh
python3 examples/rx_telemetry_server.py --host 127.0.0.1 --port 7000 --interval 1
python3 examples/rx_event_server.py --host 127.0.0.1 --port 6000
./mjpeg_rx --config rx.ini --overlay-config overlay.ini
```

O RX precisa também de uma fonte de vídeo configurada. O servidor de telemetria pode opcionalmente executar `--set-demo crosshair`, desde que `crosshair` seja um ID existente no layout. Em produção, substitua a geração simulada pela fonte de dados e o `print` do receptor por um dispatcher que valide os campos relevantes. Os exemplos mantêm cada cliente isolado em uma thread e aceitam reconexões; eles não implementam segurança nem persistência.

Para um teste rápido sem esses scripts, use o servidor existente no repositório:

```sh
node telemetry_server.js --host 127.0.0.1 --port 7000 \
  --event-host 127.0.0.1 --event-port 6000 --interval 1
```

Para inspecionar apenas eventos, `nc -l 6000` pode servir como listener, dependendo da variante instalada do netcat. Para emitir uma linha de telemetria via netcat, é necessário usá-lo como **servidor** de escuta na porta 7000, aceitar a conexão do RX e então escrever a linha seguida de Enter; conectar como cliente ao RX não funciona.

## Segurança e diagnóstico

Os canais não implementam TLS, autenticação nem autorização. Restrinja listeners à interface necessária, use firewall/rede confiável e, se cruzar rede não confiável, proteja o transporte externamente com um túnel autenticado. Um emissor com acesso ao canal de telemetria pode modificar o HUD; um receptor de eventos tem acesso a ações do usuário. Os exemplos usam loopback deliberadamente.

| Sintoma | Verificação |
|---|---|
| `connect telemetria: Connection refused` | Há um servidor em escuta no IP/porta configurados em `overlay.ini`? Esse IP é alcançável a partir da máquina do RX? |
| Variáveis aparecem vazias | A linha tem `type=telemetry`, termina em `\n` e usa os mesmos nomes de `{variavel}` do layout? O JSON é válido e tem até 8191 bytes? |
| `set` não muda nada | O ID existe? É elemento legado ou widget moderno? Os tipos e ranges das propriedades correspondem à tabela? Verifique `stderr`; não há ACK. |
| Evento não chega | `events_enabled=true`, servidor em `event_host:event_port`, janela com foco para teclado e ponteiro sobre a área de imagem para mouse? A fila pode descartar sob pressão. |
| Layout salvo não aparece | O RX usa o mesmo `overlay.ini`/layout salvo? O JSON é válido? Examine as mensagens de recarga no terminal. |
| Após trocar host/porta continua conexão antiga | Reinicie o RX; configurações de endpoint não são recarregadas em runtime. |

Fontes de comportamento: [cliente de telemetria](../src/rx/telemetry.c), [store](../src/rx/telemetry_store.c), [comandos de overlay](../src/rx/overlay_json.c), [captura de eventos](../src/rx/input.c) e [fila de eventos](../src/rx/event_sender.c).
