/*
 * batman_chase_v2.c
 * Batman: Gotham Chase — DE1-SoC / CPUlator port  (v2)
 *
 * TARGET  : ARM Cortex-A9 · DE1-SoC · CPUlator  (https://cpulator.01xz.net/?sys=arm-de1soc)
 * DISPLAY : VGA 320×240 pixel buffer (RGB565, 16-bit)
 * INPUT   : PS/2 keyboard scan-codes
 * TIMER   : DE1-SoC Interval Timer @ 0xFF202000  (50 MHz → ~60 fps)
 *
 * CHANGES vs. batman_chase_fixed.c
 * ---------------------------------
 *  [V2-F1] Restored TRUE double-buffering.
 *          front = 0xC8000000, back = 0xC0000000  (per batman_gotham_chase.md §2).
 *          The previous "fix" mistakenly banned double-buffering; the real bug was
 *          using 0xC8100000 (unmapped).  0xC0000000 IS mapped on CPUlator/DE1-SoC.
 *  [V2-F2] swap_buffers() does a real page-flip + waits for vsync status bit.
 *  [V2-F3] dim_screen_half() replaces the old every-other-row black line overlay.
 *          It reads-back the drawn pixels and halves each channel in-place, giving
 *          a smooth translucent dim with zero flicker.
 *  [V2-3D-A] Perspective road: road narrows to a vanishing point at HORIZON_Y=48.
 *             Each scan-line has its own left/right edges, drawn in depth-cued colour.
 *  [V2-3D-B] Parallax background: 3 building silhouette layers scroll at ×1/4, ×1/2,
 *             and ×3/4 road speed, giving a sense of depth before the road.
 *  [V2-3D-C] Depth scale: sprites rendered with width/height proportional to Y depth
 *             (40% of full size at horizon, 100% at screen bottom).
 *  [V2-3D-D] Drop shadows: every game sprite has a dark offset shadow drawn first,
 *             giving a "lifted off the ground" 3-D appearance.
 *  [V2-3D-E] Sprite stacking on player: the Batmobile is drawn in 3 layers (shadow,
 *             body lower, body upper) each offset slightly to fake perspective thickness.
 */

#include <stdlib.h>
#include <stdbool.h>

/* ================================================================
 *  Hardware Addresses  (DE1-SoC / CPUlator)
 * ================================================================ */
#define PIX_CTRL_BASE   0xFF203020u   /* VGA Pixel Buffer Controller          */
#define PIX_BUF1        0xC8000000u   /* VGA front buffer (displayed)         */
#define PIX_BUF2        0xC0000000u   /* VGA back buffer  (drawn into)        */
                                      /* [V2-F1] per batman_gotham_chase.md §2 */
#define PS2_BASE        0xFF200100u   /* PS/2 keyboard data register          */
#define TIMER_BASE      0xFF202000u   /* DE1-SoC Interval Timer @ 50 MHz      */

/* ================================================================
 *  Screen / Road Constants
 * ================================================================ */
#define SCR_W           320
#define SCR_H           240
#define PIX_STRIDE      512           /* hardware row stride in shorts        */

/* Perspective road — horizon at y=48, full width at bottom */
#define HORIZON_Y       48
#define ROAD_FAR_HALF   20            /* half-width at horizon                */
#define ROAD_NEAR_HALF  80            /* half-width at screen bottom          */
#define ROAD_CX         160           /* horizontal centre unchanged          */

/* Legacy flat road constants (used for HUD layout / spawn bounds) */
#define ROAD_L          80
#define ROAD_R          240
#define ROAD_W          160

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
#define C_DGREY     0x4208u
#define C_MGREY     0x8410u
#define C_LGREY     0xCE59u
#define C_ROAD      0x2104u
#define C_ROAD_FAR  0x1082u           /* road darker near horizon             */
#define C_SIDEWALK  0x7BCFu
#define C_BATBLUE   0x0019u
#define C_HEALTH    0x07E0u
#define C_STUD      0xFFE0u
#define C_SHADOW    0x2104u           /* drop shadow colour                   */

/* ================================================================
 *  Game Tuning
 * ================================================================ */
#define MAX_ENEMIES     14
#define MAX_STUDS       24
#define MAX_OBSTACLES    8
#define MAX_POWERUPS     4

#define PL_W            16
#define PL_H            24
#define EN_W            14
#define EN_H            18
#define STD_R            4
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
#define INVULN_FRAMES   90
#define BOOST_FRAMES   180
#define MAGNET_FRAMES  300
#define BOSS_PERIOD    600
#define SPAWN_PERIOD   100

/* ================================================================
 *  Enumerations
 * ================================================================ */
typedef enum { MENU, PLAYING, PAUSED, GAMEOVER } GState;

