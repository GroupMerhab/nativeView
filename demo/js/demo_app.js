(function() {
  DemoCore.registerTab('app', function() {
    var p = document.getElementById('tab-app');
    if (!p) return;

    // Helper: Add section header
    function addHeader(text) {
      var h = document.createElement('h3');
      h.textContent = text;
      h.style.margin = '16px 0 8px';
      h.style.borderBottom = '1px solid var(--border)';
      h.style.paddingBottom = '4px';
      p.appendChild(h);
    }

    // --- Section: App Info ---
    addHeader('App Info');

    var infoOps = [
      { id: 'app-getVersion', title: 'getVersion', desc: 'Get application version', op: 'app.getVersion' },
      { id: 'app-getName', title: 'getName', desc: 'Get application name', op: 'app.getName' },
      { id: 'app-getPlatform', title: 'getPlatform', desc: 'Get current platform (mac/win/linux)', op: 'app.getPlatform' }
    ];

    infoOps.forEach(function(item) {
      var card = DemoCore.createOpCard({
        id: item.id,
        title: item.title,
        description: item.desc,
        inputs: [],
        run: function() {
          return NativeView.invoke(item.op);
        }
      });
      p.appendChild(card);
      
      // Auto-run immediately
      setTimeout(function() {
        var btn = card.querySelector('.op-run-btn');
        if (btn) btn.click();
      }, 10);
    });

    // --- Section: Actions ---
    addHeader('Actions');

    var quitCard = DemoCore.createOpCard({
      id: 'app-quit',
      title: 'quit',
      description: 'Quit the application safely',
      inputs: [],
      run: async function() {
        // 1. Ask for confirmation
        var result = await NativeView.invoke('dialog.confirm', {
          title: 'Quit Demo?',
          body: 'Are you sure? This will close the demo.'
        });
        
        // 2. Check result
        if (result && result.confirmed) {
          return NativeView.invoke('app.quit');
        } else {
          throw new Error('User cancelled');
        }
      }
    });

    // Customize the button style to be red/danger
    var btn = quitCard.querySelector('.op-run-btn');
    if (btn) {
      btn.className = 'btn-danger op-run-btn';
      btn.textContent = 'Quit App';
    }

    p.appendChild(quitCard);
  });
})();
