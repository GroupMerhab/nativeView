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

/* macOS platform implementation using NSApplication + NSWindow + WKWebView */

#import <Cocoa/Cocoa.h>
#import <WebKit/WebKit.h>
#import <UserNotifications/UserNotifications.h>
#import <CoreFoundation/CoreFoundation.h>
#import <dispatch/dispatch.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <spawn.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include "nv_core_internal.h"
#include "nv_window_internal.h"
#include "nv_ipc_internal.h"
#include "nv_arena.h"
#include "nv_util.h"
#include "nv_op_dialog.h"
#include "nv_op_windows.h"
#include "nv_window_manager.h"
#include "nv.h"

@class WKContextMenuConfiguration;
@class WKContextMenuElementInfo;

/* =============================================================================
 * Menu bar extras (NSStatusItem / tray)
 * =============================================================================
 */

#define NV_MAX_TRAYS 8

typedef struct {
  long long id;
  NSStatusItem *item;
  nv_window_t *window;
} nv_mac_tray_t;

static nv_mac_tray_t g_trays[NV_MAX_TRAYS];
static int g_tray_count = 0;

@interface NVTrayActions : NSObject
+ (instancetype)shared;
- (void)onTrayClick:(id)sender;
- (void)onMenuItem:(id)sender;
@end

@implementation NVTrayActions

+ (instancetype)shared {
  static NVTrayActions *inst = nil;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    inst = [[NVTrayActions alloc] init];
  });
  return inst;
}

- (void)onTrayClick:(id)sender {
  NSStatusBarButton *btn = (NSStatusBarButton *)sender;
  for (int i = 0; i < g_tray_count; i++) {
    NSStatusItem *si = g_trays[i].item;
    if (si && si.button == btn) {
      nv_window_t *w = g_trays[i].window;
      if (!w) {
        return;
      }
      char buf[64];
      snprintf(buf, sizeof(buf), "{\"id\":%lld}", g_trays[i].id);
      nv_send(w, "tray.clicked", buf);
      return;
    }
  }
}

- (void)onMenuItem:(id)sender {
  if (![sender isKindOfClass:[NSMenuItem class]]) {
    return;
  }
  NSMenuItem *mi = (NSMenuItem *)sender;
  id rep = mi.representedObject;
  if (![rep isKindOfClass:[NSDictionary class]]) {
    return;
  }
  NSDictionary *d = (NSDictionary *)rep;
  NSValue *vp = d[@"wp"];
  if (![vp isKindOfClass:[NSValue class]]) {
    return;
  }
  nv_window_t *w = (nv_window_t *)[vp pointerValue];
  if (!w) {
    return;
  }
  long long tid = [d[@"tid"] longLongValue];
  long long iid = [d[@"iid"] longLongValue];
  char buf[96];
  snprintf(buf, sizeof(buf), "{\"id\":%lld,\"itemId\":%lld}", tid, iid);
  nv_send(w, "tray.menuAction", buf);
}

@end

static int nv_mac_tray_index_by_id(long long tray_id) {
  for (int i = 0; i < g_tray_count; i++) {
    if (g_trays[i].id == tray_id) {
      return i;
    }
  }
  return -1;
}

static void nv_mac_tray_remove_at(int idx) {
  if (idx < 0 || idx >= g_tray_count) {
    return;
  }
  NSStatusItem *item = g_trays[idx].item;
  if (item) {
    [[NSStatusBar systemStatusBar] removeStatusItem:item];
  }
  int tail = g_tray_count - idx - 1;
  if (tail > 0) {
    memmove(&g_trays[idx], &g_trays[idx + 1], (size_t)tail * sizeof(g_trays[0]));
  }
  g_tray_count--;
  memset(&g_trays[g_tray_count], 0, sizeof(g_trays[0]));
}

static int nv_mac_tray_create_impl(long long tray_id, const char *icon_path, const char *tooltip,
                                   nv_window_t *w) {
  if (tray_id <= 0) {
    return -1;
  }
  if (nv_mac_tray_index_by_id(tray_id) >= 0) {
    return -1;
  }
  if (g_tray_count >= NV_MAX_TRAYS) {
    return -1;
  }

  (void)[NSApplication sharedApplication];

  NSStatusItem *statusItem =
      [[NSStatusBar systemStatusBar] statusItemWithLength:NSVariableStatusItemLength];
  if (!statusItem) {
    return -1;
  }

  NSStatusBarButton *button = statusItem.button;
  if (!button) {
    [[NSStatusBar systemStatusBar] removeStatusItem:statusItem];
    return -1;
  }

  NSImage *img = nil;
  if (icon_path && icon_path[0]) {
    NSString *p = [NSString stringWithUTF8String:icon_path];
    if (p) {
      img = [[[NSImage alloc] initWithContentsOfFile:p] autorelease];
    }
  }
  if (!img) {
    img = [NSImage imageNamed:NSImageNameApplicationIcon];
  }
  if (!img) {
    img = [NSImage imageNamed:NSImageNameStatusAvailable];
  }
  if (img) {
    button.image = img;
  }

  if (tooltip && tooltip[0]) {
    NSString *tip = [NSString stringWithUTF8String:tooltip];
    if (tip) {
      button.toolTip = tip;
    }
  }

  NVTrayActions *actions = [NVTrayActions shared];
  button.target = actions;
  button.action = @selector(onTrayClick:);

  g_trays[g_tray_count].id = tray_id;
  g_trays[g_tray_count].item = statusItem;
  g_trays[g_tray_count].window = w;
  g_tray_count++;
  return 0;
}

static int nv_mac_tray_destroy_impl(long long tray_id) {
  int idx = nv_mac_tray_index_by_id(tray_id);
  if (idx < 0) {
    return -1;
  }
  nv_mac_tray_remove_at(idx);
  return 0;
}

static int nv_mac_tray_set_icon_impl(long long tray_id, const char *icon_path) {
  if (!icon_path || !icon_path[0]) {
    return -1;
  }
  int idx = nv_mac_tray_index_by_id(tray_id);
  if (idx < 0) {
    return -1;
  }
  NSStatusItem *statusItem = g_trays[idx].item;
  if (!statusItem || !statusItem.button) {
    return -1;
  }
  NSString *p = [NSString stringWithUTF8String:icon_path];
  if (!p) {
    return -1;
  }
  NSImage *img = [[[NSImage alloc] initWithContentsOfFile:p] autorelease];
  if (!img) {
    return -1;
  }
  statusItem.button.image = img;
  return 0;
}

static int nv_mac_tray_set_tooltip_impl(long long tray_id, const char *tooltip) {
  int idx = nv_mac_tray_index_by_id(tray_id);
  if (idx < 0) {
    return -1;
  }
  NSStatusItem *statusItem = g_trays[idx].item;
  if (!statusItem || !statusItem.button) {
    return -1;
  }
  if (tooltip && tooltip[0]) {
    NSString *tip = [NSString stringWithUTF8String:tooltip];
    statusItem.button.toolTip = tip ? tip : @"";
  } else {
    statusItem.button.toolTip = @"";
  }
  return 0;
}

static int nv_mac_tray_set_menu_impl(long long tray_id, const char **labels, const long long *item_ids,
                                     int count) {
  int idx = nv_mac_tray_index_by_id(tray_id);
  if (idx < 0) {
    return -1;
  }
  NSStatusItem *statusItem = g_trays[idx].item;
  if (!statusItem) {
    return -1;
  }
  nv_window_t *w = g_trays[idx].window;
  NSMenu *menu = [[NSMenu alloc] initWithTitle:@""];
  NVTrayActions *actions = [NVTrayActions shared];
  for (int i = 0; i < count; i++) {
    if (item_ids && item_ids[i] == -1) {
      [menu addItem:[NSMenuItem separatorItem]];
      continue;
    }
    const char *lbl = (labels && labels[i]) ? labels[i] : "";
    NSString *title = [NSString stringWithUTF8String:lbl];
    if (!title) {
      title = @"";
    }
    NSMenuItem *mi = [[NSMenuItem alloc] initWithTitle:title action:@selector(onMenuItem:) keyEquivalent:@""];
    mi.target = actions;
    long long iid = item_ids ? item_ids[i] : 0;
    NSDictionary *rep = @{
      @"wp" : [NSValue valueWithPointer:(void *)w],
      @"tid" : @(tray_id),
      @"iid" : @(iid)
    };
    mi.representedObject = rep;
    [menu addItem:mi];
    [mi release];
  }
  statusItem.menu = menu;
  [menu release];
  return 0;
}

NV_INTERNAL int nv_mac_tray_create(long long tray_id, const char *icon_path, const char *tooltip,
                                   nv_window_t *w) {
  if (!w) {
    return -1;
  }
  __block int rc = -1;
  void (^block)(void) = ^{
    rc = nv_mac_tray_create_impl(tray_id, icon_path, tooltip, w);
  };
  if ([NSThread isMainThread]) {
    block();
  } else {
    dispatch_sync(dispatch_get_main_queue(), block);
  }
  return rc;
}

NV_INTERNAL int nv_mac_tray_destroy(long long tray_id) {
  __block int rc = -1;
  void (^block)(void) = ^{
    rc = nv_mac_tray_destroy_impl(tray_id);
  };
  if ([NSThread isMainThread]) {
    block();
  } else {
    dispatch_sync(dispatch_get_main_queue(), block);
  }
  return rc;
}

NV_INTERNAL int nv_mac_tray_set_icon(long long tray_id, const char *icon_path) {
  __block int rc = -1;
  void (^block)(void) = ^{
    rc = nv_mac_tray_set_icon_impl(tray_id, icon_path);
  };
  if ([NSThread isMainThread]) {
    block();
  } else {
    dispatch_sync(dispatch_get_main_queue(), block);
  }
  return rc;
}

