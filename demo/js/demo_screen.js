(function() {
  DemoCore.registerTab('screen', function() {
    var p = document.getElementById('tab-screen');
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

    // Helper: Draw Displays on Canvas
    function drawDisplays(displays, canvas) {
      if (!displays) return;
      if (!Array.isArray(displays)) displays = [displays];
      
      var ctx = canvas.getContext('2d');
      var w = canvas.width;
      var h = canvas.height;
      ctx.clearRect(0, 0, w, h);
      
      // Calculate bounding box of all displays
      var minX = Infinity, minY = Infinity, maxX = -Infinity, maxY = -Infinity;
      displays.forEach(function(d) {
        var b = d.bounds || {x:0, y:0, width:1920, height:1080}; // fallback
        if (b.x < minX) minX = b.x;
        if (b.y < minY) minY = b.y;
        if (b.x + b.width > maxX) maxX = b.x + b.width;
        if (b.y + b.height > maxY) maxY = b.y + b.height;
      });
      
      // Add margin
      var margin = 20;
      var contentW = maxX - minX;
      var contentH = maxY - minY;
      
      // Scaling
      var scaleX = (w - margin*2) / contentW;
      var scaleY = (h - margin*2) / contentH;
      var scale = Math.min(scaleX, scaleY);
      
      // Center
      var offsetX = (w - (contentW * scale)) / 2 - (minX * scale);
      var offsetY = (h - (contentH * scale)) / 2 - (minY * scale);
      
      displays.forEach(function(d, i) {
        var b = d.bounds || {x:0, y:0, width:1920, height:1080};
        var wa = d.workArea || b;
        
        var dx = b.x * scale + offsetX;
        var dy = b.y * scale + offsetY;
        var dw = b.width * scale;
        var dh = b.height * scale;
        
        // Draw Monitor
        ctx.fillStyle = '#333';
        ctx.strokeStyle = '#888';
        ctx.lineWidth = 2;
        ctx.fillRect(dx, dy, dw, dh);
        ctx.strokeRect(dx, dy, dw, dh);
        
        // Draw Work Area (lighter)
        var wx = wa.x * scale + offsetX;
        var wy = wa.y * scale + offsetY;
        var ww = wa.width * scale;
        var wh = wa.height * scale;
        
        ctx.fillStyle = 'rgba(100, 149, 237, 0.3)'; // CornflowerBlue
        ctx.fillRect(wx, wy, ww, wh);
        
        // Label
        ctx.fillStyle = '#fff';
        ctx.font = '12px sans-serif';
        ctx.fillText('ID: ' + d.id, dx + 5, dy + 15);
        ctx.fillText((d.isPrimary ? '[Primary] ' : '') + b.width + 'x' + b.height, dx + 5, dy + 30);
        ctx.fillText('Scale: ' + d.scaleFactor, dx + 5, dy + 45);
      });
    }

    // --- Section: Displays ---
    addHeader('Displays');

    var canvas = document.createElement('canvas');
    canvas.width = 400;
    canvas.height = 200;
    canvas.style.background = '#1e1e1e';
    canvas.style.border = '1px solid var(--border)';
    canvas.style.marginBottom = '16px';
    canvas.style.display = 'block';
    p.appendChild(canvas);

    // getPrimary
    var primaryCard = DemoCore.createOpCard({
      id: 'screen-getPrimary',
      title: 'getPrimary()',
      description: 'Get primary display info',
      inputs: [],
      run: async function() {
        var res = await NativeView.invoke('screen.getPrimary');
        drawDisplays([res], canvas);
        return res;
      }
    });
    p.appendChild(primaryCard);

    // getAll
    var allCard = DemoCore.createOpCard({
      id: 'screen-getAll',
      title: 'getAll()',
      description: 'Get all connected displays',
      inputs: [],
      run: async function() {
        var res = await NativeView.invoke('screen.getAll');
        drawDisplays(res.displays || [], canvas);
        return res;
      }
    });
    p.appendChild(allCard);

    // Auto-run getAll (shows more than getPrimary)
    setTimeout(function(){ allCard.querySelector('.op-run-btn').click(); }, 100);


    // --- Section: Cursor ---
    addHeader('Cursor');

    var cursorCard = document.createElement('div');
    cursorCard.className = 'op-card';
    cursorCard.innerHTML = `
      <div class="op-title">getCursorPos()</div>
      <div class="op-description">Get global cursor position</div>
      <div class="op-inputs">
        <label style="display:inline-flex; align-items:center; gap:8px; cursor:pointer;">
          <input type="checkbox" id="screen-track-cursor"> Track Cursor (Poll 250ms)
        </label>
      </div>
      <div class="op-result" style="font-size:16px; font-weight:bold; font-family:var(--font-mono);">
        Waiting...
      </div>
    `;
    
    var trackCheck = cursorCard.querySelector('input');
    var resultEl = cursorCard.querySelector('.op-result');
    var timer = null;

    async function updateCursor() {
      try {
        var res = await NativeView.invoke('screen.getCursorPos');
        resultEl.textContent = 'X: ' + res.x + '  Y: ' + res.y;
      } catch (e) {
        resultEl.textContent = 'Error: ' + e.message;
      }
    }

    // Initial check
    updateCursor();

    trackCheck.onchange = function() {
      if (trackCheck.checked) {
        updateCursor();
        timer = setInterval(updateCursor, 250);
      } else {
        if (timer) clearInterval(timer);
        timer = null;
      }
    };

    // Cleanup when tab is hidden (using MutationObserver on parent logic if available, 
    // but for this demo we'll hook into visibilitychange or similar if needed. 
    // Simpler: hook into window unload or just rely on user toggling off)
    // For robust demo, let's just leave it manual toggle.
    
    p.appendChild(cursorCard);

  });
})();
