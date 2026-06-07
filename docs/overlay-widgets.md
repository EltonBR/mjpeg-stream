# Overlay e Widgets HUD

O overlay e configurado por `overlay.json`. Ele desenha HUD sobre o frame atual usando Cairo.

## Estrutura

```json
{
  "assets_dir": "overlays",
  "widgets": []
}
```

- `assets_dir`: diretorio base para imagens PNG.
- `widgets`: lista de widgets HUD.

Paths de imagens nao podem ser absolutos e nao podem conter `..`.

## Coordenadas

Campos `x`, `y`, `w`, `h` aceitam:

- `0.0` a `1.0`: relativo a area visivel do HUD.
- valores maiores que `1`: pixels.

Exemplo:

- `x: 0.5` significa centro horizontal da area visivel.
- `x: 20` significa 20 pixels.

O receptor desenha o video em modo cover: a imagem sempre preenche a area da janela mantendo proporcao. Quando a proporcao da janela nao bate com a do frame, o excesso e cortado pelo centro. Coordenadas relativas do HUD seguem a area visivel do widget; tamanhos em pixels dos itens nao sao ampliados pelo zoom.

## Anchors

Anchors aceitos:

- `top-left`
- `top-center`
- `top-right`
- `bottom-left`
- `bottom-right`
- `center`

## Templates

Textos e alguns valores numericos podem usar variaveis de telemetria:

```json
"text": "AZ: {azimuth}"
"rotation": "{rotate}"
"azimuth": "{azimuth}"
```

As variaveis sao atualizadas por mensagens JSON Lines de telemetria.

## Cor e fonte do HUD

`hud_color` e `hud_font` vem do `rx.ini` ou de `--hud-color` / `--hud-font`.

```ini
[overlay]
hud_color=green
hud_font=Monospace
dim_color=#000000
dim_alpha=0.20
```

Ou:

```sh
./mjpeg_rx --overlay overlay.json --hud-color amber --hud-font Monospace \
  --dim-color '#000000' --dim-alpha 0.20
```

Valores aceitos:

- `green`
- `amber`
- `#rrggbb`

`hud_color` afeta widgets desenhados por Cairo, como textos, linhas e bussola. `hud_font` define a familia usada em textos do HUD; o padrao e `Monospace`.

`dim_color` e `dim_alpha` desenham uma camada translucida sobre o video e antes do HUD, util quando a imagem esta clara demais. O padrao e preto com `0.20`. Use `dim_alpha=0` para desativar.

PNGs sao usados como estao no arquivo; para pintar PNG, use `asset_colorize`.

## Widget `compass`

Bussola estilo HUD. Mostra uma escala horizontal centralizada e o valor de azimuth.

Exemplo:

```json
{
  "id": "compass",
  "type": "compass",
  "x": 0.5,
  "y": 12,
  "anchor": "top-center",
  "w": 0.46,
  "h": 54,
  "azimuth": "{azimuth}",
  "labels": ["N", "NE", "E", "SE", "S", "SW", "W", "NW"],
  "size": 14,
  "alpha": 0.95,
  "z_index": 40,
  "visible": true
}
```

Campos principais:

- `azimuth`: valor numerico ou template. Aceita float, por exemplo `123.45`.
- `labels`: opcional. Array com 8 textos na ordem `N`, `NE`, `E`, `SE`, `S`, `SW`, `W`, `NW`.
- `w`, `h`: tamanho da bussola.
- `size`: tamanho do texto.
- `z_index`: ordem de desenho.

Se `{azimuth}` nao estiver disponivel, a bussola tenta usar `{heading}` como fallback.

Por padrao os labels sao em ingles:

```json
["N", "NE", "E", "SE", "S", "SW", "W", "NW"]
```

Exemplo em portugues:

```json
["N", "NE", "L", "SE", "S", "SO", "O", "NO"]
```

## Widgets `vertical_ruler` e `horizontal_ruler`

Regua estilo HUD. O valor atual fica no centro; os tracos se movem conforme a telemetria muda. Use `vertical_ruler` para escala em pe e `horizontal_ruler` para escala deitada.

Exemplo vertical para angulo:

