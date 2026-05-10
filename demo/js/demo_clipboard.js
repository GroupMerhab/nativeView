(function() {
  DemoCore.registerTab('clipboard', function() {
    var p = document.getElementById('tab-clipboard');
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

    // --- Section: Text ---
    addHeader('Text');

    // hasText
    var hasTextCard = DemoCore.createOpCard({
      id: 'clipboard-hasText',
      title: 'hasText()',
      description: 'Check if clipboard has text',
      inputs: [],
      run: async function() {
        return NativeView.invoke('clipboard.hasText');
      }
    });
    p.appendChild(hasTextCard);

    // readText
    p.appendChild(DemoCore.createOpCard({
      id: 'clipboard-readText',
      title: 'readText()',
      description: 'Read text content',
      inputs: [],
      run: async function() {
        var res = await NativeView.invoke('clipboard.readText');
        var text = res.text || '';
        if (text.length > 500) {
          return { 
            truncated: true, 
            length: text.length, 
            preview: text.substring(0, 500) + '...' 
          };
        }
        return { text: text };
      }
    }));

    // writeText
    p.appendChild(DemoCore.createOpCard({
      id: 'clipboard-writeText',
      title: 'writeText(text)',
      description: 'Write text to clipboard',
      inputs: [
        { id: 'text', label: 'Text', type: 'textarea', defaultValue: 'Hello from NativeView clipboard!' }
      ],
      run: async function(args) {
        await NativeView.invoke('clipboard.writeText', args);
        // Auto-refresh check
        setTimeout(function(){ hasTextCard.querySelector('.op-run-btn').click(); }, 100);
        return { success: true };
      }
    }));
    
    // Auto-run hasText initially
    setTimeout(function(){ hasTextCard.querySelector('.op-run-btn').click(); }, 50);


    // --- Section: Image ---
    addHeader('Image');

    // hasImage
    var hasImageCard = DemoCore.createOpCard({
      id: 'clipboard-hasImage',
      title: 'hasImage()',
      description: 'Check if clipboard has image',
      inputs: [],
      run: async function() {
        return NativeView.invoke('clipboard.hasImage');
      }
    });
    p.appendChild(hasImageCard);

    // readImage
    p.appendChild(DemoCore.createOpCard({
      id: 'clipboard-readImage',
      title: 'readImage()',
      description: 'Read image content',
      inputs: [],
      run: async function() {
        var res = await NativeView.invoke('clipboard.readImage');
        return res; // expect { data: "base64...", width, height, format: 'png' }
      },
      customRender: function(data, el) {
        if (!data || !data.data) {
          el.textContent = 'No image data';
          return;
        }
        var info = document.createElement('div');
        info.textContent = 'Size: ' + (data.width||'?') + 'x' + (data.height||'?') + ' | Base64 Length: ' + data.data.length;
        info.style.marginBottom = '8px';
        
        var img = document.createElement('img');
        img.src = 'data:image/png;base64,' + data.data;
        img.style.maxWidth = '200px';
        img.style.maxHeight = '200px';
        img.style.border = '1px solid var(--border)';
        img.style.background = '#checkerboard'; // CSS usually handles this or just white
        
        el.innerHTML = '';
        el.appendChild(info);
        el.appendChild(img);
      }
    }));

    // writeImage
    // 1x1 Red Pixel PNG
    var redPixel = 'iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8BQDwAEhQGAhKmMIQAAAABJRU5ErkJggg==';
    
    p.appendChild(DemoCore.createOpCard({
      id: 'clipboard-writeImage',
      title: 'writeImage({ data })',
      description: 'Write 1x1 Red Pixel PNG',
      inputs: [
        { id: 'data', label: 'Base64 PNG', type: 'textarea', defaultValue: redPixel }
      ],
      run: async function(args) {
        await NativeView.invoke('clipboard.writeImage', { format: 'png', data: args.data });
        // Auto-refresh check
        setTimeout(function(){ hasImageCard.querySelector('.op-run-btn').click(); }, 100);
        return { success: true };
      }
    }));

    // Auto-run hasImage initially
    setTimeout(function(){ hasImageCard.querySelector('.op-run-btn').click(); }, 50);


    // --- Section: Utility ---
    addHeader('Utility');

    p.appendChild(DemoCore.createOpCard({
      id: 'clipboard-clear',
      title: 'clear()',
      description: 'Clear clipboard content',
      inputs: [],
      run: async function() {
        await NativeView.invoke('clipboard.clear');
        // Auto-refresh checks
        setTimeout(function(){ 
          hasTextCard.querySelector('.op-run-btn').click(); 
          hasImageCard.querySelector('.op-run-btn').click(); 
        }, 100);
        return { cleared: true };
      }
    }));


    // --- CLIPBOARD ROUNDTRIP TEST ---
    addHeader('Clipboard Roundtrip Test');

    var rtCard = document.createElement('div');
    rtCard.className = 'op-card';
    rtCard.innerHTML = `
      <div class="op-title">Roundtrip Verify</div>
      <div class="op-description">Write timestamp -> Read back -> Verify match</div>
      <div class="op-inputs"></div>
      <button class="btn-primary op-run-btn">Run Test</button>
      <div class="op-badge pending">PENDING</div>
      <div class="op-result"></div>
    `;
    
    var rtBtn = rtCard.querySelector('button');
    var rtBadge = rtCard.querySelector('.op-badge');
    var rtRes = rtCard.querySelector('.op-result');
    
    rtBtn.onclick = async function() {
      rtBtn.disabled = true;
      rtBadge.className = 'badge op-badge running';
      rtBadge.textContent = 'RUNNING';
      rtRes.textContent = 'Starting...';
      
      try {
        var ts = 'NV_TEST_' + Date.now() + '_' + Math.random();
        
        // 1. Write
        await NativeView.invoke('clipboard.writeText', { text: ts });
        
        // 2. Read
        var res = await NativeView.invoke('clipboard.readText');
        var readBack = res.text;
        
        // 3. Verify
        if (readBack === ts) {
          rtBadge.className = 'badge op-badge pass';
          rtBadge.textContent = 'PASS';
          rtRes.textContent = 'Match confirmed: ' + ts;
          DemoCore.recordPass('clipboard-roundtrip');
        } else {
          throw new Error('Mismatch! Expected: "' + ts + '", Got: "' + readBack + '"');
        }
      } catch (err) {
        rtBadge.className = 'badge op-badge fail';
        rtBadge.textContent = 'FAIL';
        rtRes.textContent = err.message;
        DemoCore.recordFail('clipboard-roundtrip', err);
      } finally {
        rtBtn.disabled = false;
      }
    };
    
    p.appendChild(rtCard);

  });
})();