NV_INTERNAL int nv_mac_tray_set_tooltip(long long tray_id, const char *tooltip) {
  __block int rc = -1;
  void (^block)(void) = ^{
    rc = nv_mac_tray_set_tooltip_impl(tray_id, tooltip);
  };
  if ([NSThread isMainThread]) {
    block();
  } else {
    dispatch_sync(dispatch_get_main_queue(), block);
  }
  return rc;
}

NV_INTERNAL int nv_mac_tray_set_menu(long long tray_id, const char **labels, const long long *item_ids,
                                     int count) {
  if (count < 0) {
    return -1;
  }
  __block int rc = -1;
  void (^block)(void) = ^{
    rc = nv_mac_tray_set_menu_impl(tray_id, labels, item_ids, count);
  };
  if ([NSThread isMainThread]) {
    block();
  } else {
    dispatch_sync(dispatch_get_main_queue(), block);
  }
  return rc;
}

/* =============================================================================
 * Local notifications (UNUserNotificationCenter, NSUserNotification fallback)
 * =============================================================================
 */

static NSMutableDictionary *g_nv_notif_window_by_id;
static NSString *nv_mac_notif_id_string(long long nid) {
  return [NSString stringWithFormat:@"%lld", nid];
}

static nv_window_t *nv_mac_notif_window_get(long long nid) {
  NSString *key = nv_mac_notif_id_string(nid);
  @synchronized(g_nv_notif_window_by_id) {
    NSValue *v = g_nv_notif_window_by_id ? g_nv_notif_window_by_id[key] : nil;
    if (![v isKindOfClass:[NSValue class]]) {
      return NULL;
    }
    return (nv_window_t *)[v pointerValue];
  }
}

static void nv_mac_notif_window_put(long long nid, nv_window_t *w) {
  NSString *key = nv_mac_notif_id_string(nid);
  @synchronized(g_nv_notif_window_by_id) {
    if (!g_nv_notif_window_by_id) {
      g_nv_notif_window_by_id = [[NSMutableDictionary alloc] init];
    }
    g_nv_notif_window_by_id[key] = [NSValue valueWithPointer:(void *)w];
  }
}

static void nv_mac_notif_window_remove(long long nid) {
  NSString *key = nv_mac_notif_id_string(nid);
  @synchronized(g_nv_notif_window_by_id) {
    if (g_nv_notif_window_by_id) {
      [g_nv_notif_window_by_id removeObjectForKey:key];
    }
  }
}

static void nv_mac_runloop_spin_until(BOOL *done, CFTimeInterval timeoutSec) {
  if (!done) {
    return;
  }
  NSDate *end = [NSDate dateWithTimeIntervalSinceNow:timeoutSec];
  while (!*done && [end timeIntervalSinceNow] > 0.0) {
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.02, YES);
  }
}

@interface NVNotificationDelegate : NSObject <UNUserNotificationCenterDelegate, NSUserNotificationCenterDelegate>
@end

@implementation NVNotificationDelegate

+ (instancetype)shared {
  static NVNotificationDelegate *inst = nil;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    inst = [[NVNotificationDelegate alloc] init];
  });
  return inst;
}

- (void)userNotificationCenter:(UNUserNotificationCenter *)center
    didReceiveNotificationResponse:(UNNotificationResponse *)response
             withCompletionHandler:(void (^)(void))completionHandler {
  (void)center;
  NSString *ids = response.notification.request.identifier;
  long long nid = [ids longLongValue];
  nv_window_t *w = nv_mac_notif_window_get(nid);
  if (w && nid > 0) {
    char buf[64];
    snprintf(buf, sizeof(buf), "{\"id\":%lld}", nid);
    nv_send(w, "notification.clicked", buf);
  }
  if (completionHandler) {
    completionHandler();
  }
}

- (void)userNotificationCenter:(UNUserNotificationCenter *)center
       willPresentNotification:(UNNotification *)notification
         withCompletionHandler:
             (void (^)(UNNotificationPresentationOptions options))completionHandler {
  (void)center;
  NSString *ids = notification.request.identifier;
  long long nid = [ids longLongValue];
  nv_window_t *w = nv_mac_notif_window_get(nid);
  if (w && nid > 0) {
    char buf[64];
    snprintf(buf, sizeof(buf), "{\"id\":%lld}", nid);
    nv_send(w, "notification.shown", buf);
  }
  if (completionHandler) {
    if (@available(macOS 11.0, *)) {
      completionHandler(UNNotificationPresentationOptionList | UNNotificationPresentationOptionBanner |
                        UNNotificationPresentationOptionSound);
    } else {
      completionHandler(UNNotificationPresentationOptionSound);
    }
  }
}

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
- (void)userNotificationCenter:(NSUserNotificationCenter *)center didActivateNotification:(NSUserNotification *)notification {
  (void)center;
  NSString *ids = notification.identifier;
  long long nid = [ids longLongValue];
  nv_window_t *w = nv_mac_notif_window_get(nid);
  if (w && nid > 0) {
    char buf[64];
    snprintf(buf, sizeof(buf), "{\"id\":%lld}", nid);
    nv_send(w, "notification.clicked", buf);
  }
}
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

@end

static void nv_mac_notif_install_delegates(void) {
  NVNotificationDelegate *d = [NVNotificationDelegate shared];
  if (@available(macOS 10.14, *)) {
    [UNUserNotificationCenter currentNotificationCenter].delegate = d;
  }
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
  [NSUserNotificationCenter defaultUserNotificationCenter].delegate = d;
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
}

static int nv_mac_notif_un_ensure_authorized(UNUserNotificationCenter *center) {
  __block int result = -1;
  __block BOOL done = NO;
  [center getNotificationSettingsWithCompletionHandler:^(UNNotificationSettings *settings) {
    UNAuthorizationStatus st = settings.authorizationStatus;
    if (st == UNAuthorizationStatusDenied) {
      result = -1;
      done = YES;
      return;
    }
    if (st == UNAuthorizationStatusAuthorized || st == UNAuthorizationStatusProvisional) {
      result = 0;
      done = YES;
      return;
    }
    if (st == UNAuthorizationStatusNotDetermined) {
      [center requestAuthorizationWithOptions:(UNAuthorizationOptionAlert | UNAuthorizationOptionSound)
                            completionHandler:^(BOOL granted, NSError *err) {
                              (void)err;
                              result = granted ? 0 : -1;
                              done = YES;
                            }];
      return;
    }
    result = -1;
    done = YES;
  }];
  nv_mac_runloop_spin_until(&done, 120.0);
  return result;
}

static int nv_mac_notification_show_un(long long id, const char *title, const char *body, const char *icon_path,
                                       nv_window_t *w) {
  if (@available(macOS 10.14, *)) {
    UNUserNotificationCenter *center = [UNUserNotificationCenter currentNotificationCenter];
    if (nv_mac_notif_un_ensure_authorized(center) != 0) {
      return -1;
    }
    nv_mac_notif_install_delegates();
    nv_mac_notif_window_put(id, w);

    UNMutableNotificationContent *content = [[UNMutableNotificationContent alloc] init];
    content.title = title ? [NSString stringWithUTF8String:title] : @"";
    content.body = body ? [NSString stringWithUTF8String:body] : @"";
    if (icon_path && icon_path[0]) {
      NSString *p = [NSString stringWithUTF8String:icon_path];
      if (p) {
        NSURL *url = [NSURL fileURLWithPath:p];
        if (url) {
          NSError *attErr = nil;
          UNNotificationAttachment *att =
              [UNNotificationAttachment attachmentWithIdentifier:@"icon" URL:url options:nil error:&attErr];
          (void)attErr;
          if (att) {
            content.attachments = @[ att ];
          }
        }
      }
    }

    NSString *nid = nv_mac_notif_id_string(id);
    UNNotificationTrigger *trigger =
        [UNTimeIntervalNotificationTrigger triggerWithTimeInterval:1.0 repeats:NO];
    UNNotificationRequest *req = [UNNotificationRequest requestWithIdentifier:nid content:content trigger:trigger];
    [content release];

    __block int addRes = -1;
    __block BOOL addDone = NO;
    [center addNotificationRequest:req withCompletionHandler:^(NSError *error) {
      addRes = (error == nil) ? 0 : -1;
      addDone = YES;
    }];
    nv_mac_runloop_spin_until(&addDone, 10.0);
    if (addRes != 0) {
      nv_mac_notif_window_remove(id);
    }
    return addRes;
  }
  return -1;
}

static int nv_mac_notification_show_legacy(long long id, const char *title, const char *body, nv_window_t *w) {
  nv_mac_notif_install_delegates();
  nv_mac_notif_window_put(id, w);
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
  NSUserNotification *n = [[NSUserNotification alloc] init];
  n.title = title ? [NSString stringWithUTF8String:title] : @"";
  n.informativeText = body ? [NSString stringWithUTF8String:body] : @"";
  n.identifier = nv_mac_notif_id_string(id);
  [[NSUserNotificationCenter defaultUserNotificationCenter] deliverNotification:n];
  [n release];
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
  return 0;
}

static int nv_mac_notification_show_impl(long long id, const char *title, const char *body, const char *icon_path,
                                         nv_window_t *w) {
  if (id <= 0 || !w) {
    return -1;
  }
  (void)[NSApplication sharedApplication];
  if (@available(macOS 10.14, *)) {
    if ([UNUserNotificationCenter class]) {
      int ur = nv_mac_notification_show_un(id, title, body, icon_path, w);
      return ur;
    }
  }
  return nv_mac_notification_show_legacy(id, title, body, w);
}