```json
{
  "id": "angle_ruler",
  "type": "vertical_ruler",
  "x": 0.08,
  "y": 0.5,
  "anchor": "center",
  "w": 96,
  "h": 0.46,
  "value": "{angle}",
  "min": -90,
  "max": 90,
  "window": 40,
  "major_step": 10,
  "minor_step": 1,
  "flip_horizontal": true,
  "label": "ANG ",
  "suffix": "\u00b0",
  "size": 13,
  "alpha": 0.95,
  "z_index": 35,
  "visible": true
}
```

Exemplo horizontal:

```json
{
  "id": "speed_ruler",
  "type": "horizontal_ruler",
  "x": 0.5,
  "y": 0.88,
  "anchor": "center",
  "w": 0.46,
  "h": 70,
  "value": "{speed}",
  "min": 0,
  "max": 200,
  "window": 80,
  "major_step": 10,
  "minor_step": 2,
  "flip_horizontal": false,
  "flip_vertical": false,
  "label": "SPD ",
  "suffix": " km/h",
  "size": 13,
  "alpha": 0.95,
  "z_index": 35,
  "visible": true
}
```

Campos:

- `value`: valor numerico ou template, como `{angle}`.
- `min`, `max`: range aceito.
- `window`: intervalo visivel em torno do valor atual.
- `major_step`: passo dos tracos maiores, por exemplo `10`.
- `minor_step`: passo dos tracos menores, por exemplo `1`.
- `label`: prefixo do valor central.
- `suffix`: posfixo do valor central, por exemplo `\u00b0`.
- `flip_horizontal`: no `vertical_ruler`, espelha os tracos/labels para o outro lado da linha, util para colocar a regua no canto esquerdo. No `horizontal_ruler`, inverte o sentido da escala.
- `flip_vertical`: no `horizontal_ruler`, coloca tracos e labels do outro lado da linha.

Apesar do exemplo ser angulo, o widget nao e limitado a angulos. Voce pode usar altitude, velocidade, temperatura, sinal etc.

## Widget `readout`

Grupo de linhas de texto, bom para telemetria lateral.

Exemplo:

```json
{
  "id": "left_readout",
  "type": "readout",
  "x": 20,
  "y": 24,
  "anchor": "top-left",
  "size": 18,
  "alpha": 1.0,
  "z_index": 30,
  "visible": true,
  "items": [
    {"label": "ANG: ", "value": "{angle}"},
    {"label": "AZ: ", "value": "{azimuth}"},
    {"label": "BAT: ", "value": "{battery}V"}
  ]
}
```

Cada item concatena `label` + `value`.

## Widget `status`

Texto simples de uma linha.

Use `status` quando quiser mostrar um texto isolado, sem a estrutura de varias linhas do `readout`.

Exemplos de uso:

- titulo fixo do HUD
- data/hora
- modo atual
- estado de conexao
- mensagem temporaria

Exemplo:

```json
{
  "id": "status",
  "type": "status",
  "text": "MJPEG RX HUD",
  "x": 0.98,
  "y": 24,
  "anchor": "top-right",
  "size": 16,
  "alpha": 0.9,
  "z_index": 30,
  "visible": true
}
```

Exemplo com template:

```json
{
  "id": "datetime",
  "type": "status",
  "text": "{date} {time}",
  "x": 0.98,
  "y": 50,
  "anchor": "top-right",
  "size": 16,
  "alpha": 0.9,
  "z_index": 30,
  "visible": true
}
```

Resumo: `status` e para uma frase/linha independente; `readout` e para uma lista de valores.

## Widget `image`

Desenha PNG com alpha.

Exemplo:

```json
{
  "id": "crosshair",
  "type": "image",
  "file": "crosshair.png",
  "x": 0.5,
  "y": 0.5,
  "anchor": "center",
  "w": 72,
  "h": 72,
  "alpha": 0.9,
  "rotation": "{rotate}",
  "z_index": 20,
  "visible": true
}
```

Use `image` tambem para linhas rotacionaveis: crie um PNG de linha com alpha e use `rotation`.

## Ordem de desenho

Widgets sao desenhados por `z_index` crescente. Valores maiores ficam por cima.

Exemplo:

- `z_index: 10`: fundo ou linha
- `z_index: 20`: crosshair
- `z_index: 40`: bussola no topo

## Compatibilidade com `elements`

O formato antigo `elements` ainda funciona para:

- `image`
- `label`
- `line`

O formato recomendado agora e `widgets`.
