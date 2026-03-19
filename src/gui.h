// Wyn GUI Module — SDL2 backend
// Provides window creation, drawing, and event handling
// Conditionally compiled when WYN_USE_GUI is defined

#ifdef WYN_USE_GUI

#include <SDL2/SDL.h>

static SDL_Window* gui_window = NULL;
static SDL_Renderer* gui_renderer = NULL;
static int gui_width = 800;
static int gui_height = 600;
static int gui_running = 1;

long long Gui_create(const char* title, long long width, long long height) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) return -1;
    gui_width = (int)width;
    gui_height = (int)height;
    gui_window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                  gui_width, gui_height, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!gui_window) { SDL_Quit(); return -1; }
    gui_renderer = SDL_CreateRenderer(gui_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!gui_renderer) { SDL_DestroyWindow(gui_window); SDL_Quit(); return -1; }
    return 1;
}

void Gui_clear(long long r, long long g, long long b) {
    SDL_SetRenderDrawColor(gui_renderer, (Uint8)r, (Uint8)g, (Uint8)b, 255);
    SDL_RenderClear(gui_renderer);
}

void Gui_color(long long r, long long g, long long b) {
    SDL_SetRenderDrawColor(gui_renderer, (Uint8)r, (Uint8)g, (Uint8)b, 255);
}

void Gui_rect(long long x, long long y, long long w, long long h) {
    SDL_Rect rect = {(int)x, (int)y, (int)w, (int)h};
    SDL_RenderFillRect(gui_renderer, &rect);
}

void Gui_line(long long x1, long long y1, long long x2, long long y2) {
    SDL_RenderDrawLine(gui_renderer, (int)x1, (int)y1, (int)x2, (int)y2);
}

void Gui_point(long long x, long long y) {
    SDL_RenderDrawPoint(gui_renderer, (int)x, (int)y);
}

void Gui_present() {
    SDL_RenderPresent(gui_renderer);
}

// Returns event as "TYPE|KEY|X|Y" string
// Types: "quit", "key", "mouse", "none"
char* Gui_poll() {
    SDL_Event event;
    char* result = malloc(256);
    if (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                gui_running = 0;
                snprintf(result, 256, "quit|0|0|0");
                break;
            case SDL_KEYDOWN:
                snprintf(result, 256, "key|%d|0|0", event.key.keysym.sym);
                break;
            case SDL_MOUSEBUTTONDOWN:
                snprintf(result, 256, "mouse|%d|%d|%d", event.button.button, event.button.x, event.button.y);
                break;
            case SDL_MOUSEMOTION:
                snprintf(result, 256, "move|0|%d|%d", event.motion.x, event.motion.y);
                break;
            default:
                snprintf(result, 256, "none|0|0|0");
                break;
        }
    } else {
        snprintf(result, 256, "none|0|0|0");
    }
    return result;
}

long long Gui_running() {
    return gui_running;
}

void Gui_delay(long long ms) {
    SDL_Delay((Uint32)ms);
}

long long Gui_width() { return gui_width; }
long long Gui_height() { return gui_height; }

void Gui_destroy() {
    if (gui_renderer) SDL_DestroyRenderer(gui_renderer);
    if (gui_window) SDL_DestroyWindow(gui_window);
    SDL_Quit();
    gui_renderer = NULL;
    gui_window = NULL;
}

