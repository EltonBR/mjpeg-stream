'use strict';

const crypto = require('node:crypto');
const fs = require('node:fs');
const http = require('node:http');
const path = require('node:path');
let sea = null;
try { sea = require('node:sea'); } catch { /* Node 18 sem suporte a SEA. */ }
const isSingleExecutable = Boolean(sea && sea.isSea());

const MAX_JSON_BYTES = 1024 * 1024;
const MAX_PNG_BYTES = 5 * 1024 * 1024;
const MAX_PNG_DIMENSION = 8192;
const WIDGET_TYPES = new Set([
  'compass', 'readout', 'status', 'image', 'vertical_ruler', 'horizontal_ruler',
]);
const ANCHORS = new Set([
  'top-left', 'top-center', 'top-right', 'bottom-left', 'bottom-right', 'center',
]);

class HttpError extends Error {
  constructor(status, message) {
    super(message);
    this.status = status;
  }
}

function safeRelativePath(value, label) {
  if (typeof value !== 'string' || value.length === 0 || value.includes('\\') || value.includes('\0')) {
    throw new HttpError(400, `${label} must be a non-empty relative path`);
  }
  if (path.posix.isAbsolute(value) || path.win32.isAbsolute(value)) {
    throw new HttpError(400, `${label} must be relative`);
  }
  const parts = value.split('/');
  if (parts.some((part) => part === '' || part === '.' || part === '..')) {
    throw new HttpError(400, `${label} contains an unsafe path component`);
  }
  return parts;
}

function inside(root, target) {
  const relative = path.relative(root, target);
  return relative === '' || (!relative.startsWith(`..${path.sep}`) && relative !== '..' && !path.isAbsolute(relative));
}

function isLoopbackHost(value) {
  try {
    const hostname = new URL(`http://${value}`).hostname.toLowerCase();
    return hostname === '127.0.0.1' || hostname === 'localhost' || hostname === '[::1]';
  } catch {
    return false;
  }
}

function assertNoSymlink(root, target, allowMissingLeaf = false) {
  if (!inside(root, target)) throw new HttpError(400, 'path escapes configured directory');
  const relative = path.relative(root, target);
  let current = root;
  for (const part of relative.split(path.sep).filter(Boolean)) {
    current = path.join(current, part);
    try {
      const stat = fs.lstatSync(current);
      if (stat.isSymbolicLink()) throw new HttpError(400, 'symbolic links are not allowed');
    } catch (error) {
      if (error.code === 'ENOENT' && allowMissingLeaf && current === target) break;
      if (error.code === 'ENOENT') break;
      throw error;
    }
  }
}

async function ensureSafeDirectory(root, target) {
  if (!inside(root, target)) throw new HttpError(400, 'directory escapes configured directory');
  let current = root;
  const relative = path.relative(root, target);
  for (const part of relative.split(path.sep).filter(Boolean)) {
    current = path.join(current, part);
    try {
      await fs.promises.mkdir(current);
    } catch (error) {
      if (error.code !== 'EEXIST') throw error;
    }
    const stat = await fs.promises.lstat(current);
    if (stat.isSymbolicLink() || !stat.isDirectory()) throw new HttpError(400, 'symbolic links and non-directories are not allowed');
  }
}

function confinedPath(root, relative, label, allowMissingLeaf = false) {
  const target = path.resolve(root, ...safeRelativePath(relative, label));
  assertNoSymlink(root, target, allowMissingLeaf);
  return target;
}

function parseIni(text) {
  const result = {};
  let section = null;
  for (const [index, rawLine] of text.split(/\r?\n/).entries()) {
    const line = rawLine.trim();
    if (!line || line.startsWith('#') || line.startsWith(';')) continue;
    const header = line.match(/^\[([A-Za-z0-9_-]+)\]$/);
    if (header) {
      section = header[1];
      if (result[section]) throw new HttpError(400, `duplicate INI section at line ${index + 1}`);
      result[section] = {};
      continue;
    }
    const pair = line.match(/^([A-Za-z0-9_-]+)\s*=\s*(.*)$/);
    if (!pair || !section) throw new HttpError(400, `invalid INI line ${index + 1}`);
    if (Object.hasOwn(result[section], pair[1])) throw new HttpError(400, `duplicate INI key at line ${index + 1}`);
    result[section][pair[1]] = pair[2];
  }
  return result;
}