typedef enum {
    E_SWAY,
    E_DIVE,
    E_ASSAULT,
    E_PROJECTILE
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
    int   timer;
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
/* [V2-F1] True double-buffering */
static volatile short *draw_buf;     /* back  buffer — we draw here          */
static volatile short *front_buf;    /* front buffer — display shows this    */

static Player    pl;
static Enemy     enemies[MAX_ENEMIES];
static Stud      studs[MAX_STUDS];
static Obstacle  obs[MAX_OBSTACLES];
static Powerup   pwrs[MAX_POWERUPS];
static Missile   rocket;
static Boss      boss;
static Ctx       ctx;

static bool k_left, k_right, k_up, k_enter, k_esc;
static bool k_up_prev;

/* ================================================================
 *  Tiny Math
 * ================================================================ */
static int iabs(int x)                  { return x < 0 ? -x : x; }
static int iclamp(int v, int lo, int hi){ return v<lo?lo:v>hi?hi:v; }
static int imin(int a, int b)           { return a<b?a:b; }
static int imax(int a, int b)           { return a>b?a:b; }

static unsigned int rng = 0xACE1u;
static int irand(int lo, int hi) {
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    int range = hi - lo + 1;
    if (range <= 0) return lo;
    return lo + (int)(rng & 0x7FFFu) % range;
}

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
 *  Perspective Helpers  [V2-3D-A]
 * ================================================================ */

/*
 * Return road left/right edge X at a given screen Y.
 * At y=HORIZON_Y  : road is ROAD_FAR_HALF  wide each side of CX.
 * At y=SCR_H-1    : road is ROAD_NEAR_HALF wide each side of CX.
 * Linear interpolation in integer arithmetic.
 */
static int road_left(int y) {
    if (y <= HORIZON_Y) return ROAD_CX - ROAD_FAR_HALF;
    int t = ((y - HORIZON_Y) * 256) / (SCR_H - HORIZON_Y);   /* 0..256 */
    int hw = ROAD_FAR_HALF + (t * (ROAD_NEAR_HALF - ROAD_FAR_HALF)) / 256;
    return ROAD_CX - hw;
}
static int road_right(int y) {
    if (y <= HORIZON_Y) return ROAD_CX + ROAD_FAR_HALF;
    int t = ((y - HORIZON_Y) * 256) / (SCR_H - HORIZON_Y);
    int hw = ROAD_FAR_HALF + (t * (ROAD_NEAR_HALF - ROAD_FAR_HALF)) / 256;
    return ROAD_CX + hw;
}

/*
 * Depth scale factor for a sprite at screen-Y position.
 * Returns 256 at bottom of screen (full size), ~100 at horizon (40%).
 * [V2-3D-C]
 */
static int depth_scale(int y) {
    if (y <= HORIZON_Y) return 100;
    int t = ((y - HORIZON_Y) * 156) / (SCR_H - HORIZON_Y); /* 0..156 */
    return 100 + t;   /* 100..256 */
}

/* ================================================================
 *  VGA Drawing Primitives
 * ================================================================ */
static inline void pset(int x, int y, unsigned short c) {
    if ((unsigned)x < SCR_W && (unsigned)y < (unsigned)SCR_H)
        draw_buf[y * PIX_STRIDE + x] = c;
}

static void frect(int x, int y, int w, int h, unsigned short c) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > SCR_W) w = SCR_W - x;
    if (y + h > SCR_H) h = SCR_H - y;
    if (w <= 0 || h <= 0) return;
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

static void fdiamond(int cx, int cy, int r, unsigned short c) {
    for (int dy = -r; dy <= r; dy++) {
        int hw = r - iabs(dy);
        hline(cx - hw, cy + dy, hw * 2 + 1, c);
    }
}

/*
 * Depth-aware scaled filled rect  [V2-3D-C]
 * x, y = centre of sprite (road coords), base_w/h = full-size dimensions.
 */
static void frect_3d(int cx, int cy, int base_w, int base_h, unsigned short c) {
    int sc  = depth_scale(cy);
    int sw  = imax((base_w * sc) / 256, 1);
    int sh  = imax((base_h * sc) / 256, 1);
    frect(cx - sw/2, cy - sh/2, sw, sh, c);
}

/*
 * Depth-aware outline rect [V2-3D-C]
 */
static void drect_3d(int cx, int cy, int base_w, int base_h, unsigned short c) {
    int sc  = depth_scale(cy);
    int sw  = imax((base_w * sc) / 256, 1);
    int sh  = imax((base_h * sc) / 256, 1);
    drect(cx - sw/2, cy - sh/2, sw, sh, c);
}

/*
 * Drop shadow — draw a dark rect slightly offset (right+down) before the sprite [V2-3D-D]
 */
static void draw_shadow(int cx, int cy, int base_w, int base_h) {
    int sc  = depth_scale(cy);
    int sw  = imax((base_w * sc) / 256, 1);
    int sh  = imax((base_h * sc) / 256 / 3, 1);  /* shadow is flat ellipse */
    /* shadow sits below and slightly right of object */
    int sx  = cx + sw/6;
    int sy  = cy + (base_h * sc / 256) / 2 + 1;
    frect(sx - sw/2, sy, sw, sh, C_SHADOW);
}

/* ================================================================
 *  Pixel-font Digit Renderer (5 × 7)
 * ================================================================ */
static const unsigned char DGLYPH[10][7] = {
    {0x1C,0x14,0x14,0x14,0x14,0x14,0x1C},
    {0x04,0x04,0x04,0x04,0x04,0x04,0x04},
    {0x1C,0x04,0x04,0x1C,0x10,0x10,0x1C},
    {0x1C,0x04,0x04,0x1C,0x04,0x04,0x1C},
    {0x14,0x14,0x14,0x1C,0x04,0x04,0x04},
    {0x1C,0x10,0x10,0x1C,0x04,0x04,0x1C},
    {0x1C,0x10,0x10,0x1C,0x14,0x14,0x1C},
    {0x1C,0x04,0x04,0x04,0x04,0x04,0x04},
    {0x1C,0x14,0x14,0x1C,0x14,0x14,0x1C},
    {0x1C,0x14,0x14,0x1C,0x04,0x04,0x1C},
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
    for (int i = len - 1; i >= 0; i--) {
        draw_digit(x, y, (int)buf[i], c);
        x += 6;
    }
}

