#include <stdbool.h>
#include <stdint.h>

/*
 * Batman Gotham Chase - Stage 1 (Minimal Playable Core)
 * Target: CPUlator ARM DE1-SoC
 *
 * Controls:
 *   Space: Start game from menu
 *   Left/Right or A/D: Move car
 *   Esc or P: Pause/Resume
 */

/* ===================== Hardware ===================== */
#define PIX_CTRL_BASE 0xFF203020u
#define PIX_BUF_FRONT 0xC8000000u
#define PIX_BUF_BACK  0xC0000000u
#define TIMER_BASE    0xFF202000u
#define PS2_BASE      0xFF200100u
#define KEY_BASE      0xFF200050u
#define SW_BASE       0xFF200040u

/* ===================== Screen ===================== */
#define SCR_W      320
#define SCR_H      240
#define STRIDE     512
#define ROAD_L     80
#define ROAD_R     240
#define ROAD_W     (ROAD_R - ROAD_L)
#define ROAD_CX    ((ROAD_L + ROAD_R) / 2)

/* ===================== Colors (RGB565) ===================== */
#define C_BLACK    0x0000u
#define C_WHITE    0xFFFFu
#define C_RED      0xF800u
#define C_GREEN    0x07E0u
#define C_BLUE     0x001Fu
#define C_YELLOW   0xFFE0u
#define C_CYAN     0x07FFu
#define C_DGREY    0x4208u
#define C_MGREY    0x8410u
#define C_ROAD     0x2104u
#define C_SIDE     0x7BCFu

/* ===================== Types ===================== */
typedef enum { MENU, PLAYING, PAUSED } GameState;

typedef struct {
    int x;
    int y;
    int vx;
} Player;

/* ===================== Globals ===================== */
static volatile unsigned *const pbc = (volatile unsigned *)PIX_CTRL_BASE;
static volatile unsigned *const tmr = (volatile unsigned *)TIMER_BASE;
static volatile unsigned *const ps2 = (volatile unsigned *)PS2_BASE;
static volatile unsigned *const key_hw = (volatile unsigned *)KEY_BASE;
static volatile unsigned *const sw_hw = (volatile unsigned *)SW_BASE;
static volatile short *draw_buf;
static volatile short *front_buf;
static volatile short *back_buf;

static GameState state;
static Player pl;
static int frame_count;
static int road_scroll;

/* Key states */
static bool k_left, k_right, k_enter, k_space, k_esc, k_p;
static bool prev_enter, prev_space, prev_esc, prev_p;
static unsigned prev_key_bits;
static unsigned prev_sw_bits;
/* One-shot press latches so make+break in same frame still triggers actions. */
static bool ev_enter, ev_space, ev_esc, ev_p;

/* ===================== Utils ===================== */
static int clampi(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static bool edge_pressed(bool now, bool *prev) {
    bool pressed = now && !(*prev);
    *prev = now;
    return pressed;
}

/* ===================== Drawing ===================== */
/* 5x7 glyphs for minimal readable UI text */
static const unsigned char GL_A[7] = {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11};
static const unsigned char GL_B[7] = {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E};
static const unsigned char GL_C[7] = {0x0F,0x10,0x10,0x10,0x10,0x10,0x0F};
static const unsigned char GL_D[7] = {0x1E,0x11,0x11,0x11,0x11,0x11,0x1E};
static const unsigned char GL_E[7] = {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F};
static const unsigned char GL_G[7] = {0x0F,0x10,0x10,0x17,0x11,0x11,0x0F};
static const unsigned char GL_H[7] = {0x11,0x11,0x11,0x1F,0x11,0x11,0x11};
static const unsigned char GL_M[7] = {0x11,0x1B,0x15,0x15,0x11,0x11,0x11};
static const unsigned char GL_N[7] = {0x11,0x11,0x19,0x15,0x13,0x11,0x11};
static const unsigned char GL_O[7] = {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E};
static const unsigned char GL_P[7] = {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10};
static const unsigned char GL_R[7] = {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11};
static const unsigned char GL_S[7] = {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E};
static const unsigned char GL_T[7] = {0x1F,0x04,0x04,0x04,0x04,0x04,0x04};
static const unsigned char GL_U[7] = {0x11,0x11,0x11,0x11,0x11,0x11,0x0E};
static const unsigned char GL_V[7] = {0x11,0x11,0x11,0x11,0x11,0x0A,0x04};
static const unsigned char GL_Y[7] = {0x11,0x11,0x0A,0x04,0x04,0x04,0x04};
static const unsigned char GL_SLASH[7] = {0x01,0x02,0x04,0x04,0x08,0x10,0x00};

static const unsigned char *glyph_for(char ch) {
    switch (ch) {
        case 'A': return GL_A; case 'B': return GL_B; case 'C': return GL_C;
        case 'D': return GL_D; case 'E': return GL_E; case 'G': return GL_G;
        case 'H': return GL_H; case 'M': return GL_M; case 'N': return GL_N;
        case 'O': return GL_O; case 'P': return GL_P; case 'R': return GL_R;
        case 'S': return GL_S; case 'T': return GL_T; case 'U': return GL_U;
        case 'V': return GL_V;
        case 'Y': return GL_Y; case '/': return GL_SLASH;
        default: return 0;
    }
}

static void frect(int x, int y, int w, int h, unsigned short c) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > SCR_W) w = SCR_W - x;
    if (y + h > SCR_H) h = SCR_H - y;
    if (w <= 0 || h <= 0) return;
    for (int j = y; j < y + h; j++) {
        for (int i = x; i < x + w; i++) {
            draw_buf[j * STRIDE + i] = c;
        }
    }
}

