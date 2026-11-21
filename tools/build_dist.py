#!/usr/bin/env python3
"""Cria dist/tx e dist/rx sem sobrescrever uma distribuição existente."""
import configparser
import json
import os
from pathlib import Path
import re
import shutil
import sys
import tempfile

ROOT = Path(__file__).resolve().parent.parent
DIST = ROOT / 'dist'


def project_file(relative, suffix=None):
    path = Path(relative)
    if path.is_absolute() or '..' in path.parts:
        raise ValueError(f'caminho precisa ser relativo ao projeto: {relative}')
    source = ROOT / path
    if not source.resolve().is_relative_to(ROOT) or any(p.is_symlink() for p in [source, *source.parents] if p != ROOT and p.is_relative_to(ROOT)):
        raise ValueError(f'caminho com symlink ou fora do projeto: {relative}')
    if suffix and source.suffix.lower() != suffix:
        raise ValueError(f'extensao invalida: {relative}')
    return source


def copy_file(source, destination):
    if not source.is_file():
        raise FileNotFoundError(f'arquivo necessario ausente: {source}')
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, destination)


def write_launcher(path, binary, config):
    path.write_text(f'''#!/bin/sh
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$HERE"
exec "$HERE/{binary}" --config "${{CONFIG:-$HERE/{config}}}" "$@"
''', encoding='utf-8')
    path.chmod(0o755)


def build():
    destinations = [DIST / 'tx', DIST / 'rx']
    if any(path.exists() or path.is_symlink() for path in destinations):
        raise FileExistsError(f'{DIST}/tx ou {DIST}/rx ja existe; preserve ou mova a distribuicao atual antes de gerar outra')
    ini = configparser.ConfigParser(interpolation=None)
    with (ROOT / 'config/overlay.ini').open(encoding='utf-8') as source:
        ini.read_file(source)
    layout_rel = ini.get('overlay', 'layout', fallback='overlay.json')
    assets_rel = ini.get('overlay', 'assets_dir', fallback='overlays')
    layout_path = project_file(Path('config') / layout_rel, '.json')
    assets_path = project_file(Path('config') / assets_rel)
    document = json.loads(layout_path.read_text(encoding='utf-8'))
    if not isinstance(document, dict):
        raise ValueError('o layout do overlay deve ser um objeto JSON')
    refs = []
    for item in [*(document.get('widgets') or []), *(document.get('elements') or [])]:
        if isinstance(item, dict) and item.get('type') == 'image':
            name = item.get('file')
            if not isinstance(name, str) or not name or '..' in name or Path(name).is_absolute():
                raise ValueError(f'caminho de imagem invalido: {name!r}')
            refs.append(Path('config') / assets_rel / name)
    if not assets_path.is_dir():
        raise FileNotFoundError(f'diretorio de assets ausente: {assets_path}')
    with tempfile.TemporaryDirectory(prefix='.dist-build-', dir=ROOT) as temporary:
        stage = Path(temporary)
        tx, rx = stage / 'tx', stage / 'rx'
        tx.mkdir(); rx.mkdir()
        for name in ['mjpeg_tx', 'v4l2_discover']:
            copy_file(ROOT / 'dist' / name, tx / name)
        for name in ['tx.ini']:
            copy_file(ROOT / 'config' / name, tx / name)
        copy_file(ROOT / 'viewer.html', tx / 'viewer.html')
        for name in ['mjpeg_rx', 'asset_colorize', 'overlay-designer']:
            copy_file(ROOT / 'dist' / name, rx / name)
        for name in ['rx.ini', 'overlay.ini']:
            copy_file(ROOT / 'config' / name, rx / name)
        copy_file(layout_path, rx / layout_rel)
        (rx / assets_rel).mkdir(parents=True, exist_ok=True)
        for relative in refs:
            copy_file(project_file(relative), rx / assets_rel / relative.name)
        # PNGs adicionais sao uteis no editor; arquivos pessoais de outros tipos nao sao incluidos.
        for image in assets_path.rglob('*'):
            if image.is_file() and image.suffix.lower() == '.png':
                relative = image.relative_to(ROOT / 'config')
                copy_file(project_file(Path('config') / relative), rx / Path(assets_rel) / relative.name)
        for name in ['rx_telemetry_server.py', 'rx_event_server.py']:
            copy_file(ROOT / 'examples' / name, rx / 'examples' / name)
        copy_file(ROOT / 'docs/receptor.md', rx / 'docs/receptor.md')
        api_text = (ROOT / 'docs/api-telemetria-eventos-rx.md').read_text(encoding='utf-8')
        # O pacote nao inclui os fontes C; mantenha apenas os nomes em texto.
        api_text = re.sub(r'\[([^]]+)\]\(\.\./src/[^)]+\)', r'\1', api_text)
        (rx / 'docs/api-telemetria-eventos-rx.md').write_text(api_text, encoding='utf-8')
        write_launcher(tx / 'iniciar-tx.sh', 'mjpeg_tx', 'tx.ini')
        write_launcher(rx / 'iniciar-rx.sh', 'mjpeg_rx', 'rx.ini')
        (tx / 'README.txt').write_text('''TRANSMISSOR MJPEG — Linux x86-64

Execute ./iniciar-tx.sh ou ./mjpeg_tx --config tx.ini.
Edite tx.ini para escolher camera, porta, formato e endereco.
./v4l2_discover /dev/video0 lista formatos da camera.
viewer.html pode visualizar um stream HTTP MJPEG no navegador.

Necessario: camera V4L2 acessivel, libc e libjpeg.so.8 compativeis.
Os binarios foram compilados para o sistema de origem; nao sao builds ARM.
''', encoding='utf-8')
        (rx / 'README.txt').write_text('''RECEPTOR MJPEG E OVERLAY DESIGNER — Linux x86-64

Execute ./iniciar-rx.sh ou ./mjpeg_rx --config rx.ini.
Execute ./overlay-designer e abra o endereco informado no navegador.
O editor nao precisa de Node.js nem make em runtime. Mantenha o terminal
aberto; Ctrl+C encerra o editor.

rx.ini configura video e eventos; overlay.ini configura HUD e telemetria.
O layout JSON e as imagens PNG estao nos caminhos do overlay.ini.
O editor e o receptor precisam de acesso aos arquivos; para salvar, escrita.
Exemplos opcionais em examples/ exigem Python 3. A API esta em docs/.

O receptor exige ambiente grafico, GTK 3, GdkPixbuf, GLib, Cairo e json-c
compativeis. O editor inclui Node e interface, mas depende das bibliotecas
basicas do sistema. Os executaveis foram compilados para o sistema de origem.
''', encoding='utf-8')
        # Adiciona os pacotes sem substituir os binarios compilados em dist/.
        DIST.mkdir(exist_ok=True)
        os.rename(tx, DIST / 'tx')
        os.rename(rx, DIST / 'rx')
    print(f'Distribuicao criada: {DIST}/tx e {DIST}/rx')


if __name__ == '__main__':
    try:
        build()
    except (OSError, ValueError, configparser.Error, json.JSONDecodeError) as exc:
        print(f'Falha ao criar distribuicao: {exc}', file=sys.stderr)
        sys.exit(1)