static int nv_mac_notification_close_impl(long long id) {
  if (id <= 0) {
    return -1;
  }
  NSString *nid = nv_mac_notif_id_string(id);
  NSArray<NSString *> *ids = @[ nid ];
  if (@available(macOS 10.14, *)) {
    UNUserNotificationCenter *center = [UNUserNotificationCenter currentNotificationCenter];
    [center removePendingNotificationRequestsWithIdentifiers:ids];
    [center removeDeliveredNotificationsWithIdentifiers:ids];
  }
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
  NSUserNotificationCenter *legacyCenter = [NSUserNotificationCenter defaultUserNotificationCenter];
  for (NSUserNotification *n in legacyCenter.deliveredNotifications) {
    if ([n.identifier isEqualToString:nid]) {
      [legacyCenter removeDeliveredNotification:n];
    }
  }
  for (NSUserNotification *n in legacyCenter.scheduledNotifications) {
    if ([n.identifier isEqualToString:nid]) {
      [legacyCenter removeScheduledNotification:n];
    }
  }
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
  nv_mac_notif_window_remove(id);
  return 0;
}

NV_INTERNAL int __attribute__((weak)) nv_mac_notification_show(long long id, const char *title, const char *body,
                                                               const char *icon_path, nv_window_t *w) {
  __block int rc = -1;
  void (^block)(void) = ^{
    rc = nv_mac_notification_show_impl(id, title, body, icon_path, w);
  };
  if ([NSThread isMainThread]) {
    block();
  } else {
    dispatch_sync(dispatch_get_main_queue(), block);
  }
  return rc;
}

NV_INTERNAL int __attribute__((weak)) nv_mac_notification_close(long long id) {
  __block int rc = -1;
  void (^block)(void) = ^{
    rc = nv_mac_notification_close_impl(id);
  };
  if ([NSThread isMainThread]) {
    block();
  } else {
    dispatch_sync(dispatch_get_main_queue(), block);
  }
  return rc;
}

static BOOL nv_mac_env_truthy(const char* value) {
  if (!value || !*value) return NO;
  if (value[0] == '1' && value[1] == '\0') return YES;
  if (value[0] == 't' || value[0] == 'T') return YES;
  if (value[0] == 'y' || value[0] == 'Y') return YES;
  if ((value[0] == 'o' || value[0] == 'O') && (value[1] == 'n' || value[1] == 'N') && value[2] == '\0') return YES;
  return NO;
}

static BOOL nv_mac_context_menu_enabled(void) {
  const char* value = getenv("NV_WEBVIEW_CONTEXT_MENU");
  return nv_mac_env_truthy(value);
}

NV_INTERNAL void nv_mac_window_set_context_menu(nv_window_t* w, const nv_menu_item_t* items, int count) {
  (void)w;
  (void)items;
  (void)count;
  /* Menu data lives on nv_window_t; WKUIDelegate reads it when the menu opens. */
}

/* =============================================================================
 * App-level Platform State
 * =============================================================================
 */

@interface NVAppPlatform : NSObject
@property (nonatomic, strong) WKWebViewConfiguration *config;
@end

@implementation NVAppPlatform
@end

/* =============================================================================
 * Window-level Platform State
 * =============================================================================
 */

@interface NVWindowPlatform : NSObject <WKScriptMessageHandler, WKNavigationDelegate, WKUIDelegate, NSWindowDelegate>
@property (nonatomic, assign) nv_window_t *cWindow;
@property (nonatomic, strong) NSWindow *window;
@property (nonatomic, strong) WKWebView *webView;
@property (nonatomic, assign) BOOL allowClose;
@end

@implementation NVWindowPlatform

#if defined(__MAC_OS_X_VERSION_MAX_ALLOWED) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 110000
- (void)webView:(WKWebView *)webView
contextMenuConfigurationForElement:(WKContextMenuElementInfo *)elementInfo
completionHandler:(void (^)(WKContextMenuConfiguration *configuration))completionHandler {
  (void)webView;
  (void)elementInfo;
  if (self.cWindow && self.cWindow->ctx_menu_count > 0) {
    Class cfgClass = NSClassFromString(@"WKContextMenuConfiguration");
    if (cfgClass) {
      id cfg = [[[cfgClass alloc] init] autorelease];
      completionHandler((WKContextMenuConfiguration *)cfg);
      return;
    }
  }
  if (nv_mac_context_menu_enabled()) {
    Class cfgClass = NSClassFromString(@"WKContextMenuConfiguration");
    if (cfgClass) {
      id cfg = [[[cfgClass alloc] init] autorelease];
      completionHandler((WKContextMenuConfiguration *)cfg);
      return;
    }
  }
  completionHandler(nil);
}
#endif

- (void)nv_onCtxMenuItem:(id)sender {
  NSMenuItem *mi = (NSMenuItem *)sender;
  NSString *sid = mi.representedObject;
  if (!self.cWindow || !sid) return;
  NSError *err = nil;
  NSData *data = [NSJSONSerialization dataWithJSONObject:@{@"id" : sid} options:0 error:&err];
  if (err || !data) return;
  NSString *json = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
  if (!json) return;
  nv_send(self.cWindow, "window.contextMenuAction", [json UTF8String]);
  [json release];
}

- (NSArray *)webView:(WKWebView *)webView
contextMenuItemsForElement:(WKContextMenuElementInfo *)elementInfo
defaultMenuItems:(NSArray *)defaultMenuItems {
  (void)webView;
  (void)elementInfo;
  nv_window_t *cw = self.cWindow;
  if (cw && cw->ctx_menu_count > 0 && cw->ctx_menu_items) {
    NSMutableArray *out = [NSMutableArray array];
    for (int i = 0; i < cw->ctx_menu_count; i++) {
      const nv_menu_item_t *it = &cw->ctx_menu_items[i];
      if (it->separator) continue;
      const char *lid = it->id ? it->id : "";
      const char *llab = it->label ? it->label : "";
      NSString *sid = [NSString stringWithUTF8String:lid];
      NSString *lab = [NSString stringWithUTF8String:llab];
      NSMenuItem *menuItem = [[NSMenuItem alloc] initWithTitle:lab action:@selector(nv_onCtxMenuItem:) keyEquivalent:@""];
      menuItem.target = self;
      menuItem.representedObject = sid;
      menuItem.enabled = it->enabled ? YES : NO;
      [out addObject:menuItem];
      [menuItem release];
    }
    return out;
  }
  if (nv_mac_context_menu_enabled()) return defaultMenuItems;
  (void)defaultMenuItems;
  return @[];
}

- (void)dealloc {
  // Ensure script handler is removed to break potential retain cycles.
  @try {
    [self.webView.configuration.userContentController removeScriptMessageHandlerForName:@"nvBridge"];
  } @catch (__unused NSException *e) {
    // Ignore - handler may not have been set.
  }
  self.webView = nil;
  self.window = nil;
  [super dealloc];
}

- (BOOL)windowShouldClose:(id)sender {
  (void)sender;
  if (self.allowClose) return YES;
  if (!self.cWindow) return YES;
  nv_ipc_state_t *ipc = nv_window_get_ipc(self.cWindow);
  if (!ipc) return YES;
  if (nv_ipc_has_close_cb(ipc)) {
    nv_ipc_invoke_close_cb(self.cWindow, ipc);
    return NO;
  }
  return YES;
}

- (void)windowWillClose:(NSNotification *)notification {
  // macOS invokes this before the window is torn down by the system.
  // Clear cWindow before destroy so we do not use it after; destroy may
  // release this delegate so we must not touch self after nv_window_destroy.
  if (!self.cWindow) return;
  nv_window_t *win = self.cWindow;
  self.cWindow = NULL;
  const char *id = nv_wm_get_id_by_window(win);
  if (id) {
    nv_arena_t *arena = nv_arena_create(1024);
    nv_json_t *before = nv_json_object(arena);
    nv_json_str(before, "id", id);
    nv_op_windows_push_all("windows.beforeClose", before, arena);
    nv_arena_destroy(arena);
  }
  nv_window_destroy(win);
}

- (void)userContentController:(WKUserContentController *)userContentController
      didReceiveScriptMessage:(WKScriptMessage *)message {
  (void)userContentController;

  if (![message.name isEqualToString:@"nvBridge"]) {
    return;
  }

  if (!self.cWindow) {
    return;
  }

  id body = message.body;
  if (![body isKindOfClass:[NSString class]]) {
    NV_ERR("macOS: nvBridge expected string body, got %s",
           NSStringFromClass([body class]).UTF8String);
    return;
  }

  const char *json = [(NSString *)body UTF8String];
  if (!json) {
    NV_ERR("macOS: nvBridge body UTF8 conversion failed");
    return;
  }
  nv_ipc_state_t *ipc = nv_window_get_ipc(self.cWindow);
  if (!ipc) {
    NV_ERR("macOS: no IPC state for window");
    return;
  }

  /* WKScriptMessageHandler can run off main thread. Dispatch to main queue before
   * nv_ipc_dispatch (matches CrossDev behavior; handlers may do file I/O, evaluateJS). */
  size_t json_len = strlen(json) + 1;
  char *json_copy = (char *)malloc(json_len);
  if (!json_copy) {
    nv_ipc_dispatch(self.cWindow, ipc, json);
    return;
  }
  memcpy(json_copy, json, json_len);
  nv_window_t *win = self.cWindow;
  dispatch_async(dispatch_get_main_queue(), ^{
    nv_ipc_dispatch(win, ipc, json_copy);
    free(json_copy);
  });
}

/* ===== Shell platform hooks ===== */
NV_INTERNAL int nv_mac_shell_open_url(const char* url) {
  if (!url) return -1;
  @autoreleasepool {
    NSURL *nsurl = [NSURL URLWithString:[NSString stringWithUTF8String:url]];
    if (!nsurl) return -1;
    BOOL ok = [[NSWorkspace sharedWorkspace] openURL:nsurl];
    return ok ? 0 : -1;
  }
}

NV_INTERNAL int nv_mac_shell_open_path(const char* path) {
  if (!path) return -1;
  @autoreleasepool {
    NSURL *u = [NSURL fileURLWithPath:[NSString stringWithUTF8String:path]];
    if (!u) return -1;
    BOOL ok = [[NSWorkspace sharedWorkspace] openURL:u];
    return ok ? 0 : -1;
  }
}

