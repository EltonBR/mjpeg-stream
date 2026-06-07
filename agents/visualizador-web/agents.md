# Agente: Visualizador Web

## Escopo

Responsavel por `viewer.html`, uma pagina local para assistir ao stream HTTP MJPEG emitido por `mjpeg_tx --http`.

## Arquivo principal

- `viewer.html`: HTML, CSS e JavaScript em arquivo unico.

## Fluxo de uso

1. Iniciar transmissor em HTTP:

```sh
./mjpeg_tx --http --host 0.0.0.0 --port 8080
```

2. Abrir `viewer.html` no navegador.
3. Informar host, porta e path.
4. A pagina monta uma URL `http://host:port/path?t=timestamp`.
5. O stream e exibido em um `<img>`.

## Decisoes importantes

- O visualizador nao usa fetch nem canvas; usa o suporte nativo do navegador para MJPEG em `<img>`.
- Hosts IPv6 sao colocados entre colchetes quando necessario.
- O timestamp no query string evita cache ao reconectar.
- Zoom e aplicado por CSS transform, nao por alteracao do stream.
- `Parar` remove o atributo `src` para fechar a conexao do navegador.

## Pontos de cuidado

- O transmissor HTTP atual nao implementa CORS nem endpoints de API; o uso esperado e `<img>`.
- Se adicionar controles, mantenha tudo independente do receptor GTK.
- O path padrao e `/`; normalizacao adiciona `/` quando o usuario omite.
- Para IPv6 literal, preserve a logica de colchetes em `streamUrl`.

## Verificacao recomendada

```sh
./mjpeg_tx --http --host 0.0.0.0 --port 8080
```

Depois abrir:

```text
viewer.html
http://127.0.0.1:8080/
```
