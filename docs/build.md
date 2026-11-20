# Build e Dependencias

## Dependencias Debian/Ubuntu

```sh
sudo apt-get install -y build-essential libjpeg-dev libgtk-3-dev libjson-c-dev
```

## Build

```sh
make
```

Binarios gerados:

- `mjpeg_tx`
- `mjpeg_rx`
- `v4l2_discover`
- `asset_colorize`

## Alvos uteis

```sh
make mjpeg_tx
make mjpeg_rx
make v4l2_discover
make asset_colorize
make static-tx
make test
make clean
```

`mjpeg_rx` depende de GTK/GdkPixbuf e `json-c`. `asset_colorize` usa GdkPixbuf para carregar e salvar PNG.

`make test` executa testes sem câmera física nem display. Os testes de integração
precisam de sockets locais; ambientes isolados que bloqueiam sockets devem
executá-los em um ambiente que permita `socketpair` e loopback.
