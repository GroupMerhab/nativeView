/*
 * Copyright (c) 2026
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* Performance benchmarks for IPC throughput and latency */
#include "nv.h"
#include "nv_ipc_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  nv_app_t* app;
  nv_window_t* win;
  int warmup;
  int total;
  int sent;
  int done;
  uint64_t* lat;
  uint64_t t0;
  int measuring;
  int eval_done;
  int64_t create_ms;
} ctx_t;

static const char* HTML =
  "<!doctype html><html><head><meta charset='utf-8'><title>bench</title>"
  "<script>"
  "window.__pong=false;"
  "function benchKick(s){if(window.__nv&&window.__nv.send){window.__nv.send('bench',JSON.stringify({e:'bench',d:{seq:s},s:0}));}}"
  "if(window.__nv&&window.__nv.on){"
  " window.__nv.on('bench_ack',function(d){try{var seq=d.d.seq;window.__nv.send('bench_done',JSON.stringify({e:'bench_done',d:{seq:seq},s:0}));}catch(e){window.__nv.send('bench_done',JSON.stringify({e:'bench_done',d:{seq:0},s:0}));}});"
  "}"
  "</script></head><body></body></html>";

static void kick(ctx_t* c) {
  char js[128];
  snprintf(js, sizeof(js), "benchKick(%d)", c->sent + 1);
  nv_eval_js(c->win, js);
  c->sent += 1;
}

static int cmp_u64(const void* a, const void* b) {
  uint64_t x = *(const uint64_t*)a, y = *(const uint64_t*)b;
  return (x > y) - (x < y);
}

static void on_ready(nv_window_t* w, void* ud) {
  (void)w;
  ctx_t* c = (ctx_t*)ud;
  if (c->create_ms < 0) c->create_ms = 0;
  c->measuring = 0;
  c->sent = 0;
  for (int i = 0; i < c->warmup; i++) kick(c);
  c->measuring = 1;
  kick(c);
}

static void on_msg(nv_window_t* w, const char* event, const char* json, void* ud) {
  ctx_t* c = (ctx_t*)ud;
  if (strcmp(event, "bench") == 0) {
    c->t0 = nv_bench_now();
    nv_send(w, "bench_ack", json);
    return;
  }
  if (strcmp(event, "bench_done") == 0) {
    if (c->measuring) {
      uint64_t dt = nv_bench_now() - c->t0;
      c->lat[c->done++] = dt;
      if (c->done < c->total) {
        kick(c);
      } else {
        nv_app_quit(c->app);
      }
    } else {
      if (c->sent < c->warmup) {
        kick(c);
      } else {
        c->measuring = 1;
        kick(c);
      }
    }
    return;
  }
}

static void print_table(uint64_t* lat, int n, int64_t create_ms, uint64_t eval_ns, int eval_calls) {
  qsort(lat, n, sizeof(uint64_t), cmp_u64);
  uint64_t min = lat[0];
  uint64_t max = lat[n-1];
  long double sum = 0.0L;
  for (int i = 0; i < n; i++) sum += (long double)lat[i];
  long double avg = sum / (long double)n;
  int p99_idx = (int)((long double)n * 0.99L);
  if (p99_idx >= n) p99_idx = n - 1;
  uint64_t p99 = lat[p99_idx];
  long double eval_ms = (long double)eval_ns / 1.0e6L;
  long double eval_cps = ((long double)eval_calls) / (eval_ns / 1.0e9L);
  printf("\nIPC Benchmarks\n");
  printf("-------------------------------------------------------------\n");
  printf("Round-trip latency (us): min=%llu avg=%.2Lf max=%llu p99=%llu\n",
         (unsigned long long)(min/1000ull),
         avg/1000.0L,
         (unsigned long long)(max/1000ull),
         (unsigned long long)(p99/1000ull));
  printf("nv_eval_js throughput: calls=%d total_ms=%.2Lf calls/sec=%.0Lf\n",
         eval_calls, eval_ms, eval_cps);
  printf("Window creation to ready: %lld ms\n", (long long)create_ms);
  nv_ipc_stats_t st = nv_ipc_get_stats();
  size_t total = st.stack_allocs + st.heap_allocs;
  long double fast = total ? (100.0L * (long double)st.stack_allocs / (long double)total) : 0.0L;
  printf("IPC fast path: stack=%zu heap=%zu fast=%.2Lf%%\n", st.stack_allocs, st.heap_allocs, fast);
  printf("\n| RT min (us) | RT avg (us) | RT p99 (us) | RT max (us) | eval ms | eval cps | create->ready ms | fast %% |\n");
  printf("| %llu | %.2Lf | %llu | %llu | %.2Lf | %.0Lf | %lld | %.2Lf |\n",
         (unsigned long long)(min/1000ull), avg/1000.0L,
         (unsigned long long)(p99/1000ull),
         (unsigned long long)(max/1000ull),
         eval_ms, eval_cps, (long long)create_ms, fast);
}

int main(void) {
  const char* env_skip = getenv("NV_SKIP_INTEGRATION");
  if (env_skip && *env_skip == '1') {
    printf("skip\n");
    return 0;
  }
#if defined(__linux__)
  const char* d1 = getenv("DISPLAY");
  const char* d2 = getenv("WAYLAND_DISPLAY");
  if (!d1 && !d2) {
    printf("skip\n");
    return 0;
  }
#endif
  ctx_t c;
  memset(&c, 0, sizeof(c));
  c.warmup = 10;
  c.total = 1000;
  c.lat = (uint64_t*)malloc(sizeof(uint64_t) * c.total);
  if (!c.lat) {
    printf("alloc fail\n");
    return 1;
  }
  uint64_t t_create = nv_bench_now();
  c.app = nv_app_create();
  nv_window_cfg_t cfg;
  memset(&cfg, 0, sizeof(cfg));
  cfg.title = "bench";
  cfg.width = 800;
  cfg.height = 600;
  cfg.resizable = 1;
  c.win = nv_window_create(c.app, &cfg);
  nv_on_message(c.win, on_msg, &c);
  nv_on_ready(c.win, on_ready, &c);
  nv_load_html(c.win, HTML, "about:blank");
  nv_window_show(c.win);
  nv_app_run(c.app);
  c.create_ms = (int64_t)((nv_bench_now() - t_create) / 1000000ull);
  uint64_t t0 = nv_bench_now();
  for (int i = 0; i < 1000; i++) nv_eval_js(c.win, "void 0");
  uint64_t eval_ns = nv_bench_now() - t0;
  print_table(c.lat, c.total, c.create_ms, eval_ns, 1000);
  nv_app_destroy(c.app);
  free(c.lat);
  return 0;
}