static void draw_char5x7(int x, int y, char ch, unsigned short c, int scale) {
    if (ch == ' ') return;
    const unsigned char *g = glyph_for(ch);
    if (!g) return;
    for (int row = 0; row < 7; row++) {
        for (int col = 0; col < 5; col++) {
            if (g[row] & (1u << (4 - col))) {
                frect(x + col * scale, y + row * scale, scale, scale, c);
            }
        }
    }
}

static void draw_text5x7(int x, int y, const char *s, unsigned short c, int scale) {
    int cx = x;
    for (int i = 0; s[i]; i++) {
        draw_char5x7(cx, y, s[i], c, scale);
        cx += 6 * scale;
    }
}

static void clear_screen(unsigned short c) {
    for (int i = 0; i < STRIDE * SCR_H; i++) {
        draw_buf[i] = c;
    }
}

static void draw_road(void) {
    /* Sidewalks */
    frect(0, 0, ROAD_L, SCR_H, C_SIDE);
    frect(ROAD_R, 0, SCR_W - ROAD_R, SCR_H, C_SIDE);

    /* Road */
    frect(ROAD_L, 0, ROAD_W, SCR_H, C_ROAD);
    frect(ROAD_L - 1, 0, 1, SCR_H, C_WHITE);
    frect(ROAD_R, 0, 1, SCR_H, C_WHITE);

    /* Center dashed line */
    int dash = 16;
    int gap = 10;
    int period = dash + gap;
    int off = road_scroll % period;
    for (int y = -period + off; y < SCR_H; y += period) {
        frect(ROAD_CX - 1, y, 2, dash, C_WHITE);
    }
}

static void draw_player(void) {
    int w = 16;
    int h = 24;
    int x = pl.x - w / 2;
    int y = pl.y - h / 2;

    /* Car body */
    frect(x + 2, y + 2, w - 4, h - 4, C_DGREY);
    /* Windshield */
    frect(x + 4, y + 6, w - 8, 6, C_BLUE);
    /* Wheels */
    frect(x, y + 4, 3, 5, C_BLACK);
    frect(x + w - 3, y + 4, 3, 5, C_BLACK);
    frect(x, y + h - 9, 3, 5, C_BLACK);
    frect(x + w - 3, y + h - 9, 3, 5, C_BLACK);
    /* Front light */
    frect(pl.x - 1, y + 1, 3, 2, C_YELLOW);
}

static void draw_menu(void) {
    /* Gotham night gradient */
    for (int y = 0; y < SCR_H; y++) {
        int g = (y >> 3);
        int b = 4 + (y >> 4);
        unsigned short c = (unsigned short)((g << 5) | b);
        frect(0, y, SCR_W, 1, c);
    }

    /* Skyline hints */
    for (int i = 0; i < 9; i++) {
        int x = 14 + i * 34;
        int h = 20 + (i % 3) * 14;
        frect(x, 88 - h, 12, h, C_BLACK);
        frect(x + 2, 88 - h + 3, 2, 2, C_CYAN);
    }

    /* Bat-signal style emblem */
    frect(120, 20, 80, 30, C_YELLOW);
    frect(124, 24, 72, 22, C_BLACK);
    draw_text5x7(139, 31, "BAT", C_YELLOW, 1);

    /* Title card with real text */
    frect(44, 56, 232, 64, C_YELLOW);
    frect(48, 60, 224, 56, C_BLACK);
    draw_text5x7(78, 72, "BATMAN", C_YELLOW, 2);
    draw_text5x7(86, 92, "GOTHAM CHASE", C_CYAN, 1);

    /* Road preview + car to show "playable" immediately */
    frect(0, 136, ROAD_L, 76, C_SIDE);
    frect(ROAD_R, 136, SCR_W - ROAD_R, 76, C_SIDE);
    frect(ROAD_L, 136, ROAD_W, 76, C_ROAD);
    frect(ROAD_CX - 1, 146, 2, 14, C_WHITE);
    frect(ROAD_CX - 1, 170, 2, 14, C_WHITE);
    frect(ROAD_CX - 1, 194, 2, 14, C_WHITE);
    frect(ROAD_CX - 8, 176, 16, 22, C_DGREY);
    frect(ROAD_CX - 5, 181, 10, 6, C_BLUE);
    frect(ROAD_CX - 1, 177, 3, 2, C_YELLOW);

    /* Controls card */
    frect(64, 198, 192, 10, C_BLACK);
    draw_text5x7(76, 200, "A/D MOVE  P/ESC PAUSE", C_WHITE, 1);

    /* Start prompt blink */
    unsigned short blink = (frame_count & 32) ? C_YELLOW : C_CYAN;
    frect(66, 210, 188, 16, blink);
    frect(69, 213, 182, 10, C_BLACK);
    draw_text5x7(88, 215, "PRESS SPACE OR SW0", blink, 1);
}

