/* Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0 */

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <WebKit/WebKit.h>

#include <stdlib.h>
#include <string.h>

#include "nv_core_internal.h"
#include "nv_window_internal.h"
#include "nv_ipc_internal.h"
#include "nv_window_manager.h"
#include "nv.h"
#include <pthread.h>

NV_API int nv_is_process_main_thread(void) { return pthread_main_np() != 0; }

static int nv_ios_is_about_blank(const char* url) {
  if (!url) return 0;
  const char* k = "about:blank";
  size_t n = strlen(k);
  if (strncmp(url, k, n) != 0) return 0;
  char c = url[n];
  return c == '\0' || c == '#' || c == '?';
}

static NSURL* nv_ios_base_url_from_cstr(const char* base_url) {
  if (!base_url || !base_url[0]) return nil;
  if (nv_ios_is_about_blank(base_url)) return nil;
  NSString* s = [NSString stringWithUTF8String:base_url];
  if (!s) return nil;
  return [NSURL URLWithString:s];
}

@interface NVIOSAppPlatform : NSObject
@property(nonatomic, retain) WKWebViewConfiguration* config;
@end

@implementation NVIOSAppPlatform
@end

@interface NVIOSWindowPlatform : NSObject <WKScriptMessageHandler, WKNavigationDelegate, WKUIDelegate>
@property(nonatomic, assign) nv_window_t* cWindow;
@property(nonatomic, retain) UIWindow* window;
@property(nonatomic, retain) UIViewController* rootViewController;
@property(nonatomic, retain) WKWebView* webView;
@property(nonatomic, assign) int desiredVisible;
@property(nonatomic, assign) const char* pendingUrl;
@property(nonatomic, assign) const char* pendingHtml;
@property(nonatomic, assign) const char* pendingBaseUrl;
@end

@implementation NVIOSWindowPlatform

- (void)dealloc {
  if (self.webView) {
    @try {
      [self.webView.configuration.userContentController removeScriptMessageHandlerForName:@"nvBridge"];
    } @catch (__unused NSException* e) {
    }
  }
  self.webView.navigationDelegate = nil;
  self.webView.UIDelegate = nil;
  self.webView = nil;
  self.rootViewController = nil;
  self.window = nil;
  [super dealloc];
}

- (void)userContentController:(WKUserContentController*)userContentController
      didReceiveScriptMessage:(WKScriptMessage*)message {
  (void)userContentController;

  if (![message.name isEqualToString:@"nvBridge"]) {
    return;
  }
  if (!self.cWindow) {
    return;
  }

  id body = message.body;
  if (![body isKindOfClass:[NSString class]]) {
    NV_ERR("iOS: nvBridge expected string body, got %s",
           NSStringFromClass([body class]).UTF8String);
    return;
  }

  nv_ipc_state_t* ipc = nv_window_get_ipc(self.cWindow);
  if (!ipc) {
    NV_ERR("iOS: no IPC state for window");
    return;
  }

  NSString* jsonStr = (NSString*)body;
  [jsonStr retain];
  nv_window_t* win = self.cWindow;

  dispatch_async(dispatch_get_main_queue(), ^{
    if (win) {
      const char* json = [jsonStr UTF8String];
      if (json) {
        nv_ipc_dispatch(win, ipc, json);
      }
    }
    [jsonStr release];
  });
}

- (void)webView:(WKWebView*)webView didFinishNavigation:(WKNavigation*)navigation {
  (void)webView;
  (void)navigation;

  if (!self.cWindow) return;
  nv_ipc_state_t* ipc = nv_window_get_ipc(self.cWindow);
  if (!ipc) return;
  dispatch_async(dispatch_get_main_queue(), ^{
    if (!self.cWindow) return;
    nv_ipc_invoke_ready(self.cWindow, ipc);
  });
}

- (void)webView:(WKWebView*)webView
    didFailNavigation:(WKNavigation*)navigation
        withError:(NSError*)error {
  (void)webView;
  (void)navigation;
  NV_ERR("iOS: navigation failed: %s", error.localizedDescription.UTF8String);
}

- (void)webView:(WKWebView*)webView
    didFailProvisionalNavigation:(WKNavigation*)navigation
        withError:(NSError*)error {
  (void)webView;
  (void)navigation;
  NV_ERR("iOS: provisional navigation failed: %s", error.localizedDescription.UTF8String);
}

