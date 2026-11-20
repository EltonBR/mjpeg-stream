'use strict';

const assert = require('node:assert/strict');
const fs = require('node:fs/promises');
const os = require('node:os');
const path = require('node:path');
const zlib = require('node:zlib');
const { afterEach, test } = require('node:test');
const { createServer, parseIni, validateLayout, MAX_JSON_BYTES, MAX_PNG_BYTES } = require('../server');

const fixtures = [];
function crc32(buffer) {
  let crc = 0xffffffff;
  for (const byte of buffer) {
    crc ^= byte;
    for (let bit = 0; bit < 8; bit += 1) crc = (crc >>> 1) ^ ((crc & 1) ? 0xedb88320 : 0);
  }
  return (crc ^ 0xffffffff) >>> 0;
}

function pngChunk(type, data) {
  const chunk = Buffer.concat([Buffer.from(type), data]);
  const result = Buffer.alloc(data.length + 12);
  result.writeUInt32BE(data.length, 0);
  chunk.copy(result, 4);
  result.writeUInt32BE(crc32(chunk), data.length + 8);
  return result;
}

const ihdr = Buffer.alloc(13);
ihdr.writeUInt32BE(1, 0);
ihdr.writeUInt32BE(1, 4);
ihdr[8] = 8;
ihdr[9] = 6;
const PNG = Buffer.concat([
  Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]),
  pngChunk('IHDR', ihdr),
  pngChunk('IDAT', zlib.deflateSync(Buffer.from([0, 255, 0, 0, 255]))),
  pngChunk('IEND', Buffer.alloc(0)),
]);

function config() {
  return {
    overlay: { enabled: 'true', layout: 'overlay.json', assets_dir: 'overlays' },
    appearance: { hud_color: '#ffbf00', hud_font: 'Monospace', dim_color: '#000000', dim_alpha: '0.20' },
    telemetry: { enabled: 'true', host: '127.0.0.1', port: '7000', reconnect_ms: '1000' },
    designer: { host: '127.0.0.1', port: '8081' },
  };
}

function initialLayout() {
  return { assets_dir: 'overlays', widgets: [{ id: 'label', type: 'status', x: 0.5, y: 24, text: 'ready', visible: true }], elements: [{ id: 'legacy', type: 'label', text: 'retained' }] };
}

async function setup(t) {
  const root = await fs.mkdtemp(path.join(os.tmpdir(), 'overlay-designer-'));
  fixtures.push(root);
  await fs.mkdir(path.join(root, 'overlays'));
  await fs.writeFile(path.join(root, 'overlay.ini'), [
    '[overlay]', 'enabled=true', 'layout=overlay.json', 'assets_dir=overlays', '',
    '[appearance]', 'hud_color=#ffbf00', 'hud_font=Monospace', 'dim_color=#000000', 'dim_alpha=0.20', '',
    '[telemetry]', 'enabled=true', 'host=127.0.0.1', 'port=7000', 'reconnect_ms=1000', '',
    '[designer]', 'host=127.0.0.1', 'port=8081', '',
  ].join('\n'));
  await fs.writeFile(path.join(root, 'overlay.json'), JSON.stringify(initialLayout()));
  const instance = createServer({ iniPath: path.join(root, 'overlay.ini'), config: config(), token: 'test-token' });
  await new Promise((resolve) => instance.server.listen(0, '127.0.0.1', resolve));
  const address = instance.server.address();
  const origin = `http://127.0.0.1:${address.port}`;
  t.after(() => new Promise((resolve) => instance.server.close(resolve)));
  const request = (route, options = {}) => fetch(`${origin}${route}`, {
    ...options,
    headers: {
      ...(options.body ? { 'Content-Type': 'application/json' } : {}),
      ...(options.method && options.method !== 'GET' ? { Origin: origin, 'X-CSRF-Token': 'test-token' } : {}),
      ...options.headers,
    },
  });
  return { root, request, origin };
}

afterEach(async () => {
  await Promise.all(fixtures.splice(0).map((root) => fs.rm(root, { recursive: true, force: true })));
});

test('GET config and layout expose current config and preserve legacy elements', async (t) => {
  const { request } = await setup(t);
  const configResponse = await request('/api/config');
  assert.equal(configResponse.status, 200);
  assert.equal(configResponse.headers.get('x-csrf-token'), 'test-token');
  assert.deepEqual((await configResponse.json()).overlay.layout, 'overlay.json');
  const layoutResponse = await request('/api/layout');
  assert.equal(layoutResponse.status, 200);
  assert.equal((await layoutResponse.json()).elements[0].id, 'legacy');
});

test('PUT layout validates and atomically persists widgets while retaining elements', async (t) => {
  const { root, request } = await setup(t);
  const layout = initialLayout();
  layout.widgets.push({ id: 'compass2', type: 'compass', x: 0.5, y: 12, labels: ['N', 'NE', 'E', 'SE', 'S', 'SW', 'W', 'NW'] });
  const response = await request('/api/layout', { method: 'PUT', body: JSON.stringify(layout) });
  assert.equal(response.status, 200);
  assert.deepEqual(JSON.parse(await fs.readFile(path.join(root, 'overlay.json'), 'utf8')), layout);
  const invalid = { ...layout, widgets: [...layout.widgets, layout.widgets[0]] };
  const rejected = await request('/api/layout', { method: 'PUT', body: JSON.stringify(invalid) });
  assert.equal(rejected.status, 400);
  assert.deepEqual(JSON.parse(await fs.readFile(path.join(root, 'overlay.json'), 'utf8')), layout);
});

