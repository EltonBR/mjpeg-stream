#!/usr/bin/env node
'use strict';

// Gera um executável Linux autocontido. Só o empacotamento exige Node e npx.
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { spawnSync } = require('node:child_process');

const root = path.resolve(__dirname, '../..');
const output = path.join(root, 'overlay-designer');
const temp = fs.mkdtempSync(path.join(os.tmpdir(), 'overlay-designer-sea-'));
const staged = path.join(root, `.overlay-designer-build-${process.pid}`);
function run(command, args) {
  const result = spawnSync(command, args, { stdio: 'inherit', cwd: root });
  if (result.error) throw result.error;
  if (result.status !== 0) throw new Error(`${command} falhou (${result.status})`);
}
try {
  if (process.platform !== 'linux') throw new Error('Empacotamento disponivel apenas no Linux');
  if (Number(process.versions.node.split('.')[0]) < 22) throw new Error('O empacotamento exige Node.js 22 ou superior');
  if (fs.existsSync(output)) throw new Error(`Ja existe ${output}; preserve ou remova o arquivo antes de gerar outro`);
  const assetDir = __dirname;
  const blob = path.join(temp, 'designer.blob');
  const configPath = path.join(temp, 'sea.json');
  const assets = {};
  for (const name of ['index.html', 'style.css', 'app.js', 'model.mjs']) assets[name] = path.join(assetDir, name);
  fs.writeFileSync(configPath, JSON.stringify({ main: path.join(assetDir, 'server.js'), output: blob, assets, disableExperimentalSEAWarning: true }));
  run(process.execPath, ['--experimental-sea-config', configPath]);
  fs.copyFileSync(process.execPath, staged, fs.constants.COPYFILE_EXCL);
  run('npx', ['--yes', 'postject@1.0.0-alpha.6', staged, 'NODE_SEA_BLOB', blob, '--sentinel-fuse', 'NODE_SEA_FUSE_fce680ab2cc467b6e072b8b5df1996b2']);
  fs.chmodSync(staged, 0o755);
  // link() falha se outro processo criar o destino; nunca substitui arquivos do usuario.
  fs.linkSync(staged, output);
  fs.unlinkSync(staged);
  process.stdout.write(`Executavel criado: ${output}\n`);
} catch (error) {
  process.stderr.write(`Falha ao empacotar: ${error.message}\n`);
  process.exitCode = 1;
} finally {
  try { fs.unlinkSync(staged); } catch (error) { if (error.code !== 'ENOENT') throw error; }
  fs.rmSync(temp, { recursive: true, force: true });
}
