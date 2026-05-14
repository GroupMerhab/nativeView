#import <XCTest/XCTest.h>
#import <Foundation/Foundation.h>

#include <string.h>

#include "nv.h"

@interface NativeviewSmokeTests : XCTestCase
@end

@implementation NativeviewSmokeTests

- (void)testCreateWindowLoadHtmlAndClose {
  XCTestExpectation* exp = [self expectationWithDescription:@"nativeview smoke"];

  dispatch_async(dispatch_get_main_queue(), ^{
    nv_app_t* app = nv_app_create();
    XCTAssertTrue(app != NULL);
    if (!app) {
      [exp fulfill];
      return;
    }

    nv_window_cfg_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.title = "nativeview smoke";
    cfg.width = 320;
    cfg.height = 480;
    cfg.resizable = 0;
    cfg.frameless = 1;

    nv_window_t* window = nv_window_create(app, &cfg);
    XCTAssertTrue(window != NULL);
    if (!window) {
      nv_app_destroy(app);
      [exp fulfill];
      return;
    }

    nv_load_html(window, "<!doctype html><html><body>ok</body></html>", NULL);
    nv_window_show(window);

    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(200 * NSEC_PER_MSEC)),
                   dispatch_get_main_queue(), ^{
      nv_window_close(window);
      nv_app_destroy(app);
      [exp fulfill];
    });
  });

  [self waitForExpectations:@[exp] timeout:5.0];
}

@end