test('PUT config validates, serializes INI and leaves file unchanged on invalid input', async (t) => {
  const { root, request } = await setup(t);
  const updated = config();
  updated.appearance.dim_alpha = '0.4';
  const response = await request('/api/config', { method: 'PUT', body: JSON.stringify(updated) });
  assert.equal(response.status, 200);
  const text = await fs.readFile(path.join(root, 'overlay.ini'), 'utf8');
  assert.equal(parseIni(text).appearance.dim_alpha, '0.4');
  const prior = text;
  updated.appearance.dim_alpha = '2';
  assert.equal((await request('/api/config', { method: 'PUT', body: JSON.stringify(updated) })).status, 400);
  assert.equal(await fs.readFile(path.join(root, 'overlay.ini'), 'utf8'), prior);
});

test('writes require same-origin Origin and CSRF token', async (t) => {
  const { request, origin } = await setup(t);
  const body = JSON.stringify(initialLayout());
  const absent = await fetch(`${origin}/api/layout`, { method: 'PUT', body, headers: { 'Content-Type': 'application/json' } });
  assert.equal(absent.status, 403);
  const wrongOrigin = await fetch(`${origin}/api/layout`, { method: 'PUT', body, headers: { Origin: 'http://evil.example', 'X-CSRF-Token': 'test-token' } });
  assert.equal(wrongOrigin.status, 403);
  const wrongToken = await fetch(`${origin}/api/layout`, { method: 'PUT', body, headers: { Origin: origin, 'X-CSRF-Token': 'wrong' } });
  assert.equal(wrongToken.status, 403);
});

test('PNG upload accepts a PNG and serves/list assets; rejects bad types and paths', async (t) => {
  const { request, root } = await setup(t);
  const upload = await request('/api/assets?name=new-crosshair.png', { method: 'POST', body: PNG, headers: { 'Content-Type': 'image/png' } });
  assert.equal(upload.status, 201);
  assert.deepEqual(await fs.readFile(path.join(root, 'overlays', 'new-crosshair.png')), PNG);
  assert.deepEqual((await (await request('/api/assets')).json()).assets, ['new-crosshair.png']);
  const served = await request('/assets/new-crosshair.png');
  assert.equal(served.headers.get('content-type'), 'image/png');
  assert.deepEqual(Buffer.from(await served.arrayBuffer()), PNG);
  await fs.mkdir(path.join(root, 'overlays', 'nested'));
  await fs.writeFile(path.join(root, 'overlays', 'nested', 'image.png'), PNG);
  const nested = await request('/assets/nested/image.png');
  assert.equal(nested.status, 200);
  assert.deepEqual(Buffer.from(await nested.arrayBuffer()), PNG);
  assert.equal((await request('/api/assets?name=../escape.png', { method: 'POST', body: PNG, headers: { 'Content-Type': 'image/png' } })).status, 400);
  assert.equal((await request('/api/assets?name=bad.png', { method: 'POST', body: PNG, headers: { 'Content-Type': 'text/plain' } })).status, 415);
  assert.equal((await request('/assets/%2e%2e%2foverlay.ini')).status, 404);
});

test('rejects symlinked layout and assets paths', async (t) => {
  const { root, request } = await setup(t);
  const outside = path.join(root, 'outside.json');
  await fs.writeFile(outside, JSON.stringify(initialLayout()));
  await fs.rm(path.join(root, 'overlay.json'));
  await fs.symlink(outside, path.join(root, 'overlay.json'));
  assert.equal((await request('/api/layout')).status, 400);
  await fs.rm(path.join(root, 'overlays'), { recursive: true });
  await fs.symlink(root, path.join(root, 'overlays'));
  assert.equal((await request('/api/assets')).status, 400);
});

test('enforces JSON and PNG upload size limits', async (t) => {
  const { request } = await setup(t);
  const tooLarge = JSON.stringify({ widgets: [], payload: 'x'.repeat(MAX_JSON_BYTES) });
  assert.equal((await request('/api/layout', { method: 'PUT', body: tooLarge })).status, 413);
  const largePng = Buffer.alloc(MAX_PNG_BYTES + 1);
  assert.equal((await request('/api/assets?name=large.png', { method: 'POST', body: largePng, headers: { 'Content-Type': 'image/png' } })).status, 413);
});

test('layout parser rejects non-finite values and ambiguous IDs', () => {
  assert.throws(() => validateLayout({ widgets: [
    { id: 'same', type: 'status' }, { id: 'same', type: 'status' },
  ] }), /duplicate widget id/);
  assert.throws(() => validateLayout({ widgets: [{ id: 'bad', type: 'status', x: Infinity }] }), /invalid x/);
});

test('serves the editor and its resources with an executable CSP', async (t) => {
  const { request } = await setup(t);
  const page = await request('/');
  assert.equal(page.status, 200);
  assert.match(page.headers.get('content-type'), /^text\/html/);
  assert.match(page.headers.get('content-security-policy'), /script-src 'self'/);
  assert.match(await page.text(), /Overlay Designer/);
  assert.equal((await request('/app.js')).status, 200);
  assert.equal((await request('/model.mjs')).status, 200);
  assert.equal((await request('/style.css')).status, 200);
});