NV_INTERNAL int nv_mac_shell_show_in_folder(const char* path) {
  if (!path) return -1;
  @autoreleasepool {
    NSURL *u = [NSURL fileURLWithPath:[NSString stringWithUTF8String:path]];
    if (!u) return -1;
    [[NSWorkspace sharedWorkspace] activateFileViewerSelectingURLs:@[u]];
    return 0;
  }
}

extern char** environ;

enum { nv_mac_shell_capture_cap = 1048576 };

typedef struct {
  char* data;
  size_t len;
  size_t cap;
  int truncated;
} nv_mac_gbuf_t;

static int nv_mac_gbuf_append(nv_mac_gbuf_t* g, const char* src, size_t n) {
  for (size_t i = 0; i < n; i++) {
    if (g->truncated) return 0;
    if (g->len >= (size_t)nv_mac_shell_capture_cap) {
      g->truncated = 1;
      return 0;
    }
    if (g->len + 1 > g->cap) {
      size_t ncap = g->cap ? g->cap : 16;
      while (g->len + 1 > ncap) ncap *= 2;
      char* nb = (char*)realloc(g->data, ncap);
      if (!nb) return -1;
      g->data = nb;
      g->cap = ncap;
    }
    g->data[g->len++] = (unsigned char)src[i];
  }
  return 0;
}

static int nv_mac_read_both_pipes(int out_fd, int err_fd, char** out_s, char** err_s,
                                  int* out_trunc, int* err_trunc) {
  struct pollfd pf[2];
  nv_mac_gbuf_t go = {0};
  nv_mac_gbuf_t ge = {0};
  int out_done = 0;
  int err_done = 0;

  pf[0].fd = out_fd;
  pf[0].events = POLLIN;
  pf[1].fd = err_fd;
  pf[1].events = POLLIN;

  while (!out_done || !err_done) {
    int pr;
    do {
      pr = poll(pf, 2, -1);
    } while (pr < 0 && errno == EINTR);
    if (pr < 0) goto fail;

    for (int k = 0; k < 2; k++) {
      nv_mac_gbuf_t* g = (k == 0) ? &go : &ge;
      int* done = (k == 0) ? &out_done : &err_done;
      if (*done) continue;
      if (pf[k].revents & (POLLIN | POLLHUP | POLLERR | POLLNVAL)) {
        char temp[8192];
        ssize_t nr;
        do {
          nr = read((k == 0) ? out_fd : err_fd, temp, sizeof temp);
        } while (nr < 0 && errno == EINTR);
        if (nr < 0) goto fail;
        if (nr == 0) {
          *done = 1;
          pf[k].fd = -1;
        } else {
          if (nv_mac_gbuf_append(g, temp, (size_t)nr) != 0) goto fail;
        }
      }
    }
  }

  {
    char* fo = (char*)realloc(go.data, go.len + 1);
    char* fe = (char*)realloc(ge.data, ge.len + 1);
    if (!fo) {
      free(go.data);
      free(ge.data);
      return -1;
    }
    if (!fe) {
      free(fo);
      free(ge.data);
      return -1;
    }
    fo[go.len] = '\0';
    fe[ge.len] = '\0';
    *out_s = fo;
    *err_s = fe;
    *out_trunc = go.truncated;
    *err_trunc = ge.truncated;
  }
  return 0;

fail:
  free(go.data);
  free(ge.data);
  return -1;
}

NV_INTERNAL nv_shell_result_t* nv_mac_shell_exec(const char* cmd) {
  int out_pipe[2] = {-1, -1};
  int err_pipe[2] = {-1, -1};
  int null_fd = -1;
  posix_spawn_file_actions_t fa;
  pid_t pid = 0;
  nv_shell_result_t* res = NULL;
  char* argv[] = {"sh", "-c", (char*)cmd, NULL};
  int st = 0;
  int out_trunc = 0;
  int err_trunc = 0;

  if (!cmd) return NULL;

  if (pipe(out_pipe) != 0 || pipe(err_pipe) != 0) goto fail;
  null_fd = open("/dev/null", O_RDONLY);
  if (null_fd < 0) goto fail;

  if (posix_spawn_file_actions_init(&fa) != 0) goto fail;
  st = 1;
  if (posix_spawn_file_actions_adddup2(&fa, null_fd, STDIN_FILENO) != 0) goto fail;
  if (posix_spawn_file_actions_addclose(&fa, null_fd) != 0) goto fail;
  if (posix_spawn_file_actions_addclose(&fa, out_pipe[0]) != 0) goto fail;
  if (posix_spawn_file_actions_adddup2(&fa, out_pipe[1], STDOUT_FILENO) != 0) goto fail;
  if (posix_spawn_file_actions_addclose(&fa, out_pipe[1]) != 0) goto fail;
  if (posix_spawn_file_actions_addclose(&fa, err_pipe[0]) != 0) goto fail;
  if (posix_spawn_file_actions_adddup2(&fa, err_pipe[1], STDERR_FILENO) != 0) goto fail;
  if (posix_spawn_file_actions_addclose(&fa, err_pipe[1]) != 0) goto fail;

  if (posix_spawn(&pid, "/bin/sh", &fa, NULL, argv, environ) != 0) goto fail;

  close(null_fd);
  null_fd = -1;
  close(out_pipe[1]);
  out_pipe[1] = -1;
  close(err_pipe[1]);
  err_pipe[1] = -1;
  posix_spawn_file_actions_destroy(&fa);
  st = 0;

  res = (nv_shell_result_t*)calloc(1, sizeof(nv_shell_result_t));
  if (!res) goto fail;

  if (nv_mac_read_both_pipes(out_pipe[0], err_pipe[0], &res->out, &res->err, &out_trunc,
                             &err_trunc) != 0)
    goto fail;

  close(out_pipe[0]);
  out_pipe[0] = -1;
  close(err_pipe[0]);
  err_pipe[0] = -1;

  {
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) goto fail;
    pid = 0;
    if (WIFEXITED(status)) {
      res->exit_code = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
      res->exit_code = 128 + WTERMSIG(status);
    } else {
      res->exit_code = -1;
    }
  }
  res->truncated = (out_trunc || err_trunc) ? 1 : 0;
  return res;

fail:
  if (pid > 0) {
    if (out_pipe[0] >= 0) {
      close(out_pipe[0]);
      out_pipe[0] = -1;
    }
    if (err_pipe[0] >= 0) {
      close(err_pipe[0]);
      err_pipe[0] = -1;
    }
    (void)waitpid(pid, NULL, 0);
    pid = 0;
  }
  if (st) posix_spawn_file_actions_destroy(&fa);
  if (null_fd >= 0) close(null_fd);
  if (out_pipe[0] >= 0) close(out_pipe[0]);
  if (out_pipe[1] >= 0) close(out_pipe[1]);
  if (err_pipe[0] >= 0) close(err_pipe[0]);
  if (err_pipe[1] >= 0) close(err_pipe[1]);
  if (res) {
    free(res->out);
    free(res->err);
    free(res);
  }
  return NULL;
}

/* ===== Clipboard platform hooks (text only) ===== */
NV_INTERNAL char* nv_mac_clipboard_read_text(void) {
  @autoreleasepool {
    NSPasteboard *pb = [NSPasteboard generalPasteboard];
    NSString *s = [pb stringForType:NSPasteboardTypeString];
    if (!s) return NULL;
    const char* utf8 = [s UTF8String];
    if (!utf8) return NULL;
    size_t len = strlen(utf8);
    char* out = (char*)malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, utf8, len + 1);
    return out;
  }
}

NV_INTERNAL int nv_mac_clipboard_write_text(const char* text) {
  if (!text) return -1;
  @autoreleasepool {
    NSPasteboard *pb = [NSPasteboard generalPasteboard];
    [pb clearContents];
    BOOL ok = [pb setString:[NSString stringWithUTF8String:text] forType:NSPasteboardTypeString];
    return ok ? 0 : -1;
  }
}

NV_INTERNAL void nv_mac_clipboard_clear(void) {
  @autoreleasepool {
    NSPasteboard *pb = [NSPasteboard generalPasteboard];
    [pb clearContents];
  }
}

NV_INTERNAL int nv_mac_clipboard_has_text(void) {
  @autoreleasepool {
    NSPasteboard *pb = [NSPasteboard generalPasteboard];
    NSString *available = [pb availableTypeFromArray:@[NSPasteboardTypeString]];
    int has = (available != nil) ? 1 : 0;
    return has;
  }
}

#if defined(__clang__) || defined(__GNUC__)
#define NV_MAC_CLIP_IMG_ATTR __attribute__((weak))
#else
#define NV_MAC_CLIP_IMG_ATTR
#endif

static void nv_mac_png_ihdr_dims(NSData *data, int *w, int *h) {
  static const uint8_t sig[8] = {137u, 80u, 78u, 71u, 13u, 10u, 26u, 10u};
  if (w) *w = 0;
  if (h) *h = 0;
  if (!data || data.length < 24u) return;
  const uint8_t *p = (const uint8_t *)data.bytes;
  if (memcmp(p, sig, 8u) != 0) return;
  if (memcmp(p + 12u, "IHDR", 4u) != 0) return;
  if (w)
    *w = (int)((unsigned)p[16] << 24 | (unsigned)p[17] << 16 | (unsigned)p[18] << 8 | (unsigned)p[19]);
  if (h)
    *h = (int)((unsigned)p[20] << 24 | (unsigned)p[21] << 16 | (unsigned)p[22] << 8 | (unsigned)p[23]);
}

