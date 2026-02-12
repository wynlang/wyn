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
