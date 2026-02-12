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
    // Minimal 5x7 bitmap font for ASCII 32-126
    // Each char is 5 columns x 7 rows, stored as 7 bytes (5 bits each)
    static const unsigned char font[][7] = {
        {0,0,0,0,0,0,0},       // space
        {4,4,4,4,0,0,4},       // !
        {10,10,0,0,0,0,0},     // "
        {10,31,10,31,10,0,0},  // #
        {4,15,20,14,5,30,4},   // $
        {24,25,2,4,8,19,3},    // %
        {12,18,12,13,18,13,0}, // &
        {4,4,0,0,0,0,0},       // '
        {2,4,8,8,8,4,2},       // (
        {8,4,2,2,2,4,8},       // )
        {0,4,21,14,21,4,0},    // *
        {0,4,4,31,4,4,0},      // +
        {0,0,0,0,0,4,8},       // ,
        {0,0,0,31,0,0,0},      // -
        {0,0,0,0,0,0,4},       // .
        {0,1,2,4,8,16,0},      // /
        {14,17,19,21,25,17,14},// 0
        {4,12,4,4,4,4,14},     // 1
        {14,17,1,6,8,16,31},   // 2
        {14,17,1,6,1,17,14},   // 3
        {2,6,10,18,31,2,2},    // 4
        {31,16,30,1,1,17,14},  // 5
        {6,8,16,30,17,17,14},  // 6
        {31,1,2,4,8,8,8},      // 7
        {14,17,17,14,17,17,14},// 8
        {14,17,17,15,1,2,12},  // 9
        {0,0,4,0,0,4,0},       // :
    };
    if (c < 32 || c > 58) return; // Limited range for now
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
// GUI not available — stub functions
long long Gui_create(const char* t, long long w, long long h) { fprintf(stderr, "Error: GUI not available. Install SDL2: brew install sdl2\n"); return -1; }
void Gui_clear(long long r, long long g, long long b) {}
void Gui_color(long long r, long long g, long long b) {}
void Gui_rect(long long x, long long y, long long w, long long h) {}
void Gui_line(long long x1, long long y1, long long x2, long long y2) {}
void Gui_point(long long x, long long y) {}
void Gui_present() {}
char* Gui_poll() { return "quit|0|0|0"; }
long long Gui_running() { return 0; }
void Gui_delay(long long ms) {}
long long Gui_width() { return 0; }
long long Gui_height() { return 0; }
void Gui_destroy() {}
void Gui_text(long long x, long long y, const char* t, long long s) {}
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
void Gui_button(long long x, long long y, long long w, long long h, const char* l) {}
long long Gui_button_clicked(long long bx, long long by, long long bw, long long bh, long long mx, long long my) { return 0; }
void Gui_label(long long x, long long y, const char* t, long long s) {}
void Gui_panel(long long x, long long y, long long w, long long h) {}
void Gui_progress(long long x, long long y, long long w, long long h, long long p) {}
void Gui_circle(long long cx, long long cy, long long r) {}
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
long long Gui_key_pressed(long long k) { return 0; }
long long Gui_mouse_x() { return 0; }
long long Gui_mouse_y() { return 0; }
long long Gui_mouse_down() { return 0; }
long long Gui_ticks() { return 0; }
void Gui_rect_outline(long long x, long long y, long long w, long long h) {}
long long Gui_load_sprite(const char* p) { return -1; }
void Gui_draw_sprite(long long id, long long x, long long y) {}
void Gui_draw_sprite_scaled(long long id, long long x, long long y, long long w, long long h) {}
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
void Gui_text_input(long long x, long long y, long long w, long long h) {}
void Gui_text_input_activate(long long a) {}
long long Gui_text_input_key(long long k) { return 0; }
char* Gui_text_input_value() { return ""; }
void Gui_text_input_clear() {}
void Gui_text_input_set(const char* t) {}
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
void Audio_init() {}
long long Audio_load(const char* p) { return -1; }
void Audio_play(long long id) {}
void Audio_stop() {}
void Audio_close() {}
#endif
#else
void Audio_init() {}
long long Audio_load(const char* p) { return -1; }
void Audio_play(long long id) {}
void Audio_stop() {}
void Audio_close() {}
#endif
