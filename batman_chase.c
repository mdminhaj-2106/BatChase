/*
 * batman_chase.c
 * Batman: Gotham Chase — DE1-SoC / CPUlator port
 *
 * TARGET  : ARM Cortex-A9 · DE1-SoC · CPUlator  (https://cpulator.01xz.net/?sys=arm-de1soc)
 * DISPLAY : VGA 320×240 pixel buffer (RGB565, 16-bit)
 * INPUT   : PS/2 keyboard scan-codes
 * TIMER   : DE1-SoC Interval Timer @ 0xFF202000  (50 MHz → ~60 fps)
 *
 * HOW TO RUN ON CPUlator
 * ----------------------
 * 1. Open  https://cpulator.01xz.net/?sys=arm-de1soc
 * 2. Paste THIS file into the C editor.
 * 3. Click  "Compile and Load"  then  "Continue" (F5).
 * 4. Watch the VGA panel — you'll see the menu screen.
 * 5. Click anywhere in the CPUlator page so it captures keystrokes,
 *    then press ENTER to start.
 *
 * Controls:
 *   LEFT / RIGHT  — steer the Batmobile
 *   UP            — fire homing rocket (needs missile powerup)
 *   ENTER         — confirm menu / unpause
 *   ESC           — pause in-game
 *
 * FIXES vs. batman_gotham_chase.md source
 * ----------------------------------------
 *  [F1] rng is now `unsigned int` (32-bit) — avoids UB on 32-bit shifts.
 *  [F2] irand() guards against divide-by-zero when lo == hi.
 *  [F3] read_ps2() now drains the full FIFO each frame (loop until RVALID=0).
 *  [F4] draw_label() removed — was defined but never called (dead code).
 *  [F5] draw_num() reverses digits correctly; no functional change, comment clarified.
 *  [F6] All address constants use the DE1-SoC Interval Timer (0xFF202000),
 *        NOT the A9 Private Timer (0xFFFEC600) which is inaccessible in CPUlator.
 *  [F7] REMOVED double-buffering. CPUlator only maps ONE VGA pixel buffer at
 *        0xC8000000. Writing to 0xC8100000 crashes with "not mapped to any device".
 *        draw_buf always points to 0xC8000000; swap_buffers() polls vsync only.
 */

#include <stdlib.h>
#include <stdbool.h>

/* ================================================================
 *  Hardware Addresses  (DE1-SoC / CPUlator)
 * ================================================================ */
#define PIX_CTRL_BASE   0xFF203020u   /* VGA Pixel Buffer Controller          */
#define PIX_BUF1        0xC8000000u   /* Single VGA pixel buffer (CPUlator)   */
/* PIX_BUF2 (0xC8100000) removed — [F7] not mapped in CPUlator, causes crash  */
#define PS2_BASE        0xFF200100u   /* PS/2 keyboard data register          */
#define TIMER_BASE      0xFF202000u   /* DE1-SoC Interval Timer @ 50 MHz      */
                                      /* [F6] NOT 0xFFFEC600 (A9 private tmr) */

/* ================================================================
 *  Screen / Road Constants
 * ================================================================ */
#define SCR_W           320
#define SCR_H           240
#define PIX_STRIDE      512           /* hardware row stride in shorts        */

#define ROAD_L          80
#define ROAD_R          240
#define ROAD_W          160
#define ROAD_CX         ((ROAD_L + ROAD_R) / 2)

/* ================================================================
 *  Colour Palette  (RGB565)
 * ================================================================ */
#define C_BLACK     0x0000u
#define C_WHITE     0xFFFFu
#define C_RED       0xF800u
#define C_GREEN     0x07E0u
#define C_BLUE      0x001Fu
#define C_YELLOW    0xFFE0u
#define C_ORANGE    0xFC00u
#define C_CYAN      0x07FFu
#define C_MAGENTA   0xF81Fu
#define C_DGREY     0x4208u           /* dark grey  */
#define C_MGREY     0x8410u           /* mid  grey  */
#define C_LGREY     0xCE59u           /* light grey */
#define C_ROAD      0x2104u           /* asphalt    */
#define C_SIDEWALK  0x7BCFu
#define C_BATBLUE   0x0019u           /* dark blue highlights */
#define C_HEALTH    0x07E0u
#define C_STUD      0xFFE0u

/* ================================================================
 *  Game Tuning
 * ================================================================ */
#define MAX_ENEMIES     14
#define MAX_STUDS       24
#define MAX_OBSTACLES    8
#define MAX_POWERUPS     4

#define PL_W            16            /* player width  */
#define PL_H            24            /* player height */
#define EN_W            14            /* enemy  width  */
#define EN_H            18            /* enemy  height */
#define STD_R            4            /* stud radius   */
#define OBS_W           26
#define OBS_H           18
#define PWR_W           12
#define PWR_H           12
#define BOSS_W          48
#define BOSS_H          36
#define MISS_W           4
#define MISS_H           8

#define PL_MAX_HP        5
#define PL_BASE_SPD      2
#define BOSS_BASE_HP    10
#define SCROLL_BASE      1
#define INVULN_FRAMES   90            /* ~1.5 s */
#define BOOST_FRAMES   180            /* 3 s    */
#define MAGNET_FRAMES  300            /* 5 s    */
#define BOSS_PERIOD    600            /* frames between boss spawns */
#define SPAWN_PERIOD   100

/* ================================================================
 *  Enumerations
 * ================================================================ */
typedef enum { MENU, PLAYING, PAUSED, GAMEOVER } GState;

typedef enum {
    E_SWAY,       /* drone — oscillates horizontally */
    E_DIVE,       /* waits then dives at player      */
    E_ASSAULT,    /* fast straight-down scooter      */
    E_PROJECTILE  /* boss-fired hazard               */
} EType;

typedef enum { PW_MAGNET, PW_BOOST, PW_MISSILE } PwType;

typedef enum { BOSS_FREEZE, BOSS_RIDDLER, BOSS_JOKER } BossType;

/* ================================================================
 *  Structs
 * ================================================================ */
