// wyn_webview_win.c — App module for Windows using Edge WebView2
// Requires: WebView2Loader.dll from Microsoft Edge WebView2 SDK
// This is a minimal implementation using the COM API

#ifdef _WIN32

#include <windows.h>
#include <stdio.h>

// WebView2 requires C++ COM — for pure C we use a shell approach:
// Create an HTA (HTML Application) window which uses the system IE/Edge engine.
// This works on all Windows versions without any SDK.

static HWND _wv_hwnd = NULL;
static char _wv_html_path[MAX_PATH] = "";

int App_create(const char* title, long long width, long long height) {
    // Write HTML to temp file, launch with mshta.exe
    char temp[MAX_PATH];
    GetTempPathA(MAX_PATH, temp);
    snprintf(_wv_html_path, MAX_PATH, "%swyn_app.hta", temp);

    // Create initial HTA with title and size
    FILE* f = fopen(_wv_html_path, "w");
    if (!f) return -1;
    fprintf(f, "<html><head><title>%s</title>\n", title);
    fprintf(f, "<HTA:APPLICATION ID=\"wynapp\" BORDER=\"thick\" BORDERSTYLE=\"normal\" "
               "INNERBORDER=\"no\" SCROLL=\"auto\" SHOWINTASKBAR=\"yes\" "
               "WINDOWSTATE=\"normal\" />\n");
    fprintf(f, "<style>body{margin:0;font-family:system-ui}</style>\n");
    fprintf(f, "<script>window.resizeTo(%lld,%lld);</script>\n", width, height);
    fprintf(f, "</head><body></body></html>\n");
    fclose(f);

    return 0;
}

void App_html(const char* content) {
    // Rewrite the HTA file with new content
    FILE* f = fopen(_wv_html_path, "w");
    if (!f) return;
    fprintf(f, "<html><head>\n");
    fprintf(f, "<HTA:APPLICATION ID=\"wynapp\" BORDER=\"thick\" BORDERSTYLE=\"normal\" "
               "INNERBORDER=\"no\" SCROLL=\"auto\" SHOWINTASKBAR=\"yes\" />\n");
    fprintf(f, "</head>\n");
    fprintf(f, "%s\n", content);
    fprintf(f, "</html>\n");
    fclose(f);
}

void App_url(const char* url_str) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "start %s", url_str);
    system(cmd);
}

void App_eval(const char* js) {
    (void)js;
    // HTA doesn't support external JS injection easily
    // Would need WebView2 COM API for this
}

void App_run(void) {
    if (_wv_html_path[0]) {
        char cmd[MAX_PATH + 32];
        snprintf(cmd, sizeof(cmd), "mshta.exe \"%s\"", _wv_html_path);
        system(cmd); // Blocks until window is closed
    }
}

void App_destroy(void) {
    if (_wv_html_path[0]) {
        DeleteFileA(_wv_html_path);
        _wv_html_path[0] = 0;
    }
}

void App_set_title(const char* title) {
    (void)title;
    // Would need window handle to change title at runtime
}

#endif // _WIN32
