/* Window tab — demonstrates NativeView.window.* operations */
(function(global){
  function qs(sel, ctx){ return (ctx||document).querySelector(sel); }
  function nvInvoke(ev, data){
    var NV = global.NativeView;
    if (!NV || typeof NV.invoke !== 'function'){
      return Promise.reject(new Error('ERR_NOT_READY: bridge not connected'));
    }
    return NV.invoke(ev, data||{}).catch(function(err){
      var code = (err && err.code) ? err.code : 'ERR_UNKNOWN';
      var msg = (err && err.message) ? err.message : String(err);
      throw new Error(code + ': ' + msg);
    });
  }

  function addSection(panel, title){
    var h = document.createElement('h2');
    h.textContent = title;
    h.style.margin = '12px 0';
    panel.appendChild(h);
  }

  function triggerRun(card){
    try {
      var btn = card.querySelector('.op-run-btn');
      if (btn) setTimeout(function(){ btn.click(); }, 0);
    } catch(e){}
  }

  function init(){
    var panel = qs('#tab-window');
    if (!panel) return;

    /* SECTION: Query */
    addSection(panel, 'Query');

    var c_getSize = DemoCore.createOpCard({
      id: 'win-get-size',
      title: 'getSize()',
      description: 'Returns current window size { w, h }',
      inputs: [],
      run: function(){
        return nvInvoke('window.getSize', {});
      }
    });
    panel.appendChild(c_getSize); triggerRun(c_getSize);

    var c_getPos = DemoCore.createOpCard({
      id: 'win-get-pos',
      title: 'getPosition()',
      description: 'Returns current window position { x, y }',
      inputs: [],
      run: function(){ return nvInvoke('window.getPosition', {}); }
    });
    panel.appendChild(c_getPos); triggerRun(c_getPos);

    var c_isFocused = DemoCore.createOpCard({
      id: 'win-is-focused',
      title: 'isFocused()',
      description: 'Returns { value: boolean }',
      inputs: [],
      run: function(){ return nvInvoke('window.isFocused', {}); }
    });
    panel.appendChild(c_isFocused);

    var c_isFs = DemoCore.createOpCard({
      id: 'win-is-fs',
      title: 'isFullscreen()',
      description: 'Returns { value: boolean }',
      inputs: [],
      run: function(){ return nvInvoke('window.isFullscreen', {}); }
    });
    panel.appendChild(c_isFs);

    /* SECTION: Modify */
    addSection(panel, 'Modify');

    panel.appendChild(DemoCore.createOpCard({
      id: 'win-title',
      title: 'setTitle(title)',
      description: 'Sets the window title',
      inputs: [
        { id:'title', label:'Title', type:'text', defaultValue:'NativeView Demo', placeholder:'Title' }
      ],
      run: function(v){ return nvInvoke('window.setTitle', { title: v.title }); }
    }));

    panel.appendChild(DemoCore.createOpCard({
      id: 'win-size',
      title: 'setSize(w, h)',
      description: 'Resizes window',
      inputs: [
        { id:'w', label:'Width', type:'number', defaultValue:1200 },
        { id:'h', label:'Height', type:'number', defaultValue:800 }
      ],
      run: function(v){ return nvInvoke('window.setSize', { w: Number(v.w)||0, h: Number(v.h)||0 }); }
    }));

    panel.appendChild(DemoCore.createOpCard({
      id: 'win-pos',
      title: 'setPosition(x, y)',
      description: 'Moves window to position',
      inputs: [
        { id:'x', label:'X', type:'number', defaultValue:100 },
        { id:'y', label:'Y', type:'number', defaultValue:100 }
      ],
      run: function(v){ return nvInvoke('window.setPosition', { x: Number(v.x)||0, y: Number(v.y)||0 }); }
    }));

    panel.appendChild(DemoCore.createOpCard({
      id: 'win-min-size',
      title: 'setMinSize(w, h)',
      description: 'Sets minimum window size',
      inputs: [
        { id:'w', label:'Min Width', type:'number', defaultValue:400 },
        { id:'h', label:'Min Height', type:'number', defaultValue:300 }
      ],
      run: function(v){ return nvInvoke('window.setMinSize', { w: Number(v.w)||0, h: Number(v.h)||0 }); }
    }));

    panel.appendChild(DemoCore.createOpCard({
      id: 'win-center',
      title: 'center()',
      description: 'Centers the window',
      inputs: [],
      run: function(){ return nvInvoke('window.center', {}); }
    }));

    panel.appendChild(DemoCore.createOpCard({
      id: 'win-minimize',
      title: 'minimize()',
      description: 'Minimizes the window',
      inputs: [],
      run: function(){ return nvInvoke('window.minimize', {}); }
    }));

    panel.appendChild(DemoCore.createOpCard({
      id: 'win-maximize',
      title: 'maximize()',
      description: 'Maximizes the window',
      inputs: [],
      run: function(){ return nvInvoke('window.maximize', {}); }
    }));

    panel.appendChild(DemoCore.createOpCard({
      id: 'win-restore',
      title: 'restore()',
      description: 'Restores from minimized/maximized',
      inputs: [],
      run: function(){ return nvInvoke('window.restore', {}); }
    }));

    panel.appendChild(DemoCore.createOpCard({
      id: 'win-fullscreen',
      title: 'fullscreen(enable)',
      description: 'Toggle fullscreen',
      inputs: [
        { id:'enable', label:'Enable', type:'checkbox', defaultValue:false }
      ],
      run: function(v){ return nvInvoke('window.fullscreen', { enable: !!v.enable }); }
    }));

    panel.appendChild(DemoCore.createOpCard({
      id: 'win-resizable',
      title: 'setResizable(enable)',
      description: 'Enable/disable window resizing',
      inputs: [
        { id:'resizable', label:'Resizable', type:'checkbox', defaultValue:true }
      ],
      run: function(v){ return nvInvoke('window.setResizable', { resizable: !!v.resizable }); }
    }));

    panel.appendChild(DemoCore.createOpCard({
      id: 'win-ontop',
      title: 'setAlwaysOnTop(enable)',
      description: 'Keep window always on top',
      inputs: [
        { id:'enable', label:'Enable', type:'checkbox', defaultValue:false }
      ],
      run: function(v){ return nvInvoke('window.setAlwaysOnTop', { enable: !!v.enable }); }
    }));

    panel.appendChild(DemoCore.createOpCard({
      id: 'win-opacity',
      title: 'setOpacity(value)',
      description: 'Opacity 0.1–1.0 (converted to 10–100%)',
      inputs: [
        { id:'value', label:'Opacity', type:'number', defaultValue:1.0, placeholder:'0.1–1.0' }
      ],
      run: function(v){
        var f = parseFloat(v.value);
        if (isNaN(f)) f = 1.0;
        if (f < 0.1) f = 0.1;
        if (f > 1.0) f = 1.0;
        var pct = Math.round(f * 100);
        return nvInvoke('window.setOpacity', { opacity: pct });
      }
    }));

    /* SECTION: Events */
    addSection(panel, 'Events');

    var pushResized = DemoCore.createPushCard({
      id:'win-on-resized',
      title:'onResized',
      event:'window.resized',
      format:function(d){ return JSON.stringify(d, null, 2); }
    });
    panel.appendChild(pushResized);

    var pushMoved = DemoCore.createPushCard({
      id:'win-on-moved',
      title:'onMoved',
      event:'window.moved',
      format:function(d){ return JSON.stringify(d, null, 2); }
    });
    panel.appendChild(pushMoved);

    var pushFocused = DemoCore.createPushCard({
      id:'win-on-focused',
      title:'onFocused',
      event:'window.focused',
      format:function(d){ return JSON.stringify(d, null, 2); }
    });
    panel.appendChild(pushFocused);
  }

  DemoCore.registerTab('window', init);
})(typeof window!=='undefined'?window:this);

