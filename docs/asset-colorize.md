# Colorizacao de Assets PNG

`asset_colorize` gera uma copia colorida de um PNG. Ele preserva alpha e usa niveis de preto/branco do asset para variar a intensidade da cor.

## Build

```sh
make asset_colorize
```

## Uso

```sh
./asset_colorize --color amber input.png output.png
./asset_colorize --color green input.png output.png
./asset_colorize --color '#00ccff' input.png output.png
```

## Exemplo com crosshair

```sh
./asset_colorize --color amber overlays/crosshair-original.png overlays/crosshair.png
```

Depois use no overlay:

```json
{
  "type": "image",
  "file": "crosshair.png"
}
```

## Por que nao pintar em runtime?

O receptor carrega PNG como arquivo pronto. Isso evita acoplamento entre renderizacao e processamento de asset, e tambem permite validar o asset colorido antes de executar o HUD.