/* ================================================================
 *  Object Renderers
 * ================================================================ */

/* --- Batmobile (player) — sprite stacked in 3 layers [V2-3D-E] --- */
static void draw_player(void) {
    if (!pl.alive) return;
    if (pl.invuln > 0 && (ctx.frame & 3)) return;

    int cx = pl.x, cy = pl.y;
    int sc = depth_scale(cy);
    int w  = imax((PL_W * sc) / 256, 3);
    int h  = imax((PL_H * sc) / 256, 4);
    int x  = cx - w/2;
    int y  = cy - h/2;

    /* Layer 0 — drop shadow on ground */
    draw_shadow(cx, cy, PL_W, PL_H);

    /* Boost trail — under body, stacked layer below */
    if (pl.boosting) {
        frect(x + w/5,     y + h,     w - w*2/5, h/4, C_ORANGE);
        frect(x + w/4,     y + h + h/4, w - w/2,  2,   C_YELLOW);
    }

    /* Layer 1 — lower body / suspension (offset 1px down = 3D thickness) */
    frect(x+1, y+h/4+1, w-2, h - h/4, 0x2945u);   /* darker base body */

    /* Layer 2 — main car body */
    frect(x+1,     y+h/6,   w-2,  h - h/4, C_DGREY);
    /* Windshield */
    frect(x+w/4,   y+h/3,   w/2,  h/4,     C_BATBLUE);
    /* Bat fin (top) */
    frect(cx-1,    y,        3,    h/6,     C_DGREY);
    /* Wheels — 4 corners */
    frect(x,         y+h/5,   2, h/5, C_BLACK);
    frect(x+w-2,     y+h/5,   2, h/5, C_BLACK);
    frect(x,         y+h-h/4, 2, h/5, C_BLACK);
    frect(x+w-2,     y+h-h/4, 2, h/5, C_BLACK);
    /* Bat logo */
    pset(cx-1, cy+2, C_YELLOW);
    pset(cx,   cy+2, C_YELLOW);
    pset(cx+1, cy+2, C_YELLOW);
}

/* --- Enemy vehicle --- */
static void draw_enemy(const Enemy *e) {
    if (!e->active || !e->activated) return;
    int cx = e->x, cy = e->y;
    unsigned short col;
    switch (e->type) {
        case E_SWAY:       col = C_CYAN;    break;
        case E_DIVE:       col = C_ORANGE;  break;
        case E_ASSAULT:    col = C_MAGENTA; break;
        case E_PROJECTILE: col = C_RED;     break;
        default:           col = C_RED;     break;
    }
    /* shadow */
    draw_shadow(cx, cy, EN_W, EN_H);
    /* body — depth scaled */
    frect_3d(cx, cy, EN_W, EN_H, col);
    drect_3d(cx, cy, EN_W, EN_H, C_DGREY);
    /* headlights — tiny 1px dots */
    int sc = depth_scale(cy);
    int sw = imax((EN_W * sc)/256, 2);
    int sh = imax((EN_H * sc)/256, 2);
    int bx = cx - sw/2, by = cy - sh/2;
    frect(bx+1,       by+sh-3, 2, 2, C_YELLOW);
    frect(bx+sw-3,    by+sh-3, 2, 2, C_YELLOW);
}

/* --- Stud --- */
static void draw_stud(const Stud *s) {
    if (!s->active || s->collected) return;
    int r = imax((STD_R * depth_scale(s->y)) / 256, 1);
    fdiamond(s->x, s->y, r, C_STUD);
    pset(s->x-1, s->y-r+1, C_WHITE);
}

