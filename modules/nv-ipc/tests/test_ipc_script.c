#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "nv_ipc_script.h"

static int contains(const char* s, const char* sub){ return s && sub && strstr(s, sub)!=NULL; }

int main(void){
  const char* raw = nv_ipc_script_raw();
  if(!raw) { fprintf(stderr,"raw null\n"); return 1; }
  if(!contains(raw,"__nv")) { fprintf(stderr,"missing __nv\n"); return 1; }
  if(!contains(raw,"_emit")) { fprintf(stderr,"missing _emit\n"); return 1; }
  if(!contains(raw,"NV_POST")) { fprintf(stderr,"missing NV_POST token\n"); return 1; }

  char* mac = (char*)nv_ipc_script_for_platform(NV_PLATFORM_MAC);
  if(!mac) { fprintf(stderr,"mac null\n"); return 1; }
  if(!contains(mac,"window.webkit.messageHandlers.nvBridge.postMessage(msg)")) { fprintf(stderr,"mac post missing\n"); free(mac); return 1; }
  if(contains(mac,"NV_POST")) { fprintf(stderr,"mac token not removed\n"); free(mac); return 1; }
  free(mac);

  char* win = (char*)nv_ipc_script_for_platform(NV_PLATFORM_WIN);
  if(!win) { fprintf(stderr,"win null\n"); return 1; }
  if(!contains(win,"window.chrome.webview.postMessage(msg)")) { fprintf(stderr,"win post missing\n"); free(win); return 1; }
  if(contains(win,"NV_POST")) { fprintf(stderr,"win token not removed\n"); free(win); return 1; }
  free(win);

  char* linux = (char*)nv_ipc_script_for_platform(NV_PLATFORM_LINUX);
  if(!linux) { fprintf(stderr,"linux null\n"); return 1; }
  if(!contains(linux,"window.webkit.messageHandlers.nvBridge.postMessage(msg)")) { fprintf(stderr,"linux post missing\n"); free(linux); return 1; }
  if(contains(linux,"NV_POST")) { fprintf(stderr,"linux token not removed\n"); free(linux); return 1; }
  free(linux);

  printf("[PASS] test_ipc_script\n");
  return 0;
}
