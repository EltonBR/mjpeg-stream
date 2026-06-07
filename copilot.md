Resumo das alterações realizadas pelo assistente (Copilot)

Contexto
- Problema inicial: os widgets do HUD e o JSON de eventos enviados ao servidor estavam formatando números decimais usando vírgula como separador decimal em algumas localidades, causando apresentação inconsistente no HUD e JSON inválido enviado ao servidor.

Histórico curto das ações
- Primeiro, o assistente implementou um helper local (`widget_snprintf_dot` / `widget_vsnprintf_dot`) para forçar `.` como separador decimal apenas nos pontos onde a formatação era crítica (HUD e JSON). Isso foi feito em:
  - `src/rx/widgets/common/widget_common.h`
  - `src/rx/widgets/common/widget_common.c`
  - Substituições pontuais em:
    - `src/rx/widgets/vertical_ruler/vertical_ruler_widget.c`
    - `src/rx/widgets/compass/compass_widget.c`
    - `src/rx/input.c` (eventos do mouse)

- Em seguida, detectou-se que a implementação inicial do helper substituía todas as vírgulas no buffer por pontos, o que introduzia caracteres extras (problema relatado pelo usuário). O assistente propôs usar `newlocale`/`uselocale` para formatar sob a locale "C" (portanto com `.`) em vez de fazer substituições de caracteres.

- O commit que adicionou esses helpers foi revertido (para desfazer alterações amplas). Depois o assistente aplicou outra abordagem: definir a locale global do processo para `en_US.UTF-8` adicionando chamadas a `setlocale(LC_ALL, "en_US.UTF-8")` em:
  - `src/rx/main.c`
  - `src/tx/main.c`

Motivação das escolhas
- Opção 1 (helpers localizados): evita efeitos colaterais globais e é mais seguro quando queremos controlar apenas pontos específicos de formatação. Requer implementação cuidadosa (não usar substituição cega de vírgulas) e chamadas nos locais certos.
- Opção 2 (locale global): simples e garante que todas as chamadas `printf`/`snprintf` usem `.`; tem efeito global sobre bibliotecas e formatação de datas/números em todo o processo — escolha feita atualmente para simplicidade, conforme solicitado.

Estado atual no repositório
- O commit que adicionou o helper foi revertido.
- `copilot.md` original foi removido pelo revert; este arquivo foi recriado/atualizado agora com este resumo.
- As alterações atualmente aplicadas e em histórico de commits:
  - `setlocale(LC_ALL, "en_US.UTF-8")` foi adicionado em `src/rx/main.c` e `src/tx/main.c` (commit: "set global locale to en_US.UTF-8 in rx and tx mains").

Impacto e riscos
- Definir a locale global afeta todas as operações de formatação numérica e possivelmente comportamento de bibliotecas que dependem de locale (por exemplo, ordenação, formatação de datas). Teste o aplicativo para garantir que não há efeitos colaterais indesejados.

Recomendações
- Se preferir evitar efeitos globais, eu posso reimplementar o helper de forma segura (usar `newlocale`/`uselocale` apenas durante a formatação, sem substituição de caracteres) e re-aplicar apenas nas rotinas que geram HUD e JSON — posso fazer isso agora.
- Se preferir manter a solução atual (locale global), recomendo rodar testes funcionais para confirmar que outras partes do app não foram afetadas.

Próximos passos que posso executar imediatamente
- Reimplementar o helper seguro e usá-lo somente onde necessário (HUD + eventos JSON), revertendo a definição global de `setlocale` se você pedir.
- Ou rodar build/test local (`make mjpeg_rx`) e reportar erros (no macOS pode ser necessário instalar `json-c` e `gtk+3`).

Diga qual opção prefere e eu aplico a mudança: reimplantar helper localizado (recomendado) ou manter locale global.