// Text rendering (basic — uses SDL_RenderDrawPoint for pixel font)
static void gui_draw_char(int x, int y, char c, int scale) {
    // 5x7 bitmap font for ASCII 32-126
    static const unsigned char font[][7] = {
        {0,0,0,0,0,0,0},       // 32 space
        {4,4,4,4,0,0,4},       // 33 !
        {10,10,0,0,0,0,0},     // 34 "
        {10,31,10,31,10,0,0},  // 35 #
        {4,15,20,14,5,30,4},   // 36 $
        {24,25,2,4,8,19,3},    // 37 %
        {12,18,12,13,18,13,0}, // 38 &
        {4,4,0,0,0,0,0},       // 39 '
        {2,4,8,8,8,4,2},       // 40 (
        {8,4,2,2,2,4,8},       // 41 )
        {0,4,21,14,21,4,0},    // 42 *
        {0,4,4,31,4,4,0},      // 43 +
        {0,0,0,0,0,4,8},       // 44 ,
        {0,0,0,31,0,0,0},      // 45 -
        {0,0,0,0,0,0,4},       // 46 .
        {0,1,2,4,8,16,0},      // 47 /
        {14,17,19,21,25,17,14},// 48 0
        {4,12,4,4,4,4,14},     // 49 1
        {14,17,1,6,8,16,31},   // 50 2
        {14,17,1,6,1,17,14},   // 51 3
        {2,6,10,18,31,2,2},    // 52 4
        {31,16,30,1,1,17,14},  // 53 5
        {6,8,16,30,17,17,14},  // 54 6
        {31,1,2,4,8,8,8},      // 55 7
        {14,17,17,14,17,17,14},// 56 8
        {14,17,17,15,1,2,12},  // 57 9
        {0,0,4,0,0,4,0},       // 58 :
        {0,0,4,0,0,4,8},       // 59 ;
        {1,2,4,8,4,2,1},       // 60 <
        {0,0,31,0,31,0,0},     // 61 =
        {16,8,4,2,4,8,16},     // 62 >
        {14,17,1,2,4,0,4},     // 63 ?
        {14,17,23,21,23,16,14},// 64 @
        {14,17,17,31,17,17,17},// 65 A
        {30,17,17,30,17,17,30},// 66 B
        {14,17,16,16,16,17,14},// 67 C
        {30,17,17,17,17,17,30},// 68 D
        {31,16,16,30,16,16,31},// 69 E
        {31,16,16,30,16,16,16},// 70 F
        {14,17,16,19,17,17,15},// 71 G
        {17,17,17,31,17,17,17},// 72 H
        {14,4,4,4,4,4,14},     // 73 I
        {7,2,2,2,2,18,12},     // 74 J
        {17,18,20,24,20,18,17},// 75 K
        {16,16,16,16,16,16,31},// 76 L
        {17,27,21,21,17,17,17},// 77 M
        {17,25,21,19,17,17,17},// 78 N
        {14,17,17,17,17,17,14},// 79 O
        {30,17,17,30,16,16,16},// 80 P
        {14,17,17,17,21,18,13},// 81 Q
        {30,17,17,30,20,18,17},// 82 R
        {14,17,16,14,1,17,14}, // 83 S
        {31,4,4,4,4,4,4},      // 84 T
        {17,17,17,17,17,17,14},// 85 U
        {17,17,17,17,10,10,4}, // 86 V
        {17,17,17,21,21,27,17},// 87 W
        {17,17,10,4,10,17,17}, // 88 X
        {17,17,10,4,4,4,4},    // 89 Y
        {31,1,2,4,8,16,31},    // 90 Z
        {14,8,8,8,8,8,14},     // 91 [
        {0,16,8,4,2,1,0},      // 92 backslash
        {14,2,2,2,2,2,14},     // 93 ]
        {4,10,17,0,0,0,0},     // 94 ^
        {0,0,0,0,0,0,31},      // 95 _
        {8,4,0,0,0,0,0},       // 96 `
        {0,0,14,1,15,17,15},   // 97 a
        {16,16,30,17,17,17,30},// 98 b
        {0,0,14,17,16,17,14},  // 99 c
        {1,1,15,17,17,17,15},  // 100 d
        {0,0,14,17,31,16,14},  // 101 e
        {6,9,8,28,8,8,8},      // 102 f
        {0,0,15,17,15,1,14},   // 103 g
        {16,16,30,17,17,17,17},// 104 h
        {4,0,12,4,4,4,14},     // 105 i
        {2,0,6,2,2,18,12},     // 106 j
        {16,16,18,20,24,20,18},// 107 k
        {12,4,4,4,4,4,14},     // 108 l
        {0,0,26,21,21,17,17},  // 109 m
        {0,0,30,17,17,17,17},  // 110 n
        {0,0,14,17,17,17,14},  // 111 o
        {0,0,30,17,30,16,16},  // 112 p
        {0,0,15,17,15,1,1},    // 113 q
        {0,0,22,25,16,16,16},  // 114 r
        {0,0,15,16,14,1,30},   // 115 s
        {8,8,28,8,8,9,6},      // 116 t
        {0,0,17,17,17,17,15},  // 117 u
        {0,0,17,17,17,10,4},   // 118 v
        {0,0,17,17,21,21,10},  // 119 w
        {0,0,17,10,4,10,17},   // 120 x
        {0,0,17,17,15,1,14},   // 121 y
        {0,0,31,2,4,8,31},     // 122 z
        {2,4,4,8,4,4,2},       // 123 {
        {4,4,4,4,4,4,4},       // 124 |
        {8,4,4,2,4,4,8},       // 125 }
        {0,0,8,21,2,0,0},      // 126 ~
    };
    if (c < 32 || c > 126) return;
    int idx = c - 32;
    if (idx >= (int)(sizeof(font)/sizeof(font[0]))) return;
    for (int row = 0; row < 7; row++) {
        for (int col = 0; col < 5; col++) {
            if (font[idx][row] & (1 << (4 - col))) {
                for (int sy = 0; sy < scale; sy++)
                    for (int sx = 0; sx < scale; sx++)
                        SDL_RenderDrawPoint(gui_renderer, x + col*scale + sx, y + row*scale + sy);
            }
        }
    }
}