/* --- Obstacle --- */
static void draw_obstacle(const Obstacle *o) {
    if (!o->active) return;
    int cx = o->x + o->w/2, cy = o->y + o->h/2;
    draw_shadow(cx, cy, o->w, o->h);
    frect_3d(cx, cy, o->w, o->h, C_MGREY);
    drect_3d(cx, cy, o->w, o->h, C_WHITE);
    /* X mark at actual pixel position */
    int sc = depth_scale(cy);
    int sw = imax((o->w * sc)/256, 2);
    int sh = imax((o->h * sc)/256, 2);
    int bx = cx - sw/2, by = cy - sh/2;
    for (int i = 0; i < imin(sw, sh); i++) {
        pset(bx+i,       by+i,       C_RED);
        pset(bx+sw-1-i,  by+i,       C_RED);
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
    draw_shadow(p->x, p->y, PWR_W, PWR_H);
    frect_3d(p->x, p->y, PWR_W, PWR_H, c);
    drect_3d(p->x, p->y, PWR_W, PWR_H, C_WHITE);
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
    draw_shadow(boss.x, boss.y, BOSS_W, BOSS_H);
    frect(bx, by, BOSS_W, BOSS_H, col);
    drect(bx, by, BOSS_W, BOSS_H, C_WHITE);
    frect(bx+8,         by+8, 6, 6, C_BLACK);
    frect(bx+BOSS_W-14, by+8, 6, 6, C_BLACK);
    pset(bx+10,         by+10, C_RED);
    pset(bx+BOSS_W-12,  by+10, C_RED);
    int bw = (BOSS_W * boss.hp) / boss.max_hp;
    frect(bx, by-7, BOSS_W, 4, C_DGREY);
    frect(bx, by-7, bw,     4, C_RED);
    drect(bx, by-7, BOSS_W, 4, C_WHITE);
}

/* --- Homing Rocket --- */
static void draw_rocket(void) {
    if (!rocket.active) return;
    draw_shadow(rocket.x, rocket.y, MISS_W, MISS_H);
    frect_3d(rocket.x, rocket.y, MISS_W, MISS_H, C_GREEN);
    if (ctx.frame & 2)
        frect(rocket.x-1, rocket.y + MISS_H/2, 2, 3, C_ORANGE);
}

/* ================================================================
 *  Background / Road — Perspective + Parallax  [V2-3D-A/B]
 * ================================================================ */

/*
 * Parallax building silhouettes.
 * layer 0 (far)  : scrolls at scroll/4
 * layer 1 (mid)  : scrolls at scroll/2
 * layer 2 (near) : scrolls at scroll*3/4
 * Each layer uses a simple repeating pattern seeded by a fixed pseudo-random
 * sequence so buildings look varied without needing dynamic memory.
 */
static void draw_parallax(void) {
    /* Sky gradient — deep Gotham night */
    for (int y = 0; y < HORIZON_Y + 10; y++) {
        int b = 3 + (y >> 2);
        int g = y >> 4;
        unsigned short sky = (unsigned short)((g << 5) | b);
        hline(0, y, SCR_W, sky);
    }

    /* --- Layer 0: far buildings (dark, barely visible) --- */
    {
        int spd0 = ctx.scroll / 4;
        /* tile width = 320, repeat every 320 px */
        int off0 = spd0 % 320;
        unsigned int seed0 = 0xDEAD;
        for (int bld = 0; bld < 12; bld++) {
            seed0 = seed0 * 1664525u + 1013904223u;
            int bw = 12 + (int)(seed0 >>  28) * 2;        /* 12..42 px wide */
            int bh = 8  + (int)((seed0 >> 24) & 0xF);     /* 8..23 px tall  */
            int bx = (int)((seed0 >> 16) & 0xFF) % 320;
            bx = (bx - off0 + 320) % 320;
            int by = HORIZON_Y + 10 - bh;
            frect(bx, by, bw, bh, 0x18A3u);   /* very dark blue-grey */
            /* tiny neon window dots */
            if ((seed0 >> 8) & 1)
                pset(bx + bw/2, by + 2, (seed0 & 1) ? C_CYAN : C_MAGENTA);
        }
    }

    /* --- Layer 1: mid buildings (medium detail) --- */
    {
        int spd1 = ctx.scroll / 2;
        int off1 = spd1 % 320;
        unsigned int seed1 = 0xBEEF;
        for (int bld = 0; bld < 10; bld++) {
            seed1 = seed1 * 1664525u + 1013904223u;
            int bw = 14 + (int)(seed1 >> 28) * 3;
            int bh = 10 + (int)((seed1 >> 24) & 0x1F);
            int bx = (int)((seed1 >> 16) & 0xFF) % 300 + 10;
            bx = (bx - off1 + 320) % 320;
            int by = HORIZON_Y + 10 - bh;
            frect(bx, by, bw, bh, 0x3186u);   /* dark grey-blue */
            drect(bx, by, bw, bh, 0x4208u);
            /* neon windows — row of 3 */
            for (int wi = 0; wi < 3; wi++) {
                unsigned short wc = (wi & 1) ? C_YELLOW : C_CYAN;
                if ((seed1 >> (wi+4)) & 1)
                    frect(bx + 2 + wi*4, by + 2, 2, 2, wc);
            }
        }
    }

    /* --- Layer 2: near buildings (most detail, right by road edges) --- */
    {
        int spd2 = (ctx.scroll * 3) / 4;
        int off2 = spd2 % 160;
        unsigned int seed2 = 0xCAFE;
        /* Left side near buildings */
        for (int bld = 0; bld < 5; bld++) {
            seed2 = seed2 * 1664525u + 1013904223u;
            int bw = 16 + (int)(seed2 >> 28) * 3;
            int bh = 14 + (int)((seed2 >> 24) & 0x1F);
            int bx = (int)((seed2 >> 16) & 0x3F) % (ROAD_L - 4);
            bx = (bx - off2 + ROAD_L) % ROAD_L;
            int by = HORIZON_Y + 10 - bh;
            frect(bx, by, bw, bh, 0x4A49u);
            drect(bx, by, bw, bh, C_MGREY);
            /* Neon sign strip */
            frect(bx+2, by+2, bw-4, 2,
                  ((seed2 >> 3) & 1) ? C_MAGENTA : C_CYAN);
        }
        /* Right side near buildings */
        seed2 ^= 0x5A5A;
        for (int bld = 0; bld < 5; bld++) {
            seed2 = seed2 * 1664525u + 1013904223u;
            int bw = 16 + (int)(seed2 >> 28) * 3;
            int bh = 14 + (int)((seed2 >> 24) & 0x1F);
            int bx = ROAD_R + (int)((seed2 >> 16) & 0x3F) % (SCR_W - ROAD_R - 4);
            bx = ROAD_R + (bx - ROAD_R - off2 + (SCR_W - ROAD_R)) % (SCR_W - ROAD_R);
            int by = HORIZON_Y + 10 - bh;
            frect(bx, by, bw, bh, 0x4A49u);
            drect(bx, by, bw, bh, C_MGREY);
            frect(bx+2, by+2, bw-4, 2,
                  ((seed2 >> 3) & 1) ? C_ORANGE : C_YELLOW);
        }
    }
}

static void draw_background(void) {
    /* 1. Sky + Parallax buildings */
    draw_parallax();

    /* 2. Sidewalks — fill solid outside road bounds */
    frect(0,      HORIZON_Y+10, ROAD_L,          SCR_H, C_SIDEWALK);
    frect(ROAD_R, HORIZON_Y+10, SCR_W - ROAD_R,  SCR_H, C_SIDEWALK);

    /*
     * 3. Perspective road — drawn scan-line by scan-line  [V2-3D-A]
     *    Colour darkens toward horizon (depth cueing).
     */
    for (int y = HORIZON_Y; y < SCR_H; y++) {
        int rl = road_left(y);
        int rr = road_right(y);
        /* Depth-cued colour: darker near horizon (y small), normal near bottom */
        int t  = ((y - HORIZON_Y) * 256) / (SCR_H - HORIZON_Y); /* 0..256 */
        /* Interpolate C_ROAD_FAR → C_ROAD */
        /* C_ROAD_FAR=0x1082, C_ROAD=0x2104 */
        unsigned int rf = 0x1082u, rn = 0x2104u;
        unsigned int r_col  = rf + (unsigned int)(((int)(rn & 0xF800u)-(int)(rf & 0xF800u)) * t / 256) & 0xF800u
                            | rf + (unsigned int)(((int)(rn & 0x07E0u)-(int)(rf & 0x07E0u)) * t / 256) & 0x07E0u
                            | rf + (unsigned int)(((int)(rn & 0x001Fu)-(int)(rf & 0x001Fu)) * t / 256) & 0x001Fu;
        /* simpler: just lerp between two grey values */
        int grey = 8 + (t * 8) / 256;   /* 8..16 */
        unsigned short road_col = (unsigned short)((grey << 11) | (grey << 6) | grey);
        hline(rl, y, rr - rl, road_col);

        /* White road edge lines */
        if (rl > 0) pset(rl-1, y, C_WHITE);
        if (rr < SCR_W) pset(rr, y, C_WHITE);
    }

    /* 4. Dashed centre line — perspective-correct, scrolls */
    {
        int dash_len = 18, gap_len = 12, period = dash_len + gap_len;
        int off = ctx.scroll % period;
        for (int y = HORIZON_Y + (-period + off % period); y < SCR_H; y += period) {
            for (int dy = 0; dy < dash_len && y + dy < SCR_H; dy++) {
                int yy = y + dy;
                if (yy < HORIZON_Y) continue;
                pset(ROAD_CX-1, yy, C_WHITE);
                pset(ROAD_CX,   yy, C_WHITE);
            }
        }
    }

    /* 5. Kerb rumble strips — perspective, foreshortened near horizon */
    {
        int off = ctx.scroll % 10;
        for (int y = HORIZON_Y - (off % 10); y < SCR_H; y += 10) {
            if (y < HORIZON_Y) continue;
            unsigned short kc = ((y / 10) & 1) ? C_RED : C_WHITE;
            int rl = road_left(y);
            int rr = road_right(y);
            int strip_h = imax(1, 5 * depth_scale(y) / 256);
            /* clamp strip to road edge */
            frect(rl,    y, imax(1, 4 * depth_scale(y)/256), strip_h, kc);
            frect(rr-imax(1,4*depth_scale(y)/256), y,
                  imax(1, 4 * depth_scale(y)/256), strip_h, kc);
        }
    }
}

/* ================================================================
 *  HUD
 * ================================================================ */
static void draw_hud(void) {
    frect(0, 0, ROAD_L-1, SCR_H, 0x2104u);
    for (int i = 0; i < pl.hp; i++)
        fdiamond(12, 16 + i*14, 5, C_HEALTH);
    for (int i = pl.hp; i < PL_MAX_HP; i++)
        drect(7, 11 + i*14, 10, 10, C_DGREY);
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
    frect(ROAD_R+1, 0, SCR_W-ROAD_R-1, SCR_H, 0x2104u);
    draw_num(ROAD_R+4, 8,  pl.score, C_YELLOW);
    fdiamond(ROAD_R+8, 28, 4, C_STUD);
    draw_num(ROAD_R+4, 36, pl.studs, C_YELLOW);
    if (boss.active) {
        int bw = ROAD_W * boss.hp / boss.max_hp;
        frect(ROAD_L, 0, ROAD_W, 4, C_DGREY);
        frect(ROAD_L, 0, bw,     4, C_RED);
    }
}

/* ================================================================
 *  Screen Overlay — True Dim  [V2-F3]
 * ================================================================ */
/*
 * Read every pixel already in the back buffer, halve each RGB channel,
 * and write back.  This gives a genuine 50% translucent darkening with
 * no lines, no tearing, and no extra geometry.
 */
static void dim_screen_half(void) {
    for (int i = 0; i < PIX_STRIDE * SCR_H; i++) {
        unsigned short p = draw_buf[i];
        draw_buf[i] = (unsigned short)(
            ((p >> 1) & 0x7800u) |   /* R: keep top 4 bits of 5 */
            ((p >> 1) & 0x03E0u) |   /* G: keep top 5 bits of 6 */
            ((p >> 1) & 0x000Fu)      /* B: keep top 4 bits of 5 */
        );
    }
}

/* ================================================================
 *  Screens
 * ================================================================ */
static void draw_menu(void) {
    /* Sky gradient */
    for (int y = 0; y < SCR_H; y++) {
        unsigned short sky;
        if (y < 80) {
            int b = 3 + (y >> 3);
            int g = y >> 4;
            sky = (unsigned short)((g << 5) | b);
        } else if (y < 160) {
            int b = 13 + ((y - 80) >> 4);
            int g = 5  + ((y - 80) >> 4);
            sky = (unsigned short)((g << 5) | b);
        } else {
            int r = (y - 160) >> 3;
            int g = 3 + ((y - 160) >> 4);
            int b = 8;
            sky = (unsigned short)((r << 11) | (g << 5) | b);
        }
        hline(0, y, SCR_W, sky);
    }

    /* Batman silhouette */
    frect(130, 28,  60, 52, C_DGREY);
    frect(140, 18,  10, 16, C_DGREY);
    frect(170, 18,  10, 16, C_DGREY);
    frect(148, 40,  24, 20, 0x2945u);
    frect(148, 58,  24,  4, C_YELLOW);
    fdiamond(160, 50, 6, C_YELLOW);
    pset(154, 50, C_DGREY);
    pset(166, 50, C_DGREY);

    /* Neon city lights */
    for (int i = 0; i < 8; i++) {
        int lx = 10 + i * 38;
        frect(lx,    72, 3, 10, C_CYAN);
        frect(lx+10, 68, 3, 14, C_MAGENTA);
        frect(lx+20, 74, 3,  8, C_CYAN);
    }

    /* Title banner */
    frect(55,  92, 210, 26, C_YELLOW);
    frect(57,  94, 206, 22, C_BLACK);
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

    /* Subtitle */
    frect(90, 122, 140, 8, C_CYAN);
    frect(92, 123, 136, 6, C_BLACK);
    for (int i = 0; i < 10; i++)
        frect(94 + i * 13, 125, 9, 2, C_CYAN);

    /* Controls */
    frect(60, 140, 200, 1, C_MGREY);
    frect(92,  148, 18, 8, C_DGREY); drect(92,  148, 18, 8, C_LGREY);
    frect(114, 148, 18, 8, C_DGREY); drect(114, 148, 18, 8, C_LGREY);
    frect(136, 148, 18, 8, C_DGREY); drect(136, 148, 18, 8, C_LGREY);
    frect(170, 148, 34, 8, C_DGREY); drect(170, 148, 34, 8, C_LGREY);

    /* Blink */
    unsigned short blink_col = (ctx.frame & 32) ? C_YELLOW : C_ORANGE;
    frect(80, 165, 160, 14, blink_col);
    frect(82, 167, 156, 10, C_BLACK);
    for (int i = 0; i < 6; i++)
        frect(90 + i * 22, 170, 16, 4, blink_col);

    frect(60, 182, 200, 1, C_MGREY);
    fdiamond(130, 200, 4, C_STUD);
    draw_num(140, 196, pl.score, C_YELLOW);
}

static void draw_pause(void) {
    /* Smooth 50% dim over already-rendered game frame — no stripe flicker */
    dim_screen_half();
    frect(80,  90, 160, 60, C_DGREY);
    drect(80,  90, 160, 60, C_YELLOW);
    frect(100, 108, 60,  7, C_YELLOW);
    frect(100, 122, 120, 7, C_MGREY);
}

static void draw_gameover(void) {
    dim_screen_half();
    frect( 50, 70, 220, 100, C_BLACK);
    drect( 50, 70, 220, 100, C_RED);
    frect( 70, 85, 180,   7, C_RED);
    draw_num(130, 103, pl.score, C_YELLOW);
    draw_num(110, 120, pl.studs, C_STUD);
    if (ctx.frame & 32)
        frect(90, 145, 140, 6, C_WHITE);
}

/* ================================================================
 *  PS/2 Input
 * ================================================================ */
static volatile unsigned *ps2 = (volatile unsigned *)PS2_BASE;

static void read_ps2(void) {
    static bool ext = false, rel = false;
    while (1) {
        unsigned d = *ps2;
        if (!(d & 0x8000u)) break;
        unsigned byte = d & 0xFF;
        if (byte == 0xE0) { ext = true;  continue; }
        if (byte == 0xF0) { rel = true;  continue; }
        bool pressed = !rel;
        rel = false;
        if (ext) {
            switch (byte) {
                case 0x6B: k_left  = pressed; break;
                case 0x74: k_right = pressed; break;
                case 0x75: k_up    = pressed; break;
            }
            ext = false;
        } else {
            ext = false;
            switch (byte) {
                case 0x5A: k_enter = pressed; break;
                case 0x76: k_esc   = pressed; break;
            }
        }
    }
}

/* ================================================================
 *  Interval Timer  (50 MHz → ~60 fps)
 * ================================================================ */
static volatile unsigned *tmr = (volatile unsigned *)TIMER_BASE;

static void timer_init(void) {
    tmr[2] = 0xB4B3u;
    tmr[3] = 0x000Cu;
    tmr[1] = 0x0006u;   /* START | CONT — no IRQ bit [F6] */
}

static void timer_wait(void) {
    while (!(tmr[0] & 1u));
    tmr[0] = 0;
}

/* ================================================================
 *  VGA — Double-Buffered  [V2-F1, V2-F2]
 * ================================================================ */
static volatile unsigned *pbc = (volatile unsigned *)PIX_CTRL_BASE;

static void vga_init(void) {
    front_buf = (volatile short *)PIX_BUF1;   /* 0xC8000000 — displayed first */
    draw_buf  = (volatile short *)PIX_BUF2;   /* 0xC0000000 — drawn first      */

    /* Tell controller which buffer to display */
    pbc[1] = PIX_BUF1;

    /* Clear BOTH buffers to black */
    for (int i = 0; i < PIX_STRIDE * SCR_H; i++) {
        front_buf[i] = C_BLACK;
        draw_buf[i]  = C_BLACK;
    }
}

/*
 * Page-flip double buffer swap  [V2-F2]
 * Write back buffer address to pixel controller register, wait for vsync,
 * then swap which pointer is back/front for next frame.
 */
static void swap_buffers(void) {
    /* Request display of current back buffer */
    pbc[1] = (unsigned int)(unsigned long)draw_buf;

    /* Wait for vsync — pixel buffer controller clears bit 0 when flip completes */
    while (pbc[3] & 1u);

    /* Swap front/back pointers */
    volatile short *tmp = draw_buf;
    draw_buf  = front_buf;
    front_buf = tmp;
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
                case E_SWAY:       e->vy = 1; e->timer = irand(0, 255); break;
                case E_DIVE:       e->vy = 1; e->timer = irand(60, 140); break;
                case E_ASSAULT:    e->vy = 2; break;
                case E_PROJECTILE: e->vy = 3; e->vx = irand(-2, 2); break;
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
    pl.vx = 0; pl.hp = PL_MAX_HP; pl.invuln = 0;
    pl.score = 0; pl.studs = 0; pl.alive = true;
    pl.boosting = false; pl.boost_t = 0; pl.magnet_t = 0; pl.missiles = 0;
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
static bool aabb(int ax, int ay, int aw, int ah,
                 int bx, int by, int bw, int bh) {
    return !(ax+aw <= bx || bx+bw <= ax || ay+ah <= by || by+bh <= ay);
}

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
        if (pl.boost_t == 0) { pl.boosting = false; ctx.scroll_spd = SCROLL_BASE; }
    }
    if (pl.magnet_t > 0) pl.magnet_t--;
    if (ctx.frame % 20 == 0) pl.score++;
}