@end

static int g_nv_ios_started = 0;
static UIWindowScene* g_nv_ios_window_scene = nil;

static int nv_ios_is_uikit_running(void) {
  if (![NSThread isMainThread]) {
    __block int running = 0;
    dispatch_sync(dispatch_get_main_queue(), ^{
      UIApplication* app = [UIApplication sharedApplication];
      running = (app && app.delegate) ? 1 : 0;
    });
    return running;
  }
  UIApplication* app = [UIApplication sharedApplication];
  return (app && app.delegate) ? 1 : 0;
}

static NVIOSAppPlatform* nv_ios_get_app_platform(nv_app_t* app) {
  if (!app) return nil;
  void* opaque = nv_app_get_platform(app);
  if (!opaque) return nil;
  return (NVIOSAppPlatform*)opaque;
}

static NVIOSWindowPlatform* nv_ios_get_window_platform(nv_window_t* window) {
  if (!window) return nil;
  void* opaque = nv_window_get_platform(window);
  if (!opaque) return nil;
  return (NVIOSWindowPlatform*)opaque;
}

static UIWindowScene* nv_ios_get_active_window_scene(void) {
  if (g_nv_ios_window_scene) return g_nv_ios_window_scene;
  if (![NSThread isMainThread]) return nil;
  if (@available(iOS 13.0, *)) {
    NSSet* scenes = [UIApplication sharedApplication].connectedScenes;
    for (UIScene* s in scenes) {
      if (![s isKindOfClass:[UIWindowScene class]]) continue;
      UIWindowScene* ws = (UIWindowScene*)s;
      if (ws.activationState == UISceneActivationStateForegroundActive ||
          ws.activationState == UISceneActivationStateForegroundInactive) {
        g_nv_ios_window_scene = [ws retain];
        return g_nv_ios_window_scene;
      }
    }
    for (UIScene* s in scenes) {
      if (![s isKindOfClass:[UIWindowScene class]]) continue;
      UIWindowScene* ws = (UIWindowScene*)s;
      g_nv_ios_window_scene = [ws retain];
      return g_nv_ios_window_scene;
    }
  }
  return nil;
}

static void nv_ios_window_create_ui(nv_window_t* window) {
  if (!window || !window->app) return;

  NVIOSWindowPlatform* platform = nv_ios_get_window_platform(window);
  if (!platform) return;
  if (platform.window || platform.webView) return;

  NVIOSAppPlatform* appPlatform = nv_ios_get_app_platform(window->app);
  if (!appPlatform || !appPlatform.config) {
    NV_ERR("iOS: missing app platform configuration");
    return;
  }

  UIWindowScene* scene = nv_ios_get_active_window_scene();
  if (@available(iOS 13.0, *)) {
    /* In a scene-based app, creating a UIWindow without a UIWindowScene will not
       present reliably. Defer creation until SceneDelegate provides a scene. */
    if (!scene) {
      return;
    }
  }
  CGRect bounds = [UIScreen mainScreen].bounds;
  if (scene && scene.screen) {
    bounds = scene.screen.bounds;
  }
  UIWindow* uiWindow = nil;
  if (scene) {
    uiWindow = [[UIWindow alloc] initWithWindowScene:scene];
    uiWindow.frame = bounds;
  } else {
    uiWindow = [[UIWindow alloc] initWithFrame:bounds];
  }

  UIViewController* vc = [[UIViewController alloc] init];
  vc.view.backgroundColor = [UIColor systemBackgroundColor];
  uiWindow.rootViewController = vc;

  WKWebViewConfiguration* config = [appPlatform.config copy];
  WKUserContentController* newUC = [[WKUserContentController alloc] init];
  for (WKUserScript* script in appPlatform.config.userContentController.userScripts) {
    [newUC addUserScript:script];
  }
  config.userContentController = newUC;

  WKWebView* webView = [[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 0, 0) configuration:config];
  webView.translatesAutoresizingMaskIntoConstraints = NO;
  webView.navigationDelegate = platform;
  webView.UIDelegate = platform;

  [vc.view addSubview:webView];
  [NSLayoutConstraint activateConstraints:@[
    [webView.topAnchor constraintEqualToAnchor:vc.view.topAnchor],
    [webView.bottomAnchor constraintEqualToAnchor:vc.view.bottomAnchor],
    [webView.leadingAnchor constraintEqualToAnchor:vc.view.leadingAnchor],
    [webView.trailingAnchor constraintEqualToAnchor:vc.view.trailingAnchor],
  ]];

  [webView.configuration.userContentController addScriptMessageHandler:platform name:@"nvBridge"];

  platform.window = uiWindow;
  platform.rootViewController = vc;
  platform.webView = webView;

  [config release];
  [newUC release];
  [webView release];
  [vc release];

  if (platform.pendingHtml) {
    NSString* htmlStr = [NSString stringWithUTF8String:platform.pendingHtml];
    NSURL* base = nv_ios_base_url_from_cstr(platform.pendingBaseUrl);
    [platform.webView loadHTMLString:htmlStr baseURL:base];
  } else if (platform.pendingUrl) {
    NSString* urlStr = [NSString stringWithUTF8String:platform.pendingUrl];
    NSURL* nsurl = [NSURL URLWithString:urlStr];
    if (nsurl) {
      [platform.webView loadRequest:[NSURLRequest requestWithURL:nsurl]];
    }
  } else {
    [platform.webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"about:blank"]]];
  }

  if (platform.desiredVisible) {
    uiWindow.hidden = NO;
    [uiWindow makeKeyAndVisible];
    window->visible = 1;
  } else {
    uiWindow.hidden = YES;
    window->visible = 0;
  }

  [uiWindow release];
}