function validateConfig(config) {
  if (!config || typeof config !== 'object' || Array.isArray(config)) throw new HttpError(400, 'config must be an object');
  const sections = ['overlay', 'appearance', 'telemetry', 'designer'];
  for (const key of Object.keys(config)) if (!sections.includes(key)) throw new HttpError(400, `unknown config section: ${key}`);
  for (const section of sections) {
    if (!config[section] || typeof config[section] !== 'object' || Array.isArray(config[section])) {
      throw new HttpError(400, `missing or invalid [${section}] section`);
    }
  }
  const { overlay, appearance, telemetry, designer } = config;
  for (const flag of ['enabled']) if (!['true', 'false'].includes(String(overlay[flag]))) throw new HttpError(400, `overlay.${flag} must be true or false`);
  for (const flag of ['enabled']) if (!['true', 'false'].includes(String(telemetry[flag]))) throw new HttpError(400, `telemetry.${flag} must be true or false`);
  safeRelativePath(overlay.layout, 'overlay.layout');
  safeRelativePath(overlay.assets_dir, 'overlay.assets_dir');
  for (const key of ['host']) if (typeof telemetry[key] !== 'string' || !telemetry[key].trim()) throw new HttpError(400, `telemetry.${key} is required`);
  for (const [section, key] of [[telemetry, 'port'], [designer, 'port'], [telemetry, 'reconnect_ms']]) {
    const n = Number(section[key]);
    if (!Number.isInteger(n) || n < (key === 'reconnect_ms' ? 0 : 1) || n > (key === 'reconnect_ms' ? 600000 : 65535)) {
      throw new HttpError(400, `invalid ${key}`);
    }
  }
  if (typeof designer.host !== 'string' || !designer.host.trim()) throw new HttpError(400, 'designer.host is required');
  if (typeof appearance.hud_color !== 'string' || appearance.hud_color.length > 64) throw new HttpError(400, 'invalid appearance.hud_color');
  if (typeof appearance.hud_font !== 'string' || !appearance.hud_font.trim() || appearance.hud_font.length > 128) throw new HttpError(400, 'invalid appearance.hud_font');
  if (typeof appearance.dim_color !== 'string' || appearance.dim_color.length > 64) throw new HttpError(400, 'invalid appearance.dim_color');
  const alpha = Number(appearance.dim_alpha);
  if (!Number.isFinite(alpha) || alpha < 0 || alpha > 1) throw new HttpError(400, 'appearance.dim_alpha must be between 0 and 1');
}

function serializeIni(config) {
  return ['overlay', 'appearance', 'telemetry', 'designer'].map((name) => {
    const lines = [`[${name}]`];
    for (const [key, value] of Object.entries(config[name])) {
      if (typeof value === 'object' || value === undefined || /[\r\n]/.test(String(value))) throw new HttpError(400, `invalid ${name}.${key}`);
      lines.push(`${key}=${value}`);
    }
    return lines.join('\n');
  }).join('\n\n') + '\n';
}