static void fire_rocket(void) {
    if (pl.missiles > 0 && !rocket.active) {
        rocket.active = true;
        rocket.x = pl.x; rocket.y = pl.y - PL_H/2;
        rocket.vx = 0; rocket.vy = -4; rocket.life = 150;
        pl.missiles--;
    }
}

static void update_rocket(void) {
    if (!rocket.active) return;
    if (--rocket.life <= 0) { rocket.active = false; return; }
    int tx = -1, ty = -1;
    if (boss.active) { tx = boss.x; ty = boss.y; }
    else {
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
    if (boss.active &&
        iabs(rocket.x - boss.x) < BOSS_W/2 &&
        iabs(rocket.y - boss.y) < BOSS_H/2) {
        rocket.active = false; boss.hp--; pl.score += 10;
        if (boss.hp <= 0) {
            spawn_studs(boss.x, boss.y, 12);
            boss.active = false; ctx.boss_active = false;
            ctx.boss_t = BOSS_PERIOD; ctx.bosses++; pl.score += 100;
        }
        return;
    }
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!enemies[i].active) continue;
        if (iabs(rocket.x - enemies[i].x) < EN_W &&
            iabs(rocket.y - enemies[i].y) < EN_H) {
            enemies[i].active = false; rocket.active = false;
            pl.score += 5; spawn_studs(enemies[i].x, enemies[i].y, 3); return;
        }
    }
    if (rocket.y < -10 || rocket.x < ROAD_L || rocket.x > ROAD_R)
        rocket.active = false;
}

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
                            int dx = pl.x - e->x, dy = pl.y - e->y;
                            int d  = imax(iabs(dx)+iabs(dy), 1);
                            e->vx  = dx * 3 / (d / 10 + 1);
                        }
                    } else {
                        e->x += e->vx; e->y += 1;
                    }
                    e->x = iclamp(e->x, ROAD_L + EN_W/2, ROAD_R - EN_W/2);
                    break;
                case E_ASSAULT: e->y += 1; break;
                case E_PROJECTILE:
                    e->x += e->vx;
                    if (e->x < ROAD_L || e->x > ROAD_R) e->vx = -e->vx;
                    break;
            }
        }
        if (e->y > SCR_H + EN_H) e->active = false;
    }
}

