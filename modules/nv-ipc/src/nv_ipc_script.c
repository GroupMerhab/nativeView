#include <stdlib.h>
#include <string.h>
#include "nv_ipc_script.h"

static const char g_bootstrap[] =
  "(function(){"
    "if(window.__nv)return;"
    "var handlers=Object.create(null);"
    "function post(msg){/*NV_POST(msg)*/;}"
    "var api={"
      "handlers:handlers,"
      "send:function(event,data){"
        "try{var msg=JSON.stringify({e:event,d:data,s:0});post(msg);}catch(e){}"
      "},"
      "on:function(ev,fn){"
        "if(!handlers[ev])handlers[ev]=[];"
        "handlers[ev].push(fn);"
      "},"
      "off:function(ev,fn){"
        "var a=handlers[ev];if(!a)return;"
        "for(var i=a.length-1;i>=0;i--){if(a[i]===fn)a.splice(i,1);}if(a.length===0)delete handlers[ev];"
      "},"
      "_emit:function(ev,json){"
        "var a=handlers[ev];if(!a||a.length===0)return;"
        "var o;try{o=JSON.parse(json);}catch(e){return;}"
        "for(var i=0;i<a.length;i++){try{a[i](o);}catch(e){}}"
      "}"
    "};"
    "Object.defineProperty(api,'version',{value:'0.1.0',writable:false,enumerable:true});"
    "Object.defineProperty(window,'__nv',{value:api,writable:false,enumerable:false,configurable:false});"
  "})();";

NV_INTERNAL const char* nv_ipc_script_raw(void) {
  return g_bootstrap;
}

static const char* repl_for(nv_platform_t p) {
  switch(p){
    case NV_PLATFORM_WIN:
      return "window.chrome.webview.postMessage(msg)";
    case NV_PLATFORM_MAC:
    case NV_PLATFORM_LINUX:
    default:
      return "window.webkit.messageHandlers.nvBridge.postMessage(msg)";
  }
}

NV_INTERNAL const char* nv_ipc_script_for_platform(nv_platform_t platform) {
  const char* base = g_bootstrap;
  const char* tok = "/*NV_POST(msg)*/";
  const char* pos = strstr(base, tok);
  const char* rep = repl_for(platform);
  if(!pos){
    size_t n = strlen(base)+1;
    char* out = (char*)malloc(n);
    if(!out) return NULL;
    memcpy(out, base, n);
    return out;
  }
  size_t pre = (size_t)(pos - base);
  size_t tok_len = strlen(tok);
  size_t rep_len = strlen(rep);
  size_t base_len = strlen(base);
  size_t out_len = base_len - tok_len + rep_len;
  char* out = (char*)malloc(out_len + 1);
  if(!out) return NULL;
  memcpy(out, base, pre);
  memcpy(out + pre, rep, rep_len);
  memcpy(out + pre + rep_len, pos + tok_len, base_len - pre - tok_len);
  out[out_len] = '\0';
  return out;
}
