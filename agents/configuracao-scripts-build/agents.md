# Agente: Configuracao, Scripts e Build

## Escopo

Responsavel por build, dependencias, arquivos INI e scripts de inicializacao.

## Arquivos principais

- `Makefile`: alvos `all`, `mjpeg_tx`, `mjpeg_rx`, `v4l2_discover`, `static-tx` e `clean`.
- `install-deps.sh`: instalacao de dependencias do sistema.
- `tx.ini`: configuracao do transmissor.
- `rx.ini`: configuracao do receptor.
- `start_transmitter.sh`: wrapper do transmissor.
- `start_receiver.sh`: wrapper do receptor.
- `.github/workflows/c-cpp.yml`: CI de build C/C++.

## Build

- Compilador padrao: `cc`.
- Flags padrao: `-O2 -Wall -Wextra -pedantic`.
- Includes comuns: `-D_DEFAULT_SOURCE -Isrc/common`.
- `mjpeg_tx` linka com `-ljpeg`.
- `mjpeg_rx` usa `pkg-config` para `gtk+-3.0` e `gdk-pixbuf-2.0`.
- `v4l2_discover` nao depende de GTK nem libjpeg.

## Configuracao

Precedencia nos binarios:

1. defaults internos
2. arquivo `.ini`
3. argumentos CLI

`--config` explicito falha se o arquivo nao existir. Sem `--config`, os binarios tentam carregar `tx.ini` ou `rx.ini` se estiverem presentes.

## Scripts

`start_transmitter.sh`:

- Usa `CONFIG=tx.ini` por padrao.
- Compila `mjpeg_tx` se o binario nao existir.
- Aceita `DEVICE`, `PORT`, `WIDTH`, `HEIGHT`, `FPS`, `QUALITY`, `PROTO`, `LISTEN_HOST`, `HOST` e `ALLOW`.
- `ALLOW` pode conter regras separadas por virgula ou espaco.

`start_receiver.sh`:

- Usa `CONFIG=rx.ini` por padrao.
- Compila `mjpeg_rx` se o binario nao existir.
- Aceita `PROTO`, `HOST`, `LISTEN_HOST`, `PORT`, `EVENT_HOST`, `EVENT_PORT` e `JOYSTICK`.
- Exige `EVENT_HOST` e `EVENT_PORT` juntos.

## Pontos de cuidado

- Ao adicionar arquivo `.c`, atualize objetos e dependencias no `Makefile`.
- Ao adicionar dependencia externa, documente no README, `install-deps.sh` e CI.
- Scripts usam `set -eu`; variaveis opcionais precisam ser testadas com `${VAR+x}`.
- O parser INI e simples: comentarios com `#` e `;`, secoes opcionais e linhas `chave=valor`.
- Valores INI duplicados como `allow` sao aceitos porque o handler e chamado por linha.

## Verificacao recomendada

```sh
make clean
make
make static-tx
sh -n start_transmitter.sh
sh -n start_receiver.sh
```