NV_INTERNAL NV_MAC_CLIP_IMG_ATTR char *nv_mac_clipboard_read_image(int *out_w, int *out_h) {
  if (out_w) *out_w = 0;
  if (out_h) *out_h = 0;
  @autoreleasepool {
    NSPasteboard *pb = [NSPasteboard generalPasteboard];
    NSData *png = [pb dataForType:NSPasteboardTypePNG];
    if (png && png.length > 0u) {
      int w = 0, h = 0;
      NSBitmapImageRep *rep = [[NSBitmapImageRep alloc] initWithData:png];
      if (rep) {
        w = (int)rep.pixelsWide;
        h = (int)rep.pixelsHigh;
      }
      if (w <= 0 || h <= 0) nv_mac_png_ihdr_dims(png, &w, &h);
      if (w <= 0 || h <= 0) return NULL;
      NSString *b64 = [png base64EncodedStringWithOptions:0];
      const char *utf = [b64 UTF8String];
      if (!utf) return NULL;
      size_t n = strlen(utf) + 1u;
      char *out = (char *)malloc(n);
      if (!out) return NULL;
      memcpy(out, utf, n);
      if (out_w) *out_w = w;
      if (out_h) *out_h = h;
      return out;
    }
    NSImage *img = [[NSImage alloc] initWithPasteboard:pb];
    if (!img || img.size.width < 1.0 || img.size.height < 1.0) return NULL;
    NSBitmapImageRep *brep = nil;
    for (NSImageRep *r in img.representations) {
      if ([r isKindOfClass:[NSBitmapImageRep class]]) {
        brep = (NSBitmapImageRep *)r;
        break;
      }
    }
    if (!brep) {
      NSData *tiff = [img TIFFRepresentation];
      if (!tiff) return NULL;
      brep = [[NSBitmapImageRep alloc] initWithData:tiff];
    }
    if (!brep) return NULL;
    NSData *pngOut = [brep representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
    if (!pngOut || pngOut.length < 24u) return NULL;
    int w = (int)brep.pixelsWide;
    int h = (int)brep.pixelsHigh;
    if (w <= 0 || h <= 0) nv_mac_png_ihdr_dims(pngOut, &w, &h);
    if (w <= 0 || h <= 0) return NULL;
    NSString *b64 = [pngOut base64EncodedStringWithOptions:0];
    const char *utf = [b64 UTF8String];
    if (!utf) return NULL;
    size_t n = strlen(utf) + 1u;
    char *out = (char *)malloc(n);
    if (!out) return NULL;
    memcpy(out, utf, n);
    if (out_w) *out_w = w;
    if (out_h) *out_h = h;
    return out;
  }
}

NV_INTERNAL NV_MAC_CLIP_IMG_ATTR int nv_mac_clipboard_write_image(const char *base64_png) {
  if (!base64_png) return -1;
  @autoreleasepool {
    NSString *s = [NSString stringWithUTF8String:base64_png];
    if (!s) return -1;
    NSData *data = [[NSData alloc] initWithBase64EncodedString:s
                                                       options:NSDataBase64DecodingIgnoreUnknownCharacters];
    if (!data || data.length < 24u) return -1;
    static const uint8_t sig[8] = {137u, 80u, 78u, 71u, 13u, 10u, 26u, 10u};
    const uint8_t *b = (const uint8_t *)data.bytes;
    if (memcmp(b, sig, 8u) != 0) return -1;
    NSPasteboard *pb = [NSPasteboard generalPasteboard];
    [pb clearContents];
    BOOL ok = [pb setData:data forType:NSPasteboardTypePNG];
    return ok ? 0 : -1;
  }
}

NV_INTERNAL NV_MAC_CLIP_IMG_ATTR int nv_mac_clipboard_has_image(void) {
  @autoreleasepool {
    NSPasteboard *pb = [NSPasteboard generalPasteboard];
    NSArray *types = @[ NSPasteboardTypePNG, NSPasteboardTypeTIFF ];
    return ([pb availableTypeFromArray:types] != nil) ? 1 : 0;
  }
}

/* ===== Screen platform hooks (JSON malloc'd for nv_op_screen) ===== */

static CGFloat nv_mac_screens_max_top(void) {
  CGFloat maxTop = 0.0;
  for (NSScreen *s in [NSScreen screens]) {
    CGFloat t = NSMaxY(s.frame);
    if (t > maxTop) maxTop = t;
  }
  return maxTop;
}

static NSDictionary *nv_mac_flipped_rect_dict(CGFloat maxTop, NSRect r) {
  int x = (int)lround(r.origin.x);
  int y = (int)lround(maxTop - NSMaxY(r));
  int w = (int)lround(r.size.width);
  int h = (int)lround(r.size.height);
  return @{@"x" : @(x), @"y" : @(y), @"width" : @(w), @"height" : @(h)};
}

static char *nv_mac_strdup_json_object(id obj) {
  if (!obj) return NULL;
  NSError *err = nil;
  NSData *data = [NSJSONSerialization dataWithJSONObject:obj options:0 error:&err];
  (void)err;
  if (!data) return NULL;
  NSString *j = [[[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding] autorelease];
  const char *u = j ? [j UTF8String] : NULL;
  return u ? strdup(u) : NULL;
}

NV_INTERNAL char *nv_mac_screen_get_all(void) {
  __block char *out = NULL;
  void (^work)(void) = ^{
    @autoreleasepool {
      NSArray<NSScreen *> *screens = [NSScreen screens];
      NSScreen *mainSc = [NSScreen mainScreen];
      CGFloat maxTop = nv_mac_screens_max_top();
      NSMutableArray *arr = [NSMutableArray arrayWithCapacity:[screens count]];
      for (NSUInteger i = 0; i < [screens count]; i++) {
        NSScreen *s = screens[i];
        NSRect f = [s frame];
        NSRect vf = [s visibleFrame];
        NSDictionary *bounds = nv_mac_flipped_rect_dict(maxTop, f);
        NSDictionary *workArea = nv_mac_flipped_rect_dict(maxTop, vf);
        BOOL isPrimary = (s == mainSc);
        double scale = (double)[s backingScaleFactor];
        NSString *loc = @"";
        if (@available(macOS 10.15, *)) {
          loc = [s localizedName] ?: @"";
        }
        NSDictionary *entry = @{
          @"id" : @(i),
          @"x" : bounds[@"x"],
          @"y" : bounds[@"y"],
          @"width" : bounds[@"width"],
          @"height" : bounds[@"height"],
          @"scaleFactor" : @(scale),
          @"localizedName" : loc,
          @"isPrimary" : @(isPrimary),
          @"bounds" : bounds,
          @"workArea" : workArea,
        };
        [arr addObject:entry];
      }
      out = nv_mac_strdup_json_object(arr);
    }
  };
  if ([NSThread isMainThread]) {
    work();
  } else {
    dispatch_sync(dispatch_get_main_queue(), work);
  }
  return out;
}

NV_INTERNAL char *nv_mac_screen_get_primary(void) {
  __block char *out = NULL;
  void (^work)(void) = ^{
    @autoreleasepool {
      NSScreen *s = [NSScreen mainScreen];
      if (!s) {
        out = NULL;
        return;
      }
      CGFloat maxTop = nv_mac_screens_max_top();
      NSRect f = [s frame];
      NSRect vf = [s visibleFrame];
      NSDictionary *bounds = nv_mac_flipped_rect_dict(maxTop, f);
      NSDictionary *workArea = nv_mac_flipped_rect_dict(maxTop, vf);
      double scale = (double)[s backingScaleFactor];
      NSString *loc = @"";
      if (@available(macOS 10.15, *)) {
        loc = [s localizedName] ?: @"";
      }
      NSDictionary *entry = @{
        @"id" : @(0),
        @"x" : bounds[@"x"],
        @"y" : bounds[@"y"],
        @"width" : bounds[@"width"],
        @"height" : bounds[@"height"],
        @"scaleFactor" : @(scale),
        @"localizedName" : loc,
        @"isPrimary" : @(YES),
        @"bounds" : bounds,
        @"workArea" : workArea,
      };
      out = nv_mac_strdup_json_object(entry);
    }
  };
  if ([NSThread isMainThread]) {
    work();
  } else {
    dispatch_sync(dispatch_get_main_queue(), work);
  }
  return out;
}

NV_INTERNAL char *nv_mac_screen_get_cursor(void) {
  __block char *out = NULL;
  void (^work)(void) = ^{
    @autoreleasepool {
      CGFloat maxTop = nv_mac_screens_max_top();
      NSPoint p = [NSEvent mouseLocation];
      int cx = (int)lround(p.x);
      int cy = (int)lround(maxTop - p.y);
      NSDictionary *pt = @{@"x" : @(cx), @"y" : @(cy)};
      out = nv_mac_strdup_json_object(pt);
    }
  };
  if ([NSThread isMainThread]) {
    work();
  } else {
    dispatch_sync(dispatch_get_main_queue(), work);
  }
  return out;
}

/* ===== Dialog platform hooks ===== */
NV_INTERNAL void nv_mac_dialog_open_file_async(int allow_multiple, nv_dialog_ctx_t* ctx, nv_dialog_cb_t callback) {
  if (!ctx || !callback) return;
  if (![NSThread isMainThread] || ![NSApp isRunning]) { 
    callback(ctx, 1, NULL);
    return;
  }
  
  @autoreleasepool {
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    [panel setAllowsMultipleSelection: allow_multiple ? YES : NO];
    [panel setCanChooseFiles:YES];
    [panel setCanChooseDirectories:NO];
    
    /* Use async completion handler to avoid blocking */
    [panel beginWithCompletionHandler:^(NSInteger result) {
      dispatch_async(dispatch_get_main_queue(), ^{
        @autoreleasepool {
          int canceled = (result != NSModalResponseOK);
          char** paths = NULL;
          size_t count = 0;
          
          if (!canceled) {
            NSArray<NSURL*> *urls = [panel URLs];
            count = (size_t)[urls count];
            if (count > 0) {
              paths = (char**)malloc(sizeof(char*) * count + sizeof(size_t));
              *((size_t*)paths) = count;  /* Store count before paths */
              paths = (char**)((size_t*)paths + 1);  /* Move pointer past count */
              size_t idx = 0;
              for (NSURL* u in urls) {
                const char* p = [[u path] UTF8String];
                paths[idx++] = p ? strdup(p) : strdup("");
              }
            }
          }
          
          callback(ctx, canceled, paths);
        }
      });
    }];
  }
}

NV_INTERNAL void nv_mac_dialog_save_file_async(nv_dialog_ctx_t* ctx, nv_dialog_cb_t callback) {
  if (!ctx || !callback) return;
  if (![NSThread isMainThread] || ![NSApp isRunning]) {
    callback(ctx, 1, NULL);
    return;
  }
  
  @autoreleasepool {
    NSSavePanel *panel = [NSSavePanel savePanel];
    
    /* Use async completion handler to avoid blocking */
    [panel beginWithCompletionHandler:^(NSInteger result) {
      dispatch_async(dispatch_get_main_queue(), ^{
        @autoreleasepool {
          int canceled = (result != NSModalResponseOK);
          char* path = NULL;
          
          if (!canceled) {
            NSURL *u = [panel URL];
            if (u) {
              const char* p = [[u path] UTF8String];
              if (p) path = strdup(p);
            }
          }
          
          callback(ctx, canceled, (void*)path);
        }
      });
    }];
  }
}

NV_INTERNAL void nv_mac_dialog_open_folder_async(nv_dialog_ctx_t* ctx, nv_dialog_cb_t callback) {
  if (!ctx || !callback) return;
  if (![NSThread isMainThread] || ![NSApp isRunning]) {
    callback(ctx, 1, NULL);
    return;
  }
  
  @autoreleasepool {
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    [panel setAllowsMultipleSelection:NO];
    [panel setCanChooseFiles:NO];
    [panel setCanChooseDirectories:YES];
    
    /* Use async completion handler to avoid blocking */
    [panel beginWithCompletionHandler:^(NSInteger result) {
      dispatch_async(dispatch_get_main_queue(), ^{
        @autoreleasepool {
          int canceled = (result != NSModalResponseOK);
          char* path = NULL;
          
          if (!canceled) {
            NSURL *u = [[panel URLs] firstObject];
            if (u) {
              const char* p = [[u path] UTF8String];
              if (p) path = strdup(p);
            }
          }
          
          callback(ctx, canceled, (void*)path);
        }
      });
    }];
  }
}

NV_INTERNAL void nv_mac_dialog_message_async(const char* title, const char* body, const char* type, const char** buttons, size_t btn_count, nv_dialog_ctx_t* ctx, nv_dialog_cb_t callback) {
  if (!ctx || !callback) return;
  if (![NSThread isMainThread] || ![NSApp isRunning]) {
    int* idx = (int*)malloc(sizeof(int));
    *idx = 0;
    callback(ctx, 0, (void*)idx);
    return;
  }
  
  @autoreleasepool {
    NSAlert *alert = [[NSAlert alloc] init];
    [alert setMessageText: title ? [NSString stringWithUTF8String:title] : @"Message"];
    [alert setInformativeText: body ? [NSString stringWithUTF8String:body] : @""];
    if (type && strcmp(type, "warning") == 0) [alert setAlertStyle:NSAlertStyleWarning];
    else if (type && strcmp(type, "error") == 0) [alert setAlertStyle:NSAlertStyleCritical];
    else [alert setAlertStyle:NSAlertStyleInformational];
    
    if (btn_count == 0) btn_count = 1;
    for (size_t i = 0; i < btn_count; i++) {
      const char* b = buttons && buttons[i] ? buttons[i] : (i == 0 ? "OK" : "");
      if (b && b[0]) [alert addButtonWithTitle:[NSString stringWithUTF8String:b]];
    }
    
    /* Use async completion handler to avoid blocking */
    /* Get NSWindow from context */
    NVWindowPlatform *platform = (__bridge NVWindowPlatform *)nv_window_get_platform(ctx->window);
    NSWindow *nsWindow = platform ? platform.window : nil;
    
    [alert beginSheetModalForWindow:nsWindow completionHandler:^(NSModalResponse response) {
      dispatch_async(dispatch_get_main_queue(), ^{
        int* idx = (int*)malloc(sizeof(int));
        *idx = (int)response - NSAlertFirstButtonReturn;
        callback(ctx, 0, (void*)idx);
      });
    }];
  }
}

NV_INTERNAL void nv_mac_dialog_confirm_async(const char* title, const char* body, nv_dialog_ctx_t* ctx, nv_dialog_cb_t callback) {
  if (!ctx || !callback) return;
  if (![NSThread isMainThread] || ![NSApp isRunning]) {
    int* yes = (int*)malloc(sizeof(int));
    *yes = 0;
    callback(ctx, 0, (void*)yes);
    return;
  }
  
  @autoreleasepool {
    NSAlert *alert = [[NSAlert alloc] init];
    [alert setMessageText: title ? [NSString stringWithUTF8String:title] : @"Confirm"];
    [alert setInformativeText: body ? [NSString stringWithUTF8String:body] : @""];
    [alert addButtonWithTitle:@"OK"];
    [alert addButtonWithTitle:@"Cancel"];
    
    /* Use async completion handler to avoid blocking */
    /* Get NSWindow from context */
    NVWindowPlatform *platform = (__bridge NVWindowPlatform *)nv_window_get_platform(ctx->window);
    NSWindow *nsWindow = platform ? platform.window : nil;
    
    [alert beginSheetModalForWindow:nsWindow completionHandler:^(NSModalResponse response) {
      dispatch_async(dispatch_get_main_queue(), ^{
        int* yes = (int*)malloc(sizeof(int));
        *yes = (response == NSAlertFirstButtonReturn) ? 1 : 0;
        callback(ctx, 0, (void*)yes);
      });
    }];
  }
}

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation {
  (void)webView;
  (void)navigation;

  if (!self.cWindow) return;
  nv_ipc_state_t *ipc = nv_window_get_ipc(self.cWindow);
  if (!ipc) return;
  /* Dispatch async so loadHTMLString (called from on_ready / nv_ui_flush) is not
   * invoked from inside the navigation delegate, which can prevent the load. */
  dispatch_async(dispatch_get_main_queue(), ^{
    if (!self.cWindow) return;
    nv_ipc_invoke_ready(self.cWindow, ipc);
  });
}

- (void)webView:(WKWebView *)webView
didFailProvisionalNavigation:(WKNavigation *)navigation
      withError:(NSError *)error {
  (void)webView;
  (void)navigation;
  NV_ERR("macOS: provisional navigation failed: %s",
         error.localizedDescription.UTF8String);
}

- (void)webView:(WKWebView *)webView
didFailNavigation:(WKNavigation *)navigation
      withError:(NSError *)error {
  (void)webView;
  (void)navigation;
  NV_ERR("macOS: navigation failed: %s", error.localizedDescription.UTF8String);
}

@end

/* =============================================================================
 * Helpers
 * =============================================================================
 */

static NVAppPlatform *nv_mac_get_app_platform(nv_window_t *window) {
  if (!window || !window->app) return nil;
  void *opaque = nv_app_get_platform(window->app);
  if (!opaque) return nil;
  return (__bridge NVAppPlatform *)opaque;
}

static NVWindowPlatform *nv_mac_get_window_platform(nv_window_t *window) {
  if (!window) return nil;
  void *opaque = nv_window_get_platform(window);
  if (!opaque) return nil;
  return (__bridge NVWindowPlatform *)opaque;
}

/* =============================================================================
 * App Platform Hooks
 * =============================================================================
 */

NV_INTERNAL void nv_app_platform_init(nv_app_t *app) {
  if (!app) return;

  NSApplication *nsApp = [NSApplication sharedApplication];
  [nsApp setActivationPolicy:NSApplicationActivationPolicyRegular];

  /* Create main menu with Edit submenu so Cmd+C/X/V/A (copy/cut/paste/select all) work.
   * Without this, WKWebView does not receive these keyboard shortcuts. */
  NSMenu *mainMenu = [[NSMenu alloc] init];
  [nsApp setMainMenu:mainMenu];
  [mainMenu release];

  NSMenuItem *editItem = [[NSMenuItem alloc] initWithTitle:@"Edit" action:NULL keyEquivalent:@""];
  [mainMenu addItem:editItem];
  NSMenu *editMenu = [[NSMenu alloc] initWithTitle:@"Edit"];
  [editItem setSubmenu:editMenu];
  [editMenu addItemWithTitle:@"Cut" action:@selector(cut:) keyEquivalent:@"x"];
  [editMenu addItemWithTitle:@"Copy" action:@selector(copy:) keyEquivalent:@"c"];
  [editMenu addItemWithTitle:@"Paste" action:@selector(paste:) keyEquivalent:@"v"];
  [editMenu addItemWithTitle:@"Select All" action:@selector(selectAll:) keyEquivalent:@"a"];
  [editMenu release];
  [editItem release];

  // Build shared WKWebViewConfiguration with non-persistent data store
  WKWebViewConfiguration *config = [[WKWebViewConfiguration alloc] init];
  config.websiteDataStore = [WKWebsiteDataStore nonPersistentDataStore];
  @try {
    // Enable Web Inspector/DevTools for WKWebView
    [config.preferences setValue:@YES forKey:@"developerExtrasEnabled"];
    [[NSUserDefaults standardUserDefaults] setBool:YES forKey:@"WebKitDeveloperExtras"];
  } @catch (__unused NSException *e) {
  }

  WKUserContentController *uc = [[WKUserContentController alloc] init];

  const char *rawScript = nv_ipc_inject_script();
  if (rawScript) {
    NSMutableString *source =
        [NSMutableString stringWithUTF8String:rawScript];

    // Replace placeholder stub with bridge to native.
    [source replaceOccurrencesOfString:@"/*{NV_POST}*/"
                            withString:@"window.webkit.messageHandlers.nvBridge.postMessage"
                               options:0
                                 range:NSMakeRange(0, source.length)];

    WKUserScript *userScript = [[WKUserScript alloc]
        initWithSource:source
         injectionTime:WKUserScriptInjectionTimeAtDocumentStart
      forMainFrameOnly:YES];
    [uc addUserScript:userScript];
  }

  config.userContentController = uc;

  NVAppPlatform *platform = [[NVAppPlatform alloc] init];
  platform.config = config;

  // Retain platform object and stash opaque handle in nv_app_t
  void *opaque = (void *)platform; /* MRC: platform is already retained via alloc/init */
  nv_app_set_platform(app, opaque);
}

NV_INTERNAL void nv_app_platform_run(nv_app_t *app) {
  (void)app;
  [NSApp finishLaunching];
  [NSApp activateIgnoringOtherApps:YES];
  [NSApp run];
}

NV_INTERNAL void nv_app_platform_quit(nv_app_t *app) {
  (void)app;
  if ([NSThread isMainThread]) {
    [NSApp terminate:nil];
    return;
  }
  dispatch_async(dispatch_get_main_queue(), ^{
    [NSApp terminate:nil];
  });
}

/* =============================================================================
 * Window Platform Hooks
 * =============================================================================
 */

NV_INTERNAL void nv_window_platform_create(nv_window_t *window) {
  if (!window) return;

  // For internal tests that don't use nv_app_t, skip real window creation.
  if (!window->app) {
    return;
  }

  NVAppPlatform *appPlatform = nv_mac_get_app_platform(window);
  if (!appPlatform || !appPlatform.config) {
    NV_ERR("macOS: missing app platform configuration");
    return;
  }

  NSRect frame = NSMakeRect(0, 0, window->cfg.width, window->cfg.height);

  NSWindowStyleMask style = 0;
  if (window->cfg.frameless) {
    style = NSWindowStyleMaskBorderless;
  } else {
    style = NSWindowStyleMaskTitled |
            NSWindowStyleMaskClosable |
            NSWindowStyleMaskMiniaturizable;
    if (window->cfg.resizable) {
      style |= NSWindowStyleMaskResizable;
    }
  }

  NSWindow *nsWindow = [[NSWindow alloc]
      initWithContentRect:frame
                styleMask:style
                  backing:NSBackingStoreBuffered
                    defer:NO];

  NSString *title =
      window->cfg.title ? [NSString stringWithUTF8String:window->cfg.title]
                        : @"App";
  [nsWindow setTitle:title];

  if (window->cfg.transparent) {
    nsWindow.opaque = NO;
    nsWindow.backgroundColor = [NSColor clearColor];
  }

  WKWebViewConfiguration *config = [appPlatform.config copy];
  WKUserContentController *newUC = [[WKUserContentController alloc] init];
  for (WKUserScript *script in appPlatform.config.userContentController.userScripts) {
    [newUC addUserScript:script];
  }
  config.userContentController = newUC;

  WKWebView *webView =
      [[WKWebView alloc] initWithFrame:nsWindow.contentView.bounds
                          configuration:config];
  webView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;

  [nsWindow setContentView:webView];

  NVWindowPlatform *platform = [[NVWindowPlatform alloc] init];
  platform.cWindow = window;
  platform.window = nsWindow;
  platform.webView = webView;

  webView.navigationDelegate = platform;
  webView.UIDelegate = platform;
  platform.window.delegate = platform; /* handle close events */

  // Register JS→C bridge handler.
  [webView.configuration.userContentController addScriptMessageHandler:platform
                                                                  name:@"nvBridge"];

  void *opaque = (void *)platform; /* MRC: platform retained via alloc/init */
  nv_window_set_platform(window, opaque);

  /* Load about:blank so didFinishNavigation fires -> on_ready. Without this,
   * WKWebView never fires navigation delegate callbacks. */
  [platform.webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"about:blank"]]];
}

