# Overlay Designer

Para iniciar o executável Linux já gerado, no diretório do projeto:

```sh
./dist/overlay-designer
```

Abra o endereço mostrado no terminal, normalmente `http://127.0.0.1:8081/`. Mantenha o processo aberto e pressione `Ctrl+C` para encerrá-lo. O executável inclui o runtime e a interface: **não é necessário Node.js, npm ou make para executá-lo**. O `config/overlay.ini`, o layout e os assets permanecem arquivos externos graváveis. Por padrão, o executável procura `config/overlay.ini` no diretório atual e depois ao lado do próprio binário; use `./dist/overlay-designer --overlay-config /caminho/config/overlay.ini` para escolher outro. A porta e o endereço vêm de `[designer]` no INI. O arquivo gerado neste ambiente é para Linux x86-64.

Para gerar o executável novamente a partir dos fontes, são necessários Node.js 22 ou superior e `npx` com acesso ao pacote `postject` durante a geração:

```sh
node tools/overlay-designer/build-executable.js
```

O script não sobrescreve `dist/overlay-designer` se já existir; mova ou remova a versão antiga antes de gerar outra. O executável é um artefato local ignorado pelo Git. Como alternativa de desenvolvimento, Node.js 18 ou superior pode executar `node tools/overlay-designer/server.js --overlay-config config/overlay.ini`. Os testes do editor podem ser executados com `node --test tools/overlay-designer/tests/*.test.js`.

A interface carrega e salva layout e configuração, oferece os seis widgets, seleção, movimento, redimensionamento, duplicação, remoção, layers, propriedades, simulador e upload de PNG. A área do HUD representa a área visível do receptor. O fundo de vídeo é uma imagem separada do canvas. O preview Canvas é uma aproximação do Cairo e as fontes podem diferir.

O layout é monitorado pelo receptor; arquivos novos são validados antes da troca, e variáveis de telemetria permanecem. O editor grava por substituição atômica. Alterações no endpoint de telemetria e na aparência em `config/overlay.ini` exigem reiniciar o RX; reinicie também o editor para alterar seu endereço. A configuração aceita caminhos de layout e assets relativos ao diretório do arquivo INI.

A API de gravação exige origem local e token da sessão. JSON e PNG possuem limites de tamanho, e o servidor restringe caminhos e symlinks. Não exponha o servidor à rede externa.
