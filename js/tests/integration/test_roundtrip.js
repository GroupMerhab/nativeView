// Integration tests for JS round-trip using mock bridge (no C required)
// Loads the JS sources in build order into a shared global context,
// uses the mock bridge to verify request/reply and push paths.
const fs = require('fs');
const path = require('path');
const vm = require('vm');

function load(file) {
  const code = fs.readFileSync(file, 'utf8');
  vm.runInThisContext(code, { filename: file });
}

const root = path.resolve(__dirname, '..', '..');
const SRC = p => path.join(root, 'src', p);
const MOCK = p => path.join(root, 'tests', 'mock', p);

// Load mock bridge first (defines window.__nv, send hooks, helpers)
load(MOCK('bridge.js'));

// Load core/library in manifest order
load(SRC('core/constants.js'));
load(SRC('core/state.js'));
load(SRC('core/error.js'));
load(SRC('core/log.js'));
load(SRC('core/ipc.js'));
load(SRC('core/router.js'));
load(SRC('core/request.js'));
load(SRC('core/events.js'));
load(SRC('core/init.js'));
load(SRC('modules/window.js'));
load(SRC('modules/app.js'));
load(SRC('modules/fs.js'));
load(SRC('modules/dialog.js'));
load(SRC('modules/clipboard.js'));
load(SRC('modules/shell.js'));
load(SRC('modules/screen.js'));
load(SRC('modules/notification.js'));
load(SRC('modules/tray.js'));
load(SRC('modules/windows.js'));
load(SRC('modules/ipc_bus.js'));
load(SRC('debug/debug.js'));
load(SRC('public/api.js'));

// Wire bridge emit path to router via ipc (same as _init.connect does)
if (typeof window !== 'undefined' && window.__nv) {
  window.__nv._emit = function (event, rawJson) {
    _ipc.receive(event, rawJson);
  };
}

// Tiny test harness
function assert(cond, msg) {
  if (!cond) throw new Error('Assertion failed: ' + msg);
}
function assertEq(a, b, msg) { assert(a === b, (msg || '') + ' expected ' + b + ' got ' + a); }
function assertDeepEq(a, b, msg) { assert(JSON.stringify(a) === JSON.stringify(b), (msg || '') + ' expected ' + JSON.stringify(b) + ' got ' + JSON.stringify(a)); }
function sleep(ms) { return new Promise(r => setTimeout(r, ms)); }