function validateLayout(layout) {
  if (!layout || typeof layout !== 'object' || Array.isArray(layout)) throw new HttpError(400, 'layout must be a JSON object');
  if (layout.schema_version !== undefined && layout.schema_version !== 2) throw new HttpError(400, 'unsupported schema_version');
  if (layout.widgets !== undefined && !Array.isArray(layout.widgets)) throw new HttpError(400, 'layout.widgets must be an array');
  if (!layout.widgets && !layout.elements) throw new HttpError(400, 'layout needs widgets or elements');
  if ((layout.widgets || []).length > 500) throw new HttpError(400, 'layout has too many widgets');
  if (layout.elements !== undefined && !Array.isArray(layout.elements)) throw new HttpError(400, 'layout.elements must be an array');
  if (layout.assets_dir !== undefined) safeRelativePath(layout.assets_dir, 'layout.assets_dir');
  const ids = new Set();
  for (const [index, widget] of (layout.widgets || []).entries()) {
    if (!widget || typeof widget !== 'object' || Array.isArray(widget)) throw new HttpError(400, `widget ${index} must be an object`);
    if (typeof widget.id !== 'string' || !/^[A-Za-z0-9_-]{1,64}$/.test(widget.id)) throw new HttpError(400, `widget ${index} has invalid id`);
    if (ids.has(widget.id)) throw new HttpError(400, `duplicate widget id: ${widget.id}`);
    ids.add(widget.id);
    if (!WIDGET_TYPES.has(widget.type)) throw new HttpError(400, `widget ${widget.id} has invalid type`);
    for (const key of ['x', 'y', 'w', 'h', 'alpha', 'z_index', 'size', 'min', 'max', 'window', 'major_step', 'minor_step']) {
      if (widget[key] !== undefined && (typeof widget[key] !== 'number' || !Number.isFinite(widget[key]))) {
        throw new HttpError(400, `widget ${widget.id} has invalid ${key}`);
      }
    }
    for (const key of ['x_unit', 'y_unit', 'w_unit', 'h_unit']) {
      if (widget[key] !== undefined && !['relative', 'px'].includes(widget[key])) throw new HttpError(400, `widget ${widget.id} has invalid ${key}`);
    }
    for (const key of ['window', 'major_step', 'minor_step']) {
      if (widget[key] !== undefined && widget[key] <= 0) throw new HttpError(400, `widget ${widget.id} has invalid ${key}`);
    }
    if (widget.min !== undefined && widget.max !== undefined && widget.min >= widget.max) throw new HttpError(400, `widget ${widget.id} has invalid range`);
    if (widget.alpha !== undefined && (Number(widget.alpha) < 0 || Number(widget.alpha) > 1)) throw new HttpError(400, `widget ${widget.id} alpha is out of range`);
    if (widget.anchor !== undefined && !ANCHORS.has(widget.anchor)) throw new HttpError(400, `widget ${widget.id} has invalid anchor`);
    if (widget.visible !== undefined && typeof widget.visible !== 'boolean') throw new HttpError(400, `widget ${widget.id} visible must be boolean`);
    if (widget.type === 'image') {
      safeRelativePath(widget.file, `widget ${widget.id} file`);
      if (!widget.file.toLowerCase().endsWith('.png')) throw new HttpError(400, `widget ${widget.id} image must be PNG`);
    }
    for (const key of ['labels', 'items']) {
      if (widget[key] !== undefined && !Array.isArray(widget[key])) throw new HttpError(400, `widget ${widget.id} ${key} must be an array`);
    }
    if (widget.labels && (widget.labels.length !== 8 || widget.labels.some((v) => typeof v !== 'string' || v.length > 128))) throw new HttpError(400, `widget ${widget.id} has invalid labels`);
    if (widget.items && (widget.items.length > 200 || widget.items.some((v) => !v || typeof v !== 'object' || typeof v.label !== 'string' || typeof v.value !== 'string'))) throw new HttpError(400, `widget ${widget.id} has invalid items`);
  }
  if (layout.elements && layout.elements.length > 500) throw new HttpError(400, 'layout has too many elements');
  for (const element of layout.elements || []) {
    if (!element || typeof element !== 'object' || Array.isArray(element)) throw new HttpError(400, 'invalid legacy element');
    if (!['label', 'line', 'image'].includes(element.type)) throw new HttpError(400, 'invalid legacy element type');
    if (element.id) {
      if (typeof element.id !== 'string' || ids.has(element.id)) throw new HttpError(400, 'duplicate element id');
      ids.add(element.id);
    }
  }
  return layout;
}

function crc32(buffer) {
  let crc = 0xffffffff;
  for (const byte of buffer) {
    crc ^= byte;
    for (let bit = 0; bit < 8; bit += 1) crc = (crc >>> 1) ^ ((crc & 1) ? 0xedb88320 : 0);
  }
  return (crc ^ 0xffffffff) >>> 0;
}