void Gui_text(long long x, long long y, const char* text, long long scale) {
    int s = scale > 0 ? (int)scale : 1;
    int cx = (int)x;
    for (int i = 0; text[i]; i++) {
        gui_draw_char(cx, (int)y, text[i], s);
        cx += 6 * s;
    }
}

#else
// GUI not available — all functions print error on first use
static int _gui_warned = 0;
static void _gui_warn(void) {
    if (!_gui_warned) {
        fprintf(stderr, "\033[31mError:\033[0m Gui module requires SDL2.\n");
        fprintf(stderr, "  macOS:  brew install sdl2\n");
        fprintf(stderr, "  Linux:  apt install libsdl2-dev\n");
        fprintf(stderr, "  Or use the App module (webview) instead — no dependencies.\n");
        _gui_warned = 1;
    }
}
long long Gui_create(const char* t, long long w, long long h) { (void)t;(void)w;(void)h; _gui_warn(); return -1; }
void Gui_clear(long long r, long long g, long long b) { (void)r;(void)g;(void)b; }
void Gui_color(long long r, long long g, long long b) { (void)r;(void)g;(void)b; }
void Gui_rect(long long x, long long y, long long w, long long h) { (void)x;(void)y;(void)w;(void)h; }
void Gui_line(long long x1, long long y1, long long x2, long long y2) { (void)x1;(void)y1;(void)x2;(void)y2; }
void Gui_point(long long x, long long y) { (void)x;(void)y; }
void Gui_present() {}
char* Gui_poll() { return "quit|0|0|0"; }
long long Gui_running() { return 0; }
void Gui_delay(long long ms) { (void)ms; }
long long Gui_width() { return 0; }
long long Gui_height() { return 0; }
void Gui_destroy() {}
void Gui_text(long long x, long long y, const char* t, long long s) { (void)x;(void)y;(void)t;(void)s; }
#endif

// === Widget helpers (built on top of primitives) ===
#ifdef WYN_USE_GUI

void Gui_button(long long x, long long y, long long w, long long h, const char* label) {
    // Button background
    SDL_Rect rect = {(int)x, (int)y, (int)w, (int)h};
    SDL_SetRenderDrawColor(gui_renderer, 60, 60, 80, 255);
    SDL_RenderFillRect(gui_renderer, &rect);
    // Border
    SDL_SetRenderDrawColor(gui_renderer, 120, 120, 160, 255);
    SDL_RenderDrawRect(gui_renderer, &rect);
    // Label centered
    int text_x = (int)x + ((int)w - (int)strlen(label) * 8) / 2;
    int text_y = (int)y + ((int)h - 10) / 2;
    SDL_SetRenderDrawColor(gui_renderer, 255, 255, 255, 255);
    Gui_text(text_x, text_y, label, 1);
}