static void draw_pause_overlay(void) {
    for (int y = 0; y < SCR_H; y += 2) {
        frect(0, y, SCR_W, 1, C_BLACK);
    }
    frect(95, 100, 130, 40, C_DGREY);
    frect(96, 101, 128, 38, C_BLACK);
    frect(110, 116, 100, 8, C_YELLOW);
}

/* ===================== Input ===================== */
static void apply_key_code(unsigned code, bool ext, bool pressed) {
    /*
     * Supports common PS/2 set2 (CPUlator typical) and set1 fallback codes.
     * set2 examples: A=1C, D=23, P=4D, SPACE=29, ENTER=5A, ESC=76, LEFT=E0 6B, RIGHT=E0 74
     * set1 examples: A=1E, D=20, P=19, SPACE=39, ENTER=1C, ESC=01, LEFT=E0 4B, RIGHT=E0 4D
     */
    if (ext) {
        if (code == 0x6Bu || code == 0x4Bu) k_left = pressed;   /* set2/set1 left  */
        if (code == 0x74u || code == 0x4Du) k_right = pressed;  /* set2/set1 right */
        return;
    }

    if (code == 0x1Cu || code == 0x1Eu) k_left = pressed;   /* A set2/set1      */
    if (code == 0x23u || code == 0x20u) k_right = pressed;  /* D set2/set1      */
    if (code == 0x29u || code == 0x39u) {
        k_space = pressed;                                   /* Space set2/set1  */
        if (pressed) ev_space = true;
    }
    if (code == 0x76u || code == 0x01u) {
        k_esc = pressed;                                     /* Esc set2/set1    */
        if (pressed) ev_esc = true;
    }
    if (code == 0x4Du || code == 0x19u) {
        k_p = pressed;                                       /* P set2/set1      */
        if (pressed) ev_p = true;
    }
}

static void read_ps2(void) {
    static bool ext = false;
    static bool rel = false;

    while (1) {
        unsigned d = *ps2;
        if (!(d & 0x8000u)) break;
        unsigned b = d & 0xFFu;

        if (b == 0xE0u) { ext = true; continue; }
        if (b == 0xF0u) { rel = true; continue; }

        /* Set1 break code fallback: high-bit marks key release. */
        if (b & 0x80u) {
            unsigned code = b & 0x7Fu;
            apply_key_code(code, ext, false);
            ext = false;
            rel = false;
            continue;
        }

        bool pressed = !rel;
        rel = false;
        apply_key_code(b, ext, pressed);
        ext = false;
    }
}

static void read_pushbuttons(void) {
    /*
     * Fallback controls on DE1 keys:
     *   KEY0 -> START (SPACE)
     *   KEY1 -> PAUSE (P)
     *   KEY2 -> LEFT
     *   KEY3 -> RIGHT
     */
    unsigned bits = *key_hw & 0xFu;
    unsigned changed = bits ^ prev_key_bits;
    if (changed == 0) return;

    if (changed & 0x1u) {
        k_space = (bits & 0x1u) != 0;
        if (k_space) ev_space = true;
    }
    if (changed & 0x2u) {
        k_p = (bits & 0x2u) != 0;
        if (k_p) ev_p = true;
    }
    if (changed & 0x4u) k_left  = (bits & 0x4u) != 0;
    if (changed & 0x8u) k_right = (bits & 0x8u) != 0;
    prev_key_bits = bits;
}

