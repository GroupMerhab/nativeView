/*
 * Copyright (c) 2026
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 */

/* Application main menu (NSApp.setMainMenu). Separate TU to keep nv_mac.m smaller. */

#import <Cocoa/Cocoa.h>
#include "nv_menu.h"
#include "nv_window_internal.h"
#include "nv.h"

/* Forward: full definition lives in nv_mac.m */
@interface NVWindowPlatform : NSObject
@property(nonatomic, assign) nv_window_t *cWindow;
@end

@interface NVAppMenuActions : NSObject
+ (instancetype)shared;
- (void)onAppMenu:(id)sender;
@end

@implementation NVAppMenuActions

+ (instancetype)shared {
  static NVAppMenuActions *inst = nil;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    inst = [[NVAppMenuActions alloc] init];
  });
  return inst;
}

static nv_window_t *nv_mac_menu_target_window(void) {
  NSWindow *kw = [NSApp keyWindow];
  if (kw) {
    id del = kw.delegate;
    if ([del isKindOfClass:[NVWindowPlatform class]]) {
      return [(NVWindowPlatform *)del cWindow];
    }
  }
  for (NSWindow *win in [NSApp windows]) {
    if (![win isVisible]) {
      continue;
    }
    id del = win.delegate;
    if ([del isKindOfClass:[NVWindowPlatform class]]) {
      return [(NVWindowPlatform *)del cWindow];
    }
  }
  return NULL;
}

- (void)onAppMenu:(id)sender {
  if (![sender isKindOfClass:[NSMenuItem class]]) {
    return;
  }
  NSMenuItem *mi = (NSMenuItem *)sender;
  NSDictionary *rep = mi.representedObject;
  if (![rep isKindOfClass:[NSDictionary class]]) {
    return;
  }
  NSString *sid = rep[@"id"];
  if (![sid isKindOfClass:[NSString class]] || sid.length == 0) {
    return;
  }
  nv_window_t *w = nv_mac_menu_target_window();
  if (!w) {
    return;
  }
  NSDictionary *payload = @{@"id" : sid};
  NSData *data = [NSJSONSerialization dataWithJSONObject:payload options:0 error:nil];
  if (!data) {
    return;
  }
  NSString *json = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
  if (!json) {
    return;
  }
  nv_send(w, "app.menuAction", json.UTF8String);
  [json release];
}

@end

static NSString *nv_mac_menu_title_cstr(const char *s) {
  if (!s) {
    return @"";
  }
  NSString *t = [NSString stringWithUTF8String:s];
  return t ? t : @"";
}

static NSEventModifierFlags nv_mac_menu_mod_for_token(NSString *tok) {
  NSString *t = [[tok stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]]
      lowercaseString];
  if ([t isEqualToString:@"cmdorctrl"] || [t isEqualToString:@"commandorcontrol"]) {
    return NSEventModifierFlagCommand;
  }
  if ([t isEqualToString:@"cmd"] || [t isEqualToString:@"command"]) {
    return NSEventModifierFlagCommand;
  }
  if ([t isEqualToString:@"ctrl"] || [t isEqualToString:@"control"]) {
    return NSEventModifierFlagControl;
  }
  if ([t isEqualToString:@"shift"]) {
    return NSEventModifierFlagShift;
  }
  if ([t isEqualToString:@"alt"] || [t isEqualToString:@"option"]) {
    return NSEventModifierFlagOption;
  }
  return 0;
}

static void nv_mac_menu_parse_shortcut(const char *shortcut, NSEventModifierFlags *outMods,
                                       NSString **outKey) {
  *outMods = 0;
  *outKey = @"";
  if (!shortcut || !shortcut[0]) {
    return;
  }
  NSString *s = [NSString stringWithUTF8String:shortcut];
  if (!s || s.length == 0) {
    return;
  }
  NSArray *parts = [s componentsSeparatedByString:@"+"];
  if (parts.count == 0) {
    return;
  }
  for (NSUInteger i = 0; i + 1 < parts.count; i++) {
    *outMods |= nv_mac_menu_mod_for_token(parts[i]);
  }
  NSString *last = [parts lastObject];
  last = [last stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
  if (last.length == 0) {
    return;
  }
  if (last.length == 1) {
    *outKey = [last lowercaseString];
    return;
  }
  NSString *low = [last lowercaseString];
  if ([low hasPrefix:@"f"] && low.length <= 3) {
    unichar c = [low characterAtIndex:1];
    if (c >= '1' && c <= '9' && low.length == 2) {
      *outKey = [NSString stringWithFormat:@"f%c", (char)c];
      *outMods |= NSEventModifierFlagFunction;
    }
  }
}

static void nv_mac_menu_fill(NSMenu *menu, const nv_menu_item_t *items, int count,
                             NVAppMenuActions *actions) {
  if (!menu || !items || count <= 0) {
    return;
  }
  for (int i = 0; i < count; i++) {
    const nv_menu_item_t *it = &items[i];
    if (it->separator) {
      [menu addItem:[NSMenuItem separatorItem]];
      continue;
    }
    if (it->child_count > 0) {
      NSString *title = nv_mac_menu_title_cstr(it->label);
      NSMenuItem *top = [[NSMenuItem alloc] initWithTitle:title action:NULL keyEquivalent:@""];
      NSMenu *sub = [[NSMenu alloc] initWithTitle:title];
      [top setSubmenu:sub];
      nv_mac_menu_fill(sub, it->children, it->child_count, actions);
      [menu addItem:top];
      [top release];
      [sub release];
      continue;
    }
    NSString *title = nv_mac_menu_title_cstr(it->label);
    NSEventModifierFlags mods = 0;
    NSString *keyEq = @"";
    nv_mac_menu_parse_shortcut(it->shortcut, &mods, &keyEq);
    NSMenuItem *mi = [[NSMenuItem alloc] initWithTitle:title action:@selector(onAppMenu:)
                                           keyEquivalent:keyEq ? keyEq : @""];
    [mi setKeyEquivalentModifierMask:mods];
    [mi setTarget:actions];
    [mi setEnabled:(it->enabled ? YES : NO)];
    const char *iid = it->id ? it->id : "";
    NSString *nsid = [NSString stringWithUTF8String:iid];
    if (!nsid) {
      nsid = @"";
    }
    [mi setRepresentedObject:@{@"id" : nsid}];
    [menu addItem:mi];
    [mi release];
  }
}

NV_INTERNAL void nv_mac_app_set_menu(const nv_menu_item_t *items, int count) {
  (void)[NSApplication sharedApplication];
  void (^work)(void) = ^{
    NSMenu *bar = [[NSMenu alloc] initWithTitle:@"MainMenu"];
    if (items && count > 0) {
      nv_mac_menu_fill(bar, items, count, [NVAppMenuActions shared]);
    }
    [NSApp setMainMenu:bar];
    [bar release];
  };
  if ([NSThread isMainThread]) {
    work();
  } else {
    dispatch_async(dispatch_get_main_queue(), work);
  }
}
