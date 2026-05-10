(function() {
  DemoCore.registerTab('shell', async function() {
    var p = document.getElementById('tab-shell');
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

    // --- Init: Fetch Environment Info ---
    var platform = '';
    var dataDir = '';
    var exePath = '';

    try {
      var platRes = await NativeView.invoke('app.getPlatform');
      platform = platRes.os;
      var dataRes = await NativeView.invoke('app.getDataDir');
      dataDir = dataRes.path;
      var exeRes = await NativeView.invoke('app.getExePath');
      exePath = exeRes.path;
    } catch (e) {
      console.error('Shell Init Error', e);
    }

    var isWin = (platform === 'windows');


    // --- Section: Open ---
    addHeader('Open');

    // openUrl
    p.appendChild(DemoCore.createOpCard({
      id: 'shell-openUrl',
      title: 'openUrl(url)',
      description: 'Open URL in default browser',
      inputs: [
        { id: 'url', label: 'URL', defaultValue: 'https://github.com' }
      ],
      run: async function(args) {
        await NativeView.invoke('shell.openUrl', args);
        return { message: 'Opened in browser' };
      }
    }));

    // openPath
    p.appendChild(DemoCore.createOpCard({
      id: 'shell-openPath',
      title: 'openPath(path)',
      description: 'Open file/directory in system viewer',
      inputs: [
        { id: 'path', label: 'Path', defaultValue: dataDir }
      ],
      run: async function(args) {
        await NativeView.invoke('shell.openPath', args);
        return { message: 'Opened in file manager' };
      }
    }));

    // showInFolder
    p.appendChild(DemoCore.createOpCard({
      id: 'shell-showInFolder',
      title: 'showInFolder(path)',
      description: 'Reveal file in system file manager',
      inputs: [
        { id: 'path', label: 'Path', defaultValue: exePath }
      ],
      run: async function(args) {
        await NativeView.invoke('shell.showInFolder', args);
        return { message: 'Revealed in folder' };
      }
    }));


    // --- Section: Execute ---
    addHeader('Execute');

    // Warning Banner
    var warn = document.createElement('div');
    warn.style.padding = '8px 12px';
    warn.style.marginBottom = '16px';
    warn.style.background = 'rgba(255, 193, 7, 0.1)';
    warn.style.border = '1px solid rgba(255, 193, 7, 0.3)';
    warn.style.borderRadius = '4px';
    warn.style.color = 'var(--status-warn)';
    warn.style.fontSize = '12px';
    warn.innerHTML = '<strong>Note:</strong> <code>exec</code> may fail or be restricted if <code>NV_ALLOW_SHELL_EXEC</code> is not set in build configuration.';
    p.appendChild(warn);

    // Defaults based on platform
    var defCmd = isWin ? 'cmd' : 'echo';
    var defArgs = isWin ? '/c,echo,hello nativeview' : 'hello,nativeview';
    var defCwd = isWin ? 'C:\\Temp' : '/tmp';

    // exec Op Card
    var execCard = DemoCore.createOpCard({
      id: 'shell-exec',
      title: 'exec(cmd, args, cwd)',
      description: 'Execute system command',
      inputs: [
        { id: 'cmd', label: 'Command', defaultValue: defCmd },
        { id: 'args', label: 'Args (comma-separated)', defaultValue: defArgs },
        { id: 'cwd', label: 'CWD', defaultValue: defCwd }
      ],
      run: async function(args) {
        var argList = args.args ? args.args.split(',').map(function(s){ return s.trim(); }) : [];
        var req = {
          cmd: args.cmd,
          args: argList,
          cwd: args.cwd
        };
        return NativeView.invoke('shell.exec', req);
      },
      customRender: function(data, el) {
        var pre = document.createElement('pre');
        pre.style.background = '#1e1e1e';
        pre.style.color = '#d4d4d4';
        pre.style.padding = '8px';
        pre.style.borderRadius = '4px';
        pre.style.overflowX = 'auto';
        pre.style.fontFamily = 'var(--font-mono)';
        pre.style.fontSize = '11px';
        
        var out = '';
        out += 'Exit Code: ' + data.exitCode + '\n';
        if (data.stdout) out += 'STDOUT:\n' + data.stdout + '\n';
        if (data.stderr) out += 'STDERR:\n' + data.stderr + '\n';
        
        pre.textContent = out;
        el.innerHTML = '';
        el.appendChild(pre);
      }
    });
    p.appendChild(execCard);


    // --- Quick Exec Presets ---
    var presetsDiv = document.createElement('div');
    presetsDiv.style.marginTop = '16px';
    presetsDiv.style.display = 'flex';
    presetsDiv.style.gap = '8px';
    
    var label = document.createElement('span');
    label.textContent = 'Quick Presets: ';
    label.style.alignSelf = 'center';
    label.style.fontSize = '12px';
    label.style.opacity = '0.7';
    presetsDiv.appendChild(label);

    function addPreset(name, cmd, argsStr, cwd) {
      var btn = document.createElement('button');
      btn.className = 'btn-secondary';
      btn.textContent = name;
      btn.style.fontSize = '11px';
      btn.style.padding = '4px 8px';
      btn.onclick = function() {
        // Update inputs in the exec card
        var inputs = execCard.querySelectorAll('input');
        if (inputs.length >= 3) {
          inputs[0].value = cmd;
          inputs[1].value = argsStr;
          inputs[2].value = cwd;
          // Trigger run
          execCard.querySelector('.op-run-btn').click();
        }
      };
      presetsDiv.appendChild(btn);
    }

    if (isWin) {
      addPreset('List Dir', 'cmd', '/c,dir', dataDir);
    } else {
      addPreset('List Dir', 'ls', '-la', dataDir);
    }
    
    addPreset('Echo Test', isWin ? 'cmd' : 'echo', isWin ? '/c,echo,NativeView Rocks' : 'NativeView Rocks', isWin ? 'C:\\' : '/');

    p.appendChild(presetsDiv);

  });
})();
