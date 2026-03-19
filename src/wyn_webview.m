// wyn_webview.m — App module implementation (Objective-C)
// macOS: Cocoa + WebKit
// Compile: clang -ObjC -framework WebKit -framework Cocoa -c wyn_webview.m

#import <Cocoa/Cocoa.h>
#import <WebKit/WebKit.h>

static NSWindow* _wv_window = nil;
static WKWebView* _wv_webview = nil;

int App_create(const char* title, long long width, long long height) {
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

    NSRect frame = NSMakeRect(200, 200, (CGFloat)width, (CGFloat)height);
    _wv_window = [[NSWindow alloc]
        initWithContentRect:frame
        styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable)
        backing:NSBackingStoreBuffered
        defer:NO];

    [_wv_window setTitle:[NSString stringWithUTF8String:title]];

    WKWebViewConfiguration* config = [[WKWebViewConfiguration alloc] init];
    _wv_webview = [[WKWebView alloc] initWithFrame:[[_wv_window contentView] bounds] configuration:config];
    [_wv_webview setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];
    [[_wv_window contentView] addSubview:_wv_webview];

    [_wv_window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];

    return 0;
}

void App_html(const char* content) {
    if (!_wv_webview) return;
    [_wv_webview loadHTMLString:[NSString stringWithUTF8String:content] baseURL:nil];
}

void App_url(const char* url_str) {
    if (!_wv_webview) return;
    NSURL* url = [NSURL URLWithString:[NSString stringWithUTF8String:url_str]];
    [_wv_webview loadRequest:[NSURLRequest requestWithURL:url]];
}

void App_eval(const char* js) {
    if (!_wv_webview) return;
    [_wv_webview evaluateJavaScript:[NSString stringWithUTF8String:js] completionHandler:nil];
}

void App_run(void) {
    if (![NSApp isRunning]) {
        [NSApp run];
    }
}

void App_destroy(void) {
    if (_wv_window) {
        [_wv_window close];
        _wv_window = nil;
    }
    _wv_webview = nil;
}

void App_set_title(const char* title) {
    if (!_wv_window) return;
    [_wv_window setTitle:[NSString stringWithUTF8String:title]];
}