typedef struct {
    int  x, y, vx;
    int  hp, invuln;
    int  score, studs;
    bool alive, boosting;
    int  boost_t, magnet_t, missiles;
} Player;

typedef struct {
    int   x, y, vx, vy;
    EType type;
    bool  active, activated, diving;
    int   init_x;
    int   timer;          /* sway phase / dive countdown */
} Enemy;

typedef struct {
    int  x, y;
    bool active, collected;
} Stud;

typedef struct {
    int  x, y, w, h;
    bool active;
} Obstacle;

typedef struct {
    int   x, y;
    PwType type;
    bool  active;
    int   anim;
} Powerup;

typedef struct {
    int  x, y, vx, vy, life;
    bool active;
} Missile;

typedef struct {
    int      x, y, hp, max_hp;
    BossType type;
    bool     active, intro;
    int      timer, atk_timer;
} Boss;

typedef struct {
    GState state;
    int    frame, scroll, scroll_spd;
    int    spawn_t, boss_t, bosses;
    bool   boss_active;
} Ctx;

/* ================================================================
 *  Globals
 * ================================================================ */
/* [F7] Single buffer — CPUlator only maps 0xC8000000 */
static volatile short *draw_buf = (volatile short *)PIX_BUF1;

static Player    pl;
static Enemy     enemies[MAX_ENEMIES];
static Stud      studs[MAX_STUDS];
static Obstacle  obs[MAX_OBSTACLES];
static Powerup   pwrs[MAX_POWERUPS];
static Missile   rocket;
static Boss      boss;
static Ctx       ctx;

/* PS/2 key state */
static bool k_left, k_right, k_up, k_enter, k_esc;
static bool k_up_prev;

/* ================================================================
 *  Tiny Math
 * ================================================================ */
static int iabs(int x)                { return x < 0 ? -x : x; }
static int iclamp(int v, int lo, int hi) { return v<lo?lo:v>hi?hi:v; }
static int imin(int a, int b)         { return a<b?a:b; }
static int imax(int a, int b)         { return a>b?a:b; }

/* Lightweight PRNG (Galois LFSR) — [F1] use unsigned int (32-bit) */
static unsigned int rng = 0xACE1u;
static int irand(int lo, int hi) {
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    /* [F2] guard against divide-by-zero when lo == hi */
    int range = hi - lo + 1;
    if (range <= 0) return lo;
    return lo + (int)(rng & 0x7FFFu) % range;
}

/* Integer sin: angle in [0..255] → output in [-256..256] */
static const short SIN4[64] = {
      0,  25,  50,  75, 100, 125, 150, 175,
    198, 222, 245, 267, 289, 310, 330, 350,
    368, 385, 401, 416, 429, 441, 452, 462,
    471, 478, 484, 489, 492, 494, 495, 495,
    494, 492, 489, 484, 478, 471, 462, 452,
    441, 429, 416, 401, 385, 368, 350, 330,
    310, 289, 267, 245, 222, 198, 175, 150,
    125, 100,  75,  50,  25,   0, -25, -50
};
static int isin(int a) {
    a &= 255;
    if (a <  64) return  SIN4[a];
    if (a < 128) return  SIN4[127 - a];
    if (a < 192) return -SIN4[a - 128];
    return             -SIN4[255 - a];
}

/* ================================================================
 *  VGA Drawing Primitives
 * ================================================================ */
static inline void pset(int x, int y, unsigned short c) {
    if ((unsigned)x < SCR_W && (unsigned)y < SCR_H)
        draw_buf[y * PIX_STRIDE + x] = c;
}

static void frect(int x, int y, int w, int h, unsigned short c) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > SCR_W) w = SCR_W - x;
    if (y + h > SCR_H) h = SCR_H - y;
    for (int j = y; j < y+h; j++)
        for (int i = x; i < x+w; i++)
            draw_buf[j * PIX_STRIDE + i] = c;
}

static void hline(int x, int y, int len, unsigned short c) {
    frect(x, y, len, 1, c);
}

static void vline(int x, int y, int len, unsigned short c) {
    frect(x, y, 1, len, c);
}

static void drect(int x, int y, int w, int h, unsigned short c) {
    hline(x,     y,       w, c);
    hline(x,     y+h-1,   w, c);
    vline(x,     y,       h, c);
    vline(x+w-1, y,       h, c);
}

/* Filled diamond ≈ circle, used for studs and HP dots */
static void fdiamond(int cx, int cy, int r, unsigned short c) {
    for (int dy = -r; dy <= r; dy++) {
        int hw = r - iabs(dy);
        hline(cx - hw, cy + dy, hw * 2 + 1, c);
    }
}

/* ================================================================
 *  Pixel-font Digit Renderer (5 × 7)
 * ================================================================ */
static const unsigned char DGLYPH[10][7] = {
    {0x1C,0x14,0x14,0x14,0x14,0x14,0x1C}, /* 0 */
    {0x04,0x04,0x04,0x04,0x04,0x04,0x04}, /* 1 */
    {0x1C,0x04,0x04,0x1C,0x10,0x10,0x1C}, /* 2 */
    {0x1C,0x04,0x04,0x1C,0x04,0x04,0x1C}, /* 3 */
    {0x14,0x14,0x14,0x1C,0x04,0x04,0x04}, /* 4 */
    {0x1C,0x10,0x10,0x1C,0x04,0x04,0x1C}, /* 5 */
    {0x1C,0x10,0x10,0x1C,0x14,0x14,0x1C}, /* 6 */
    {0x1C,0x04,0x04,0x04,0x04,0x04,0x04}, /* 7 */
    {0x1C,0x14,0x14,0x1C,0x14,0x14,0x1C}, /* 8 */
    {0x1C,0x14,0x14,0x1C,0x04,0x04,0x1C}, /* 9 */
};

static void draw_digit(int x, int y, int d, unsigned short c) {
    if (d < 0 || d > 9) return;
    for (int row = 0; row < 7; row++) {
        unsigned char bits = DGLYPH[d][row];
        for (int col = 0; col < 5; col++)
            if (bits & (0x10 >> col))
                pset(x + col, y + row, c);
    }
}

