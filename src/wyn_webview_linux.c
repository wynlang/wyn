// wyn_webview_linux.c — App module for Linux using WebKitGTK
// Compile: gcc -c wyn_webview_linux.c $(pkg-config --cflags gtk+-3.0 webkit2gtk-4.0)
// Link: $(pkg-config --libs gtk+-3.0 webkit2gtk-4.0)

#ifdef __linux__

#include <gtk/gtk.h>
#include <webkit2/webkit2.h>

static GtkWidget* _wv_window = NULL;
static WebKitWebView* _wv_webview = NULL;

int App_create(const char* title, long long width, long long height) {
    if (!gtk_init_check(NULL, NULL)) {
        fprintf(stderr, "\033[31mError:\033[0m App module requires GTK3 + WebKitGTK.\n");
        fprintf(stderr, "  Install: apt install libgtk-3-dev libwebkit2gtk-4.0-dev\n");
        return -1;
    }

    _wv_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(_wv_window), title);
    gtk_window_set_default_size(GTK_WINDOW(_wv_window), (gint)width, (gint)height);
    g_signal_connect(_wv_window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    _wv_webview = WEBKIT_WEB_VIEW(webkit_web_view_new());
    gtk_container_add(GTK_CONTAINER(_wv_window), GTK_WIDGET(_wv_webview));

    gtk_widget_show_all(_wv_window);
    return 0;
}

void App_html(const char* content) {
    if (!_wv_webview) return;
    webkit_web_view_load_html(_wv_webview, content, NULL);
}

void App_url(const char* url_str) {
    if (!_wv_webview) return;
    webkit_web_view_load_uri(_wv_webview, url_str);
}

void App_eval(const char* js) {
    if (!_wv_webview) return;
    webkit_web_view_run_javascript(_wv_webview, js, NULL, NULL, NULL);
}

void App_run(void) {
    gtk_main();
}

void App_destroy(void) {
    if (_wv_window) {
        gtk_widget_destroy(_wv_window);
        _wv_window = NULL;
    }
    _wv_webview = NULL;
}

void App_set_title(const char* title) {
    if (!_wv_window) return;
    gtk_window_set_title(GTK_WINDOW(_wv_window), title);
}

#endif // __linux__
