
// Demo: Multi-Window & IPC
// Demonstrates window management, IPC messaging, and event handling.

(function() {
  var panel = document.getElementById('tab-multiwindow');
  
  // Render layout
  panel.innerHTML = `
    <h2>Multi-Window & IPC</h2>
    <div class="two-col-grid">
      <div class="col">
        <div class="card">
          <h3>This Window</h3>
          <div class="field-row"><label>My Window ID:</label><span id="mw-self-id" class="status-val">-</span></div>
          <div class="field-row"><label>Platform:</label><span id="mw-platform" class="status-val">-</span></div>
        </div>

        <div class="card">
          <h3>Open Child Window</h3>
          <div class="field-row"><label>ID:</label><input type="text" id="mw-child-id" value="child-1"></div>
          <div class="field-row"><label>URL:</label><input type="text" id="mw-child-url" value="windows/child.html"></div>
          <div class="field-row"><label>Title:</label><input type="text" id="mw-child-title" value="Child Window"></div>
          <div class="field-row"><label>Width:</label><input type="number" id="mw-child-w" value="700" style="width:60px"> Height: <input type="number" id="mw-child-h" value="500" style="width:60px"></div>
          <div class="field-row"><label>DevTools:</label><input type="checkbox" id="mw-child-dev"></div>
          <button onclick="demo_mw_openChild()">Open Child Window</button>
          <div id="mw-open-res" class="result-box"></div>
        </div>

        <div class="card">
          <h3>Open Settings Window</h3>
          <p class="desc">Opens a fixed-size, non-resizable settings panel.</p>
          <button onclick="demo_mw_openSettings()">Open Settings</button>
          <div id="mw-settings-res" class="result-box"></div>
        </div>

        <div class="card">
          <h3>Open Modal Dialog</h3>
          <p class="desc">Opens a modal window that blocks interaction with other windows.</p>
          <button onclick="demo_mw_openModal()">Open Modal Dialog</button>
          <div id="mw-modal-res" class="result-box"></div>
        </div>

        <div class="card">
          <h3>Window Management</h3>
          <div class="field-row">
            <input type="text" id="mw-target-id" value="child-1" placeholder="Window ID">
            <button onclick="demo_mw_focus()">Focus</button>
            <button class="danger" onclick="demo_mw_close()">Close</button>
          </div>
          <hr>
          <div class="field-row" style="justify-content:space-between">
            <label>Open Windows:</label>
            <button class="sm-btn" onclick="demo_mw_list()">Refresh</button>
          </div>
          <div id="mw-list-res" class="result-box" style="max-height:150px;overflow-y:auto"></div>
        </div>
      </div>

      <div class="col">
        <div class="card">
          <h3>Inter-Window Messaging</h3>
          <div class="field-row"><label>To:</label><input type="text" id="mw-msg-to" value="child-1"></div>
          <div class="field-row"><label>Event:</label><input type="text" id="mw-msg-event" value="ping"></div>
          <div class="field-row"><label>Data:</label><textarea id="mw-msg-data">{"message":"hello from main"}</textarea></div>
          <button onclick="demo_mw_send()">Send Message</button>
          <div id="mw-send-res" class="result-box"></div>
          
          <hr style="margin:10px 0">
          
          <h3>Broadcast</h3>
          <div class="field-row"><label>Event:</label><input type="text" id="mw-bc-event" value="broadcast-event"></div>
          <div class="field-row"><label>Data:</label><textarea id="mw-bc-data">{"from":"main","time":0}</textarea></div>
          <button onclick="demo_mw_broadcast()">Broadcast to All</button>
          <div id="mw-bc-res" class="result-box"></div>
          
          <hr style="margin:10px 0">
          
          <button onclick="demo_mw_sendToSettings()">Quick: Send to Settings</button>
        </div>

        <div class="card">
          <h3>Receive Messages</h3>
          <div id="mw-msg-log" class="log-box" style="height:200px"></div>
          <div class="field-row" style="margin-top:5px;border-top:1px solid #444;padding-top:5px">
             <label>Last Setting:</label>
             <span id="last-setting" class="status-val" style="color:#ce9178">-</span>
          </div>
        </div>

        <div class="card">
          <h3>Window Events</h3>
          <div id="mw-event-log" class="log-box" style="height:100px"></div>
        </div>
      </div>
    </div>

    <div class="card" style="margin-top:20px">
      <h3>Multi-Window Scenario Demo</h3>
      <p class="desc">Runs a full sequence: Open Child -> Ping -> Pong -> Open Settings -> Change Theme -> List -> Close Child -> Close Settings.</p>
      <button class="primary" onclick="demo_mw_runScenario()">Run Full Scenario</button>
      <div id="mw-scenario-log" class="log-box" style="height:150px;margin-top:10px"></div>
    </div>
  `;

  // Global functions for UI
  window.demo_mw_openChild = function() {
    var id = document.getElementById('mw-child-id').value;
    var url = document.getElementById('mw-child-url').value;
    var title = document.getElementById('mw-child-title').value;
    var w = parseInt(document.getElementById('mw-child-w').value) || 700;
    var h = parseInt(document.getElementById('mw-child-h').value) || 500;
    var dev = document.getElementById('mw-child-dev').checked;

    NativeView.windows.open({
      id: id,
      url: url,
      title: title,
      width: w,
      height: h,
      devtools: dev
    }).then(res => {
      showResult('mw-open-res', res);
      demo_mw_list();
    }).catch(err => showError('mw-open-res', err));
  };

  window.demo_mw_openSettings = function() {
    NativeView.windows.open({
      id: "settings",
      url: "windows/settings.html",
      title: "Settings",
      width: 500,
      height: 450,
      resizable: false
    }).then(res => {
      showResult('mw-settings-res', res);
      demo_mw_list();
    }).catch(err => showError('mw-settings-res', err));
  };

  window.demo_mw_openModal = function() {
    NativeView.windows.open({
      id: "modal-1",
      url: "windows/child.html",
      title: "Modal Dialog",
      width: 600,
      height: 400,
      modal: true
    }).then(res => {
      showResult('mw-modal-res', res);
      demo_mw_list();
    }).catch(err => showError('mw-modal-res', err));
  };

  window.demo_mw_list = function() {
    NativeView.windows.list().then(list => {
      var html = '<table style="width:100%;font-size:12px;border-collapse:collapse">';
      html += '<tr><th style="text-align:left">ID</th><th style="text-align:left">Title</th><th>Vis</th></tr>';
      list.forEach(w => {
        html += `<tr><td>${w.id}</td><td>${w.title}</td><td style="text-align:center">${w.visible?'✓':'-'}</td></tr>`;
      });
      html += '</table>';
      document.getElementById('mw-list-res').innerHTML = html;
    }).catch(err => showError('mw-list-res', err));
  };

  window.demo_mw_focus = function() {
    var id = document.getElementById('mw-target-id').value;
    NativeView.windows.focus(id).catch(err => alert(err));
  };

  window.demo_mw_close = function() {
    var id = document.getElementById('mw-target-id').value;
    NativeView.windows.close(id).then(() => {
      setTimeout(demo_mw_list, 500);
    }).catch(err => alert(err));
  };

  window.demo_mw_send = function() {
    var to = document.getElementById('mw-msg-to').value;
    var event = document.getElementById('mw-msg-event').value;
    var dataStr = document.getElementById('mw-msg-data').value;
    try {
      var data = JSON.parse(dataStr);
      NativeView.ipc.send(to, event, data).then(res => {
        showResult('mw-send-res', res);
      }).catch(err => showError('mw-send-res', err));
    } catch(e) {
      showError('mw-send-res', "Invalid JSON");
    }
  };

  window.demo_mw_broadcast = function() {
    var event = document.getElementById('mw-bc-event').value;
    var dataStr = document.getElementById('mw-bc-data').value;
    try {
      var data = JSON.parse(dataStr);
      if (data.time === 0) data.time = Date.now();
      NativeView.ipc.broadcast(event, data).then(res => {
        showResult('mw-bc-res', res);
      }).catch(err => showError('mw-bc-res', err));
    } catch(e) {
      showError('mw-bc-res', "Invalid JSON");
    }
  };

  window.demo_mw_sendToSettings = function() {
    NativeView.ipc.send("settings", "settings-changed", { key: "theme", value: "dark" })
      .catch(err => console.error(err));
  };

  // Logging helpers
  function logMsg(from, event, data) {
    var log = document.getElementById('mw-msg-log');
    if (!log) return;
    var entry = document.createElement('div');
    entry.className = 'log-entry';
    var time = new Date().toLocaleTimeString();
    entry.innerHTML = `<span style="color:#569cd6">[${time}]</span> <span style="color:#4ec9b0">from:${from}</span> <span style="color:#dcdcaa">event:${event}</span> <span style="color:#ce9178">${JSON.stringify(data)}</span>`;
    log.insertBefore(entry, log.firstChild);
    while (log.children.length > 20) log.removeChild(log.lastChild);
  }

  function logEvent(msg) {
    var log = document.getElementById('mw-event-log');
    if (!log) return;
    var entry = document.createElement('div');
    entry.className = 'log-entry';
    entry.innerText = msg;
    log.insertBefore(entry, log.firstChild);
    while (log.children.length > 20) log.removeChild(log.lastChild);
  }

  function showResult(id, res) {
    var el = document.getElementById(id);
    el.innerHTML = `<pre style="color:#4ec9b0">${JSON.stringify(res, null, 2)}</pre>`;
  }

  function showError(id, err) {
    var el = document.getElementById(id);
    el.innerHTML = `<span style="color:#f44747">Error: ${err.message || err}</span>`;
  }

  // Setup Listeners (run once when tab is loaded/script executes)
  if (typeof NativeView !== 'undefined') {
    // Populate identity
    setTimeout(() => {
      if (NativeView.windows && NativeView.windows.selfId) {
        document.getElementById('mw-self-id').textContent = NativeView.windows.selfId();
      }
      if (NativeView.app && NativeView.app.getPlatform) {
        NativeView.app.getPlatform().then(p => {
          document.getElementById('mw-platform').textContent = p.platform;
        });
      }
      demo_mw_list();
    }, 500);

    // IPC handlers
    NativeView.ipc.on("ping", function(data, from) {
      logMsg(from, "ping", data);
      NativeView.ipc.send(from, "pong", { 
        original: data, 
        from: NativeView.windows.selfId(), 
        time: Date.now() 
      });
    });

    NativeView.ipc.on("pong", function(data, from) {
      logMsg(from, "pong", data);
      // for scenario
      if (window._scenarioCallback) window._scenarioCallback("pong");
    });

    NativeView.ipc.on("settings-changed", function(data, from) {
      logMsg(from, "settings-changed", data);
      var el = document.getElementById("last-setting");
      if (el) el.textContent = `${from} → ${data.key} = ${data.value}`;
    });

    // Generic catch-all listener via raw events
    NativeView.on("ipc_bus.message", function(payload) {
      // Avoid duplicate logging if handled above? No, this is "raw" view
      // But let's just use the specific handlers for clean log, or
      // if we want to log EVERYTHING:
      if (["ping","pong","settings-changed"].indexOf(payload.event) === -1) {
        logMsg(payload.from, payload.event, payload.data);
      }
    });

    // Window events
    NativeView.windows.onOpened(function(e) {
      logEvent(`Window opened: ${e.id}`);
      demo_mw_list();
      if (window._scenarioCallback) window._scenarioCallback("opened", e.id);
    });
    NativeView.windows.onBeforeClose(function(e) {
      logEvent(`Window beforeClose: ${e.id}`);
    });
    NativeView.windows.onClosed(function(e) {
      logEvent(`Window closed: ${e.id}`);
      demo_mw_list();
      if (window._scenarioCallback) window._scenarioCallback("closed", e.id);
    });
    NativeView.windows.onFocused(function(e) { logEvent(`Window focused: ${e.id}`); });
    NativeView.windows.onBlurred(function(e) { logEvent(`Window blurred: ${e.id}`); });
  }

  // Scenario Runner
  window.demo_mw_runScenario = async function() {
    var log = document.getElementById('mw-scenario-log');
    log.innerHTML = '';
    
    function step(msg) {
      var div = document.createElement('div');
      div.className = 'log-entry';
      div.innerHTML = `<span style="color:#aaa">Step:</span> ${msg} ...`;
      log.appendChild(div);
      return div;
    }
    
    function done(div, ok) {
      div.innerHTML += ok ? ' <span style="color:#4ec9b0">OK</span>' : ' <span style="color:#f44747">FAIL</span>';
      div.scrollIntoView();
    }
    
    function wait(ms) { return new Promise(r => setTimeout(r, ms)); }
    
    function waitForEvent(type, matchId) {
      return new Promise(resolve => {
        var to = setTimeout(() => { window._scenarioCallback = null; resolve(false); }, 3000);
        window._scenarioCallback = (evt, id) => {
          if (evt === type && (!matchId || id === matchId)) {
            clearTimeout(to);
            window._scenarioCallback = null;
            resolve(true);
          }
        };
      });
    }

    var s;

    // 1. Open child
    s = step("1. Open child window 'demo-child'");
    NativeView.windows.open({ id: "demo-child", url: "windows/child.html", title: "Scenario Child", width: 400, height: 300 });
    var opened = await waitForEvent("opened", "demo-child");
    done(s, opened);
    if (!opened) return;
    await wait(500);

    // 2. Send ping
    s = step("2. Send ping to 'demo-child'");
    NativeView.ipc.send("demo-child", "ping", { msg: "scenario" });
    var ponged = await waitForEvent("pong");
    done(s, ponged);
    if (!ponged) return;
    await wait(500);

    // 3. Open settings
    s = step("3. Open settings window");
    NativeView.windows.open({ id: "settings", url: "windows/settings.html", title: "Settings", width: 500, height: 450 });
    opened = await waitForEvent("opened", "settings");
    done(s, opened);
    await wait(500);

    // 4. Send settings change
    s = step("4. Send theme:dark to 'settings'");
    try {
      await NativeView.ipc.send("settings", "settings-changed", { key: "theme", value: "dark" });
      done(s, true);
    } catch(e) { done(s, false); }
    await wait(500);

    // 5. List windows
    s = step("5. List windows (expect >= 3)");
    var list = await NativeView.windows.list();
    done(s, list.length >= 3);
    await wait(500);

    // 6. Close child
    s = step("6. Close 'demo-child'");
    NativeView.windows.close("demo-child");
    var closed = await waitForEvent("closed", "demo-child");
    done(s, closed);
    await wait(500);

    // 7. Close settings
    s = step("7. Close 'settings'");
    NativeView.windows.close("settings");
    closed = await waitForEvent("closed", "settings");
    done(s, closed);
    
    step("Scenario Complete");
  };

})();