static void read_switches(void) {
    /*
     * CPUlator slider switch fallback (SW0..SW3):
     *   SW0 -> START (latch edge to SPACE)
     *   SW1 -> PAUSE (latch edge to P)
     *   SW2 -> LEFT hold
     *   SW3 -> RIGHT hold
     */
    unsigned bits = *sw_hw & 0xFu;
    unsigned changed = bits ^ prev_sw_bits;

    k_left = (bits & 0x4u) != 0;
    k_right = (bits & 0x8u) != 0;

    if ((changed & 0x1u) && (bits & 0x1u)) ev_space = true;
    if ((changed & 0x2u) && (bits & 0x2u)) ev_p = true;

    prev_sw_bits = bits;
}

static void read_input(void) {
    read_ps2();
    read_pushbuttons();
    read_switches();
}
static void reset_input_state(void) {
    k_left = k_right = k_enter = k_space = k_esc = k_p = false;
    prev_enter = prev_space = prev_esc = prev_p = false;
    ev_enter = ev_space = ev_esc = ev_p = false;
    prev_key_bits = *key_hw & 0xFu;
    prev_sw_bits = *sw_hw & 0xFu;
}

/* ===================== Timer ===================== */
static void timer_init(void) {
    /* 50,000,000 / 60 ~= 833,333 = 0x000C_B4B3 */
    tmr[2] = 0xB4B3u;  /* period low */
    tmr[3] = 0x000Cu;  /* period high */
    tmr[1] = 0x0006u;  /* START | CONT (polling, no IRQ) */
}

static void timer_wait(void) {
    unsigned guard = 0;
    while (!(tmr[0] & 1u)) {
        if (++guard > 2000000u) break;
    }
    tmr[0] = 0;
}

/* ===================== Game ===================== */
static void reset_keys_for_transition(void) {
    k_enter = k_space = k_esc = k_p = false;
    prev_enter = prev_space = prev_esc = prev_p = false;
    ev_enter = ev_space = ev_esc = ev_p = false;
}

static void game_init(void) {
    pl.x = ROAD_CX;
    pl.y = SCR_H - 36;
    pl.vx = 0;
    road_scroll = 0;
}

static void update_playing(void) {
    int accel = 0;
    if (k_left) accel -= 1;
    if (k_right) accel += 1;

    pl.vx += accel;
    pl.vx = (pl.vx * 7) / 8; /* damping */
    pl.vx = clampi(pl.vx, -5, 5);
    pl.x += pl.vx;
    pl.x = clampi(pl.x, ROAD_L + 10, ROAD_R - 10);

    road_scroll += 3;
}

static void update(void) {
    frame_count++;
    read_input();

    bool start_pressed = ev_space ||
                         edge_pressed(k_space, &prev_space);
    bool pause_pressed = ev_esc || ev_p ||
                         edge_pressed(k_esc, &prev_esc) ||
                         edge_pressed(k_p, &prev_p);
    ev_enter = ev_space = ev_esc = ev_p = false;

    if (state == MENU) {
        if (start_pressed) {
            game_init();
            state = PLAYING;
            reset_keys_for_transition();
        }
        return;
    }

    if (state == PLAYING) {
        if (pause_pressed) {
            state = PAUSED;
            reset_keys_for_transition();
            return;
        }
        update_playing();
        return;
    }

    if (state == PAUSED) {
        if (pause_pressed || start_pressed) {
            state = PLAYING;
            reset_keys_for_transition();
        }
    }
}

static void render(void) {
    if (state == MENU) {
        draw_menu();
        return;
    }

    draw_road();
    draw_player();

    if (state == PAUSED) {
        draw_pause_overlay();
    }
}

static void vga_init(void) {
    front_buf = (volatile short *)PIX_BUF_FRONT;
    back_buf  = (volatile short *)PIX_BUF_BACK;
    draw_buf = back_buf;

    /* Set initial back buffer address. */
    pbc[1] = (unsigned int)(uintptr_t)back_buf;

    /* Clear both pages once. */
    draw_buf = front_buf;
    clear_screen(C_BLACK);
    draw_buf = back_buf;
    clear_screen(C_BLACK);
}

static void present_frame(void) {
    /* Request page flip at vsync. */
    pbc[0] = 1u;
    /* Must wait until swap completes (Status.S == 0) before drawing again. */
    while (pbc[3] & 1u) { }

    /* Our back buffer becomes front, and vice versa. */
    volatile short *tmp = front_buf;
    front_buf = back_buf;
    back_buf = tmp;
    draw_buf = back_buf;

    /* Tell controller which buffer to use as back buffer for next frame. */
    pbc[1] = (unsigned int)(uintptr_t)back_buf;
}

/* ===================== Entry ===================== */
int main(void) {
    vga_init();
    timer_init();

    state = MENU;
    frame_count = 0;
    reset_input_state();

    while (1) {
        timer_wait();
        update();
        render();
        present_frame();
    }

    return 0;
}