function pngDimensions(buffer) {
  const signature = Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]);
  if (buffer.length < 45 || !buffer.subarray(0, 8).equals(signature)) {
    throw new HttpError(400, 'upload is not a valid PNG header');
  }
  let offset = 8;
  let width;
  let height;
  let seenIdat = false;
  let seenIend = false;
  while (offset + 12 <= buffer.length) {
    const length = buffer.readUInt32BE(offset);
    const end = offset + 12 + length;
    if (length > MAX_PNG_BYTES || end > buffer.length) throw new HttpError(400, 'PNG contains an invalid chunk');
    const type = buffer.toString('ascii', offset + 4, offset + 8);
    const chunk = buffer.subarray(offset + 4, offset + 8 + length);
    if (crc32(chunk) !== buffer.readUInt32BE(offset + 8 + length)) throw new HttpError(400, 'PNG chunk checksum is invalid');
    if (offset === 8) {
      if (type !== 'IHDR' || length !== 13) throw new HttpError(400, 'PNG must begin with IHDR');
      width = buffer.readUInt32BE(offset + 8);
      height = buffer.readUInt32BE(offset + 12);
    } else if (type === 'IHDR') {
      throw new HttpError(400, 'PNG has duplicate IHDR');
    }
    if (type === 'IDAT') seenIdat = true;
    if (type === 'IEND') {
      if (length !== 0) throw new HttpError(400, 'PNG IEND chunk is invalid');
      seenIend = true;
      offset = end;
      break;
    }
    offset = end;
  }
  if (!seenIdat || !seenIend || offset !== buffer.length) throw new HttpError(400, 'PNG is incomplete');
  if (!width || !height || width > MAX_PNG_DIMENSION || height > MAX_PNG_DIMENSION) throw new HttpError(400, 'PNG dimensions are invalid or too large');
}

async function readBody(req, limit) {
  const chunks = [];
  let size = 0;
  for await (const chunk of req) {
    size += chunk.length;
    if (size > limit) throw new HttpError(413, 'request body too large');
    chunks.push(chunk);
  }
  return Buffer.concat(chunks);
}

async function atomicWrite(target, content) {
  const temp = path.join(path.dirname(target), `.${path.basename(target)}.${process.pid}.${crypto.randomBytes(8).toString('hex')}.tmp`);
  try {
    await fs.promises.writeFile(temp, content, { flag: 'wx', mode: 0o600 });
    await fs.promises.rename(temp, target);
  } catch (error) {
    await fs.promises.rm(temp, { force: true }).catch(() => {});
    throw error;
  }
}

function loadOptions(argv) {
  let iniPath = path.resolve('overlay.ini');
  if (isSingleExecutable && !fs.existsSync(iniPath)) {
    iniPath = path.join(path.dirname(process.execPath), 'overlay.ini');
  }
  for (let i = 0; i < argv.length; i += 1) {
    if (argv[i] === '--overlay-config' && argv[i + 1]) iniPath = path.resolve(argv[++i]);
    else throw new Error(`unknown or incomplete argument: ${argv[i]}`);
  }
  if (!fs.existsSync(iniPath)) throw new Error(`overlay config not found: ${iniPath}`);
  const config = parseIni(fs.readFileSync(iniPath, 'utf8'));
  validateConfig(config);
  return { iniPath, config };
}