static void draw_num(int x, int y, int n, unsigned short c) {
    if (n < 0) n = 0;
    char buf[10];
    int len = 0;
    if (n == 0) { draw_digit(x, y, 0, c); return; }
    while (n > 0) { buf[len++] = (char)(n % 10); n /= 10; }
    /* digits stored LSB-first in buf; render most-significant first */
    for (int i = len - 1; i >= 0; i--) {
        draw_digit(x, y, (int)buf[i], c);
        x += 6;
    }
}

/* [F4] draw_label() removed — was defined but never called anywhere. */

/* ================================================================
 *  Object Renderers
 * ================================================================ */

/* --- Batmobile (player) --- */
static void draw_player(void) {
    if (!pl.alive) return;
    /* Invulnerability blink — skip every other 4-frame pair */
    if (pl.invuln > 0 && (ctx.frame & 3)) return;

    int x = pl.x - PL_W/2, y = pl.y - PL_H/2;

    /* Boost trail */
    if (pl.boosting) {
        frect(x+3,       y+PL_H,   PL_W-6, 5, C_ORANGE);
        frect(x+4,       y+PL_H+5, PL_W-8, 3, C_YELLOW);
    }
    /* Car body */
    frect(x+2,       y+3,       PL_W-4, PL_H-6, C_DGREY);
    /* Windshield */
    frect(x+4,       y+7,       PL_W-8, 6,      C_BATBLUE);
    /* Bat fin (top centre) */
    frect(x+PL_W/2-1, y,        3,      4,      C_DGREY);
    /* Wheels */
    frect(x,          y+4,      3,      5,      C_BLACK);
    frect(x+PL_W-3,   y+4,      3,      5,      C_BLACK);
    frect(x,          y+PL_H-9, 3,      5,      C_BLACK);
    frect(x+PL_W-3,   y+PL_H-9, 3,     5,      C_BLACK);
    /* Bat logo — three yellow pixels at centre */
    pset(pl.x-1, pl.y+2, C_YELLOW);
    pset(pl.x,   pl.y+2, C_YELLOW);
    pset(pl.x+1, pl.y+2, C_YELLOW);
}

/* --- Enemy vehicle --- */
static void draw_enemy(const Enemy *e) {
    if (!e->active || !e->activated) return;
    int x = e->x - EN_W/2, y = e->y - EN_H/2;
    unsigned short col;
    switch (e->type) {
        case E_SWAY:       col = C_CYAN;    break;
        case E_DIVE:       col = C_ORANGE;  break;
        case E_ASSAULT:    col = C_MAGENTA; break;
        case E_PROJECTILE: col = C_RED;     break;
        default:           col = C_RED;     break;
    }
    frect(x, y, EN_W, EN_H, col);
    /* headlights */
    frect(x+2,       y+EN_H-4, 3, 3, C_YELLOW);
    frect(x+EN_W-5,  y+EN_H-4, 3, 3, C_YELLOW);
    /* outline */
    drect(x, y, EN_W, EN_H, C_DGREY);
}

/* --- Stud --- */
static void draw_stud(const Stud *s) {
    if (!s->active || s->collected) return;
    fdiamond(s->x, s->y, STD_R, C_STUD);
    pset(s->x-1, s->y-2, C_WHITE);   /* shine pixel */
}

/* --- Obstacle --- */
static void draw_obstacle(const Obstacle *o) {
    if (!o->active) return;
    frect(o->x, o->y, o->w, o->h, C_MGREY);
    drect(o->x, o->y, o->w, o->h, C_WHITE);
    /* X mark */
    for (int i = 0; i < imin(o->w, o->h); i++) {
        pset(o->x + i,           o->y + i,         C_RED);
        pset(o->x + o->w-1 - i, o->y + i,         C_RED);
    }
}

/* --- Powerup --- */
static void draw_powerup(const Powerup *p) {
    if (!p->active) return;
    unsigned short col, hi;
    switch (p->type) {
        case PW_MAGNET:  col = C_BLUE;   hi = C_CYAN;   break;
        case PW_BOOST:   col = C_ORANGE; hi = C_YELLOW; break;
        case PW_MISSILE: col = C_GREEN;  hi = C_WHITE;  break;
        default:         col = C_WHITE;  hi = C_YELLOW; break;
    }
    unsigned short c = (p->anim & 8) ? hi : col;
    frect(p->x - PWR_W/2, p->y - PWR_H/2, PWR_W, PWR_H, c);
    drect(p->x - PWR_W/2, p->y - PWR_H/2, PWR_W, PWR_H, C_WHITE);
}

/* --- Boss --- */
static void draw_boss(void) {
    if (!boss.active) return;
    unsigned short col;
    switch (boss.type) {
        case BOSS_FREEZE:  col = C_CYAN;    break;
        case BOSS_RIDDLER: col = C_GREEN;   break;
        case BOSS_JOKER:   col = C_MAGENTA; break;
        default:           col = C_RED;     break;
    }
    int bx = boss.x - BOSS_W/2, by = boss.y - BOSS_H/2;
    frect(bx, by, BOSS_W, BOSS_H, col);
    drect(bx, by, BOSS_W, BOSS_H, C_WHITE);
    /* eyes */
    frect(bx+8,          by+8, 6, 6, C_BLACK);
    frect(bx+BOSS_W-14,  by+8, 6, 6, C_BLACK);
    pset(bx+10,          by+10, C_RED);
    pset(bx+BOSS_W-12,   by+10, C_RED);
    /* HP bar (above sprite) */
    int bw = (BOSS_W * boss.hp) / boss.max_hp;
    frect(bx, by-7, BOSS_W, 4, C_DGREY);
    frect(bx, by-7, bw,     4, C_RED);
    drect(bx, by-7, BOSS_W, 4, C_WHITE);
}

/* --- Homing Rocket --- */
static void draw_rocket(void) {
    if (!rocket.active) return;
    frect(rocket.x - MISS_W/2, rocket.y - MISS_H/2, MISS_W, MISS_H, C_GREEN);
    /* exhaust flicker */
    if (ctx.frame & 2)
        frect(rocket.x - 1, rocket.y + MISS_H/2, 2, 3, C_ORANGE);
}