long long Gui_button_clicked(long long bx, long long by, long long bw, long long bh, long long mx, long long my) {
    return (mx >= bx && mx <= bx + bw && my >= by && my <= by + bh) ? 1 : 0;
}

void Gui_label(long long x, long long y, const char* text, long long scale) {
    SDL_SetRenderDrawColor(gui_renderer, 220, 220, 220, 255);
    Gui_text((int)x, (int)y, text, (int)scale);
}

void Gui_panel(long long x, long long y, long long w, long long h) {
    SDL_Rect rect = {(int)x, (int)y, (int)w, (int)h};
    SDL_SetRenderDrawColor(gui_renderer, 45, 45, 55, 255);
    SDL_RenderFillRect(gui_renderer, &rect);
    SDL_SetRenderDrawColor(gui_renderer, 80, 80, 100, 255);
    SDL_RenderDrawRect(gui_renderer, &rect);
}

void Gui_progress(long long x, long long y, long long w, long long h, long long pct) {
    // Background
    SDL_Rect bg = {(int)x, (int)y, (int)w, (int)h};
    SDL_SetRenderDrawColor(gui_renderer, 40, 40, 50, 255);
    SDL_RenderFillRect(gui_renderer, &bg);
    // Fill
    int fill_w = (int)(w * pct / 100);
    if (fill_w > 0) {
        SDL_Rect fill = {(int)x, (int)y, fill_w, (int)h};
        if (pct > 80) SDL_SetRenderDrawColor(gui_renderer, 80, 200, 80, 255);
        else if (pct > 50) SDL_SetRenderDrawColor(gui_renderer, 200, 200, 80, 255);
        else SDL_SetRenderDrawColor(gui_renderer, 200, 80, 80, 255);
        SDL_RenderFillRect(gui_renderer, &fill);
    }
    // Border
    SDL_SetRenderDrawColor(gui_renderer, 80, 80, 100, 255);
    SDL_RenderDrawRect(gui_renderer, &bg);
}

void Gui_circle(long long cx, long long cy, long long radius) {
    int r = (int)radius;
    for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
            if (dx*dx + dy*dy <= r*r) {
                SDL_RenderDrawPoint(gui_renderer, (int)cx + dx, (int)cy + dy);
            }
        }
    }
}

#else
void Gui_button(long long x, long long y, long long w, long long h, const char* l) { (void)x;(void)y;(void)w;(void)h;(void)l; }
long long Gui_button_clicked(long long bx, long long by, long long bw, long long bh, long long mx, long long my) { (void)bx;(void)by;(void)bw;(void)bh;(void)mx;(void)my; return 0; }
void Gui_label(long long x, long long y, const char* t, long long s) { (void)x;(void)y;(void)t;(void)s; }
void Gui_panel(long long x, long long y, long long w, long long h) { (void)x;(void)y;(void)w;(void)h; }
void Gui_progress(long long x, long long y, long long w, long long h, long long p) { (void)x;(void)y;(void)w;(void)h;(void)p; }
void Gui_circle(long long cx, long long cy, long long r) { (void)cx;(void)cy;(void)r; }
#endif

// === Game helpers ===
#ifdef WYN_USE_GUI

// Keyboard state
long long Gui_key_pressed(long long keycode) {
    const Uint8* state = SDL_GetKeyboardState(NULL);
    SDL_Scancode sc = SDL_GetScancodeFromKey((SDL_Keycode)keycode);
    return state[sc] ? 1 : 0;
}

