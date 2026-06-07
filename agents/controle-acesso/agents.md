# Agente: Controle de Acesso

## Escopo

Responsavel pelas regras `allow` usadas pelo transmissor para aceitar ou rejeitar clientes TCP e HTTP.

## Arquivos principais

- `src/tx/access_control.c` e `src/tx/access_control.h`: parser e avaliacao das regras.
- `src/tx/config.c`: leitura de `allow` por INI e CLI.
- `src/tx/stream.c`: aplica `access_control_allowed_sockaddr` em clientes aceitos por `accept`.
- `tx.ini`: exemplos de regras.
- `README.md`: documentacao de formatos aceitos.

## Formatos aceitos

- `all` ou `*`: permite todos e limpa regras anteriores.
- `192.168.0.10`: IPv4 exato.
- `192.168.0.20-192.168.0.30`: range IPv4.
- `10.0.0.0/24`: CIDR IPv4.
- `::1`: IPv6 exato.
- `2001:db8::1-2001:db8::ffff`: range IPv6.
- `2001:db8::/32`: CIDR IPv6.

## Fluxo

1. `access_control_init` inicia com `allow_all = 1`.
2. Ao adicionar uma regra especifica, `allow_all` vira `0`.
3. Cada regra vira um intervalo de bytes `start..end`.
4. Na conexao TCP/HTTP, `stream_accept_pending` pega o endereco do peer.
5. `access_control_allowed_sockaddr` normaliza IPv4-mapped IPv6 e compara com as regras.

## Pontos de cuidado

- UDP nao usa controle de acesso; ele envia para o destino configurado.
- O limite de regras e `MAX_ACCESS_RULES`.
- Ranges com inicio maior que fim sao normalizados internamente.
- CIDR valida prefixo maximo 32 para IPv4 e 128 para IPv6.
- Hostnames nao sao aceitos nas regras; apenas IP literal.
- Se `allow=all` aparecer depois de regras especificas, as regras sao limpas.

## Verificacao recomendada

```sh
make mjpeg_tx
./mjpeg_tx --http --port 8080 --allow 127.0.0.1
./mjpeg_tx --http --port 8080 --allow 10.0.0.0/24
./mjpeg_tx --http --port 8080 --allow ::1
```

Para testes automatizados futuros, o melhor alvo e extrair casos de `access_control_add` e `access_control_allowed_sockaddr` com enderecos IPv4, IPv6 e IPv4-mapped IPv6.