static void update_studs(void) {
    for (int i = 0; i < MAX_STUDS; i++) {
        Stud *s = &studs[i];
        if (!s->active) continue;
        s->y += ctx.scroll_spd;
        if (pl.magnet_t > 0) {
            int dx = pl.x - s->x, dy = pl.y - s->y;
            int d  = iabs(dx) + iabs(dy);
            if (d < 80) { s->x += dx/4; s->y += dy/4; }
        }
        if (!s->collected) {
            int range = pl.magnet_t > 0 ? 14 : 9;
            if (iabs(s->x - pl.x) < range && iabs(s->y - pl.y) < range) {
                s->collected = true; pl.studs++; pl.score++;
            }
        }
        if (s->y > SCR_H + 8 || s->collected) s->active = false;
    }
}

static void update_obstacles(void) {
    for (int i = 0; i < MAX_OBSTACLES; i++) {
        Obstacle *o = &obs[i];
        if (!o->active) continue;
        o->y += ctx.scroll_spd;
        if (pl.invuln <= 0 && pl.alive) {
            if (aabb(pl.x-PL_W/2, pl.y-PL_H/2, PL_W, PL_H,
                     o->x, o->y, o->w, o->h)) {
                pl.hp--; pl.invuln = INVULN_FRAMES;
                if (pl.hp <= 0) { pl.alive = false; ctx.state = GAMEOVER; }
            }
        }
        if (o->y > SCR_H + OBS_H) o->active = false;
    }
}