static void nv_ios_window_create(nv_window_t* window) {
  if (!window) return;

  if (!window->app) {
    return;
  }

  NVIOSWindowPlatform* platform = [[NVIOSWindowPlatform alloc] init];
  platform.cWindow = window;
  platform.desiredVisible = 0;
  platform.pendingUrl = NULL;
  platform.pendingHtml = NULL;
  platform.pendingBaseUrl = NULL;
  nv_window_set_platform(window, (void*)platform);

  if (g_nv_ios_started) {
    dispatch_async(dispatch_get_main_queue(), ^{
      nv_ios_window_create_ui(window);
    });
  }
}

static void nv_ios_window_destroy(nv_window_t* window) {
  if (!window || !window->app) return;

  void* opaque = nv_window_get_platform(window);
  if (!opaque) return;

  NVIOSWindowPlatform* platform = (NVIOSWindowPlatform*)opaque;
  nv_window_set_platform(window, NULL);
  platform.cWindow = NULL;

  dispatch_async(dispatch_get_main_queue(), ^{
    if (platform.window) {
      platform.window.hidden = YES;
    }
    [platform release];
  });
}

static void nv_ios_window_show(nv_window_t* window) {
  if (!window || !window->app) return;
  NVIOSWindowPlatform* platform = nv_ios_get_window_platform(window);
  if (!platform) return;
  platform.desiredVisible = 1;
  window->visible = 1;
  if (!g_nv_ios_started) return;
  dispatch_async(dispatch_get_main_queue(), ^{
    nv_ios_window_create_ui(window);
    if (platform.window) {
      platform.window.hidden = NO;
      [platform.window makeKeyAndVisible];
    }
  });
}

static void nv_ios_window_hide(nv_window_t* window) {
  if (!window || !window->app) return;
  NVIOSWindowPlatform* platform = nv_ios_get_window_platform(window);
  if (!platform) return;
  platform.desiredVisible = 0;
  window->visible = 0;
  if (!g_nv_ios_started) return;
  dispatch_async(dispatch_get_main_queue(), ^{
    if (platform.window) {
      platform.window.hidden = YES;
    }
  });
}

static void nv_ios_window_set_title(nv_window_t* window, const char* title) {
  (void)window;
  (void)title;
}

static void nv_ios_window_set_size(nv_window_t* window, int width, int height) {
  (void)window;
  (void)width;
  (void)height;
}

static void nv_ios_window_get_size(nv_window_t* window, int* out_w, int* out_h) {
  if (out_w) *out_w = window ? window->cfg.width : 0;
  if (out_h) *out_h = window ? window->cfg.height : 0;
}

static void nv_ios_window_close(nv_window_t* window) {
  if (!window) return;
  nv_window_destroy(window);
}