NV_INTERNAL void nv_window_platform_destroy(nv_window_t *window) {
  if (!window || !window->app) return;

  nv_mac_fs_watch_detach_for_window(window);

  void *opaque = nv_window_get_platform(window);
  if (!opaque) return;

  NVWindowPlatform *platform = (NVWindowPlatform *)opaque;
  nv_window_set_platform(window, NULL);

  [platform.window orderOut:nil];
  /* Defer release so we are not deallocated while still in windowWillClose:. */
  dispatch_async(dispatch_get_main_queue(), ^{
    [platform release]; /* MRC balance for alloc/init */
  });
}

NV_INTERNAL void nv_window_platform_show(nv_window_t *window) {
  if (!window || !window->app) return;
  NVWindowPlatform *platform = nv_mac_get_window_platform(window);
  if (!platform) return;

  [platform.window makeKeyAndOrderFront:nil];
  dispatch_async(dispatch_get_main_queue(), ^{
    [platform.window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];
  });
  [NSApp activateIgnoringOtherApps:YES];
  window->visible = 1;
}

NV_INTERNAL void nv_window_platform_hide(nv_window_t *window) {
  if (!window || !window->app) return;
  NVWindowPlatform *platform = nv_mac_get_window_platform(window);
  if (!platform) return;

  [platform.window orderOut:nil];
  window->visible = 0;
}