static void update_powerups(void) {
    for (int i = 0; i < MAX_POWERUPS; i++) {
        Powerup *p = &pwrs[i];
        if (!p->active) continue;
        p->y += ctx.scroll_spd;
        p->anim++;
        if (iabs(p->x - pl.x) < PWR_W && iabs(p->y - pl.y) < PWR_H) {
            switch (p->type) {
                case PW_MAGNET:  pl.magnet_t = MAGNET_FRAMES; break;
                case PW_BOOST:
                    pl.boosting = true; pl.boost_t = BOOST_FRAMES;
                    ctx.scroll_spd = SCROLL_BASE * 2; break;
                case PW_MISSILE: if (pl.missiles < 5) pl.missiles++; break;
            }
            p->active = false;
        }
        if (p->y > SCR_H + PWR_H) p->active = false;
    }
}

static void update_boss(void) {
    if (!boss.active) return;
    boss.timer++;
    if (!boss.intro) {
        if (boss.y < 45) boss.y += 2;
        else             boss.intro = true;
        return;
    }
    boss.x = ROAD_CX + (isin(boss.timer & 0xFF) * 40) / 256;
    int period = (boss.hp < boss.max_hp/2) ? 25 : 50;
    if (boss.timer % period == 0)
        spawn_proj(boss.x, boss.y + BOSS_H/2 + 2);
    if (boss.hp <= boss.max_hp/4 && boss.timer % 80 == 0) {
        spawn_proj(boss.x - 20, boss.y + BOSS_H/2);
        spawn_proj(boss.x + 20, boss.y + BOSS_H/2);
    }
}

