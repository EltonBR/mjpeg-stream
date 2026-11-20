'use strict';
const assert = require('node:assert/strict');
const { test } = require('node:test');

test('serializes widgets without losing legacy fields or templates', async () => {
  const { parseLayout, serializeLayout } = await import('../model.mjs');
  const input = { schema_version: 2, widgets: [{ id: 'heading', type: 'compass', x: 1, x_unit: 'px', azimuth: '{azimuth}' }], elements: [{ id: 'legacy', type: 'label', text: '{heading}', custom: 'keep' }] };
  assert.deepEqual(parseLayout(serializeLayout(input)), input);
});

test('rejects duplicate widget IDs and invalid units', async () => {
  const { parseLayout, serializeLayout } = await import('../model.mjs');
  const item = { id: 'same', type: 'status', x: 1 };
  assert.throws(() => serializeLayout({ widgets: [item, {...item}] }), /duplicado/);
  assert.throws(() => parseLayout('{"widgets":[{"id":"one","type":"status","x_unit":"cm"}]}'), /relative ou px/);
});