/* --- Background / Road --- */
static void draw_background(void) {
    /* Sidewalks */
    frect(0,      0, ROAD_L,         SCR_H, C_SIDEWALK);
    frect(ROAD_R, 0, SCR_W - ROAD_R, SCR_H, C_SIDEWALK);
    /* Road surface */
    frect(ROAD_L, 0, ROAD_W, SCR_H, C_ROAD);
    /* Road edges */
    vline(ROAD_L-1, 0, SCR_H, C_WHITE);
    vline(ROAD_R,   0, SCR_H, C_WHITE);
    /* Dashed centre line (scrolls with ctx.scroll) */
    int dash_len = 18, gap_len = 12, period = dash_len + gap_len;
    int off = ctx.scroll % period;
    for (int y = -period + off; y < SCR_H; y += period)
        frect(ROAD_CX-1, y, 2, dash_len, C_WHITE);
    /* Kerb rumble strips */
    for (int y = -(ctx.scroll % 10); y < SCR_H; y += 10) {
        unsigned short kc = ((y / 10) & 1) ? C_RED : C_WHITE;
        frect(ROAD_L,   y, 4, 5, kc);
        frect(ROAD_R-4, y, 4, 5, kc);
    }
}

/* --- HUD --- */
static void draw_hud(void) {
    /* Left panel background */
    frect(0, 0, ROAD_L-1, SCR_H, 0x2104u);
    /* HP dots */
    for (int i = 0; i < pl.hp; i++)
        fdiamond(12, 16 + i*14, 5, C_HEALTH);
    /* HP empty slots */
    for (int i = pl.hp; i < PL_MAX_HP; i++)
        drect(7, 11 + i*14, 10, 10, C_DGREY);
    /* Powerup timer bars */
    if (pl.magnet_t > 0) {
        frect(4, 90, 12, 6, C_BLUE);
        frect(4, 90, 12 * pl.magnet_t / MAGNET_FRAMES, 6, C_CYAN);
    }
    if (pl.boost_t > 0) {
        frect(4, 100, 12, 6, C_DGREY);
        frect(4, 100, 12 * pl.boost_t / BOOST_FRAMES, 6, C_ORANGE);
    }
    if (pl.missiles > 0) {
        frect(4, 110, 12, 6, C_GREEN);
        draw_num(2, 120, pl.missiles, C_WHITE);
    }

    /* Right panel background */
    frect(ROAD_R+1, 0, SCR_W-ROAD_R-1, SCR_H, 0x2104u);
    /* Score */
    draw_num(ROAD_R+4, 8,  pl.score, C_YELLOW);
    /* Stud icon + count */
    fdiamond(ROAD_R+8, 28, 4, C_STUD);
    draw_num(ROAD_R+4, 36, pl.studs, C_YELLOW);
    /* Boss HP bar (top road strip) */
    if (boss.active) {
        int bw = ROAD_W * boss.hp / boss.max_hp;
        frect(ROAD_L, 0, ROAD_W, 4, C_DGREY);
        frect(ROAD_L, 0, bw,     4, C_RED);
    }
}

/* --- Screens --- */
static void draw_menu(void) {
    /*
     * SINGLE-PASS background: paint all 240 rows with a city-night gradient
     * so there is NO separate black-fill pass that causes visible flicker on
     * the single VGA buffer.
     *
     * RGB565 city gradient:
     *   Top (y=0)   → near-black (0x0003)
     *   Mid (y=120) → deep Gotham blue  (0x0013)
     *   Bottom      → dark amber city glow
     */
    for (int y = 0; y < SCR_H; y++) {
        unsigned short sky;
        if (y < 80) {
            /* Upper sky: deep blue that gets slightly brighter */
            int b = 3 + (y >> 3);          /* B channel 3..13  */
            int g = y >> 4;                /* G channel 0..5   */
            sky = (unsigned short)((g << 5) | b);
        } else if (y < 160) {
            /* City mid-section: deep purple-blue */
            int b = 13 + ((y - 80) >> 4);
            int g = 5  + ((y - 80) >> 4);
            sky = (unsigned short)((g << 5) | b);
        } else {
            /* Street level: dark warm amber glow */
            int r = (y - 160) >> 3;        /* R channel 0..10  */
            int g = 3 + ((y - 160) >> 4);
            int b = 8;
            sky = (unsigned short)((r << 11) | (g << 5) | b);
        }
        hline(0, y, SCR_W, sky);
    }

    /* Batman silhouette — C_DGREY so it's visible on the dark blue sky */
    frect(130, 28,  60, 52, C_DGREY);   /* cape / body */
    frect(140, 18,  10, 16, C_DGREY);   /* left ear    */
    frect(170, 18,  10, 16, C_DGREY);   /* right ear   */
    frect(148, 40,  24, 20, 0x2945u);   /* torso — slightly lighter grey */
    /* Yellow utility belt */
    frect(148, 58,  24,  4, C_YELLOW);
    /* Bat logo on chest */
    fdiamond(160, 50, 6, C_YELLOW);
    pset(154, 50, C_DGREY);
    pset(166, 50, C_DGREY);

    /* Neon city lights on skyline (give sense of Gotham depth) */
    for (int i = 0; i < 8; i++) {
        int lx = 10 + i * 38;
        frect(lx,    72, 3, 10, C_CYAN);
        frect(lx+10, 68, 3, 14, C_MAGENTA);
        frect(lx+20, 74, 3,  8, C_CYAN);
    }

    /* Title banner — bright yellow border, black fill */
    frect(55,  92, 210, 26, C_YELLOW);
    frect(57,  94, 206, 22, C_BLACK);
    /* "BATMAN" letter blocks in yellow */
    int tx = 65;
    unsigned short tc = C_YELLOW;
    /* B */
    frect(tx,   96, 4, 18, tc); frect(tx+4,  96,  8, 4, tc);
    frect(tx+4, 103, 8,  4, tc); frect(tx+4, 110,  8, 4, tc);
    tx += 16;
    /* A */
    frect(tx,   100, 4, 14, tc); frect(tx+8, 100, 4, 14, tc);
    frect(tx+4,  96, 4,  4, tc); frect(tx+4, 103, 4,  4, tc);
    tx += 16;
    /* T */
    frect(tx,    96, 16,  4, tc); frect(tx+6,  96, 4, 18, tc);
    tx += 18;
    /* M */
    frect(tx,    96,  4, 18, tc); frect(tx+12, 96, 4, 18, tc);
    frect(tx+4,  98,  4,  5, tc); frect(tx+8,  98, 4,  5, tc);
    tx += 18;
    /* A */
    frect(tx,   100, 4, 14, tc); frect(tx+8, 100, 4, 14, tc);
    frect(tx+4,  96, 4,  4, tc); frect(tx+4, 103, 4,  4, tc);
    tx += 16;
    /* N */
    frect(tx,    96,  4, 18, tc); frect(tx+12, 96, 4, 18, tc);
    frect(tx+4,  98,  4,  4, tc); frect(tx+8, 102, 4,  4, tc);

    /* Subtitle "GOTHAM CHASE" hint bar */
    frect(90, 122, 140, 8, C_CYAN);
    frect(92, 123, 136, 6, C_BLACK);
    /* cyan dashes to represent text */
    for (int i = 0; i < 10; i++)
        frect(94 + i * 13, 125, 9, 2, C_CYAN);

    /* Controls section */
    frect(60, 140, 200, 1, C_MGREY);
    /* Arrow key icons */
    frect(92,  148, 18, 8, C_DGREY); drect(92,  148, 18, 8, C_LGREY);  /* ◄ */
    frect(114, 148, 18, 8, C_DGREY); drect(114, 148, 18, 8, C_LGREY);  /* ↑ */
    frect(136, 148, 18, 8, C_DGREY); drect(136, 148, 18, 8, C_LGREY);  /* ► */
    /* ENTER key icon */
    frect(170, 148, 34, 8, C_DGREY); drect(170, 148, 34, 8, C_LGREY);

    /*
     * "PRESS ENTER TO START" blink — alternate YELLOW ↔ ORANGE so it is
     * always bright and clearly visible (never goes dark grey like before).
     */
    unsigned short blink_col = (ctx.frame & 32) ? C_YELLOW : C_ORANGE;
    frect(80, 165, 160, 14, blink_col);
    frect(82, 167, 156, 10, C_BLACK);
    /* draw "ENTER" as bright coloured dashes inside the box */
    for (int i = 0; i < 6; i++)
        frect(90 + i * 22, 170, 16, 4, blink_col);

    frect(60, 182, 200, 1, C_MGREY);

    /* High score */
    fdiamond(130, 200, 4, C_STUD);
    draw_num(140, 196, pl.score, C_YELLOW);
}