static void nv_ios_window_load_html(nv_window_t* window, const char* html, const char* base_url) {
  if (!window || !window->app || !html) return;
  NVIOSWindowPlatform* platform = nv_ios_get_window_platform(window);
  if (!platform) return;

  platform.pendingHtml = nv_arena_str_dup(nv_window_get_arena(window), html);
  platform.pendingBaseUrl = base_url ? nv_arena_str_dup(nv_window_get_arena(window), base_url) : NULL;
  platform.pendingUrl = NULL;

  if (!g_nv_ios_started) return;
  dispatch_async(dispatch_get_main_queue(), ^{
    nv_ios_window_create_ui(window);
    if (!platform.webView) return;
    NSString* htmlStr = [NSString stringWithUTF8String:platform.pendingHtml ? platform.pendingHtml : ""];
    NSURL* base = nv_ios_base_url_from_cstr(platform.pendingBaseUrl);
    [platform.webView loadHTMLString:htmlStr baseURL:base];
  });
}

static void nv_ios_window_load_url(nv_window_t* window, const char* url) {
  if (!window || !window->app || !url) return;
  NVIOSWindowPlatform* platform = nv_ios_get_window_platform(window);
  if (!platform) return;

  platform.pendingUrl = nv_arena_str_dup(nv_window_get_arena(window), url);
  platform.pendingHtml = NULL;
  platform.pendingBaseUrl = NULL;

  if (!g_nv_ios_started) return;
  dispatch_async(dispatch_get_main_queue(), ^{
    nv_ios_window_create_ui(window);
    if (!platform.webView || !platform.pendingUrl) return;
    NSString* urlStr = [NSString stringWithUTF8String:platform.pendingUrl];
    NSURL* nsurl = [NSURL URLWithString:urlStr];
    if (!nsurl) return;
    [platform.webView loadRequest:[NSURLRequest requestWithURL:nsurl]];
  });
}

static void nv_ios_window_eval_js(nv_window_t* window, const char* js) {
  if (!window || !window->app || !js) return;
  NVIOSWindowPlatform* platform = nv_ios_get_window_platform(window);
  if (!platform) return;
  if (!g_nv_ios_started) return;

  const char* arena_js = nv_arena_str_dup(nv_window_get_arena(window), js);
  if (!arena_js) return;

  dispatch_async(dispatch_get_main_queue(), ^{
    if (!platform.webView) {
      nv_ios_window_create_ui(window);
    }
    if (!platform.webView) return;
    NSString* script = [NSString stringWithUTF8String:arena_js];
    [platform.webView evaluateJavaScript:script completionHandler:nil];
  });
}

static int nv_ios_shell_open_url(const char* url) {
  if (!url) return -1;
  @autoreleasepool {
    NSString* s = [NSString stringWithUTF8String:url];
    if (!s) return -1;
    NSURL* u = [NSURL URLWithString:s];
    if (!u) return -1;
    dispatch_async(dispatch_get_main_queue(), ^{
      [[UIApplication sharedApplication] openURL:u options:@{} completionHandler:nil];
    });
    return 0;
  }
}

@interface NVIOSAppDelegate : UIResponder <UIApplicationDelegate>
@property(nonatomic, retain) UIWindow* window;
@end

@interface NVIOSSceneDelegate : UIResponder <UIWindowSceneDelegate>
@property(nonatomic, retain) UIWindow* window;
@end

@implementation NVIOSAppDelegate

- (BOOL)application:(UIApplication*)application didFinishLaunchingWithOptions:(NSDictionary*)launchOptions {
  (void)application;
  (void)launchOptions;

  g_nv_ios_started = 1;

  nv_wm_entry_t entries[NV_MAX_WINDOWS];
  size_t count = NV_MAX_WINDOWS;
  if (nv_wm_list(entries, &count) == 0) {
    for (size_t i = 0; i < count; i++) {
      nv_window_t* w = entries[i].window;
      if (!w) continue;
      NVIOSWindowPlatform* wp = nv_ios_get_window_platform(w);
      if (!wp) {
        continue;
      }
      nv_ios_window_create_ui(w);
    }
  }

  return YES;
}

- (UISceneConfiguration*)application:(UIApplication*)application
    configurationForConnectingSceneSession:(UISceneSession*)connectingSceneSession
                                 options:(UISceneConnectionOptions*)options API_AVAILABLE(ios(13.0)) {
  (void)application;
  (void)options;
  UISceneConfiguration* config =
      [UISceneConfiguration configurationWithName:@"Default Configuration" sessionRole:connectingSceneSession.role];
  config.delegateClass = [NVIOSSceneDelegate class];
  return config;
}