async function main() {
  // _state_reset(); // Don't reset, it wipes module listeners like ipc_bus
  if (window.__nv && window.__nv._clearSent) window.__nv._clearSent();

  console.log('1) Handshake flow');
  _init.connect();
  let last = window.__nv._lastSent();
  assert(last && last.event === 'app.handshake', 'handshake sent');
  const hsSeq = last.parsed && last.parsed.s;
  const hsReply = { e: 'app.handshake', s: hsSeq, ok: 1, d: { wireVersion: NV_WIRE_VERSION, cVersion: '1.2.3', platform: 'mac', windowId: 'main' } };
  window.__nv._emit('app.handshake', JSON.stringify(hsReply));
  await sleep(0);
  assertEq(_state.ready, true, 'state.ready true');
  assertEq(_state.cVersion, '1.2.3', 'cVersion set');
  assertEq(_state.platform, 'mac', 'platform set');
  assertEq(NativeView.windows.selfId(), 'main', 'windows.selfId() is main');
  assertEq(NativeView.ipc.selfId(), 'main', 'ipc.selfId() is main');

  console.log('2) Request/reply round-trip');
  const p1 = NativeView.window.setTitle('Test');
  last = window.__nv._lastSent();
  assert(last && last.event === 'window.setTitle', 'setTitle sent');
  assertDeepEq(last.parsed.d, { e: 'window.setTitle', d: { title: 'Test' }, s: last.parsed.s }.d, 'payload ok'); // compare payload only
  const s1 = last.parsed.s;
  window.__nv._emit('window.setTitle', JSON.stringify({ e: 'window.setTitle', s: s1, ok: 1, d: {} }));
  await p1; // should resolve

  console.log('3) Error reply');
  const p2 = NativeView.fs.readText('/bad/path');
  last = window.__nv._lastSent();
  assert(last && last.event === 'fs.readText', 'fs.readText sent');
  const s2 = last.parsed.s;
  window.__nv._emit('fs.readText', JSON.stringify({ e: 'fs.readText', s: s2, ok: 0, d: { code: NV_ERR_NOT_FOUND, msg: 'not found' } }));
  try {
    await p2;
    throw new Error('expected rejection');
  } catch (e) {
    assert(e && e.code === NV_ERR_NOT_FOUND, 'rejects with ERR_NOT_FOUND');
  }

  console.log('4) Push event delivery');
  let clickedArg = null;
  NativeView.notification.onClicked(function (data) { clickedArg = data; });
  window.__nv._emit('notification.clicked', JSON.stringify({ e: 'notification.clicked', s: 0, d: { id: 'n1' } }));
  await sleep(0);
  assertDeepEq(clickedArg, { id: 'n1' }, 'clicked payload delivered');

  console.log('5) Timeout');
  const p3 = NativeView.invoke('app.getVersion', {}, 100);
  try {
    await p3;
    throw new Error('expected timeout');
  } catch (e) {
    assert(e && e.code === NV_ERR_TIMEOUT, 'rejects with ERR_TIMEOUT');
  }

  console.log('6) Multiple pending requests (reverse replies)');
  const reqs = [];
  const seqs = [];
  for (let i = 0; i < 5; i++) {
    const pr = NativeView.invoke('app.getName', { idx: i }, 1000);
    const sent = window.__nv._lastSent();
    seqs.push(sent.parsed.s);
    reqs.push(pr);
  }
  // Reply in reverse order
  for (let i = 4; i >= 0; i--) {
    const payload = { e: 'app.getName', s: seqs[i], ok: 1, d: { ok: true, idx: i } };
    window.__nv._emit('app.getName', JSON.stringify(payload));
  }
  const results = await Promise.all(reqs);
  // Verify order and data match their original requests
  for (let i = 0; i < 5; i++) {
    assert(results[i] && results[i].idx === i, 'result matches request index ' + i);
  }

  console.log('7) Window open/close lifecycle');
  const pOpen = NativeView.windows.open({ id: 'test-win', url: 'test.html' });
  last = window.__nv._lastSent();
  assert(last && last.event === 'windows.open', 'windows.open sent');
  // Check payload
  assertDeepEq(last.parsed.d, { id: 'test-win', url: 'test.html', width: 800, height: 600, resizable: true, devtools: false }, 'windows.open payload');
  
  // Reply
  const sOpen = last.parsed.s;
  window.__nv._emit('windows.open', JSON.stringify({ e: 'windows.open', s: sOpen, ok: 1, d: { id: 'test-win', created: 1 } }));
  
  const resOpen = await pOpen;
  assertDeepEq(resOpen, { id: 'test-win', created: 1 }, 'windows.open resolved');

  console.log('8) selfId populated after handshake');
  // Verified in step 1, but double checking here
  assertEq(NativeView.windows.selfId(), 'main', 'windows.selfId() is main');
  assertEq(NativeView.ipc.selfId(), 'main', 'ipc.selfId() is main');

  console.log('9) IPC bus send/receive round-trip');
  let greetingReceived = null;
  let greetingFrom = null;
  NativeView.ipc.on('greeting', (data, from) => {
    greetingReceived = data;
    greetingFrom = from;
  });
  
  // Simulate incoming push
  window.__nv._emit('ipc_bus.message', JSON.stringify({
    e: 'ipc_bus.message',
    d: { from: 'child-1', event: 'greeting', data: { text: 'hello' } }
  }));
  await sleep(0);
  
  assertDeepEq(greetingReceived, { text: 'hello' }, 'greeting received data');
  assertEq(greetingFrom, 'child-1', 'greeting received from');

  console.log('10) IPC bus broadcast');
  const pBroadcast = NativeView.ipc.broadcast('ping', { seq: 1 });
  last = window.__nv._lastSent();
  assert(last && last.event === 'ipc_bus.send', 'ipc_bus.send (broadcast) sent');
  // Check payload for broadcast (to="*")
  assertDeepEq(last.parsed.d, { to: '*', event: 'ping', data: { seq: 1 } }, 'broadcast payload');
  
  // Reply to broadcast
  const sBroadcast = last.parsed.s;
  window.__nv._emit('ipc_bus.send', JSON.stringify({ e: 'ipc_bus.send', s: sBroadcast, ok: 1, d: { delivered: 1 } }));
  await pBroadcast;

  console.log('11) IPC bus once');
  let oneShotCount = 0;
  NativeView.ipc.once('one-shot', (data) => {
    oneShotCount++;
  });
  
  // Simulate push twice
  const pushMsg = JSON.stringify({
    e: 'ipc_bus.message',
    d: { from: 'any', event: 'one-shot', data: {} }
  });
  
  window.__nv._emit('ipc_bus.message', pushMsg);
  await sleep(0);
  window.__nv._emit('ipc_bus.message', pushMsg);
  await sleep(0);
  
  assertEq(oneShotCount, 1, 'once handler called exactly once');

  console.log('12) Window beforeClose and destruction lifecycle');
  let beforeCloseCalled = false;
  let closedCalled = false;
  const targetWinId = 'win-to-close';
  
  NativeView.windows.onBeforeClose((e) => {
    if (e.id === targetWinId) beforeCloseCalled = true;
  });
  
  NativeView.windows.onClosed((e) => {
    if (e.id === targetWinId) closedCalled = true;
  });
  
  // 1. Simulate beforeClose event
  window.__nv._emit('windows.beforeClose', JSON.stringify({
    e: 'windows.beforeClose',
    s: 0,
    d: { id: targetWinId }
  }));
  await sleep(0);
  assertEq(beforeCloseCalled, true, 'onBeforeClose called');
  assertEq(closedCalled, false, 'onClosed not yet called');
  
  // 2. Simulate closed event
  window.__nv._emit('windows.closed', JSON.stringify({
    e: 'windows.closed',
    s: 0,
    d: { id: targetWinId }
  }));
  await sleep(0);
  assertEq(closedCalled, true, 'onClosed called');
  
  // 3. Verify it's gone from list (mocking list response)
  const pList = NativeView.windows.list();
  last = window.__nv._lastSent();
  assert(last && last.event === 'windows.list', 'windows.list sent');
  const sList = last.parsed.s;
  
  // Mock reply with empty list (or list without targetWinId)
  window.__nv._emit('windows.list', JSON.stringify({
    e: 'windows.list',
    s: sList,
    ok: 1,
    d: [] // Empty list implies it's gone
  }));
  
  const listRes = await pList;
  assert(Array.isArray(listRes) && listRes.length === 0, 'window list is empty');

  console.log('All scenarios passed.');
}

main().catch(err => {
  console.error('Test failed:', err && err.stack || err);
  process.exit(1);
});