static void draw_pause(void) {
    /* Dim overlay — darken every even row */
    for (int y = 0; y < SCR_H; y += 2)
        hline(0, y, SCR_W, C_BLACK);
    frect(80,  90, 160, 60, C_DGREY);
    drect(80,  90, 160, 60, C_YELLOW);
    frect(100, 108, 60,  7, C_YELLOW);   /* "PAUSED" block  */
    frect(100, 122, 120, 7, C_MGREY);    /* hint block      */
}

static void draw_gameover(void) {
    frect( 50, 70, 220, 100, C_BLACK);
    drect( 50, 70, 220, 100, C_RED);
    frect( 70, 85, 180,   7, C_RED);     /* "GAME OVER" bar */
    draw_num(130, 103, pl.score, C_YELLOW);
    draw_num(110, 120, pl.studs, C_STUD);
    if (ctx.frame & 32)
        frect(90, 145, 140, 6, C_WHITE); /* "PRESS ENTER" blink */
}

/* ================================================================
 *  PS/2 Input   [F3] — drain the full FIFO each frame
 * ================================================================ */
static volatile unsigned *ps2 = (volatile unsigned *)PS2_BASE;

static void read_ps2(void) {
    static bool ext = false, rel = false;

    /* Loop until the RVALID bit (bit 15) is clear */
    while (1) {
        unsigned d = *ps2;
        if (!(d & 0x8000u)) break;     /* no more data in FIFO */

        unsigned byte = d & 0xFF;

        if (byte == 0xE0) { ext = true;  continue; }
        if (byte == 0xF0) { rel = true;  continue; }

        bool pressed = !rel;
        rel = false;

        if (ext) {
            switch (byte) {
                case 0x6B: k_left  = pressed; break;   /* ← */
                case 0x74: k_right = pressed; break;   /* → */
                case 0x75: k_up    = pressed; break;   /* ↑ */
            }
            ext = false;
        } else {
            switch (byte) {
                case 0x5A: k_enter = pressed; break;   /* Enter */
                case 0x76: k_esc   = pressed; break;   /* ESC   */
            }
        }
    }
}

/* ================================================================
 *  Interval Timer  (DE1-SoC, 50 MHz → ~60 fps)
 *  Register map (word offsets):
 *    [0] = Status   [1] = Control
 *    [2] = Period-lo [3] = Period-hi
 * ================================================================ */
static volatile unsigned *tmr = (volatile unsigned *)TIMER_BASE;

static void timer_init(void) {
    /* 50 000 000 / 60 ≈ 833 333 = 0x000C_B4B3 */
    tmr[2] = 0xB4B3u;   /* period low  */
    tmr[3] = 0x000Cu;   /* period high */
    tmr[1] = 0x0007u;   /* START | CONT | ITO */
}

static void timer_wait(void) {
    while (!(tmr[0] & 1u));   /* spin until timeout flag set */
    tmr[0] = 0;               /* clear TO bit                */
}

/* ================================================================
 *  VGA — Single Buffer  [F7]
 *  CPUlator maps only ONE pixel buffer at 0xC8000000.
 *  Double-buffering removed to prevent "not mapped" crash.
 * ================================================================ */
static volatile unsigned *pbc = (volatile unsigned *)PIX_CTRL_BASE;

static void vga_init(void) {
    /* draw_buf is statically set to PIX_BUF1 at declaration */
    /* Clear the single buffer to black */
    for (int i = 0; i < PIX_STRIDE * SCR_H; i++)
        draw_buf[i] = C_BLACK;
}