NV_INTERNAL void nv_window_platform_load_html(nv_window_t *window, const char *html, const char *base_url) {
  NVWindowPlatform *platform = nv_mac_get_window_platform(window);
  if (!platform || !platform.webView || !html) return;
  NSString *htmlStr = [NSString stringWithUTF8String:html];
  NSURL *base = nil;
  if (base_url && base_url[0]) {
    base = [NSURL URLWithString:[NSString stringWithUTF8String:base_url]];
  }
  [platform.webView loadHTMLString:htmlStr baseURL:base];
}

NV_INTERNAL void nv_window_platform_load_url(nv_window_t *window, const char *url) {
  NVWindowPlatform *platform = nv_mac_get_window_platform(window);
  if (!platform || !platform.webView || !url) return;
  NSString *urlStr = [NSString stringWithUTF8String:url];
  NSURL *nsurl = [NSURL URLWithString:urlStr];
  if (!nsurl) return;

  if ([nsurl isFileURL]) {
    // WKWebView restricts file:// subresource access to the same directory tree.
    // Our demo loads ../../js from megademo/web, so grant read access to repo root.
    NSURL *access = [nsurl URLByDeletingLastPathComponent]; // .../megademo/web
    if (access) access = [access URLByDeletingLastPathComponent]; // .../megademo
    if (access) access = [access URLByDeletingLastPathComponent]; // .../<repo root>
    if (!access) access = [nsurl URLByDeletingLastPathComponent];
    if (@available(macOS 10.11, *)) {
      [platform.webView loadFileURL:nsurl allowingReadAccessToURL:access];
      return;
    }
    // Fallback for older systems: continue to loadRequest (may block subresources).
  }

  NSURLRequest *req = [NSURLRequest requestWithURL:nsurl];
  [platform.webView loadRequest:req];
}

/* UTF-8 replacement character */
static const char kUTF8Replacement[] = "\xEF\xBF\xBD";

/* Sanitize invalid UTF-8 to valid UTF-8. Caller frees returned buffer. */
static char* nv_sanitize_utf8(const char* in, size_t in_len, size_t* out_len) {
  size_t buf_size = in_len * 3 + 1;
  char* out = (char*)malloc(buf_size);
  if (!out) return NULL;
  size_t j = 0;
  size_t i = 0;
  while (i < in_len) {
    unsigned char c = (unsigned char)in[i];
    if (c <= 0x7F) {
      if (j + 1 >= buf_size) break;
      out[j++] = in[i++];
    } else if ((c & 0xE0) == 0xC0) {
      if (i + 2 <= in_len && ((unsigned char)in[i+1] & 0xC0) == 0x80 &&
          (c >= 0xC2)) {
        if (j + 2 >= buf_size) break;
        out[j++] = in[i++]; out[j++] = in[i++];
      } else {
        if (j + 3 >= buf_size) break;
        memcpy(out + j, kUTF8Replacement, 3); j += 3; i++;
      }
    } else if ((c & 0xF0) == 0xE0) {
      if (i + 3 <= in_len && ((unsigned char)in[i+1] & 0xC0) == 0x80 &&
          ((unsigned char)in[i+2] & 0xC0) == 0x80) {
        if (j + 3 >= buf_size) break;
        out[j++] = in[i++]; out[j++] = in[i++]; out[j++] = in[i++];
      } else {
        if (j + 3 >= buf_size) break;
        memcpy(out + j, kUTF8Replacement, 3); j += 3; i++;
      }
    } else if ((c & 0xF8) == 0xF0) {
      if (i + 4 <= in_len && ((unsigned char)in[i+1] & 0xC0) == 0x80 &&
          ((unsigned char)in[i+2] & 0xC0) == 0x80 &&
          ((unsigned char)in[i+3] & 0xC0) == 0x80 &&
          (c <= 0xF4)) {
        if (j + 4 >= buf_size) break;
        out[j++] = in[i++]; out[j++] = in[i++]; out[j++] = in[i++]; out[j++] = in[i++];
      } else {
        if (j + 3 >= buf_size) break;
        memcpy(out + j, kUTF8Replacement, 3); j += 3; i++;
      }
    } else {
      if (j + 3 >= buf_size) break;
      memcpy(out + j, kUTF8Replacement, 3); j += 3; i++;
    }
  }
  out[j] = '\0';
  if (out_len) *out_len = j;
  return out;
}

static void nv_eval_js_impl(NVWindowPlatform *platform, NSString *code) {
  if (!platform || !platform.webView || !code) return;
  [platform.webView evaluateJavaScript:code completionHandler:nil];
}