function createServer(options) {
  const iniPath = path.resolve(options.iniPath);
  const configRoot = path.dirname(iniPath);
  const config = options.config;
  validateConfig(config);
  const token = options.token || crypto.randomBytes(32).toString('hex');
  const configPath = () => confinedPath(configRoot, config.overlay.layout, 'overlay.layout', true);
  const assetsRoot = () => confinedPath(configRoot, config.overlay.assets_dir, 'overlay.assets_dir', true);
  const ensureAssetsRoot = async () => {
    const root = assetsRoot();
    await ensureSafeDirectory(configRoot, root);
    return root;
  };
  const server = http.createServer(async (req, res) => {
    const send = (status, data, headers = {}) => {
      const body = Buffer.isBuffer(data) ? data : Buffer.from(JSON.stringify(data));
      res.writeHead(status, {
        'Content-Length': body.length,
        'X-Content-Type-Options': 'nosniff',
        'Content-Security-Policy': "default-src 'none'; frame-ancestors 'none'; base-uri 'none'",
        'Cache-Control': 'no-store',
        ...headers,
      });
      res.end(body);
    };
    try {
      const host = req.headers.host;
      if (!host) throw new HttpError(400, 'Host header required');
      if (!isLoopbackHost(host)) throw new HttpError(403, 'Host must be loopback');
      const url = new URL(req.url, `http://${host}`);
      const write = !['GET', 'HEAD', 'OPTIONS'].includes(req.method);
      if (write) {
        const origin = req.headers.origin;
        let originUrl;
        try { originUrl = new URL(origin); } catch { throw new HttpError(403, 'Origin rejected'); }
        if (origin !== `http://${host}` || originUrl.protocol !== 'http:' || !isLoopbackHost(originUrl.host)) {
          throw new HttpError(403, 'Origin rejected');
        }
        if (req.headers['x-csrf-token'] !== token) throw new HttpError(403, 'CSRF token rejected');
      }
      if (req.method === 'OPTIONS') return send(204, '', { Allow: 'GET, HEAD, PUT, POST, OPTIONS' });
      const staticFiles = {
        '/': ['index.html', 'text/html; charset=utf-8'],
        '/index.html': ['index.html', 'text/html; charset=utf-8'],
        '/style.css': ['style.css', 'text/css; charset=utf-8'],
        '/app.js': ['app.js', 'text/javascript; charset=utf-8'],
        '/model.mjs': ['model.mjs', 'text/javascript; charset=utf-8'],
      };
      if (req.method === 'GET' && Object.hasOwn(staticFiles, url.pathname)) {
        const [filename, contentType] = staticFiles[url.pathname];
        const body = isSingleExecutable
          ? Buffer.from(sea.getAsset(filename))
          : await fs.promises.readFile(path.join(__dirname, filename));
        return send(200, body, {
          'Content-Type': contentType,
          'Content-Security-Policy': "default-src 'self'; img-src 'self' data: blob: http: https:; style-src 'self'; script-src 'self'; connect-src 'self'; frame-ancestors 'none'; base-uri 'none'",
        });
      }
      if (req.method === 'GET' && url.pathname === '/api/config') {
        return send(200, config, { 'Content-Type': 'application/json; charset=utf-8', 'X-CSRF-Token': token });
      }
      if (req.method === 'PUT' && url.pathname === '/api/config') {
        const body = await readBody(req, MAX_JSON_BYTES);
        let next;
        try { next = JSON.parse(body.toString('utf8')); } catch { throw new HttpError(400, 'invalid JSON'); }
        validateConfig(next);
        const serialized = serializeIni(next);
        const parsed = parseIni(serialized);
        const dest = iniPath;
        assertNoSymlink(path.dirname(dest), dest, true);
        await atomicWrite(dest, serialized);
        for (const section of Object.keys(config)) delete config[section];
        Object.assign(config, parsed);
        return send(200, config, { 'Content-Type': 'application/json; charset=utf-8' });
      }
      if (req.method === 'GET' && url.pathname === '/api/layout') {
        const target = configPath();
        let layout;
        try { layout = JSON.parse(await fs.promises.readFile(target, 'utf8')); } catch (error) {
          if (error.code === 'ENOENT') throw new HttpError(404, 'layout not found');
          if (error instanceof SyntaxError) throw new HttpError(422, 'layout file contains invalid JSON');
          throw error;
        }
        validateLayout(layout);
        return send(200, layout, { 'Content-Type': 'application/json; charset=utf-8' });
      }
      if (req.method === 'PUT' && url.pathname === '/api/layout') {
        const body = await readBody(req, MAX_JSON_BYTES);
        let layout;
        try { layout = JSON.parse(body.toString('utf8')); } catch { throw new HttpError(400, 'invalid JSON'); }
        validateLayout(layout);
        const target = configPath();
        await ensureSafeDirectory(configRoot, path.dirname(target));
        assertNoSymlink(configRoot, target, true);
        await atomicWrite(target, `${JSON.stringify(layout, null, 2)}\n`);
        return send(200, layout, { 'Content-Type': 'application/json; charset=utf-8' });
      }
      if (req.method === 'GET' && url.pathname === '/api/assets') {
        const root = await ensureAssetsRoot();
        const names = (await fs.promises.readdir(root, { withFileTypes: true }))
          .filter((entry) => entry.isFile() && entry.name.toLowerCase().endsWith('.png'))
          .map((entry) => entry.name).sort();
        return send(200, { assets: names }, { 'Content-Type': 'application/json; charset=utf-8' });
      }
      if (req.method === 'POST' && url.pathname === '/api/assets') {
        const name = url.searchParams.get('name');
        if (!name || !/^[A-Za-z0-9][A-Za-z0-9._-]{0,127}\.png$/i.test(name)) throw new HttpError(400, 'invalid asset name');
        const contentType = String(req.headers['content-type'] || '').split(';')[0].trim().toLowerCase();
        if (contentType !== 'image/png') throw new HttpError(415, 'Content-Type must be image/png');
        const bytes = await readBody(req, MAX_PNG_BYTES);
        pngDimensions(bytes);
        const root = await ensureAssetsRoot();
        const target = confinedPath(root, name, 'asset name', true);
        await atomicWrite(target, bytes);
        return send(201, { name, size: bytes.length }, { 'Content-Type': 'application/json; charset=utf-8' });
      }
      if (req.method === 'GET' && url.pathname.startsWith('/assets/')) {
        let name;
        try { name = decodeURIComponent(url.pathname.slice('/assets/'.length)); } catch { throw new HttpError(400, 'invalid asset path'); }
        try { safeRelativePath(name, 'asset name'); } catch { throw new HttpError(404, 'asset not found'); }
        const root = await ensureAssetsRoot();
        const target = confinedPath(root, name, 'asset name');
        if (!name.toLowerCase().endsWith('.png')) throw new HttpError(404, 'asset not found');
        const bytes = await fs.promises.readFile(target).catch((error) => {
          if (error.code === 'ENOENT' || error.code === 'EISDIR') throw new HttpError(404, 'asset not found');
          throw error;
        });
        if (bytes.length > MAX_PNG_BYTES) throw new HttpError(413, 'asset exceeds size limit');
        return send(200, bytes, { 'Content-Type': 'image/png', 'Cache-Control': 'no-cache' });
      }
      throw new HttpError(404, 'not found');
    } catch (error) {
      const status = error instanceof HttpError ? error.status : (error.code === 'ENOENT' ? 404 : 500);
      send(status, { error: status === 500 ? 'internal server error' : error.message }, { 'Content-Type': 'application/json; charset=utf-8' });
    }
  });
  return { server, token, config };
}

if (isSingleExecutable || require.main === module) {
  try {
    const { iniPath, config } = loadOptions(process.argv.slice(2));
    const host = config.designer.host || '127.0.0.1';
    if (host !== '127.0.0.1' && host !== '::1' && host !== 'localhost') throw new Error('designer.host must be loopback');
    const { server } = createServer({ iniPath, config });
    server.on('error', (error) => {
      process.stderr.write(`Nao foi possivel iniciar o editor: ${error.message}\n`);
      process.exitCode = 1;
    });
    server.listen(Number(config.designer.port), host, () => {
      const displayHost = host.includes(':') ? `[${host}]` : host;
      process.stdout.write(`Editor de overlay pronto: http://${displayHost}:${config.designer.port}/\nPressione Ctrl+C para encerrar.\n`);
    });
  } catch (error) {
    process.stderr.write(`${error.message}\n`);
    process.exitCode = 1;
  }
}

module.exports = {
  createServer, loadOptions, parseIni, validateConfig, validateLayout,
  safeRelativePath, pngDimensions, MAX_JSON_BYTES, MAX_PNG_BYTES,
};
