// wyn_webview.h — App module declarations
#ifndef WYN_WEBVIEW_H
#define WYN_WEBVIEW_H

int App_create(const char* title, long long width, long long height);
void App_html(const char* content);
void App_url(const char* url_str);
void App_eval(const char* js);
void App_run(void);
void App_destroy(void);
void App_set_title(const char* title);

#endif
