/* DemoCore: shared infrastructure for the NativeView mega demo (no direct NV ops) */
(function(global){
  var qs = function(sel, ctx){ return (ctx||document).querySelector(sel); };
  var qsa = function(sel, ctx){ return Array.prototype.slice.call((ctx||document).querySelectorAll(sel)); };

  var state = {
    connected: false,
    connecting: true,
    platform: 'unknown',
    cVersion: 'n/a',
    versionMismatch: false,
    pass: 0,
    fail: 0,
    tabInited: Object.create(null)
  };

  var EXPECTED_WIRE_VERSION = 1;

  /* --- UI Helpers --- */
  function ensureSpinner(){
    var s = document.getElementById('demo-spinner');
    if (s) return s;
    s = document.createElement('div');
    s.id = 'demo-spinner';
    s.style.cssText = 'position:fixed;top:12px;right:12px;color:var(--text-muted);font-size:12px;display:flex;align-items:center;gap:8px;z-index:1000;';
    s.innerHTML = '<span class="spin" style="display:inline-block;width:12px;height:12px;border:2px solid var(--text-muted);border-top-color:transparent;border-radius:50%;animation:spin 1s linear infinite;"></span> Connecting...';
    // Add keyframes if needed, but simple way:
    if (!document.getElementById('spin-style')){
      var st = document.createElement('style');
      st.id = 'spin-style';
      st.textContent = '@keyframes spin { to { transform: rotate(360deg); } }';
      document.head.appendChild(st);
    }
    document.body.appendChild(s);
    return s;
  }

  function ensureBanner(msg, type){
    var b = document.getElementById('demo-banner');
    if (!b) {
      b = document.createElement('div');
      b.id = 'demo-banner';
      b.style.cssText = 'position:fixed;top:0;left:0;right:0;padding:8px;text-align:center;font-weight:bold;z-index:10001;';
      document.body.appendChild(b);
    }
    b.textContent = msg;
    if (type==='error') {
      b.style.background = '#e94560';
      b.style.color = '#fff';
    } else {
      b.style.background = '#eee';
      b.style.color = '#000';
    }
    b.style.display = 'block';
  }

  var logPanel, logVisible = false;
  function ensureLogPanel(){
    if (logPanel) return logPanel;
    var panel = document.createElement('div');
    panel.setAttribute('role','log');
    panel.style.position='fixed';
    panel.style.right='12px';
    panel.style.bottom='48px';
    panel.style.width='420px';
    panel.style.maxHeight='40vh';
    panel.style.overflow='auto';
    panel.style.background='var(--bg-panel)';
    panel.style.border='1px solid var(--border)';
    panel.style.borderRadius='6px';
    panel.style.padding='8px';
    panel.style.fontFamily='var(--font-mono)';
    panel.style.fontSize='12px';
    panel.style.display='none';
    panel.id='demo-log-panel';
    document.body.appendChild(panel);
    logPanel = panel;
    return panel;
  }

  function renderStatus(){
    var c = qs('#sb-connected'); 
    if (c) {
      c.textContent = state.connected ? 'YES' : 'NO';
      c.style.color = state.connected ? 'var(--status-pass)' : 'var(--status-fail)';
    }
    
    var p = qs('#sb-platform');  
    if (p) {
      p.innerHTML = state.platform;
      if (state.platform.includes('mac')) p.innerHTML += ' ';
      else if (state.platform.includes('win')) p.innerHTML += ' ⊞';
      else if (state.platform.includes('linux')) p.innerHTML += ' 🐧';
    }
    
    var v = qs('#sb-cversion');  if (v) v.textContent = state.cVersion;
    var ps = qs('#sb-pass');     if (ps) ps.textContent = String(state.pass);
    var fl = qs('#sb-fail');     if (fl) fl.textContent = String(state.fail);
    
    document.body.classList.toggle('connected', !!state.connected);
    document.body.classList.toggle('disconnected', !state.connected);

    var s = document.getElementById('demo-spinner');
    if (s) s.style.display = state.connecting ? 'flex' : 'none';
  }

  function detectBridge(){
    var tries = 0, maxTries = 100; // 100 * 100ms = 10s
    state.connecting = true;
    ensureSpinner();
    renderStatus();

    var tick = function(){
      tries++;
      var NV = global.NativeView;
      // Check for bridge presence and handshake readiness
      if (NV && (typeof NV.on === 'function' || typeof NV.invoke === 'function')){
        
        // Check if handshake has completed by looking at _state (exposed in debug builds)
        var stateReady = NV._state && NV._state.ready;
        var jsWireVersion = NV.wireVersion || 0;
        
        if (!stateReady) {
          // Handshake not yet complete, keep waiting
          if (tries < maxTries){
            setTimeout(tick, 100);
          } else {
            state.connecting = false;
            state.connected = false;
            renderStatus();
          }
          return;
        }
        
        // Version Check (compare JS wire version with expected)
        if (jsWireVersion !== EXPECTED_WIRE_VERSION) {
           state.connecting = false;
           state.connected = false;
           state.versionMismatch = true;
           state.cVersion = (NV._state && NV._state.cVersion) || ('wire v'+jsWireVersion);
           renderStatus();
           ensureBanner("Version Mismatch: JS expects wire v"+EXPECTED_WIRE_VERSION+", C reports wire v"+jsWireVersion, 'error');
           return;
        }

        // Handshake successful!
        state.connected = true;
        state.connecting = false;
        state.platform = (NV._state && NV._state.platform) || 'unknown';
        state.cVersion = (NV._state && NV._state.cVersion) || 'n/a';
        renderStatus();
        
        // Auto-run ops on active tab if any
        setTimeout(function(){ DemoCore.runActive(); }, 500);
        return;
      }
      
      if (tries < maxTries){
        setTimeout(tick, 100);
      } else {
        state.connecting = false;
        state.connected = false;
        renderStatus();
        // Optional: show "Connection Failed" toast?
      }
    };
    tick();
  }

  function activateTab(name){
    var panels = qsa('section[data-panel]');
    var btns = qsa('nav button.tab-btn');
    panels.forEach(function(p){
      if (p.getAttribute('data-panel')===name){ p.classList.add('active'); p.setAttribute('data-visible','true'); }
      else { p.classList.remove('active'); p.removeAttribute('data-visible'); }
    });
    btns.forEach(function(b){
      var on = b.getAttribute('data-tab')===name;
      if (on){ b.classList.add('active'); b.setAttribute('aria-current','true'); }
      else { b.classList.remove('active'); b.removeAttribute('aria-current'); }
    });
    if (!state.tabInited[name] && DemoCore._tabs[name]){
      try { DemoCore._tabs[name](); } catch(e){ DemoCore.log('error','tab', String(e)); }
      state.tabInited[name] = true;
    }
  }

  function initTabs(){
    var nav = qs('nav');
    if (!nav) return;
    var btns = qsa('button.tab-btn', nav);
    btns.forEach(function(b){
      b.addEventListener('click', function(){ activateTab(b.getAttribute('data-tab')); });
    });
    if (btns.length) activateTab(btns[0].getAttribute('data-tab'));
  }

  function opBadge(text, cls){
    var span = document.createElement('span');
    span.className = 'badge op-badge ' + (cls||'');
    span.textContent = text;
    return span;
  }

  function createOpCard(cfg){
    var card = document.createElement('div');
    card.className = 'op-card';
    card.id = cfg.id || '';

    var title = document.createElement('div');
    title.className = 'op-title';
    title.textContent = cfg.title || 'Operation';

    var desc = document.createElement('div');
    desc.className = 'op-description';
    desc.textContent = cfg.description || '';

    var inputs = document.createElement('div');
    inputs.className = 'op-inputs';
    var values = {};
    (cfg.inputs||[]).forEach(function(inp){
      var wrap = document.createElement('label');
      wrap.style.display='inline-flex';
      wrap.style.flexDirection='column';
      wrap.style.gap='4px';
      var lab = document.createElement('span');
      lab.textContent = inp.label || inp.id;
      lab.style.fontSize='var(--size-sm)';
      var el;
      if (inp.type === 'checkbox' || inp.type === 'toggle'){
        el = document.createElement('input');
        el.type = 'checkbox';
        el.checked = !!inp.defaultValue;
      } else if (inp.type === 'number'){
        el = document.createElement('input');
        el.type = 'number';
        if (inp.defaultValue!=null) el.value = String(inp.defaultValue);
        if (inp.placeholder) el.placeholder = inp.placeholder;
      } else if (inp.type === 'textarea'){
        el = document.createElement('textarea');
        el.style.minHeight = '60px';
        el.style.width = '100%';
        el.style.resize = 'vertical';
        if (inp.defaultValue!=null) el.value = String(inp.defaultValue);
        if (inp.placeholder) el.placeholder = inp.placeholder;
      } else {
        el = document.createElement('input');
        el.type = 'text';
        if (inp.defaultValue!=null) el.value = String(inp.defaultValue);
        if (inp.placeholder) el.placeholder = inp.placeholder;
      }
      el.id = inp.id;
      wrap.appendChild(lab);
      wrap.appendChild(el);
      inputs.appendChild(wrap);
      values[inp.id] = function(){ return (el.type==='checkbox') ? el.checked : el.value; };
    });

    var runBtn = document.createElement('button');
    runBtn.className = 'btn-primary op-run-btn';
    runBtn.textContent = 'Run';

    var badge = opBadge('PENDING','pending');

    var result = document.createElement('pre');
    result.className = 'op-result';
    result.textContent = '';

    card.appendChild(title);
    card.appendChild(desc);
    card.appendChild(inputs);
    card.appendChild(runBtn);
    card.appendChild(badge);
    card.appendChild(result);

    runBtn.addEventListener('click', function(){
      if (!state.connected || state.versionMismatch) {
        badge.className = 'badge op-badge fail';
        badge.textContent = 'FAIL';
        result.textContent = state.versionMismatch 
          ? 'Version mismatch — ops unavailable' 
          : 'Not connected';
        DemoCore.recordFail(cfg.id||cfg.title||'op', new Error('Not connected'));
        return;
      }

      badge.className = 'badge op-badge running';
      badge.textContent = 'RUNNING';
      result.textContent = '';
      var collect = {};
      Object.keys(values).forEach(function(k){ collect[k] = values[k](); });
      Promise.resolve().then(function(){ return cfg.run ? cfg.run(collect) : null; })
      .then(function(data){
        badge.className = 'badge op-badge pass';
        badge.textContent = 'PASS';
        if (cfg.customRender) {
          result.innerHTML = '';
          cfg.customRender(data, result);
        } else {
          try { result.textContent = JSON.stringify(data, null, 2); }
          catch(e){ result.textContent = String(data); }
        }
        DemoCore.recordPass(cfg.id||cfg.title||'op');
      })
      .catch(function(err){
        badge.className = 'badge op-badge fail';
        badge.textContent = 'FAIL';
        var msg = (err && err.message) ? err.message : String(err);
        result.textContent = msg;
        DemoCore.recordFail(cfg.id||cfg.title||'op', err);
      });
    });

    return card;
  }

  function createPushCard(cfg){
    var card = document.createElement('div');
    card.className = 'op-card';
    card.id = cfg.id || '';

    var title = document.createElement('div');
    title.className = 'op-title';
    title.textContent = cfg.title || 'Push Listener';

    var desc = document.createElement('div');
    desc.className = 'op-description';
    desc.textContent = 'Listening for ' + (cfg.event||'event');

    var badge = opBadge('LISTENING','pending');
    var result = document.createElement('pre');
    result.className = 'op-result';
    result.textContent = '';

    card.appendChild(title);
    card.appendChild(desc);
    card.appendChild(document.createElement('div')); /* inputs placeholder */
    card.appendChild(document.createElement('div')); /* run btn placeholder */
    card.appendChild(badge);
    card.appendChild(result);

    try {
      var NV = global.NativeView;
      if (NV && typeof NV.on === 'function' && cfg.event){
        NV.on(cfg.event, function(data){
          badge.className = 'badge op-badge push';
          badge.textContent = 'PUSH';
          var txt;
          try { txt = cfg.format ? String(cfg.format(data)) : JSON.stringify(data, null, 2); }
          catch(e){ txt = String(data); }
          result.textContent = txt;
        });
      }
    } catch(e){
      // ignore
    }
    return card;
  }

  var DemoCore = {
    _tabs: Object.create(null),
    registerTab: function(tabId, initFn){ this._tabs[tabId]=initFn; },
    isConnected: function(){ return state.connected && !state.versionMismatch; },
    isVersionMismatch: function(){ return state.versionMismatch; },
    recordPass: function(){ state.pass++; renderStatus(); },
    recordFail: function(){ state.fail++; renderStatus(); },
    log: function(level, module, message){
      var p = ensureLogPanel();
      var line = document.createElement('div');
      var ts = new Date().toISOString();
      line.textContent = '['+ts+']['+level+']['+module+'] ' + message;
      p.appendChild(line);
      while (p.childNodes.length > 400){ p.removeChild(p.firstChild); }
      if (logVisible) p.scrollTop = p.scrollHeight;
    },
    toggleLog: function(){
      var p = ensureLogPanel();
      logVisible = !logVisible;
      p.style.display = logVisible ? 'block' : 'none';
    },
    createOpCard: createOpCard,
    createPushCard: createPushCard,
    _activateTab: activateTab,

    /* --- New Keyboard Ops --- */
    runActive: function(){
      var activePanel = qs('section.active');
      if (!activePanel) return;
      var runs = qsa('.op-run-btn', activePanel);
      runs.forEach(function(btn){ btn.click(); });
    },
    runAll: function(){
      var tabs = Object.keys(DemoCore._tabs);
      var runNext = function(i){
        if (i >= tabs.length) return;
        var tab = tabs[i];
        activateTab(tab);
        setTimeout(function(){
          DemoCore.runActive();
          setTimeout(function(){ runNext(i+1); }, 500);
        }, 100);
      };
      runNext(0);
    },
    clearActive: function(){
      var activePanel = qs('section.active');
      if (!activePanel) return;
      var results = qsa('.op-result', activePanel);
      results.forEach(function(r){ r.textContent = ''; });
      var badges = qsa('.op-badge', activePanel);
      badges.forEach(function(b){
        b.className = 'badge op-badge pending';
        b.textContent = 'PENDING';
      });
    },
    reloadActive: function(){
      var activeBtn = qs('nav button.tab-btn.active');
      if (!activeBtn) return;
      var name = activeBtn.getAttribute('data-tab');
      var panel = qs('section[data-panel="'+name+'"]');
      if (panel) panel.innerHTML = '';
      state.tabInited[name] = false;
      activateTab(name);
    },
    showHelp: function(){
      var h = document.getElementById('demo-help-modal');
      if (!h) {
        h = document.createElement('div');
        h.id = 'demo-help-modal';
        h.style.cssText = 'position:fixed;top:0;left:0;right:0;bottom:0;background:rgba(0,0,0,0.8);z-index:9999;display:flex;align-items:center;justify-content:center;';
        h.innerHTML = `
          <div style="background:var(--bg-panel);border:1px solid var(--border);padding:24px;border-radius:8px;max-width:500px;width:90%;box-shadow:0 4px 12px rgba(0,0,0,0.5);">
            <h2 style="margin-top:0;border-bottom:1px solid var(--border);padding-bottom:12px;">Keyboard Shortcuts</h2>
            <ul style="list-style:none;padding:0;line-height:1.8;">
              <li><kbd>1-9, 0</kbd> Switch to tab 1-10</li>
              <li><kbd>R</kbd> Run all ops on current tab</li>
              <li><kbd>Shift+R</kbd> Run all tabs</li>
              <li><kbd>L</kbd> Toggle IPC log panel</li>
              <li><kbd>C</kbd> Clear all results on current tab</li>
              <li><kbd>F5</kbd> Reload current tab ops</li>
              <li><kbd>Esc</kbd> Close modal / hint bar</li>
              <li><kbd>?</kbd> Show this help</li>
            </ul>
            <div style="margin-top:16px;text-align:right;">
              <button class="btn-primary" onclick="document.getElementById('demo-help-modal').style.display='none'">Close</button>
            </div>
          </div>
        `;
        document.body.appendChild(h);
      }
      h.style.display = 'flex';
      // Also show hint bar if hidden
      var bar = document.getElementById('demo-hint-bar');
      if (bar) bar.style.display = 'block';
    },
    hideModals: function(){
      var h = document.getElementById('demo-help-modal');
      if (h) h.style.display = 'none';
      var bar = document.getElementById('demo-hint-bar');
      if (bar) bar.style.display = 'none';
      // Also close log if open? Maybe not. "Close any open modal"
    }
  };

  global.DemoCore = DemoCore;

  /* --- Hint Bar --- */
  function ensureHintBar(){
    if (document.getElementById('demo-hint-bar')) return;
    var bar = document.createElement('div');
    bar.id = 'demo-hint-bar';
    bar.style.cssText = 'background:var(--bg-panel);border-bottom:1px solid var(--border);padding:4px 12px;font-size:11px;color:var(--text-muted);display:flex;justify-content:space-between;align-items:center;';
    bar.innerHTML = '<span>Press <strong>?</strong> for keyboard shortcuts</span> <span style="cursor:pointer" onclick="this.parentElement.style.display=\'none\'">&times;</span>';
    var nav = qs('nav');
    if (nav) nav.parentNode.insertBefore(bar, nav.nextSibling);
  }

  document.addEventListener('keydown', function(e){
    // Never intercept if focus in input/textarea
    if (e.target.matches('input, textarea')) return;

    if (e.key === '?') { DemoCore.showHelp(); }
    if (e.key === 'Escape') { DemoCore.hideModals(); }
    if ((e.key === 'l' || e.key === 'L') && !e.ctrlKey && !e.metaKey && !e.altKey) { DemoCore.toggleLog(); }
    
    // Tabs 1-9, 0
    if (e.key >= '1' && e.key <= '9') {
      var idx = parseInt(e.key, 10) - 1;
      var btns = qsa('nav button.tab-btn');
      if (btns[idx]) activateTab(btns[idx].getAttribute('data-tab'));
    }
    if (e.key === '0') {
      var btns = qsa('nav button.tab-btn');
      if (btns[9]) activateTab(btns[9].getAttribute('data-tab'));
    }

    // R / Shift+R
    if (e.key === 'r' || e.key === 'R') {
      if (e.shiftKey) DemoCore.runAll();
      else DemoCore.runActive();
    }

    // C
    if (e.key === 'c' || e.key === 'C') { DemoCore.clearActive(); }

    // F5
    if (e.key === 'F5') {
      e.preventDefault();
      DemoCore.reloadActive();
    }
  });

  document.addEventListener('DOMContentLoaded', function(){
    initTabs();
    ensureHintBar();
    renderStatus();
    detectBridge();
  });

  (function selfTest(){
    try {
      var card = DemoCore.createOpCard({ id:'__test', title:'SelfTest', run: async function(){ return { ok:true }; } });
      if (!(card && card.nodeType === 1)){ DemoCore.log('error','selftest','createOpCard did not return HTMLElement'); }
      var p0 = state.pass, f0 = state.fail;
      DemoCore.recordPass('__t'); DemoCore.recordFail('__t', new Error('x'));
      if (state.pass !== p0+1){ DemoCore.log('error','selftest','recordPass did not increment'); }
      if (state.fail !== f0+1){ DemoCore.log('error','selftest','recordFail did not increment'); }
    } catch(e){
      // silent
    }
  })();
})(typeof window!=='undefined'?window:this);
