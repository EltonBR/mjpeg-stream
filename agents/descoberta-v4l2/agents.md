# Agente: Descoberta V4L2

## Escopo

Responsavel pelo binario `v4l2_discover`, que lista capacidades, formatos, resolucoes e FPS suportados por uma camera V4L2.

## Arquivos principais

- `src/discover/main.c`: implementacao completa do discovery.
- `Makefile`: alvo `v4l2_discover`.
- `discover_camera.sh`: script auxiliar para executar o discovery.

## Fluxo de execucao

1. Usa `/dev/video0` por padrao ou o dispositivo passado como argumento.
2. Abre o dispositivo com `open`.
3. Consulta capacidades com `VIDIOC_QUERYCAP`.
4. Enumera formatos com `VIDIOC_ENUM_FMT`.
5. Para cada formato, enumera resolucoes com `VIDIOC_ENUM_FRAMESIZES`.
6. Para cada resolucao discreta, enumera intervalos/FPS com `VIDIOC_ENUM_FRAMEINTERVALS`.
7. Imprime dados em texto para ajudar a configurar `tx.ini`.

## Decisoes importantes

- O discovery nao compartilha codigo com `src/tx/camera.c`; ele e um utilitario direto e isolado.
- Suporta formatos de tamanho discreto, stepwise e continuous.
- FPS e exibido como `denominator / numerator`.
- Quando o driver nao informa resolucoes ou FPS, o programa reporta isso sem falhar.

## Pontos de cuidado

- V4L2 retorna `EINVAL` ao final das enumeracoes; isso e esperado.
- `cap.capabilities` pode ser substituido por `cap.device_caps` quando `V4L2_CAP_DEVICE_CAPS` estiver presente.
- Mensagens sao em portugues e orientadas para diagnostico de camera.
- Para cameras que exigem permissao, o problema costuma ser grupo `video` ou acesso ao device.

## Verificacao recomendada

```sh
make v4l2_discover
./v4l2_discover /dev/video0
```