// Mouse state
long long Gui_mouse_x() { int x, y; SDL_GetMouseState(&x, &y); return x; }
long long Gui_mouse_y() { int x, y; SDL_GetMouseState(&x, &y); return y; }
long long Gui_mouse_down() { return (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(1)) ? 1 : 0; }

// Timing
long long Gui_ticks() { return (long long)SDL_GetTicks(); }

// Outline rect
void Gui_rect_outline(long long x, long long y, long long w, long long h) {
    SDL_Rect rect = {(int)x, (int)y, (int)w, (int)h};
    SDL_RenderDrawRect(gui_renderer, &rect);
}

// Image loading (BMP only — no SDL_image dependency)
typedef struct { SDL_Texture* tex; int w; int h; } WynSprite;
#define MAX_SPRITES 64
static WynSprite sprites[MAX_SPRITES] = {0};

long long Gui_load_sprite(const char* bmp_path) {
    SDL_Surface* surface = SDL_LoadBMP(bmp_path);
    if (!surface) return -1;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(gui_renderer, surface);
    int w = surface->w, h = surface->h;
    SDL_FreeSurface(surface);
    if (!tex) return -1;
    for (int i = 1; i < MAX_SPRITES; i++) {
        if (!sprites[i].tex) { sprites[i] = (WynSprite){tex, w, h}; return i; }
    }
    SDL_DestroyTexture(tex);
    return -1;
}

void Gui_draw_sprite(long long id, long long x, long long y) {
    if (id <= 0 || id >= MAX_SPRITES || !sprites[id].tex) return;
    SDL_Rect dst = {(int)x, (int)y, sprites[id].w, sprites[id].h};
    SDL_RenderCopy(gui_renderer, sprites[id].tex, NULL, &dst);
}

void Gui_draw_sprite_scaled(long long id, long long x, long long y, long long w, long long h) {
    if (id <= 0 || id >= MAX_SPRITES || !sprites[id].tex) return;
    SDL_Rect dst = {(int)x, (int)y, (int)w, (int)h};
    SDL_RenderCopy(gui_renderer, sprites[id].tex, NULL, &dst);
}

#else
long long Gui_key_pressed(long long k) { (void)k; return 0; }
long long Gui_mouse_x() { return 0; }
long long Gui_mouse_y() { return 0; }
long long Gui_mouse_down() { return 0; }
long long Gui_ticks() { return 0; }
void Gui_rect_outline(long long x, long long y, long long w, long long h) { (void)x;(void)y;(void)w;(void)h; }
long long Gui_load_sprite(const char* p) { (void)p; _gui_warn(); return -1; }
void Gui_draw_sprite(long long id, long long x, long long y) { (void)id;(void)x;(void)y; }
void Gui_draw_sprite_scaled(long long id, long long x, long long y, long long w, long long h) { (void)id;(void)x;(void)y;(void)w;(void)h; }
#endif

// === Text Input Widget ===
#ifdef WYN_USE_GUI

static char gui_text_buffer[1024] = "";
static int gui_text_cursor = 0;
static int gui_text_active = 0;

void Gui_text_input(long long x, long long y, long long w, long long h) {
    // Background
    SDL_Rect rect = {(int)x, (int)y, (int)w, (int)h};
    SDL_SetRenderDrawColor(gui_renderer, gui_text_active ? 50 : 35, gui_text_active ? 50 : 35, gui_text_active ? 60 : 45, 255);
    SDL_RenderFillRect(gui_renderer, &rect);
    SDL_SetRenderDrawColor(gui_renderer, gui_text_active ? 150 : 80, gui_text_active ? 150 : 80, gui_text_active ? 180 : 100, 255);
    SDL_RenderDrawRect(gui_renderer, &rect);
    // Text
    SDL_SetRenderDrawColor(gui_renderer, 220, 220, 220, 255);
    Gui_text((int)x + 5, (int)y + ((int)h - 7) / 2, gui_text_buffer, 1);
    // Cursor
    if (gui_text_active) {
        int cx = (int)x + 5 + gui_text_cursor * 6;
        SDL_SetRenderDrawColor(gui_renderer, 200, 200, 255, 255);
        SDL_RenderDrawLine(gui_renderer, cx, (int)y + 3, cx, (int)y + (int)h - 3);
    }
}