NV_INTERNAL void nv_window_platform_eval_js(nv_window_t *window, const char *js) {
  NVWindowPlatform *platform = nv_mac_get_window_platform(window);
  if (!platform || !platform.webView || !js) return;
  size_t len = strlen(js);
  /* Sanitize UTF-8 before passing to NSString; invalid bytes can crash stringWithUTF8String */
  size_t sane_len = 0;
  char* sane = nv_sanitize_utf8(js, len, &sane_len);
  if (!sane) {
    return;
  }
  NSString *code = [NSString stringWithUTF8String:sane];
  free(sane);
  if (!code) {
    return;
  }
  if ([NSThread isMainThread]) {
    nv_eval_js_impl(platform, code);
  } else {
    NSString *codeCopy = [code copy];
    dispatch_async(dispatch_get_main_queue(), ^{
      nv_eval_js_impl(platform, codeCopy);
    });
  }
}

NV_INTERNAL void nv_window_platform_get_position(nv_window_t *window, int *out_x, int *out_y) {
  if (!out_x || !out_y) return;
  *out_x = 0; *out_y = 0;
  if (!window || !window->app) return;
  NVWindowPlatform *platform = nv_mac_get_window_platform(window);
  if (!platform || !platform.window) return;
  NSRect frame = [platform.window frame];
  *out_x = (int)frame.origin.x;
  *out_y = (int)frame.origin.y;
}

NV_INTERNAL void nv_window_platform_center(nv_window_t *window) {
  if (!window || !window->app) return;
  NVWindowPlatform *platform = nv_mac_get_window_platform(window);
  if (!platform || !platform.window) return;
  [platform.window center];
}

NV_INTERNAL void nv_window_platform_minimize(nv_window_t *window) {
  if (!window || !window->app) return;
  NVWindowPlatform *platform = nv_mac_get_window_platform(window);
  if (!platform || !platform.window) return;
  [platform.window miniaturize:nil];
}

NV_INTERNAL void nv_window_platform_maximize(nv_window_t *window) {
  if (!window || !window->app) return;
  NVWindowPlatform *platform = nv_mac_get_window_platform(window);
  if (!platform || !platform.window) return;
  [platform.window zoom:nil];
}

NV_INTERNAL void nv_window_platform_restore(nv_window_t *window) {
  if (!window || !window->app) return;
  NVWindowPlatform *platform = nv_mac_get_window_platform(window);
  if (!platform || !platform.window) return;
  [platform.window deminiaturize:nil];
}

NV_INTERNAL void nv_window_platform_fullscreen(nv_window_t *window, int enable) {
  if (!window || !window->app) return;
  NVWindowPlatform *platform = nv_mac_get_window_platform(window);
  if (!platform || !platform.window) return;
  BOOL isFs = ([platform.window styleMask] & NSWindowStyleMaskFullScreen) != 0;
  if ((enable && !isFs) || (!enable && isFs)) {
    [platform.window toggleFullScreen:nil];
  }
}

NV_INTERNAL int nv_window_platform_is_fullscreen(nv_window_t *window) {
  if (!window || !window->app) return 0;
  NVWindowPlatform *platform = nv_mac_get_window_platform(window);
  if (!platform || !platform.window) return 0;
  return (([platform.window styleMask] & NSWindowStyleMaskFullScreen) != 0) ? 1 : 0;
}

NV_INTERNAL void nv_window_platform_focus(nv_window_t *window) {
  if (!window || !window->app) return;
  NVWindowPlatform *platform = nv_mac_get_window_platform(window);
  if (!platform || !platform.window) return;
  [platform.window makeKeyAndOrderFront:nil];
}

NV_INTERNAL int nv_window_platform_is_focused(nv_window_t *window) {
  if (!window || !window->app) return 0;
  NVWindowPlatform *platform = nv_mac_get_window_platform(window);
  if (!platform || !platform.window) return 0;
  return [platform.window isKeyWindow] ? 1 : 0;
}

NV_INTERNAL void nv_window_platform_set_resizable(nv_window_t *window, int enable) {
  if (!window || !window->app) return;
  NVWindowPlatform *platform = nv_mac_get_window_platform(window);
  if (!platform || !platform.window) return;
  NSWindowStyleMask mask = [platform.window styleMask];
  if (enable) mask |= NSWindowStyleMaskResizable;
  else mask &= ~NSWindowStyleMaskResizable;
  [platform.window setStyleMask:mask];
}

NV_INTERNAL void nv_window_platform_set_always_on_top(nv_window_t *window, int enable) {
  if (!window || !window->app) return;
  NVWindowPlatform *platform = nv_mac_get_window_platform(window);
  if (!platform || !platform.window) return;
  [platform.window setLevel: enable ? NSFloatingWindowLevel : NSNormalWindowLevel];
}

NV_INTERNAL void nv_window_platform_set_opacity(nv_window_t *window, int opacity_pct) {
  if (!window || !window->app) return;
  NVWindowPlatform *platform = nv_mac_get_window_platform(window);
  if (!platform || !platform.window) return;
  CGFloat alpha = (CGFloat)opacity_pct / 100.0;
  if (alpha < 0.0) alpha = 0.0; if (alpha > 1.0) alpha = 1.0;
  [platform.window setAlphaValue:alpha];
}

NV_INTERNAL void nv_window_platform_set_zoom_factor(nv_window_t *window, double factor) {
  if (!window || !window->app) return;
  NVWindowPlatform *platform = nv_mac_get_window_platform(window);
  if (!platform || !platform.webView) return;
  CGFloat z = (CGFloat)factor;
  if (z < 0.25) z = 0.25;
  if (z > 5.0) z = 5.0;
  void (^applyZoom)(void) = ^{
    if ([platform.webView respondsToSelector:@selector(setPageZoom:)]) {
      platform.webView.pageZoom = z;
      return;
    }
    if ([platform.webView respondsToSelector:@selector(setMagnification:)]) {
      [platform.webView setMagnification:z];
    }
  };
  if ([NSThread isMainThread]) {
    applyZoom();
    return;
  }
  dispatch_async(dispatch_get_main_queue(), applyZoom);
}

NV_INTERNAL void nv_window_platform_set_modal(nv_window_t *window, int enable) {
  if (!window || !window->app) return;
  NVWindowPlatform *platform = nv_mac_get_window_platform(window);
  if (!platform || !platform.window) return;
  
  if (enable) {
    // Make window modal by setting it as a modal dialog
    [platform.window setLevel:NSModalPanelWindowLevel];
  } else {
    // Return to normal level
    [platform.window setLevel:NSNormalWindowLevel];
  }
}

NV_INTERNAL void nv_window_platform_close(nv_window_t *window) {
  if (!window || !window->app) return;
  NVWindowPlatform *platform = nv_mac_get_window_platform(window);
  if (!platform || !platform.window) return;
  platform.allowClose = YES;
  NSWindow *w = platform.window;
  if ([NSThread isMainThread]) {
    [w close];
    return;
  }
  dispatch_async(dispatch_get_main_queue(), ^{
    [w close];
  });
}
NV_INTERNAL void nv_window_platform_set_title(nv_window_t *window, const char *title) {
  if (!window || !window->app) return; // guard for headless
  NVWindowPlatform *platform = nv_mac_get_window_platform(window);
  if (!platform || !platform.window || !title) return;
  NSString *t = [NSString stringWithUTF8String:title];
  [platform.window setTitle:t];
}

NV_INTERNAL void nv_window_platform_set_size(nv_window_t *window, int width, int height) {
  if (!window || !window->app) return; // guard for headless
  NVWindowPlatform *platform = nv_mac_get_window_platform(window);
  if (!platform || !platform.window) return;
  NSRect frame = [platform.window frame];
  frame.size = NSMakeSize(width, height);
  [platform.window setFrame:frame display:YES];
}

NV_INTERNAL void nv_window_platform_get_size(nv_window_t *window, int *out_w, int *out_h) {
  if (!out_w || !out_h) return;
  if (!window || !window->app) return; // guard for headless
  NVWindowPlatform *platform = nv_mac_get_window_platform(window);
  if (!platform || !platform.window) return;
  NSRect frame = [platform.window frame];
  *out_w = (int)frame.size.width;
  *out_h = (int)frame.size.height;
}

NV_INTERNAL void nv_window_platform_set_position(nv_window_t *window, int x, int y) {
  if (!window || !window->app) return; // guard for headless
  NVWindowPlatform *platform = nv_mac_get_window_platform(window);
  if (!platform || !platform.window) return;
  NSRect frame = [platform.window frame];
  frame.origin = NSMakePoint(x, y);
  [platform.window setFrame:frame display:YES];
}

/* =============================================================================
 * App paths helpers
 * =============================================================================
 */
NV_INTERNAL char* nv_mac_get_exe_path(void) {
  @autoreleasepool {
    NSString* path = [[NSBundle mainBundle] executablePath];
    if (!path) {
      path = [[[NSProcessInfo processInfo] arguments] firstObject];
    }
    if (!path) return NULL;
    const char* c = [path fileSystemRepresentation];
    return c ? strdup(c) : NULL;
  }
}

NV_INTERNAL char* nv_mac_get_resource_dir(void) {
  @autoreleasepool {
    NSString* dir = [[NSBundle mainBundle] resourcePath];
    if (!dir) return NULL;
    const char* c = [dir fileSystemRepresentation];
    return c ? strdup(c) : NULL;
  }
}

NV_INTERNAL char* nv_mac_get_data_dir(void) {
  @autoreleasepool {
    NSArray<NSURL*>* urls = [[NSFileManager defaultManager] URLsForDirectory:NSApplicationSupportDirectory inDomains:NSUserDomainMask];
    NSURL* base = urls.firstObject;
    if (!base) return NULL;
    NSString* bundleName = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleName"];
    NSString* dir = bundleName ? [[base path] stringByAppendingPathComponent:bundleName] : [base path];
    const char* c = [dir fileSystemRepresentation];
    return c ? strdup(c) : NULL;
  }
}
