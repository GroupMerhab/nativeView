(function() {
  DemoCore.registerTab('fs', async function() {
    var p = document.getElementById('tab-fs');
    if (!p) return;

    // --- Setup: Test Directory ---
    var dataDir = (await NativeView.invoke('app.getDataDir')).path;
    var testDir = dataDir + (dataDir.endsWith('/') ? '' : '/') + 'nv_demo_test/';
    
    // Banner
    var banner = document.createElement('div');
    banner.style.padding = '12px';
    banner.style.marginBottom = '16px';
    banner.style.background = 'var(--bg-panel)';
    banner.style.border = '1px solid var(--border)';
    banner.style.borderRadius = '6px';
    banner.style.fontFamily = 'var(--font-mono)';
    banner.innerHTML = 'Test Directory: <strong>' + testDir + '</strong> <button class="btn-secondary" style="font-size:10px; padding:2px 6px; margin-left:8px">Open</button>';
    banner.querySelector('button').onclick = function() {
      NativeView.invoke('shell.showInFolder', { path: testDir });
    };
    p.appendChild(banner);

    // Initial Setup Sequence
    try {
      await NativeView.invoke('fs.mkdir', { path: testDir }); // No recursive flag in C, but good practice to try
      await NativeView.invoke('fs.writeText', { path: testDir + 'hello.txt', text: 'Hello NativeView!' });
    } catch (e) {
      console.error('FS Setup failed', e);
    }

    // Helper: Add section header
    function addHeader(text) {
      var h = document.createElement('h3');
      h.textContent = text;
      h.style.margin = '24px 0 12px';
      h.style.borderBottom = '1px solid var(--border)';
      h.style.paddingBottom = '4px';
      p.appendChild(h);
    }

    // Helper: Render path link
    function renderPathLink(path, container) {
      var link = document.createElement('a');
      link.href = '#';
      link.textContent = path;
      link.style.color = 'var(--accent)';
      link.onclick = function(e) {
        e.preventDefault();
        NativeView.invoke('shell.showInFolder', { path: path });
      };
      container.appendChild(link);
    }

    // --- Section: Read ---
    addHeader('Read');

    // readText
    p.appendChild(DemoCore.createOpCard({
      id: 'fs-readText',
      title: 'readText(path)',
      description: 'Read text file contents',
      inputs: [{ id: 'path', label: 'Path', defaultValue: testDir + 'hello.txt' }],
      run: async function(args) {
        return NativeView.invoke('fs.readText', args);
      }
    }));

    // readBinary
    p.appendChild(DemoCore.createOpCard({
      id: 'fs-readBinary',
      title: 'readBinary(path)',
      description: 'Read binary file (shows base64 length)',
      inputs: [{ id: 'path', label: 'Path', defaultValue: testDir + 'hello.txt' }],
      run: async function(args) {
        var res = await NativeView.invoke('fs.readBinary', args);
        return { length: res.bytes ? res.bytes.length : 0 };
      }
    }));

    // exists
    p.appendChild(DemoCore.createOpCard({
      id: 'fs-exists',
      title: 'exists(path)',
      description: 'Check if file exists',
      inputs: [{ id: 'path', label: 'Path', defaultValue: testDir + 'hello.txt' }],
      run: async function(args) {
        return NativeView.invoke('fs.exists', args);
      }
    }));

    // stat
    p.appendChild(DemoCore.createOpCard({
      id: 'fs-stat',
      title: 'stat(path)',
      description: 'Get file status',
      inputs: [{ id: 'path', label: 'Path', defaultValue: testDir + 'hello.txt' }],
      run: async function(args) {
        return NativeView.invoke('fs.stat', args);
      }
    }));

    // readDir
    p.appendChild(DemoCore.createOpCard({
      id: 'fs-readDir',
      title: 'readDir(path)',
      description: 'List directory entries',
      inputs: [{ id: 'path', label: 'Path', defaultValue: testDir }],
      run: async function(args) {
        return NativeView.invoke('fs.readDir', args);
      },
      customRender: function(data, el) {
        if (!data.entries || !data.entries.length) {
          el.textContent = 'Empty directory';
          return;
        }
        var ul = document.createElement('ul');
        ul.style.listStyle = 'none';
        ul.style.padding = '0';
        data.entries.forEach(function(e) {
          var li = document.createElement('li');
          var fullPath = testDir + (testDir.endsWith('/')?'':'/') + e.name;
          
          var a = document.createElement('a');
          a.href = '#';
          a.textContent = e.name + (e.isDir ? '/' : '');
          a.style.color = 'var(--accent)';
          a.style.textDecoration = 'none';
          a.onclick = function(evt) {
            evt.preventDefault();
            NativeView.invoke('shell.showInFolder', { path: fullPath });
          };
          li.appendChild(a);
          ul.appendChild(li);
        });
        el.appendChild(ul);
      }
    }));

    // --- Section: Write ---
    addHeader('Write');

    // writeText
    p.appendChild(DemoCore.createOpCard({
      id: 'fs-writeText',
      title: 'writeText(path, text)',
      description: 'Write text to file',
      inputs: [
        { id: 'path', label: 'Path', defaultValue: testDir + 'hello.txt' },
        { id: 'text', label: 'Text', defaultValue: 'Hello NativeView!' }
      ],
      run: async function(args) {
        return NativeView.invoke('fs.writeText', args);
      }
    }));

    // writeBinary
    p.appendChild(DemoCore.createOpCard({
      id: 'fs-writeBinary',
      title: 'writeBinary(path, b64)',
      description: 'Write base64 data (simulated)',
      inputs: [
        { id: 'path', label: 'Path', defaultValue: testDir + 'binary.bin' },
        { id: 'b64', label: 'Base64', defaultValue: btoa('binary test') }
      ],
      run: async function(args) {
        // Encode if needed, but input is already expected to be b64 or text
        // Demo: just pass it through
        return NativeView.invoke('fs.writeBinary', args);
      }
    }));

    // mkdir
    p.appendChild(DemoCore.createOpCard({
      id: 'fs-mkdir',
      title: 'mkdir(path)',
      description: 'Create directory',
      inputs: [
        { id: 'path', label: 'Path', defaultValue: testDir + 'subdir' },
        { id: 'recursive', label: 'Recursive', type: 'checkbox', defaultValue: true }
      ],
      run: async function(args) {
        // Native mkdir might not support recursive, so we might need manual logic or just try
        return NativeView.invoke('fs.mkdir', { path: args.path });
      }
    }));

    // move
    p.appendChild(DemoCore.createOpCard({
      id: 'fs-move',
      title: 'move(src, dst)',
      description: 'Move/Rename file',
      inputs: [
        { id: 'src', label: 'Source', defaultValue: testDir + 'hello.txt' },
        { id: 'dst', label: 'Dest', defaultValue: testDir + 'hello_moved.txt' }
      ],
      run: async function(args) {
        return NativeView.invoke('fs.move', args);
      }
    }));

    // copy
    p.appendChild(DemoCore.createOpCard({
      id: 'fs-copy',
      title: 'copy(src, dst)',
      description: 'Copy file',
      inputs: [
        { id: 'src', label: 'Source', defaultValue: testDir + 'hello.txt' },
        { id: 'dst', label: 'Dest', defaultValue: testDir + 'hello_copy.txt' }
      ],
      run: async function(args) {
        return NativeView.invoke('fs.copy', args);
      }
    }));

    // remove
    var removeCard = DemoCore.createOpCard({
      id: 'fs-remove',
      title: 'remove(path)',
      description: 'Delete file',
      inputs: [{ id: 'path', label: 'Path', defaultValue: testDir + 'hello.txt' }],
      run: async function(args) {
        return NativeView.invoke('fs.remove', args);
      }
    });
    var rmBtn = removeCard.querySelector('.op-run-btn');
    if (rmBtn) rmBtn.className = 'btn-danger op-run-btn';
    p.appendChild(removeCard);


    // --- Section: Watch ---
    addHeader('Watch');

    // watch
    p.appendChild(DemoCore.createOpCard({
      id: 'fs-watch',
      title: 'watch(path, id)',
      description: 'Watch directory for changes',
      inputs: [
        { id: 'path', label: 'Path', defaultValue: testDir },
        { id: 'id', label: 'ID', type: 'number', defaultValue: 1 }
      ],
      run: async function(args) {
        // Create a push card for this watch if not exists
        var pushId = 'fs-watch-' + args.id;
        if (!document.getElementById(pushId)) {
          var pc = DemoCore.createPushCard({
            id: pushId,
            title: 'fs.changed (ID ' + args.id + ')',
            event: 'fs.changed',
            filter: function(evt) { return evt.id == args.id; }
          });
          p.insertBefore(pc, removeCard.nextSibling); // Insert somewhere visible
        }
        return NativeView.invoke('fs.watch', args);
      }
    }));

    // unwatch
    p.appendChild(DemoCore.createOpCard({
      id: 'fs-unwatch',
      title: 'unwatch(id)',
      description: 'Stop watching',
      inputs: [{ id: 'id', label: 'ID', type: 'number', defaultValue: 1 }],
      run: async function(args) {
        return NativeView.invoke('fs.unwatch', args);
      }
    }));


    // --- Section: Cleanup ---
    addHeader('Cleanup');

    var rmdirCard = DemoCore.createOpCard({
      id: 'fs-rmdir',
      title: 'rmdir(path, recursive)',
      description: 'Remove directory (Recursive)',
      inputs: [
        { id: 'path', label: 'Path', defaultValue: testDir },
        { id: 'recursive', label: 'Recursive', type: 'checkbox', defaultValue: true }
      ],
      run: async function(args) {
        if (!args.recursive) {
          return NativeView.invoke('fs.rmdir', { path: args.path });
        }
        
        // Recursive delete helper
        async function rimraf(dir) {
          var st = await NativeView.invoke('fs.stat', { path: dir });
          if (!st.isDir) {
            return NativeView.invoke('fs.remove', { path: dir });
          }
          var list = await NativeView.invoke('fs.readDir', { path: dir });
          if (list.entries) {
            for (var i = 0; i < list.entries.length; i++) {
              var name = list.entries[i].name;
              await rimraf(dir + (dir.endsWith('/')?'':'/') + name);
            }
          }
          return NativeView.invoke('fs.rmdir', { path: dir });
        }
        
        return rimraf(args.path);
      }
    });
    var rmdirBtn = rmdirCard.querySelector('.op-run-btn');
    if (rmdirBtn) rmdirBtn.className = 'btn-danger op-run-btn';
    p.appendChild(rmdirCard);

  });
})();