void Gui_text_input_activate(long long active) { gui_text_active = (int)active; }

// Process keyboard event for text input. Returns 1 if Enter pressed.
long long Gui_text_input_key(long long keycode) {
    if (!gui_text_active) return 0;
    if (keycode == 13 || keycode == 10) return 1; // Enter
    if (keycode == 8 || keycode == 127) { // Backspace
        if (gui_text_cursor > 0) {
            gui_text_cursor--;
            memmove(gui_text_buffer + gui_text_cursor, gui_text_buffer + gui_text_cursor + 1,
                    strlen(gui_text_buffer) - gui_text_cursor);
        }
        return 0;
    }
    if (keycode >= 32 && keycode < 127 && gui_text_cursor < 1023) {
        gui_text_buffer[gui_text_cursor++] = (char)keycode;
        gui_text_buffer[gui_text_cursor] = 0;
    }
    return 0;
}

char* Gui_text_input_value() { return gui_text_buffer; }
void Gui_text_input_clear() { gui_text_buffer[0] = 0; gui_text_cursor = 0; }
void Gui_text_input_set(const char* text) { strncpy(gui_text_buffer, text, 1023); gui_text_cursor = strlen(gui_text_buffer); }

#else
void Gui_text_input(long long x, long long y, long long w, long long h) { (void)x;(void)y;(void)w;(void)h; }
void Gui_text_input_activate(long long a) { (void)a; }
long long Gui_text_input_key(long long k) { (void)k; return 0; }
char* Gui_text_input_value() { return ""; }
void Gui_text_input_clear() {}
void Gui_text_input_set(const char* t) { (void)t; }
#endif

// === Audio (SDL2_mixer or basic beep) ===
#ifdef WYN_USE_GUI
#if __has_include(<SDL2/SDL_mixer.h>)
#include <SDL2/SDL_mixer.h>
#define WYN_HAS_AUDIO 1
static int audio_initialized = 0;

void Audio_init() {
    if (audio_initialized) return;
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) return;
    audio_initialized = 1;
}

static Mix_Chunk* audio_chunks[32] = {0};
long long Audio_load(const char* path) {
    Audio_init();
    Mix_Chunk* chunk = Mix_LoadWAV(path);
    if (!chunk) return -1;
    for (int i = 1; i < 32; i++) {
        if (!audio_chunks[i]) { audio_chunks[i] = chunk; return i; }
    }
    Mix_FreeChunk(chunk);
    return -1;
}

void Audio_play(long long id) {
    if (id <= 0 || id >= 32 || !audio_chunks[id]) return;
    Mix_PlayChannel(-1, audio_chunks[id], 0);
}

void Audio_stop() { Mix_HaltChannel(-1); }
void Audio_close() { if (audio_initialized) { Mix_CloseAudio(); audio_initialized = 0; } }

#else
#define WYN_HAS_AUDIO 0
static int _audio_warned = 0;
void Audio_init() { if (!_audio_warned) { fprintf(stderr, "\033[31mError:\033[0m Audio requires SDL2_mixer.\n  macOS: brew install sdl2_mixer\n  Linux: apt install libsdl2-mixer-dev\n"); _audio_warned = 1; } }
long long Audio_load(const char* p) { (void)p; Audio_init(); return -1; }
void Audio_play(long long id) { (void)id; }
void Audio_stop() {}
void Audio_close() {}
#endif
#else
void Audio_init() { fprintf(stderr, "\033[31mError:\033[0m Audio requires SDL2 + SDL2_mixer.\n  macOS: brew install sdl2 sdl2_mixer\n  Linux: apt install libsdl2-dev libsdl2-mixer-dev\n"); }
long long Audio_load(const char* p) { (void)p; return -1; }
void Audio_play(long long id) { (void)id; }
void Audio_stop() {}
void Audio_close() {}
#endif
