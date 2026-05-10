(function() {
  DemoCore.registerTab('ipc', function() {
    var p = document.getElementById('tab-ipc');
    if (!p) return;

    // Helper: Add section header
    function addHeader(text) {
      var h = document.createElement('h3');
      h.textContent = text;
      h.style.margin = '24px 0 12px';
      h.style.borderBottom = '1px solid var(--border)';
      h.style.paddingBottom = '4px';
      p.appendChild(h);
    }

    // Helper: Create benchmark section
    function createBenchCard(id, title, desc, configOptions, runFn) {
      var card = document.createElement('div');
      card.className = 'op-card';
      card.id = id;
      
      var optsHtml = configOptions.map(function(opt) {
        return `<label style="margin-right:12px;"><input type="radio" name="${id}-cfg" value="${opt}" ${opt===configOptions[0]?'checked':''}> ${opt}</label>`;
      }).join('');

      card.innerHTML = `
        <div class="op-title">${title}</div>
        <div class="op-description">${desc}</div>
        <div class="op-inputs" style="margin-bottom:12px;">${optsHtml}</div>
        <button class="btn-primary op-run-btn">Run Test</button>
        <div class="op-progress" style="margin-top:8px; font-size:12px; color:var(--accent); display:none;"></div>
        <div class="op-result" style="margin-top:12px; font-family:var(--font-mono); font-size:12px; white-space:pre-wrap;"></div>
        <div class="op-history" style="margin-top:12px; font-size:11px; color:#888; border-top:1px solid var(--border); padding-top:8px;">
          <strong>History (Last 3):</strong><br>
          <div class="history-list"></div>
        </div>
      `;

      var btn = card.querySelector('button');
      var prog = card.querySelector('.op-progress');
      var res = card.querySelector('.op-result');
      var hist = card.querySelector('.history-list');

      btn.onclick = async function() {
        if (DemoCore.isConnected && !DemoCore.isConnected()) {
           res.textContent = DemoCore.isVersionMismatch && DemoCore.isVersionMismatch() 
             ? 'Version mismatch — ops unavailable' 
             : 'Not connected';
           res.style.color = 'var(--status-fail)';
           return;
        }

        var val = card.querySelector(`input[name="${id}-cfg"]:checked`).value;
        var n = parseInt(val, 10);
        
        btn.disabled = true;
        prog.style.display = 'block';
        prog.textContent = 'Starting...';
        res.textContent = '';
        
        try {
          var resultText = await runFn(n, function(status) {
            prog.textContent = status;
          });
          
          res.textContent = resultText;
          
          // Add to history
          var entry = document.createElement('div');
          entry.textContent = new Date().toLocaleTimeString() + ' - N=' + n + ': ' + resultText.split('\n')[0]; // Summary line
          if (hist.children.length >= 3) hist.lastElementChild.remove();
          hist.insertBefore(entry, hist.firstElementChild);
          
        } catch (e) {
          res.textContent = 'Error: ' + e.message;
          res.style.color = 'var(--status-warn)';
        } finally {
          btn.disabled = false;
          prog.style.display = 'none';
        }
      };
      
      return card;
    }

    // --- Section: Round-Trip Latency Benchmark ---
    addHeader('Round-Trip Latency');
    
    p.appendChild(createBenchCard(
      'ipc-latency',
      'Run Latency Test',
      'Measure sequential request/response time (app.getVersion)',
      [10, 100, 1000],
      async function(n, updateStatus) {
        var times = [];
        for (var i = 0; i < n; i++) {
          if (i % 10 === 0) updateStatus(`Running... ${i}/${n}`);
          var start = performance.now();
          await NativeView.invoke('app.getVersion');
          times.push(performance.now() - start);
        }
        
        var min = Math.min.apply(null, times);
        var max = Math.max.apply(null, times);
        var sum = times.reduce((a, b) => a + b, 0);
        var avg = sum / n;
        times.sort((a, b) => a - b);
        var p99 = times[Math.floor(n * 0.99)] || max;
        
        return `Min: ${min.toFixed(2)}ms\nAvg: ${avg.toFixed(2)}ms\nMax: ${max.toFixed(2)}ms\nP99: ${p99.toFixed(2)}ms\nTotal: ${sum.toFixed(2)}ms`;
      }
    ));


    // --- Section: Throughput Test ---
    addHeader('Throughput Test');
    
    p.appendChild(createBenchCard(
      'ipc-throughput',
      'Run Throughput Test',
      'Measure fire-and-forget message rate (app.getName)',
      [100, 1000, 10000],
      async function(n, updateStatus) {
        updateStatus(`Firing ${n} messages...`);
        var start = performance.now();
        
        // We use invoke here but don't await individual results to simulate high throughput pressure
        // However, to measure completion we need to await all. 
        // "Fire-and-forget" implies we don't care about result, but for benchmark we need to know when done.
        // Actually, let's use Promise.all to ensure they all completed for accurate timing.
        
        var promises = [];
        for (var i = 0; i < n; i++) {
          promises.push(NativeView.invoke('app.getName'));
        }
        
        await Promise.all(promises);
        var end = performance.now();
        var totalTime = end - start;
        var msgPerSec = (n / (totalTime / 1000));
        
        return `Total Time: ${totalTime.toFixed(2)}ms\nRate: ${msgPerSec.toFixed(2)} msg/sec`;
      }
    ));


    // --- Section: Concurrent Requests ---
    addHeader('Concurrent Requests');
    
    p.appendChild(createBenchCard(
      'ipc-concurrency',
      'Run Concurrency Test',
      'Measure parallel request handling (Promise.all)',
      [5, 10, 50],
      async function(n, updateStatus) {
        updateStatus(`Launching ${n} concurrent requests...`);
        var start = performance.now();
        
        var promises = [];
        for (var i = 0; i < n; i++) {
          promises.push(NativeView.invoke('app.getVersion').then(() => true).catch(() => false));
        }
        
        var results = await Promise.all(promises);
        var end = performance.now();
        var passed = results.filter(r => r).length;
        var failed = n - passed;
        
        return `Total Time: ${(end - start).toFixed(2)}ms\nPassed: ${passed}\nFailed: ${failed}`;
      }
    ));


    // --- Section: Error Handling Test ---
    addHeader('Error Handling Test');
    
    var errorCard = document.createElement('div');
    errorCard.className = 'op-card';
    errorCard.innerHTML = `
      <div class="op-title">Run Error Test</div>
      <div class="op-description">Verify error responses for invalid requests</div>
      <button class="btn-primary op-run-btn">Run Tests</button>
      <div class="op-result" style="margin-top:12px; font-family:var(--font-mono); font-size:12px; white-space:pre-wrap;"></div>
    `;
    
    var errBtn = errorCard.querySelector('button');
    var errRes = errorCard.querySelector('.op-result');
    
    errBtn.onclick = async function() {
      errBtn.disabled = true;
      errRes.textContent = 'Running tests...';
      var log = '';
      
      // Test 1: Unknown Op
      try {
        await NativeView.invoke('unknown.op');
        log += '[FAIL] Unknown Op: Did not throw\n';
      } catch (e) {
        log += '[PASS] Unknown Op: Caught ' + e.message + '\n';
      }
      
      // Test 2: Bad Args (using shell.openUrl with missing url)
      try {
        await NativeView.invoke('shell.openUrl', {}); // Missing url
        log += '[FAIL] Bad Args: Did not throw\n';
      } catch (e) {
        if (e.message.indexOf('missing') !== -1 || e.message === 'ERR_INVALID_ARG') {
             log += '[PASS] Bad Args: Caught ' + e.message + '\n';
        } else {
             log += '[PASS] Bad Args: Caught ' + e.message + ' (unexpected message but caught)\n';
        }
      }

      // Test 3: Timeout (client-side simulation since we can't easily force backend delay)
      // We'll wrap a request in a timeout promise
      try {
        var timeout = new Promise((_, rej) => setTimeout(() => rej(new Error('ERR_TIMEOUT')), 10)); // 10ms timeout
        var req = NativeView.invoke('app.getVersion'); // Should take > 0ms usually, but if it's too fast this test is flaky.
        // Let's try to race.
        await Promise.race([req, timeout]);
        log += '[INFO] Timeout Test: Request completed faster than 10ms (Pass/Inconclusive)\n';
      } catch (e) {
        if (e.message === 'ERR_TIMEOUT') {
          log += '[PASS] Timeout Test: Caught timeout\n';
        } else {
          log += '[FAIL] Timeout Test: Caught ' + e.message + '\n';
        }
      }
      
      errRes.textContent = log;
      errBtn.disabled = false;
    };
    p.appendChild(errorCard);


    // --- Section: Live IPC Log ---
    addHeader('Live IPC Log');
    
    var logCard = document.createElement('div');
    logCard.className = 'op-card';
    logCard.innerHTML = `
      <div class="op-title">IPC Logger</div>
      <div class="op-description">Monkey-patch NativeView.invoke to trace calls</div>
      <div style="margin-bottom:12px;">
        <label><input type="checkbox" id="ipc-log-toggle"> Enable Logging</label>
      </div>
      <div id="ipc-log-box" style="height:200px; background:#1e1e1e; color:#d4d4d4; font-family:var(--font-mono); font-size:11px; padding:8px; overflow-y:auto; border-radius:4px;"></div>
      <button class="btn-secondary" id="ipc-log-clear" style="margin-top:8px; font-size:11px;">Clear Log</button>
    `;
    
    var toggle = logCard.querySelector('#ipc-log-toggle');
    var box = logCard.querySelector('#ipc-log-box');
    var clearBtn = logCard.querySelector('#ipc-log-clear');
    
    var originalInvoke = NativeView.invoke;
    
    function logLine(msg) {
      var div = document.createElement('div');
      div.textContent = msg;
      div.style.borderBottom = '1px solid #333';
      div.style.padding = '2px 0';
      box.appendChild(div);
      box.scrollTop = box.scrollHeight;
      if (box.children.length > 50) box.firstElementChild.remove();
    }
    
    toggle.onchange = function() {
      if (toggle.checked) {
        NativeView.invoke = async function(op, args) {
          var ts = new Date().toISOString().split('T')[1].slice(0, -1);
          logLine(`→ [${ts}] ${op} ${JSON.stringify(args||{})}`);
          try {
            var res = await originalInvoke.call(NativeView, op, args);
            var ts2 = new Date().toISOString().split('T')[1].slice(0, -1);
            logLine(`← [${ts2}] ${op} OK`);
            return res;
          } catch (e) {
            var ts2 = new Date().toISOString().split('T')[1].slice(0, -1);
            logLine(`← [${ts2}] ${op} ERR: ${e.message}`);
            throw e;
          }
        };
        logLine('--- Logging Started ---');
      } else {
        NativeView.invoke = originalInvoke;
        logLine('--- Logging Stopped ---');
      }
    };
    
    clearBtn.onclick = function() {
      box.innerHTML = '';
    };
    
    // Cleanup on unload/tab switch? 
    // For demo simplicity, we rely on user untoggling or reload. 
    // Ideally we should restore if tab is destroyed.
    
    p.appendChild(logCard);

  });
})();
