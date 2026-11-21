# Transmissor `mjpeg_tx`

`mjpeg_tx` captura frames de camera V4L2 e transmite JPEG por TCP, UDP ou HTTP MJPEG.

## Configuracao

O transmissor carrega `config/tx.ini` automaticamente se existir.

```sh
./dist/mjpeg_tx --config config/tx.ini
```

Precedencia:

1. defaults internos
2. arquivo INI
3. argumentos CLI

## Protocolos

- `tcp`: servidor TCP; cada frame usa prefixo de tamanho `uint32_t`.
- `udp`: envia datagramas fragmentados para um destino.
- `http`: servidor MJPEG para navegador ou tag `<img>`.

## Uso rapido

```sh
./dist/mjpeg_tx --config config/tx.ini
```

HTTP:

```sh
./dist/mjpeg_tx --http --host 0.0.0.0 --port 8080
```

TCP:

```sh
./dist/mjpeg_tx --tcp --host 0.0.0.0 --port 5000
```

UDP:

```sh
./dist/mjpeg_tx --udp --host 127.0.0.1 --port 5000
```

## Controle de acesso

Controle por `allow` vale para TCP e HTTP.

Exemplos:

```ini
allow=192.168.0.10
allow=10.0.0.0/24
allow=::1
```