/* No buffer swap — just poll the VGA status bit for vsync pacing */
static void swap_buffers(void) {
    /* S bit (bit 0 of pbc[3]) goes low after the frame is scanned out.
     * Waiting for it gives ~60 Hz pacing without a second buffer. */
    while (pbc[3] & 1u);   /* wait until display is not mid-swap */
}

/* ================================================================
 *  Spawn Helpers
 * ================================================================ */
static void spawn_studs(int cx, int cy, int n) {
    for (int i = 0; i < MAX_STUDS && n > 0; i++) {
        if (!studs[i].active) {
            studs[i].active    = true;
            studs[i].collected = false;
            studs[i].x = cx + irand(-28, 28);
            studs[i].y = cy;
            n--;
        }
    }
}

static void spawn_enemy(EType t) {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!enemies[i].active) {
            Enemy *e = &enemies[i];
            e->active    = true;
            e->activated = false;
            e->diving    = false;
            e->type      = t;
            e->x         = irand(ROAD_L + EN_W/2 + 4, ROAD_R - EN_W/2 - 4);
            e->y         = -EN_H;
            e->init_x    = e->x;
            e->vx        = 0;
            e->timer     = 0;
            switch (t) {
                case E_SWAY:
                    e->vy    = 1;
                    e->timer = irand(0, 255);   /* phase offset */
                    break;
                case E_DIVE:
                    e->vy    = 1;
                    e->timer = irand(60, 140);  /* dive countdown */
                    break;
                case E_ASSAULT:
                    e->vy    = 2;
                    break;
                case E_PROJECTILE:
                    e->vy    = 3;
                    e->vx    = irand(-2, 2);
                    break;
            }
            return;
        }
    }
}

static void spawn_proj(int x, int y) {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!enemies[i].active) {
            enemies[i].active    = true;
            enemies[i].activated = true;
            enemies[i].type      = E_PROJECTILE;
            enemies[i].x        = x;
            enemies[i].y        = y;
            enemies[i].vx       = irand(-2, 2);
            enemies[i].vy       = 3;
            enemies[i].timer    = 0;
            return;
        }
    }
}

static void spawn_obstacle(void) {
    for (int i = 0; i < MAX_OBSTACLES; i++) {
        if (!obs[i].active) {
            obs[i].active = true;
            obs[i].x = irand(ROAD_L + 4, ROAD_R - OBS_W - 4);
            obs[i].y = -OBS_H;
            obs[i].w = OBS_W;
            obs[i].h = OBS_H;
            return;
        }
    }
}

static void spawn_powerup(void) {
    for (int i = 0; i < MAX_POWERUPS; i++) {
        if (!pwrs[i].active) {
            pwrs[i].active = true;
            pwrs[i].x      = irand(ROAD_L + PWR_W, ROAD_R - PWR_W);
            pwrs[i].y      = -PWR_H;
            pwrs[i].type   = (PwType)irand(0, 2);
            pwrs[i].anim   = 0;
            return;
        }
    }
}

/* ================================================================
 *  Game Logic — Init
 * ================================================================ */
static void game_init(void) {
    pl.x = ROAD_CX; pl.y = SCR_H - 38;
    pl.vx     = 0;
    pl.hp     = PL_MAX_HP;
    pl.invuln = 0;
    pl.score  = 0;
    pl.studs  = 0;
    pl.alive  = true;
    pl.boosting  = false;
    pl.boost_t   = 0;
    pl.magnet_t  = 0;
    pl.missiles  = 0;

    for (int i = 0; i < MAX_ENEMIES;   i++) enemies[i].active = false;
    for (int i = 0; i < MAX_STUDS;     i++) studs[i].active   = false;
    for (int i = 0; i < MAX_OBSTACLES; i++) obs[i].active     = false;
    for (int i = 0; i < MAX_POWERUPS;  i++) pwrs[i].active    = false;
    rocket.active   = false;
    boss.active     = false;

    ctx.scroll      = 0;
    ctx.scroll_spd  = SCROLL_BASE;
    ctx.frame       = 0;
    ctx.spawn_t     = SPAWN_PERIOD;
    ctx.boss_t      = BOSS_PERIOD;
    ctx.bosses      = 0;
    ctx.boss_active = false;
}

/* ================================================================
 *  Game Logic — Update
 * ================================================================ */

/* AABB collision test */
static bool aabb(int ax, int ay, int aw, int ah,
                 int bx, int by, int bw, int bh) {
    return !(ax+aw <= bx || bx+bw <= ax || ay+ah <= by || by+bh <= ay);
}

/* --- Player --- */
static void update_player(void) {
    if (!pl.alive) return;

    int spd = pl.boosting ? PL_BASE_SPD + 2 : PL_BASE_SPD;

    if (k_left)       pl.vx -= 1;
    else if (k_right) pl.vx += 1;
    else              pl.vx  = pl.vx * 3 / 4;

    pl.vx = iclamp(pl.vx, -spd*2, spd*2);
    pl.x += pl.vx;
    pl.x  = iclamp(pl.x, ROAD_L + PL_W/2 + 1, ROAD_R - PL_W/2 - 1);

    if (pl.invuln  > 0) pl.invuln--;
    if (pl.boost_t > 0) {
        pl.boost_t--;
        if (pl.boost_t == 0) {
            pl.boosting    = false;
            ctx.scroll_spd = SCROLL_BASE;
        }
    }
    if (pl.magnet_t > 0) pl.magnet_t--;

    /* Distance score tick */
    if (ctx.frame % 20 == 0) pl.score++;
}

/* --- Fire rocket --- */
static void fire_rocket(void) {
    if (pl.missiles > 0 && !rocket.active) {
        rocket.active = true;
        rocket.x      = pl.x;
        rocket.y      = pl.y - PL_H/2;
        rocket.vx     = 0;
        rocket.vy     = -4;
        rocket.life   = 150;
        pl.missiles--;
    }
}

