(function() {
  DemoCore.registerTab('notification', async function() {
    var p = document.getElementById('tab-notification');
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

    // Default icon path
    var exePath = '';
    try {
      var exeRes = await NativeView.invoke('app.getExePath');
      // Assume icon is in ../demo/assets relative to exe or just use empty if not found
      // For demo purposes, we'll just put a placeholder or use the exe itself if platform supports
      exePath = exeRes.path;
    } catch(e) {}
    
    // --- Section: Send Notifications ---
    addHeader('Send Notifications');

    // show
    p.appendChild(DemoCore.createOpCard({
      id: 'notification-show',
      title: 'show(id, title, body, icon)',
      description: 'Show a notification',
      inputs: [
        { id: 'id', label: 'ID', defaultValue: 'demo-notif-1' },
        { id: 'title', label: 'Title', defaultValue: 'NativeView Demo' },
        { id: 'body', label: 'Body', type: 'textarea', defaultValue: 'Hello from NativeView notifications!' },
        { id: 'icon', label: 'Icon Path', defaultValue: '' }
      ],
      run: async function(args) {
        await NativeView.invoke('notification.show', args);
        return { message: 'Notification shown' };
      }
    }));

    // close
    p.appendChild(DemoCore.createOpCard({
      id: 'notification-close',
      title: 'close(id)',
      description: 'Close a notification programmatically',
      inputs: [
        { id: 'id', label: 'ID', defaultValue: 'demo-notif-1' }
      ],
      run: async function(args) {
        await NativeView.invoke('notification.close', args);
        return { message: 'Close command sent' };
      }
    }));


    // --- Section: Push Events ---
    addHeader('Push Events');

    // notification.clicked
    p.appendChild(DemoCore.createPushCard({
      id: 'notification-clicked',
      title: 'Event: notification.clicked',
      description: 'Triggered when user clicks a notification',
      event: 'notification.clicked',
      onEvent: function(payload) {
        var ts = new Date().toLocaleTimeString();
        return { 
          text: '[' + ts + '] Clicked: ' + (payload.id || 'unknown'),
          raw: payload 
        };
      }
    }));

    // notification.closed
    p.appendChild(DemoCore.createPushCard({
      id: 'notification-closed',
      title: 'Event: notification.closed',
      description: 'Triggered when notification is closed (by user or system)',
      event: 'notification.closed',
      onEvent: function(payload) {
        var ts = new Date().toLocaleTimeString();
        return { 
          text: '[' + ts + '] Closed: ' + (payload.id || 'unknown'),
          raw: payload 
        };
      }
    }));


    // --- Section: Sequence Demo ---
    addHeader('Sequence Demo');

    var seqCard = document.createElement('div');
    seqCard.className = 'op-card';
    seqCard.innerHTML = `
      <div class="op-title">Fire 3 Notifications</div>
      <div class="op-description">Sequence: demo-1, demo-2, demo-3 (500ms delay)</div>
      <div class="op-inputs"></div>
      <button class="btn-primary op-run-btn">Fire Sequence</button>
      <div class="op-result"></div>
    `;
    
    var seqBtn = seqCard.querySelector('button');
    var seqRes = seqCard.querySelector('.op-result');
    
    seqBtn.onclick = async function() {
      seqBtn.disabled = true;
      seqRes.textContent = 'Starting sequence...';
      
      try {
        // 1
        await NativeView.invoke('notification.show', {
          id: 'demo-1',
          title: 'Sequence #1',
          body: 'First notification'
        });
        seqRes.textContent += '\nSent #1';
        
        await new Promise(r => setTimeout(r, 500));
        
        // 2
        await NativeView.invoke('notification.show', {
          id: 'demo-2',
          title: 'Sequence #2',
          body: 'Second notification'
        });
        seqRes.textContent += '\nSent #2';

        await new Promise(r => setTimeout(r, 500));

        // 3
        await NativeView.invoke('notification.show', {
          id: 'demo-3',
          title: 'Sequence #3',
          body: 'Third notification'
        });
        seqRes.textContent += '\nSent #3 (Done)';

      } catch (e) {
        seqRes.textContent += '\nError: ' + e.message;
      } finally {
        seqBtn.disabled = false;
      }
    };
    
    p.appendChild(seqCard);

  });
})();
