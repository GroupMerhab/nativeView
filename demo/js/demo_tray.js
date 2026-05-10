(function() {
  DemoCore.registerTab('tray', async function() {
    var p = document.getElementById('tab-tray');
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

    // Default icon path logic
    var exePath = '';
    try {
      var exeRes = await NativeView.invoke('app.getExePath');
      exePath = exeRes.path;
    } catch(e) {}
    
    // --- Section: Lifecycle ---
    addHeader('Lifecycle');

    // create
    p.appendChild(DemoCore.createOpCard({
      id: 'tray-create',
      title: 'create(id, icon, tooltip)',
      description: 'Create a system tray icon',
      inputs: [
        { id: 'id', label: 'ID', defaultValue: 'demo-tray' },
        { id: 'icon', label: 'Icon Path', defaultValue: '' }, // Should be icon_16.png ideally
        { id: 'tooltip', label: 'Tooltip', defaultValue: 'NativeView Demo' }
      ],
      run: async function(args) {
        await NativeView.invoke('tray.create', args);
        return { message: 'Tray icon created' };
      }
    }));

    // destroy
    var destroyCard = DemoCore.createOpCard({
      id: 'tray-destroy',
      title: 'destroy(id)',
      description: 'Remove tray icon',
      inputs: [
        { id: 'id', label: 'ID', defaultValue: 'demo-tray' }
      ],
      run: async function(args) {
        await NativeView.invoke('tray.destroy', args);
        return { message: 'Tray icon destroyed' };
      }
    });
    // Style the button as danger
    destroyCard.querySelector('.op-run-btn').classList.add('btn-danger');
    p.appendChild(destroyCard);


    // --- Section: Modify ---
    addHeader('Modify');

    // setIcon
    p.appendChild(DemoCore.createOpCard({
      id: 'tray-setIcon',
      title: 'setIcon(id, icon)',
      description: 'Update tray icon',
      inputs: [
        { id: 'id', label: 'ID', defaultValue: 'demo-tray' },
        { id: 'icon', label: 'New Icon Path', defaultValue: '' }
      ],
      run: async function(args) {
        await NativeView.invoke('tray.setIcon', args);
        return { message: 'Icon updated' };
      }
    }));

    // setTooltip
    p.appendChild(DemoCore.createOpCard({
      id: 'tray-setTooltip',
      title: 'setTooltip(id, tooltip)',
      description: 'Update tooltip text',
      inputs: [
        { id: 'id', label: 'ID', defaultValue: 'demo-tray' },
        { id: 'tooltip', label: 'New Tooltip', defaultValue: 'Updated tooltip' }
      ],
      run: async function(args) {
        await NativeView.invoke('tray.setTooltip', args);
        return { message: 'Tooltip updated' };
      }
    }));

    // setMenu (Custom Editor)
    var menuCard = document.createElement('div');
    menuCard.className = 'op-card';
    menuCard.innerHTML = `
      <div class="op-title">setMenu(id, items)</div>
      <div class="op-description">Define context menu items</div>
      
      <div style="margin-bottom:8px; display:flex; gap:8px;">
        <label>Tray ID: <input type="text" id="tray-menu-id" value="demo-tray" style="width:100px; padding:4px;"></label>
      </div>

      <div id="tray-menu-list" style="border:1px solid var(--border); padding:8px; max-height:200px; overflow:auto; margin-bottom:8px;">
        <!-- Rows go here -->
      </div>
      
      <div style="display:flex; gap:8px; margin-bottom:8px;">
        <button id="tray-menu-add" class="btn-secondary" style="font-size:11px;">+ Add Item</button>
        <button id="tray-menu-apply" class="btn-primary" style="flex:1;">Apply Menu</button>
      </div>
      
      <div class="op-result"></div>
    `;

    var menuList = menuCard.querySelector('#tray-menu-list');
    var btnAdd = menuCard.querySelector('#tray-menu-add');
    var btnApply = menuCard.querySelector('#tray-menu-apply');
    var menuRes = menuCard.querySelector('.op-result');
    var inputId = menuCard.querySelector('#tray-menu-id');

    // Helper: Create menu item row
    function addRow(data) {
      data = data || { id: 'item-'+Date.now(), label: 'Item', enabled: true, separator: false };
      var row = document.createElement('div');
      row.style.display = 'flex';
      row.style.gap = '4px';
      row.style.marginBottom = '4px';
      row.style.alignItems = 'center';
      
      row.innerHTML = `
        <input type="text" class="row-id" placeholder="ID" value="${data.id}" style="width:60px; font-size:11px;">
        <input type="text" class="row-label" placeholder="Label" value="${data.label}" style="flex:1; font-size:11px;">
        <label style="font-size:10px;"><input type="checkbox" class="row-en" ${data.enabled?'checked':''}> En</label>
        <label style="font-size:10px;"><input type="checkbox" class="row-sep" ${data.separator?'checked':''}> Sep</label>
        <button class="btn-danger row-del" style="padding:0 4px; font-size:10px;">X</button>
      `;
      
      row.querySelector('.row-del').onclick = function() { row.remove(); };
      menuList.appendChild(row);
    }

    // Default items
    addRow({ id: 'open', label: 'Open Demo', enabled: true, separator: false });
    addRow({ id: 'sep1', label: '', enabled: true, separator: true });
    addRow({ id: 'quit', label: 'Quit', enabled: true, separator: false });

    btnAdd.onclick = function() { addRow(); };

    btnApply.onclick = async function() {
      btnApply.disabled = true;
      try {
        var items = [];
        var rows = menuList.children;
        for (var i=0; i<rows.length; i++) {
          var r = rows[i];
          items.push({
            id: r.querySelector('.row-id').value,
            label: r.querySelector('.row-label').value,
            enabled: r.querySelector('.row-en').checked,
            separator: r.querySelector('.row-sep').checked
          });
        }
        
        var id = inputId.value;
        await NativeView.invoke('tray.setMenu', { id: id, items: items });
        menuRes.textContent = 'Menu updated with ' + items.length + ' items';
        menuRes.style.color = 'var(--accent)';
      } catch (e) {
        menuRes.textContent = 'Error: ' + e.message;
        menuRes.style.color = 'var(--status-warn)';
      } finally {
        btnApply.disabled = false;
      }
    };
    
    p.appendChild(menuCard);


    // --- Section: Push Events ---
    addHeader('Push Events');

    // tray.clicked
    p.appendChild(DemoCore.createPushCard({
      id: 'tray-clicked',
      title: 'Event: tray.clicked',
      description: 'Triggered when tray icon is clicked',
      event: 'tray.clicked',
      onEvent: function(payload) {
        var ts = new Date().toLocaleTimeString();
        return { 
          text: '[' + ts + '] Clicked: ' + (payload.id || 'unknown'),
          raw: payload 
        };
      }
    }));

    // tray.menuAction
    p.appendChild(DemoCore.createPushCard({
      id: 'tray-menuAction',
      title: 'Event: tray.menuAction',
      description: 'Triggered when menu item is selected',
      event: 'tray.menuAction',
      onEvent: function(payload) {
        var ts = new Date().toLocaleTimeString();
        return { 
          text: '[' + ts + '] Selected: ' + (payload.itemId || 'unknown'),
          raw: payload 
        };
      }
    }));


    // --- Section: Full Demo ---
    addHeader('Full Demo');

    var fullCard = document.createElement('div');
    fullCard.className = 'op-card';
    fullCard.innerHTML = `
      <div class="op-title">Setup Demo Tray</div>
      <div class="op-description">Reset and create standard demo tray</div>
      <div class="op-inputs"></div>
      <button class="btn-primary op-run-btn">Setup Tray</button>
      <div class="op-result"></div>
    `;
    
    var fullBtn = fullCard.querySelector('button');
    var fullRes = fullCard.querySelector('.op-result');
    
    fullBtn.onclick = async function() {
      fullBtn.disabled = true;
      fullRes.textContent = 'Setting up...';
      
      try {
        var id = 'demo-tray';
        // 1. Destroy existing
        try { await NativeView.invoke('tray.destroy', { id: id }); } catch(e){}
        
        // 2. Create
        await NativeView.invoke('tray.create', { 
          id: id, 
          tooltip: 'NativeView Demo',
          icon: '' // Platform default or placeholder
        });
        
        // 3. Set Menu
        var items = [
          { id: 'open', label: 'Open Demo', enabled: true },
          { id: 'sep1', separator: true },
          { id: 'quit', label: 'Quit', enabled: true }
        ];
        await NativeView.invoke('tray.setMenu', { id: id, items: items });
        
        fullRes.innerHTML = '<span style="color:var(--status-pass)">Success!</span><br>Look for the tray icon in your system tray.';
      } catch (e) {
        fullRes.textContent = 'Error: ' + e.message;
      } finally {
        fullBtn.disabled = false;
      }
    };
    
    p.appendChild(fullCard);

  });
})();