/* --- Update rocket (homing) --- */
static void update_rocket(void) {
    if (!rocket.active) return;
    if (--rocket.life <= 0) { rocket.active = false; return; }

    /* Find target: boss first, then nearest enemy */
    int tx = -1, ty = -1;
    if (boss.active) {
        tx = boss.x; ty = boss.y;
    } else {
        int best = 99999;
        for (int i = 0; i < MAX_ENEMIES; i++) {
            if (!enemies[i].active) continue;
            int d = iabs(enemies[i].x - rocket.x) + iabs(enemies[i].y - rocket.y);
            if (d < best) { best = d; tx = enemies[i].x; ty = enemies[i].y; }
        }
    }

    if (tx >= 0) {
        int dx = tx - rocket.x, dy = ty - rocket.y;
        int d  = imax(iabs(dx) + iabs(dy), 1);
        rocket.vx = iclamp(rocket.vx + dx * 3 / d, -5, 5);
        rocket.vy = iclamp(rocket.vy + dy * 3 / d, -7, 3);
    }

    rocket.x += rocket.vx;
    rocket.y += rocket.vy;

    /* Hit boss */
    if (boss.active &&
        iabs(rocket.x - boss.x) < BOSS_W/2 &&
        iabs(rocket.y - boss.y) < BOSS_H/2) {
        rocket.active = false;
        boss.hp--;
        pl.score += 10;
        if (boss.hp <= 0) {
            spawn_studs(boss.x, boss.y, 12);
            boss.active     = false;
            ctx.boss_active = false;
            ctx.boss_t      = BOSS_PERIOD;
            ctx.bosses++;
            pl.score += 100;
        }
        return;
    }
    /* Hit enemy */
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!enemies[i].active) continue;
        if (iabs(rocket.x - enemies[i].x) < EN_W &&
            iabs(rocket.y - enemies[i].y) < EN_H) {
            enemies[i].active = false;
            rocket.active     = false;
            pl.score += 5;
            spawn_studs(enemies[i].x, enemies[i].y, 3);
            return;
        }
    }
    /* Despawn when off-screen */
    if (rocket.y < -10 || rocket.x < ROAD_L || rocket.x > ROAD_R)
        rocket.active = false;
}

/* --- Enemies --- */
static void update_enemies(void) {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy *e = &enemies[i];
        if (!e->active) continue;

        e->y += ctx.scroll_spd;

        if (!e->activated && e->y >= 0) e->activated = true;

        if (e->activated) {
            switch (e->type) {
                case E_SWAY:
                    e->timer++;
                    e->x = e->init_x + (isin(e->timer * 2 & 0xFF) * 35) / 256;
                    e->x = iclamp(e->x, ROAD_L + EN_W/2+2, ROAD_R - EN_W/2-2);
                    break;
                case E_DIVE:
                    if (!e->diving) {
                        if (--e->timer <= 0) {
                            e->diving = true;
                            int dx = pl.x - e->x;
                            int dy = pl.y - e->y;
                            int d  = imax(iabs(dx)+iabs(dy), 1);
                            e->vx  = dx * 3 / (d / 10 + 1);
                        }
                    } else {
                        e->x += e->vx;
                        e->y += 1;   /* extra downward speed while diving */
                    }
                    e->x = iclamp(e->x, ROAD_L + EN_W/2, ROAD_R - EN_W/2);
                    break;
                case E_ASSAULT:
                    e->y += 1;       /* extra fast */
                    break;
                case E_PROJECTILE:
                    e->x += e->vx;
                    if (e->x < ROAD_L || e->x > ROAD_R) e->vx = -e->vx;
                    break;
            }
        }

        if (e->y > SCR_H + EN_H) e->active = false;
    }
}

/* --- Studs --- */
static void update_studs(void) {
    for (int i = 0; i < MAX_STUDS; i++) {
        Stud *s = &studs[i];
        if (!s->active) continue;
        s->y += ctx.scroll_spd;

        /* Magnet pull */
        if (pl.magnet_t > 0) {
            int dx = pl.x - s->x, dy = pl.y - s->y;
            int d  = iabs(dx) + iabs(dy);
            if (d < 80) { s->x += dx/4; s->y += dy/4; }
        }

        /* Collect */
        if (!s->collected) {
            int range = pl.magnet_t > 0 ? 14 : 9;
            if (iabs(s->x - pl.x) < range && iabs(s->y - pl.y) < range) {
                s->collected = true;
                pl.studs++;
                pl.score++;
            }
        }
        if (s->y > SCR_H + 8 || s->collected) s->active = false;
    }
}

/* --- Obstacles --- */
static void update_obstacles(void) {
    for (int i = 0; i < MAX_OBSTACLES; i++) {
        Obstacle *o = &obs[i];
        if (!o->active) continue;
        o->y += ctx.scroll_spd;

        if (pl.invuln <= 0 && pl.alive) {
            if (aabb(pl.x-PL_W/2, pl.y-PL_H/2, PL_W, PL_H,
                     o->x, o->y, o->w, o->h)) {
                pl.hp--;
                pl.invuln = INVULN_FRAMES;
                if (pl.hp <= 0) { pl.alive = false; ctx.state = GAMEOVER; }
            }
        }
        if (o->y > SCR_H + OBS_H) o->active = false;
    }
}

/* --- Powerups --- */
static void update_powerups(void) {
    for (int i = 0; i < MAX_POWERUPS; i++) {
        Powerup *p = &pwrs[i];
        if (!p->active) continue;
        p->y   += ctx.scroll_spd;
        p->anim++;

        if (iabs(p->x - pl.x) < PWR_W && iabs(p->y - pl.y) < PWR_H) {
            switch (p->type) {
                case PW_MAGNET:
                    pl.magnet_t = MAGNET_FRAMES;
                    break;
                case PW_BOOST:
                    pl.boosting    = true;
                    pl.boost_t     = BOOST_FRAMES;
                    ctx.scroll_spd = SCROLL_BASE * 2;
                    break;
                case PW_MISSILE:
                    if (pl.missiles < 5) pl.missiles++;
                    break;
            }
            p->active = false;
        }
        if (p->y > SCR_H + PWR_H) p->active = false;
    }
}

