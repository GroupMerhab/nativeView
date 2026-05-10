(function() {
  DemoCore.registerTab('dialog', function() {
    var p = document.getElementById('tab-dialog');
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

    // Helper: Create a distinct visual result for user cancellation
    function formatResult(value, cancelled) {
      if (cancelled) {
        return { 
          __html: '<span style="color:var(--status-warn); font-weight:bold;">User Cancelled</span>' 
        };
      }
      return value;
    }

    // --- Section: File Dialogs ---
    addHeader('File Dialogs');

    // openFile
    p.appendChild(DemoCore.createOpCard({
      id: 'dialog-openFile',
      title: 'openFile(title, filters, multiple)',
      description: 'Open file selection dialog',
      inputs: [
        { id: 'title', label: 'Title', defaultValue: 'Open File' },
        { id: 'filters', label: 'Filters', defaultValue: 'Text Files (*.txt)|All Files (*.*)' },
        { id: 'multiple', label: 'Multiple', type: 'checkbox', defaultValue: false }
      ],
      run: async function(args) {
        var res = await NativeView.invoke('dialog.openFile', args);
        if (res.canceled) return formatResult(null, true);
        return res.paths;
      },
      customRender: function(data, el) {
        if (data && data.__html) { el.innerHTML = data.__html; return; }
        el.textContent = JSON.stringify(data, null, 2);
      }
    }));

    // saveFile
    p.appendChild(DemoCore.createOpCard({
      id: 'dialog-saveFile',
      title: 'saveFile(title, filters, defaultName)',
      description: 'Save file dialog',
      inputs: [
        { id: 'title', label: 'Title', defaultValue: 'Save File' },
        { id: 'filters', label: 'Filters', defaultValue: 'Text Files (*.txt)' },
        { id: 'defaultName', label: 'Default Name', defaultValue: 'output.txt' }
      ],
      run: async function(args) {
        var res = await NativeView.invoke('dialog.saveFile', args);
        if (!res.path) return formatResult(null, true);
        return res.path;
      },
      customRender: function(data, el) {
        if (data && data.__html) { el.innerHTML = data.__html; return; }
        el.textContent = String(data);
      }
    }));

    // openFolder
    p.appendChild(DemoCore.createOpCard({
      id: 'dialog-openFolder',
      title: 'openFolder(title)',
      description: 'Select directory dialog',
      inputs: [
        { id: 'title', label: 'Title', defaultValue: 'Choose Folder' }
      ],
      run: async function(args) {
        var res = await NativeView.invoke('dialog.openFolder', args);
        if (!res.path) return formatResult(null, true);
        return res.path;
      },
      customRender: function(data, el) {
        if (data && data.__html) { el.innerHTML = data.__html; return; }
        el.textContent = String(data);
      }
    }));


    // --- Section: Message Dialogs ---
    addHeader('Message Dialogs');

    // message
    p.appendChild(DemoCore.createOpCard({
      id: 'dialog-message',
      title: 'message(title, body, type, buttons)',
      description: 'Show message dialog',
      inputs: [
        { id: 'title', label: 'Title', defaultValue: 'Hello' },
        { id: 'body', label: 'Body', defaultValue: 'This is a NativeView message dialog.' }, // textarea simulated via text input for now as per DemoCore limit, actually DemoCore inputs are text inputs. 
        { id: 'type', label: 'Type (info/warning/error)', defaultValue: 'info' },
        { id: 'buttons', label: 'Buttons (comma-sep)', defaultValue: 'OK,Cancel' }
      ],
      run: async function(args) {
        var btns = (args.buttons || '').split(',');
        var req = {
          title: args.title,
          body: args.body,
          type: args.type,
          buttonA: btns[0] ? btns[0].trim() : 'OK',
          buttonB: btns[1] ? btns[1].trim() : null
        };
        var res = await NativeView.invoke('dialog.message', req);
        return {
          clickedButton: req[res.buttonIndex === 0 ? 'buttonA' : 'buttonB'] || 'Unknown',
          index: res.buttonIndex
        };
      }
    }));

    // confirm
    p.appendChild(DemoCore.createOpCard({
      id: 'dialog-confirm',
      title: 'confirm(title, body)',
      description: 'Confirmation dialog (Yes/No style)',
      inputs: [
        { id: 'title', label: 'Title', defaultValue: 'Confirm Action' },
        { id: 'body', label: 'Body', defaultValue: 'Do you want to proceed?' }
      ],
      run: async function(args) {
        var res = await NativeView.invoke('dialog.confirm', args);
        if (!res.confirmed) return formatResult(null, true);
        return { confirmed: true };
      },
      customRender: function(data, el) {
        if (data && data.__html) { el.innerHTML = data.__html; return; }
        el.textContent = JSON.stringify(data, null, 2);
      }
    }));


    // --- SPECIAL: Dialog Chain Demo ---
    addHeader('Dialog Chain Demo');
    
    var chainCard = document.createElement('div');
    chainCard.className = 'op-card';
    chainCard.innerHTML = `
      <div class="op-title">Run All Dialogs (Sequence)</div>
      <div class="op-description">Runs 5 dialogs in sequence. Stops if cancelled.</div>
      <div class="op-inputs"></div>
      <button class="btn-primary op-run-btn">Run Chain</button>
      <div class="op-result" style="white-space:pre-wrap; max-height:300px; overflow:auto;"></div>
    `;
    
    var chainBtn = chainCard.querySelector('button');
    var chainRes = chainCard.querySelector('.op-result');
    
    chainBtn.onclick = async function() {
      chainBtn.disabled = true;
      chainRes.textContent = 'Starting chain...\n';
      
      function log(msg) {
        chainRes.textContent += msg + '\n';
        chainRes.scrollTop = chainRes.scrollHeight;
      }

      try {
        // 1. Confirm start
        log('[1/5] Confirming start...');
        var c1 = await NativeView.invoke('dialog.confirm', { title: 'Start Chain?', body: 'Ready to run 5 dialogs?' });
        if (!c1.confirmed) { log('Users cancelled start.'); return; }
        log('-> Confirmed.');

        // 2. Open File
        log('[2/5] Pick a file...');
        var c2 = await NativeView.invoke('dialog.openFile', { title: 'Pick any file', multiple: false });
        if (c2.canceled) { log('User cancelled file pick.'); return; }
        log('-> Selected: ' + (c2.paths[0] || 'none'));

        // 3. Save File
        log('[3/5] Save a file...');
        var c3 = await NativeView.invoke('dialog.saveFile', { title: 'Save output', defaultName: 'chain.txt' });
        if (!c3.path) { log('User cancelled save.'); return; }
        log('-> Saved to: ' + c3.path);

        // 4. Message Choice
        log('[4/5] Choose an option...');
        var c4 = await NativeView.invoke('dialog.message', { 
          title: 'Choice', 
          body: 'Blue pill or Red pill?', 
          buttonA: 'Blue', 
          buttonB: 'Red' 
        });
        log('-> You chose: ' + (c4.buttonIndex === 0 ? 'Blue' : 'Red'));

        // 5. Final Confirmation
        log('[5/5] Final check...');
        var c5 = await NativeView.invoke('dialog.confirm', { title: 'Done?', body: 'Did you enjoy the chain?' });
        log('-> ' + (c5.confirmed ? 'Glad to hear it!' : 'Oh well.'));

        log('Chain complete!');
        
      } catch (err) {
        log('Error: ' + err.message);
      } finally {
        chainBtn.disabled = false;
      }
    };
    
    p.appendChild(chainCard);
  });
})();