- (void)dealloc {
  self.window = nil;
  [super dealloc];
}

@end

@implementation NVIOSSceneDelegate

- (void)scene:(UIScene*)scene
    willConnectToSession:(UISceneSession*)session
                 options:(UISceneConnectionOptions*)connectionOptions API_AVAILABLE(ios(13.0)) {
  (void)session;
  (void)connectionOptions;

  if (![scene isKindOfClass:[UIWindowScene class]]) return;
  UIWindowScene* ws = (UIWindowScene*)scene;

  if (g_nv_ios_window_scene != ws) {
    [g_nv_ios_window_scene release];
    g_nv_ios_window_scene = [ws retain];
  }

  g_nv_ios_started = 1;

  nv_wm_entry_t entries[NV_MAX_WINDOWS];
  size_t count = NV_MAX_WINDOWS;
  if (nv_wm_list(entries, &count) == 0) {
    for (size_t i = 0; i < count; i++) {
      nv_window_t* w = entries[i].window;
      if (!w) continue;
      NVIOSWindowPlatform* wp = nv_ios_get_window_platform(w);
      if (!wp) continue;
      nv_ios_window_create_ui(w);
    }
  }
}

- (void)sceneDidDisconnect:(UIScene*)scene API_AVAILABLE(ios(13.0)) {
  (void)scene;
  [g_nv_ios_window_scene release];
  g_nv_ios_window_scene = nil;
}

- (void)dealloc {
  self.window = nil;
  [super dealloc];
}

@end

NV_INTERNAL void nv_app_platform_init(nv_app_t* app) {
  if (!app) return;

  app->platform_api.platform_name = "ios";

  app->platform_api.shell_open_url = nv_ios_shell_open_url;

  app->platform_api.window_create = nv_ios_window_create;
  app->platform_api.window_destroy = nv_ios_window_destroy;
  app->platform_api.window_show = nv_ios_window_show;
  app->platform_api.window_hide = nv_ios_window_hide;
  app->platform_api.window_set_title = nv_ios_window_set_title;
  app->platform_api.window_set_size = nv_ios_window_set_size;
  app->platform_api.window_get_size = nv_ios_window_get_size;
  app->platform_api.window_close = nv_ios_window_close;
  app->platform_api.window_load_html = nv_ios_window_load_html;
  app->platform_api.window_load_url = nv_ios_window_load_url;
  app->platform_api.window_eval_js = nv_ios_window_eval_js;

  WKWebViewConfiguration* config = [[WKWebViewConfiguration alloc] init];
  config.websiteDataStore = [WKWebsiteDataStore nonPersistentDataStore];
  @try {
    [config.preferences setValue:@YES forKey:@"developerExtrasEnabled"];
  } @catch (__unused NSException* e) {
  }

  WKUserContentController* uc = [[WKUserContentController alloc] init];
  const char* rawScript = nv_ipc_inject_script();
  if (rawScript) {
    NSMutableString* source = [NSMutableString stringWithUTF8String:rawScript];
    [source replaceOccurrencesOfString:@"/*{NV_POST}*/"
                            withString:@"window.webkit.messageHandlers.nvBridge.postMessage"
                               options:0
                                 range:NSMakeRange(0, source.length)];

    WKUserScript* userScript =
        [[WKUserScript alloc] initWithSource:source
                               injectionTime:WKUserScriptInjectionTimeAtDocumentStart
                            forMainFrameOnly:YES];
    [uc addUserScript:userScript];
    [userScript release];
  }
  config.userContentController = uc;

  NVIOSAppPlatform* platform = [[NVIOSAppPlatform alloc] init];
  platform.config = config;

  nv_app_set_platform(app, (void*)platform);

  if (!g_nv_ios_started && nv_ios_is_uikit_running()) {
    g_nv_ios_started = 1;
  }

  [uc release];
  [config release];
}

NV_INTERNAL void nv_app_platform_run(nv_app_t* app) {
  (void)app;
  @autoreleasepool {
    char argv0[] = "nativeview";
    char* argv[] = {argv0, NULL};
    UIApplicationMain(1, argv, nil, NSStringFromClass([NVIOSAppDelegate class]));
  }
}

NV_INTERNAL void nv_app_platform_quit(nv_app_t* app) {
  (void)app;
  dispatch_async(dispatch_get_main_queue(), ^{
    exit(0);
  });
}