/* --- Boss --- */
static void update_boss(void) {
    if (!boss.active) return;
    boss.timer++;

    /* Intro slide-in from top */
    if (!boss.intro) {
        if (boss.y < 45) boss.y += 2;
        else             boss.intro = true;
        return;
    }

    /* Side-to-side patrol */
    boss.x = ROAD_CX + (isin(boss.timer & 0xFF) * 40) / 256;

    /* Projectile attacks — faster at low HP */
    int period = (boss.hp < boss.max_hp/2) ? 25 : 50;
    if (boss.timer % period == 0)
        spawn_proj(boss.x, boss.y + BOSS_H/2 + 2);

    /* Spread volley at very low HP */
    if (boss.hp <= boss.max_hp/4 && boss.timer % 80 == 0) {
        spawn_proj(boss.x - 20, boss.y + BOSS_H/2);
        spawn_proj(boss.x + 20, boss.y + BOSS_H/2);
    }
}

/* --- Boss start --- */
static void start_boss(void) {
    ctx.boss_active = true;
    boss.active     = true;
    boss.hp         = BOSS_BASE_HP + ctx.bosses * 2;
    boss.max_hp     = boss.hp;
    boss.x          = ROAD_CX;
    boss.y          = -BOSS_H;
    boss.type       = (BossType)(ctx.bosses % 3);
    boss.timer      = 0;
    boss.intro      = false;
    boss.atk_timer  = 0;
    /* Clear road for boss entrance */
    for (int i = 0; i < MAX_ENEMIES;   i++) enemies[i].active = false;
    for (int i = 0; i < MAX_OBSTACLES; i++) obs[i].active     = false;
}

/* --- Player ↔ enemy / boss collision --- */
static void check_player_hits(void) {
    if (pl.invuln > 0 || !pl.alive) return;
    int px = pl.x - PL_W/2, py = pl.y - PL_H/2;

    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy *e = &enemies[i];
        if (!e->active || !e->activated) continue;
        if (aabb(px, py, PL_W, PL_H,
                 e->x - EN_W/2, e->y - EN_H/2, EN_W, EN_H)) {
            pl.hp--;
            pl.invuln = INVULN_FRAMES;
            e->active = false;
            if (pl.hp <= 0) { pl.alive = false; ctx.state = GAMEOVER; }
            return;
        }
    }
    if (boss.active && boss.intro) {
        if (aabb(px, py, PL_W, PL_H,
                 boss.x-BOSS_W/2, boss.y-BOSS_H/2, BOSS_W, BOSS_H)) {
            pl.hp--;
            pl.invuln = INVULN_FRAMES;
            if (pl.hp <= 0) { pl.alive = false; ctx.state = GAMEOVER; }
        }
    }
}

/* --- Spawning scheduler --- */
static void handle_spawning(void) {
    ctx.scroll = (ctx.scroll + ctx.scroll_spd) % 512;

    if (--ctx.spawn_t <= 0) {
        ctx.spawn_t = imax(SPAWN_PERIOD - ctx.bosses * 8, 40);
        if (!ctx.boss_active) {
            int r = irand(0, 99);
            if      (r < 40)              spawn_enemy(E_SWAY);
            else if (r < 70 || ctx.bosses >= 1) spawn_enemy(E_DIVE);
            else                          spawn_enemy(E_ASSAULT);

            if (irand(0,2)==0) spawn_studs(irand(ROAD_L+20,ROAD_R-20), -8, irand(3,8));
            if (irand(0,3)==0) spawn_obstacle();
            if (irand(0,5)==0) spawn_powerup();
        }
    }

    if (!ctx.boss_active) {
        if (--ctx.boss_t <= 0) start_boss();
    }
}

/* ================================================================
 *  Top-Level Update & Render
 * ================================================================ */
static void update(void) {
    ctx.frame++;
    read_ps2();

    switch (ctx.state) {
        /* ---- MENU ---- */
        case MENU:
            if (k_enter) { game_init(); ctx.state = PLAYING; k_enter = false; }
            break;

        /* ---- PLAYING ---- */
        case PLAYING:
            update_player();
            handle_spawning();
            update_enemies();
            update_studs();
            update_obstacles();
            update_powerups();
            update_boss();
            update_rocket();
            check_player_hits();

            /* Fire rocket on fresh UP press (edge-detect) */
            if (k_up && !k_up_prev) fire_rocket();
            k_up_prev = k_up;

            if (k_esc) { ctx.state = PAUSED; k_esc = false; }
            break;

        /* ---- PAUSED ---- */
        case PAUSED:
            if (k_esc || k_enter) { ctx.state = PLAYING; k_esc = k_enter = false; }
            break;

        /* ---- GAME OVER ---- */
        case GAMEOVER:
            if (k_enter) { ctx.state = MENU; k_enter = false; }
            break;
    }
}

static void render(void) {
    switch (ctx.state) {
        case MENU:
            draw_menu();
            break;

        case PLAYING:
        case PAUSED:
            draw_background();
            for (int i = 0; i < MAX_STUDS;     i++) draw_stud(&studs[i]);
            for (int i = 0; i < MAX_OBSTACLES; i++) draw_obstacle(&obs[i]);
            for (int i = 0; i < MAX_POWERUPS;  i++) draw_powerup(&pwrs[i]);
            for (int i = 0; i < MAX_ENEMIES;   i++) draw_enemy(&enemies[i]);
            draw_boss();
            draw_rocket();
            draw_player();
            draw_hud();
            if (ctx.state == PAUSED) draw_pause();
            break;

        case GAMEOVER:
            draw_background();
            for (int i = 0; i < MAX_ENEMIES; i++) draw_enemy(&enemies[i]);
            draw_player();
            draw_hud();
            draw_gameover();
            break;
    }
}

/* ================================================================
 *  Entry Point
 * ================================================================ */
int main(void) {
    vga_init();
    timer_init();

    ctx.state = MENU;
    ctx.frame = 0;
    k_left = k_right = k_up = k_enter = k_esc = false;
    k_up_prev = false;

    while (1) {
        timer_wait();    /* ~60 fps frame gate            */
        update();        /* game state machine            */
        render();        /* draw everything to back buf   */
        swap_buffers();  /* vsync + present               */
    }

    return 0; /* unreachable on bare metal */
}
