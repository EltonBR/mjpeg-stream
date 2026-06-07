# Agente: Utilitarios Comuns

## Escopo

Responsavel por codigo compartilhado entre transmissor, receptor e outros binarios.

## Arquivos principais

- `src/common/net.c` e `src/common/net.h`: sockets, IP/porta, read/write completos e `recv_intr`.
- `src/common/ini.c` e `src/common/ini.h`: parser INI simples com callback.

## `net.c`

Funcoes publicas:

- `tcp_connect(host, port)`
- `tcp_listen(host, port)`
- `udp_connect(host, port)`
- `udp_bind_socket(host, port)`
- `set_reuseaddr(fd)`
- `write_all(fd, buf, len)`
- `read_all(fd, buf, len)`
- `recv_intr(fd, buf, len, flags)`

Comportamento:

- Aceita apenas IP literal ou `*`, nao resolve nomes DNS.
- Valida porta entre 1 e 65535.
- Tenta dual-stack para bind IPv6.
- `read_all` retorna `1` quando le tudo, `0` em EOF e `-1` em erro.

## `ini.c`

Comportamento:

- Le linha por linha.
- Ignora linhas vazias e comentarios iniciados por `#` ou `;`.
- Aceita secoes no formato `[secao]`.
- Chama callback para cada `chave=valor`.
- Trunca linhas em buffer de 512 bytes e secoes em 64 bytes.

## Pontos de cuidado

- O parser INI nao entende aspas, escapes ou comentarios dentro de valores.
- Como `#` e `;` sempre iniciam comentarios, valores contendo esses caracteres nao sao suportados.
- `net.c` nao faz resolucao por hostname; se quiser aceitar nomes, isso muda o contrato de configuracao.
- Alterar `read_all` impacta TCP do receptor e envio de eventos.
- Alterar `write_all` impacta canal de eventos.

## Verificacao recomendada

```sh
make clean
make
```

Para mudancas no parser INI, teste com `tx.ini` e `rx.ini` reais:

```sh
./mjpeg_tx --config tx.ini --help
./mjpeg_rx --config rx.ini --help
```