static void start_boss(void) {
    ctx.boss_active = true; boss.active = true;
    boss.hp = BOSS_BASE_HP + ctx.bosses * 2;
    boss.max_hp = boss.hp;
    boss.x = ROAD_CX; boss.y = -BOSS_H;
    boss.type = (BossType)(ctx.bosses % 3);
    boss.timer = 0; boss.intro = false; boss.atk_timer = 0;
    for (int i = 0; i < MAX_ENEMIES;   i++) enemies[i].active = false;
    for (int i = 0; i < MAX_OBSTACLES; i++) obs[i].active     = false;
}

static void check_player_hits(void) {
    if (pl.invuln > 0 || !pl.alive) return;
    int px = pl.x - PL_W/2, py = pl.y - PL_H/2;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy *e = &enemies[i];
        if (!e->active || !e->activated) continue;
        if (aabb(px, py, PL_W, PL_H, e->x-EN_W/2, e->y-EN_H/2, EN_W, EN_H)) {
            pl.hp--; pl.invuln = INVULN_FRAMES; e->active = false;
            if (pl.hp <= 0) { pl.alive = false; ctx.state = GAMEOVER; }
            return;
        }
    }
    if (boss.active && boss.intro) {
        if (aabb(px, py, PL_W, PL_H,
                 boss.x-BOSS_W/2, boss.y-BOSS_H/2, BOSS_W, BOSS_H)) {
            pl.hp--; pl.invuln = INVULN_FRAMES;
            if (pl.hp <= 0) { pl.alive = false; ctx.state = GAMEOVER; }
        }
    }
}

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
        case MENU:
            if (k_enter) { game_init(); ctx.state = PLAYING; k_enter = false; }
            break;
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
            if (k_up && !k_up_prev) fire_rocket();
            k_up_prev = k_up;
            if (k_esc) { ctx.state = PAUSED; k_esc = false; }
            break;
        case PAUSED:
            if (k_esc || k_enter) { ctx.state = PLAYING; k_esc = k_enter = false; }
            break;
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
        timer_wait();      /* ~60 fps frame gate               */
        update();          /* game state machine               */
        render();          /* draw everything to BACK buffer   */
        swap_buffers();    /* vsync page-flip — zero tearing   */
    }
    return 0;
}
