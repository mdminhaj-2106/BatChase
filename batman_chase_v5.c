/*
 * batman_chase_v5.c — Batman: Gotham Chase
 * Pseudo-3D retro racer for DE1-SoC / CPUlator
 *
 * ARCHITECTURE
 *   Software framebuffer fb[] → single blit per frame → zero flicker
 *   Segment-based road with per-scanline perspective projection
 *   Fixed-point physics (FP=1024) for smooth accel/steer/drag
 *   4-lane system with hard boundary clamping
 *   True sprite stacking: player=8 layers, enemies=5 layers
 *   Z-sorted far-to-near enemy rendering
 *   4-layer parallax Gotham skyline
 *   Rain particles, street lamps, Bat-Signal, manhole details
 *
 * CONTROLS
 *   W / ↑       accelerate
 *   S / ↓       brake / reverse
 *   A / ←       steer left
 *   D / →       steer right
 *   L-SHIFT     boost
 *   ENTER       start / confirm
 *   ESC         pause / resume
 */

#include <stdbool.h>

/* ================================================================
 * Hardware
 * ================================================================ */
#define PIX_CTRL  0xFF203020u
#define PIX_BUF   0xC8000000u
#define PS2_ADDR  0xFF200100u
#define TMR_ADDR  0xFF202000u

/* ================================================================
 * Display
 * ================================================================ */
#define W         320
#define H         240
#define STRIDE    512
#define CX        (W/2)

/* ================================================================
 * Fixed-point
 * ================================================================ */
#define FP        1024
#define FP_SH     10
#define TOFP(x)   ((x)<<FP_SH)
#define FRFP(x)   ((x)>>FP_SH)

/* ================================================================
 * Camera / projection
 * ================================================================ */
#define HOR_Y     62
#define CAM_H     860
#define CAM_D     168
#define PK        (CAM_H*CAM_D)    /* 144480 */

/* ================================================================
 * Road
 * ================================================================ */
#define NSEG      320
#define SEGLEN    220
#define TRACKLEN  (NSEG*SEGLEN)    /* 70400 */
#define NLANES    4
#define EDGE_M    150

/* ================================================================
 * Gameplay
 * ================================================================ */
#define NEMAX     18
#define SPAWN_LO  16
#define SPAWN_HI  44
#define ESPAWN_Z  TOFP(6600)
#define EDESPAWN  TOFP(-320)
#define COLL_ZW   TOFP(240)
#define COLL_XW   220

/* ================================================================
 * Physics
 * ================================================================ */
#define MAXSPD    TOFP(28)
#define MAXREV    TOFP(10)
#define ACC_FP    390
#define BRK_FP    620
#define RVACC     260
#define DRAG      150
#define BSTSPD    TOFP(39)
#define BSTACC    700
#define BSTDUR    110
#define SACC      270
#define SDAMP_N   12
#define SDAMP_D   16
#define SMAX      TOFP(8)

/* ================================================================
 * Render
 * ================================================================ */
#define PLR_SY    206
#define PLR_BW    66
#define PLR_BH    56
#define EN_BW     54
#define EN_BH     46

/* ================================================================
 * Colours (RGB565)
 * ================================================================ */
/* General */
#define K         0x0000u
#define Wh        0xFFFFu
#define Re        0xF800u
#define Or        0xFD20u
#define Ye        0xFFE0u
#define Cy        0x07FFu
#define Bl        0x001Fu
#define Gr        0x07E0u
#define Mg        0xF81Fu
#define DG        0x4208u
#define MG        0x8410u
#define LG        0xCE59u

/* Themed */
#define SKY_T     0x0002u
#define SKY_M     0x0826u
#define HOR_G     0x18A8u
#define RD_F      0x2124u
#define RD_N      0x31A6u
#define GR_F      0x01C0u
#define GR_N      0x02A0u
#define RUM_A     0xF800u
#define RUM_B     0xFFFFu
#define LANE_C    0xEF5Du
#define SW_C      0x4A09u
#define BLD_1     0x0841u
#define BLD_2     0x18C3u
#define BLD_3     0x294Au
#define BLD_4     0x3186u
#define NEON_P    0xF81Fu
#define NEON_T    0x07EFu
#define RAIN_C    0x4C99u
#define SHAD_C    0x0841u

/* Batmobile */
#define BB0       0x0841u   /* charcoal body      */
#define BB1       0x1082u   /* mid body           */
#define BB2       0x18C3u   /* upper body         */
#define BB3       0x2104u   /* roof highlight     */
#define BCK       0x0019u   /* cockpit blue       */
#define BED       0x294Au   /* edge/fin highlight */
#define BAC       0xFFE0u   /* yellow accent      */

/* Enemies */
#define EN0       0xF980u   /* orange car         */
#define EN1       0x07FFu   /* cyan car           */
#define EN2       0xF81Fu   /* magenta car        */
#define EN3       0x7BEFu   /* grey car           */

/* ================================================================
 * Types
 * ================================================================ */
typedef struct { int curve, width, rumble; } Seg;

typedef struct {
    int x_fp, steer_fp, spd_fp;
    int boost_t, hp, score, invuln;
    bool alive;
} Plr;

typedef struct {
    bool active;
    int lane, loff, z_fp, spd_fp, type;
} Enm;

typedef enum { ST_MENU, ST_PLAY, ST_PAUSE, ST_OVER } Gst;

typedef struct {
    Gst st;
    int fr, cam_z, lap, spawn_cd;
    bool flash;
} Gam;

/* ================================================================
 * Globals
 * ================================================================ */
static volatile unsigned int * const ps2 = (volatile unsigned int *)PS2_ADDR;
static volatile unsigned int * const tmr = (volatile unsigned int *)TMR_ADDR;
static volatile unsigned int * const pbc = (volatile unsigned int *)PIX_CTRL;

static unsigned short fb[H * STRIDE];

static Seg  road[NSEG];
static int  cpfx[NSEG + 1];

static Plr  pl;
static Enm  en[NEMAX];
static Gam  gm;

static bool kU, kD, kL, kR, kSh, kEnt, kEsc;

static unsigned int rng = 0x93A1u;

/* ================================================================
 * Math
 * ================================================================ */
static int ia(int v) { return v < 0 ? -v : v; }
static int ic(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
static int imx(int a, int b) { return a > b ? a : b; }
static int imn(int a, int b) { return a < b ? a : b; }

static int ir(int lo, int hi) {
    rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
    if (hi <= lo) return lo;
    return lo + (int)(rng & 0x7FFFu) % (hi - lo + 1);
}

static unsigned short sh(unsigned short c, int s) {
    int r = ((c >> 11) & 0x1F) * s >> 8;
    int g = ((c >>  5) & 0x3F) * s >> 8;
    int b = ( c        & 0x1F) * s >> 8;
    if (r > 31) r = 31; if (g > 63) g = 63; if (b > 31) b = 31;
    return (unsigned short)((r << 11) | (g << 5) | b);
}

/* ================================================================
 * Drawing primitives — write to fb[] only
 * ================================================================ */
static inline void px(int x, int y, unsigned short c) {
    if ((unsigned)x < W && (unsigned)y < (unsigned)H)
        fb[y * STRIDE + x] = c;
}

static void hl(int x, int y, int l, unsigned short c) {
    if ((unsigned)y >= H || l <= 0) return;
    int x0 = x, x1 = x + l - 1;
    if (x1 < 0 || x0 >= W) return;
    if (x0 < 0) x0 = 0; if (x1 >= W) x1 = W - 1;
    for (int i = x0; i <= x1; i++) fb[y * STRIDE + i] = c;
}

static void fr(int x, int y, int w, int h, unsigned short c) {
    if (w <= 0 || h <= 0) return;
    int x0 = x, y0 = y, x1 = x + w, y1 = y + h;
    if (x1 <= 0 || y1 <= 0 || x0 >= W || y0 >= H) return;
    if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0;
    if (x1 > W) x1 = W; if (y1 > H) y1 = H;
    for (int j = y0; j < y1; j++)
        for (int i = x0; i < x1; i++)
            fb[j * STRIDE + i] = c;
}

static void dr(int x, int y, int w, int h, unsigned short c) {
    hl(x, y, w, c); hl(x, y + h - 1, w, c);
    for (int j = y; j < y + h; j++) { px(x, j, c); px(x + w - 1, j, c); }
}

static void vl(int x, int y, int l, unsigned short c) {
    for (int j = 0; j < l; j++) px(x, y + j, c);
}

static void diam(int cx, int cy, int r, unsigned short c) {
    for (int dy = -r; dy <= r; dy++) {
        int hw = r - ia(dy);
        hl(cx - hw, cy + dy, hw * 2 + 1, c);
    }
}

/* ================================================================
 * 5×7 digit font
 * ================================================================ */
static const unsigned char DIG[10][7] = {
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

static void ddig(int x, int y, int d, unsigned short c) {
    if (d < 0 || d > 9) return;
    for (int r = 0; r < 7; r++) {
        unsigned char b = DIG[d][r];
        for (int col = 0; col < 5; col++)
            if (b & (0x10 >> col)) px(x + col, y + r, c);
    }
}

static void dnum(int x, int y, int n, unsigned short c) {
    if (n < 0) n = 0;
    if (n == 0) { ddig(x, y, 0, c); return; }
    char buf[10]; int len = 0;
    while (n > 0) { buf[len++] = (char)(n % 10); n /= 10; }
    for (int i = len - 1; i >= 0; i--) { ddig(x, y, (int)buf[i], c); x += 6; }
}

/* ================================================================
 * Clear
 * ================================================================ */
static void clr(unsigned short c) {
    unsigned int pk = ((unsigned int)c << 16) | c;
    unsigned int *p = (unsigned int *)fb;
    int n = (STRIDE * H) >> 1;
    for (int i = 0; i < n; i++) p[i] = pk;
}

/* ================================================================
 * Hardware
 * ================================================================ */
static void blit(void) {
    volatile unsigned int *d = (volatile unsigned int *)PIX_BUF;
    unsigned int *s = (unsigned int *)fb;
    int n = (STRIDE * H) >> 1;
    for (int i = 0; i < n; i++) d[i] = s[i];
}

static void vinit(void) { pbc[1] = PIX_BUF; clr(0); blit(); }
static void tinit(void) { tmr[2]=0xB4B3u; tmr[3]=0x000Cu; tmr[1]=0x0006u; }
static void twait(void) { while (!(tmr[0] & 1u)); tmr[0] = 0; }

/* ================================================================
 * Road
 * ================================================================ */
static int wrapz(int z) { int m = z % TRACKLEN; return m < 0 ? m + TRACKLEN : m; }
static int segidx(int wz) { return wrapz(wz) / SEGLEN; }

static void sample(int wz, int *cx, int *hw, int *rum) {
    int i = segidx(wz);
    int loc = wrapz(wz) % SEGLEN;
    int t = (loc * FP) / SEGLEN;
    int i1 = (i + 1) % NSEG;
    *cx = cpfx[i] + ((cpfx[i + 1] - cpfx[i]) * t) / FP;
    *hw = road[i].width + ((road[i1].width - road[i].width) * t) / FP;
    *rum = road[i].rumble;
}

static int lanex(int lane, int hw) {
    int lw = (hw * 2) / NLANES;
    return -hw + lw * lane + lw / 2;
}

/* ================================================================
 * Projection
 * ================================================================ */
static int pjy(int z) { return z < 1 ? H : HOR_Y + PK / z; }
static int pjx(int wx, int z) { return z < 1 ? CX : CX + (wx * CAM_D) / z; }
static int dshade(int z) { return ic(255 - z / 32, 80, 255); }

/* ================================================================
 * Input
 * ================================================================ */
static void input(void) {
    static bool ext = false, rel = false;
    while (1) {
        unsigned int d = *ps2;
        if (!(d & 0x8000u)) break;
        unsigned int b = d & 0xFFu;
        if (b == 0xE0u) { ext = true; continue; }
        if (b == 0xF0u) { rel = true; continue; }
        bool p = !rel; rel = false;
        if (ext) {
            if      (b == 0x75u) kU = p;
            else if (b == 0x72u) kD = p;
            else if (b == 0x6Bu) kL = p;
            else if (b == 0x74u) kR = p;
            ext = false; continue;
        }
        if      (b == 0x1Du) kU  = p;
        else if (b == 0x1Bu) kD  = p;
        else if (b == 0x1Cu) kL  = p;
        else if (b == 0x23u) kR  = p;
        else if (b == 0x12u || b == 0x59u) kSh = p;
        else if (b == 0x5Au) kEnt = p;
        else if (b == 0x76u) kEsc = p;
    }
}

/* ================================================================
 * Build track
 * ================================================================ */
static void buildTrack(void) {
    int i;
    for (i = 0; i < NSEG; i++) {
        road[i].curve = 0;
        road[i].width = 1500;
        road[i].rumble = i & 1;
    }
    /* Curves */
    for (i =  20; i <  70; i++) road[i].curve =  18;
    for (i =  70; i < 110; i++) road[i].curve =  32;
    for (i = 110; i < 150; i++) road[i].curve = -26;
    for (i = 150; i < 200; i++) road[i].curve = -36;
    for (i = 200; i < 240; i++) road[i].curve =  14;
    for (i = 240; i < 285; i++) road[i].curve = -10;
    /* Width variation */
    for (i = 0; i < NSEG; i++)
        road[i].width = ic(1450 + ((i * 53) % 260) - 130, 1280, 1640);
    /* Prefix sum for curve → lateral offset */
    cpfx[0] = 0;
    for (i = 0; i < NSEG; i++)
        cpfx[i + 1] = cpfx[i] + road[i].curve;
    /* Close the loop */
    int drift = cpfx[NSEG];
    if (drift)
        for (i = 0; i <= NSEG; i++)
            cpfx[i] -= (drift * i) / NSEG;
}

/* ================================================================
 * SKY + BAT SIGNAL
 * ================================================================ */
static void drawSky(void) {
    for (int y = 0; y <= HOR_Y + 2; y++) {
        int t = (y * 255) / (HOR_Y + 2);
        unsigned short c;
        if (y <= HOR_Y / 2)
            c = sh(SKY_T, 255 - t / 2);
        else
            c = sh(SKY_M, 170 + t / 3);
        hl(0, y, W, c);
    }
    hl(0, HOR_Y, W, HOR_G);
    hl(0, HOR_Y + 1, W, sh(HOR_G, 180));

    /* Bat signal */
    int bx = 264, by = 26;
    for (int y = 6; y < HOR_Y; y++) {
        int hw = 2 + (y - 6) * 3 / (HOR_Y - 6);
        hl(bx - hw, y, hw * 2, 0x0103u);
    }
    diam(bx, by, 12, 0x3180u);
    diam(bx, by, 10, 0x8C00u);
    diam(bx, by, 8, Ye);
    /* bat silhouette cutout */
    fr(bx - 5, by - 3, 10, 8, 0x0001u);
    fr(bx - 9, by - 1, 5, 4, 0x0001u);
    fr(bx + 4, by - 1, 5, 4, 0x0001u);
    fr(bx - 3, by - 6, 2, 4, 0x0001u);
    fr(bx + 1, by - 6, 2, 4, 0x0001u);
}

/* ================================================================
 * 4-LAYER PARALLAX GOTHAM SKYLINE
 * ================================================================ */
static void drawSkyline(void) {
    int scr = FRFP(gm.cam_z);

    /* Layer 0: distant — very dark, tiny, 1/6 scroll */
    { int off = (scr / 6) % 320; unsigned int s = 0xCAFE0000u;
      for (int b = 0; b < 20; b++) {
          s = s * 1664525u + 1013904223u;
          int bw = 8 + (int)((s >> 28) & 0xF) * 2;
          int bh = 5 + (int)((s >> 24) & 0xF);
          int bx = ((int)((s >> 8) & 0xFF) % 320 - off + 640) % 320;
          int by = HOR_Y + 3 - bh;
          fr(bx, by, bw, bh, BLD_1);
          if ((s >> 3) & 1) px(bx + bw / 2, by + 2, ((s >> 2) & 1) ? 0x0040u : 0x0003u);
      }
    }

    /* Layer 1: mid — neon windows, 1/3 scroll */
    { int off = (scr / 3) % 320; unsigned int s = 0xDEADBEEFu;
      for (int b = 0; b < 14; b++) {
          s = s * 1664525u + 1013904223u;
          int bw = 14 + (int)((s >> 28) & 0xF) * 3;
          int bh = 12 + (int)((s >> 22) & 0x1F);
          int bx = ((int)((s >> 8) & 0x1FF) % 320 - off + 960) % 320;
          int by = HOR_Y + 4 - bh; if (by < 2) by = 2;
          fr(bx, by, bw, bh, BLD_3);
          dr(bx, by, bw, bh, 0x31A6u);
          /* art-deco stepped parapet */
          fr(bx + 2, by - 2, bw - 4, 3, BLD_3);
          fr(bx + 4, by - 4, imx(bw - 8, 2), 3, BLD_3);
          /* neon sign */
          unsigned short nc = ((s >> 1) & 1) ? NEON_P : NEON_T;
          fr(bx + 2, by + bh - 4, bw - 4, 2, nc);
          /* windows */
          for (int wi = 0; wi < (bw - 4) / 5 && wi < 4; wi++)
              for (int wj = 0; wj < 3; wj++)
                  if ((s >> (wi + wj * 3)) & 1)
                      fr(bx + 3 + wi * 5, by + 4 + wj * 5, 3, 3, Ye);
      }
    }

    /* Layer 2: near-right — large buildings, 3/4 scroll */
    { int off = (scr * 3 / 4) % 200; unsigned int s = 0xABCD1234u;
      for (int b = 0; b < 5; b++) {
          s = s * 1664525u + 1013904223u;
          int bw = 22 + (int)((s >> 28) & 0xF) * 4;
          int bh = 28 + (int)((s >> 22) & 0x1F) * 2;
          int bx = 224 + (int)((s >> 8) & 0x3F) % (W - 224 - bw + 1);
          if (bx + bw > W) bx = W - bw;
          int by = HOR_Y + 4 - bh; if (by < 2) by = 2;
          int voff = ((b * 47 + off * 2) % (bh + 40)) - 20;
          by += voff; if (by < 2) by = 2;
          fr(bx, by, bw, bh, BLD_4); dr(bx, by, bw, bh, MG);
          unsigned short nc = ((b ^ (gm.fr / 60)) & 1) ? NEON_P : Or;
          fr(bx + 3, by + 3, bw - 6, 5, K);
          for (int si = 0; si < (bw - 8) / 4; si++)
              fr(bx + 4 + si * 4, by + 4, 2, 3, nc);
          /* water tower */
          if ((s >> 5) & 1) { fr(bx + bw / 2 - 3, by - 8, 6, 8, MG); diam(bx + bw / 2, by - 9, 3, DG); }
      }
    }

    /* Layer 3: near-left — large buildings, 3/4 scroll */
    { int off = (scr * 3 / 4 + 100) % 200; unsigned int s = 0x5A5ABEADu;
      for (int b = 0; b < 5; b++) {
          s = s * 1664525u + 1013904223u;
          int bw = 22 + (int)((s >> 28) & 0xF) * 4;
          int bh = 28 + (int)((s >> 22) & 0x1F) * 2;
          int bx = (int)((s >> 8) & 0x3F) % imx(96 - bw, 1);
          if (bx < 0) bx = 0; if (bx + bw > 96) bx = 96 - bw;
          int by = HOR_Y + 4 - bh; if (by < 2) by = 2;
          int voff = ((b * 53 + off * 2) % (bh + 40)) - 20;
          by += voff; if (by < 2) by = 2;
          fr(bx, by, bw, bh, BLD_4); dr(bx, by, bw, bh, MG);
          unsigned short nc = ((b ^ (gm.fr / 60 + 1)) & 1) ? Or : Cy;
          fr(bx + 3, by + 3, bw - 6, 5, K);
          for (int si = 0; si < (bw - 8) / 4; si++)
              fr(bx + 4 + si * 4, by + 4, 2, 3, nc);
          if ((s >> 5) & 1) { fr(bx + bw / 2 - 3, by - 8, 6, 8, MG); diam(bx + bw / 2, by - 9, 3, DG); }
      }
    }
}

/* ================================================================
 * PERSPECTIVE ROAD (per-scanline)
 * ================================================================ */
static void drawRoad(void) {
    for (int y = HOR_Y + 2; y < H; y++) {
        int dy = y - HOR_Y;
        if (dy <= 0) continue;
        int zv = PK / dy;
        int wz = FRFP(gm.cam_z) + zv;

        int rcx, rhw, rum;
        sample(wz, &rcx, &rhw, &rum);

        int center = pjx(rcx - FRFP(pl.x_fp), zv);
        int rhpx = imx((rhw * CAM_D) / zv, 2);

        int gs = ic(70 + dy, 70, 220);
        int rs = ic(90 + dy, 90, 230);

        unsigned short grass = sh(dy < 95 ? GR_F : GR_N, gs);
        unsigned short road_c = sh(dy < 120 ? RD_F : RD_N, rs);

        int stripe = ((wz / 160) + rum) & 1;
        if (stripe) road_c = sh(road_c, 230);  /* alternate shade */

        unsigned short rum_c = stripe ? RUM_A : RUM_B;
        int kw = imx(rhpx / 10, 1);
        int lft = center - rhpx;
        int rgt = center + rhpx;

        /* Grass + sidewalk */
        hl(0, y, W, grass);
        /* Sidewalk strips at road edges */
        if (dy > 20) {
            int sw = imx(rhpx / 16, 1);
            hl(lft - sw, y, sw, SW_C);
            hl(rgt, y, sw, SW_C);
        }

        /* Road surface */
        hl(lft, y, rgt - lft, road_c);
        /* Kerb rumble */
        hl(lft, y, kw, rum_c);
        hl(rgt - kw, y, kw, rum_c);

        /* Lane dividers (dashed) */
        for (int lane = 1; lane < NLANES; lane++) {
            int lwx = lanex(lane, rhw) - lanex(0, rhw);
            int lx = pjx(rcx + lwx - FRFP(pl.x_fp), zv);
            int dash = ((wz / 220) + lane) & 1;
            if (dash || dy > 120) {
                px(lx, y, LANE_C);
                if (dy > 80) px(lx + 1, y, sh(LANE_C, 180));
            }
        }
        /* Centre line (solid yellow) */
        px(center, y, Ye);
        if (rhpx > 10) px(center + 1, y, sh(Ye, 180));
    }
}

/* ================================================================
 * STREET LAMPS
 * ================================================================ */
static void drawLamps(void) {
    int camz = FRFP(gm.cam_z);
    int spacing = 90;
    int off = camz % spacing;
    for (int yi = -spacing; yi < H + spacing; yi += spacing) {
        int y = yi + off;
        if (y <= HOR_Y || y >= H) continue;
        int dy = y - HOR_Y;
        int zv = PK / dy;
        int wz = camz + zv;
        int rcx, rhw, rum;
        sample(wz, &rcx, &rhw, &rum);
        int center = pjx(rcx - FRFP(pl.x_fp), zv);
        int rhpx = imx((rhw * CAM_D) / zv, 2);
        int ph = imx(dy * 18 / 512 + 2, 3);
        int arm = imx(dy * 10 / 512 + 1, 2);
        /* Left */
        { int lx = center - rhpx - imx(dy * 8 / 512, 2);
          int ty = y - ph; if (ty < 0) ty = 0;
          vl(lx, ty, ph, LG);
          hl(lx, ty, arm, LG);
          diam(lx + arm, ty, imx(dy * 2 / 512, 1), Ye);
          if (dy > 60) fr(lx + arm - 3, ty + 2, 6, 3, 0x0840u); }
        /* Right */
        { int rx = center + rhpx + imx(dy * 8 / 512, 2);
          int ty = y - ph; if (ty < 0) ty = 0;
          vl(rx, ty, ph, LG);
          hl(rx - arm, ty, arm, LG);
          diam(rx - arm, ty, imx(dy * 2 / 512, 1), Ye);
          if (dy > 60) fr(rx - arm - 3, ty + 2, 6, 3, 0x0840u); }
    }
}

/* ================================================================
 * RAIN
 * ================================================================ */
static void drawRain(void) {
    for (int i = 0; i < 80; i++) {
        int rx = (i * 73 + gm.fr * 2) % W;
        int ry = (i * 53 + gm.fr * 5) % H;
        unsigned short rc = (i & 3) ? RAIN_C : 0x7BDFu;
        px(rx, ry, rc);
        if (rx > 0 && ry > 0) px(rx - 1, ry - 1, rc);
        if (rx > 1 && ry < H - 1) px(rx - 2, ry + 1, 0x2946u);
    }
}

/* ================================================================
 * BATMOBILE — 8-layer BTAS sprite stack
 *
 * BTAS shape: blunt slatted nose → cockpit bubble → wide body →
 *             horizontal tailfins flanking central turbine → jet exhaust
 * ================================================================ */
static void drawPlayer(void) {
    if (!pl.alive) return;
    if (pl.invuln > 0 && (gm.fr & 3)) return;

    int zp = PK / (PLR_SY - HOR_Y);
    int rcx, rhw, rum;
    sample(FRFP(gm.cam_z) + zp, &rcx, &rhw, &rum);
    int cx = pjx(rcx - FRFP(pl.x_fp), zp);

    /* Slight visual lean when steering */
    cx += ic(pl.steer_fp / (FP / 4), -10, 10);

    int cy = PLR_SY;

    /* === SHADOW === */
    fr(cx - 20, cy + 3, 40, 5, SHAD_C);

    /* === BOOST / EXHAUST === */
    if (pl.boost_t > 0) {
        int fh = 8 + ((gm.fr >> 1) & 3);
        fr(cx - 5, cy + 2, 10, fh, Or);
        fr(cx - 3, cy + 3, 6, fh - 2, Ye);
    } else if (gm.fr & 8) {
        fr(cx - 3, cy + 2, 6, 5, Or);
    }

    /* === LAYER 0: Tail fins (widest, rearmost) === */
    fr(cx - 24, cy - 4, 14, 10, BB0);       /* left fin  */
    hl(cx - 24, cy - 4, 14, BED);           /* fin edge  */
    fr(cx + 10, cy - 4, 14, 10, BB0);       /* right fin */
    hl(cx + 10, cy - 4, 14, BED);
    /* Centre turbine housing */
    fr(cx - 9, cy - 4, 18, 10, BB1);
    diam(cx, cy + 1, 3, DG);

    /* === LAYER 1: Lower hull === */
    fr(cx - 11, cy - 14, 22, 11, BB0);
    vl(cx - 11, cy - 14, 9, BED);
    vl(cx + 10, cy - 14, 9, BED);

    /* === LAYER 2: Mid body === */
    fr(cx - 10, cy - 24, 20, 11, BB0);
    hl(cx - 7, cy - 23, 14, BED);
    hl(cx - 7, cy - 21, 14, BED);
    hl(cx - 7, cy - 19, 14, BED);

    /* === LAYER 3: Upper hood === */
    fr(cx - 8, cy - 32, 16, 9, BB1);

    /* === LAYER 4: Nose tip (blunt slatted grill) === */
    fr(cx - 5, cy - 38, 10, 7, BB1);
    fr(cx - 3, cy - 42, 6, 5, BB2);
    hl(cx - 3, cy - 41, 6, BED);
    hl(cx - 3, cy - 39, 6, BED);

    /* === LAYER 5: Cockpit canopy === */
    fr(cx - 7, cy - 28, 14, 7, BCK);
    fr(cx - 5, cy - 27, 10, 5, 0x0017u);
    hl(cx - 7, cy - 28, 14, BED);
    hl(cx - 7, cy - 22, 14, 0x3166u);

    /* === LAYER 6: Bat-wing emblem === */
    fr(cx - 1, cy - 19, 2, 4, BAC);
    fr(cx - 8, cy - 18, 7, 2, BAC);
    fr(cx + 1, cy - 18, 7, 2, BAC);

    /* === LAYER 7: Headlights + Taillights === */
    fr(cx - 6, cy - 41, 3, 2, Wh);
    fr(cx + 3, cy - 41, 3, 2, Wh);
    fr(cx - 12, cy - 6, 3, 2, Re);
    fr(cx + 9, cy - 6, 3, 2, Re);
}

/* ================================================================
 * ENEMY — 5-layer depth-scaled sprite stack
 *
 * Layer 0: shadow
 * Layer 1: bumper/undercarriage (widest)
 * Layer 2: lower body + wheel arches
 * Layer 3: upper body + windshield glass
 * Layer 4: roof (narrower = wedge illusion)
 * + type-specific details (siren, stripe, exhaust)
 * ================================================================ */
static void drawEnemy(const Enm *e) {
    int z = FRFP(e->z_fp);
    if (z < 60) z = 60;
    int wz = FRFP(gm.cam_z) + z;

    int rcx, rhw, rum;
    sample(wz, &rcx, &rhw, &rum);

    int lwx = lanex(e->lane, rhw) + e->loff;
    int ewx = rcx + lwx;

    int sx = pjx(ewx - FRFP(pl.x_fp), z);
    int sy = pjy(z);

    if (sy <= HOR_Y + 1 || sy >= H + 20) return;
    if (sx < -80 || sx > W + 80) return;

    int bw = imx((EN_BW * CAM_D) / z, 4);
    int bh = imx((EN_BH * CAM_D) / z, 4);
    int ds = dshade(z);
    int bw2 = bw / 2;

    /* Colour by type */
    unsigned short bc, rc, gc;
    switch (e->type) {
        case 0: bc = EN0; rc = sh(EN0, 230); gc = 0x4208u; break;
        case 1: bc = EN1; rc = sh(EN1, 230); gc = 0x2104u; break;
        case 2: bc = EN2; rc = sh(EN2, 230); gc = 0x2104u; break;
        default: bc = EN3; rc = sh(EN3, 230); gc = 0x4208u; break;
    }

    /* L0: Shadow */
    fr(sx - bw2 - 2, sy + 1, bw + 4, imx(bh / 6, 1), sh(K, 120));

    /* L1: Bumper (full width) */
    fr(sx - bw2, sy - bh / 4, bw, bh / 4, sh(bc, ds));
    /* Wheel arches — dark rectangles at corners */
    { int ww = imx(bw / 5, 1);
      fr(sx - bw2, sy - bh / 4, ww, bh / 4, sh(K, ds));
      fr(sx + bw2 - ww, sy - bh / 4, ww, bh / 4, sh(K, ds)); }

    /* L2: Lower body */
    fr(sx - bw2, sy - bh * 3 / 4, bw, bh / 2, sh(bc, ds));
    /* Side panel highlight line */
    hl(sx - bw2, sy - bh / 2, bw, sh(rc, ds));

    /* L3: Windshield glass strip */
    { int gw = bw * 3 / 4, gh = imx(bh / 7, 1);
      fr(sx - gw / 2, sy - bh * 5 / 8, gw, gh, sh(gc, ds)); }

    /* L4: Roof (narrower top — 3D wedge) */
    { int rw = bw * 3 / 4, rh = imx(bh / 5, 1);
      int ry = sy - bh - rh;
      fr(sx - rw / 2, ry, rw, rh, sh(rc, ds));
      hl(sx - rw / 2, ry, rw, sh(MG, ds)); }

    /* Headlights */
    if (bw > 4) {
        fr(sx - bw2 + 1, sy - bh / 5, imx(bw / 5, 1), imx(bh / 8, 1), sh(Wh, ds));
        fr(sx + bw2 - imx(bw / 5, 1) - 1, sy - bh / 5, imx(bw / 5, 1), imx(bh / 8, 1), sh(Wh, ds));
    }

    /* Type details */
    switch (e->type) {
        case 0: {
            /* Police siren on roof */
            int rw = bw * 3 / 4, rh = imx(bh / 5, 1);
            int ry = sy - bh - rh;
            int sw = imx(rw / 2, 2);
            unsigned short sl = (gm.fr & 8) ? Re : Bl;
            fr(sx - sw / 2, ry - imx(rh / 2, 1), sw / 2, imx(rh / 2, 1), sh(sl, ds));
            fr(sx, ry - imx(rh / 2, 1), sw / 2, imx(rh / 2, 1), sh(sl == Re ? Bl : Re, ds));
            /* Blue stripe across mid */
            if (bw > 3) fr(sx - bw2, sy - bh * 5 / 8 - 1, bw, imx(bh / 10, 1), sh(Bl, ds));
            break; }
        case 1: {
            /* Racing stripe down centre */
            if (bh > 4) vl(sx, sy - bh * 3 / 4, bh * 3 / 4, sh(K, ds));
            break; }
        case 2: {
            /* Exhaust pipes */
            if (bw > 4) {
                fr(sx - bw2, sy - bh / 8, imx(bw / 6, 1), imx(bh / 8, 1), sh(DG, ds));
                fr(sx + bw2 - imx(bw / 6, 1), sy - bh / 8, imx(bw / 6, 1), imx(bh / 8, 1), sh(DG, ds));
            }
            break; }
        default: break;
    }

    /* Taillight blink */
    if ((gm.fr >> 3) & 1) {
        fr(sx - bw2, sy - 2, imx(bw / 6, 1), 2, sh(Re, ds));
        fr(sx + bw2 - imx(bw / 6, 1), sy - 2, imx(bw / 6, 1), 2, sh(Re, ds));
    }
}

/* ================================================================
 * HUD
 * ================================================================ */
static void drawHUD(void) {
    /* Speed bar + HP — top-left */
    fr(0, 0, 86, 44, sh(K, 180));
    dr(0, 0, 86, 44, DG);
    /* Speed bar */
    fr(4, 4, 62, 8, sh(BLD_2, 200));
    { int bar = ic((pl.spd_fp * 60) / BSTSPD, 0, 60);
      unsigned short sc = pl.boost_t > 0 ? Or : Cy;
      fr(5, 5, bar, 6, sc); }
    dnum(68, 5, FRFP(pl.spd_fp), LG);
    /* HP hearts */
    for (int i = 0; i < 5; i++) {
        unsigned short hc = i < pl.hp ? Gr : sh(DG, 120);
        diam(8 + i * 12, 18, 4, hc);
    }
    /* Score */
    dnum(4, 30, pl.score, Ye);

    /* Speed dial — right side */
    fr(W - 20, 0, 20, 50, sh(K, 180));
    dr(W - 20, 0, 20, 50, DG);
    { int spd = FRFP(pl.spd_fp);
      int bh = ic(spd * 2, 0, 44);
      fr(W - 16, 48 - bh, 12, bh, pl.boost_t > 0 ? Or : NEON_T); }

    /* Boost indicator */
    if (pl.boost_t > 0) {
        int bw = (pl.boost_t * 60) / BSTDUR;
        fr(4, 40, bw, 3, Or);
    }
}

/* ================================================================
 * OVERLAY SCREENS
 * ================================================================ */
static void dimHalf(void) {
    int n = STRIDE * H;
    for (int i = 0; i < n; i++) {
        unsigned short p = fb[i];
        fb[i] = (unsigned short)(((p >> 1) & 0x7800u) | ((p >> 1) & 0x03E0u) | ((p >> 1) & 0x000Fu));
    }
}

static void drawMenu(void) {
    clr(K);
    /* Sky gradient */
    for (int y = 0; y < H; y++) {
        unsigned short sky;
        if (y < 80) { int b = 3 + (y >> 3); int g = y >> 4; sky = (unsigned short)((g << 5) | b); }
        else if (y < 160) { int b = 13 + ((y - 80) >> 4); int g = 5 + ((y - 80) >> 4); sky = (unsigned short)((g << 5) | b); }
        else { int r = (y - 160) >> 3; int g = 3 + ((y - 160) >> 4); sky = (unsigned short)((r << 11) | (g << 5) | 8); }
        hl(0, y, W, sky);
    }

    drawSky();
    drawRain();

    /* Gotham silhouette at bottom */
    for (int b = 0; b < 12; b++) {
        int bx = b * 27; int bh = 18 + (b * 7) % 28;
        fr(bx, 68 - bh, 22, bh, BLD_3);
        fr(bx + 2, 68 - bh - 3, 4, 4, BLD_3);
        fr(bx + 15, 68 - bh - 3, 4, 4, BLD_3);
    }

    /* Batman silhouette */
    fr(108, 48, 22, 32, DG); fr(190, 48, 22, 32, DG);  /* cape wings */
    fr(128, 30, 64, 52, DG);
    fr(140, 18, 12, 18, DG); fr(168, 18, 12, 18, DG);  /* ears */
    fr(148, 42, 24, 22, 0x2945u); /* face */
    fr(148, 60, 24, 5, Ye);       /* belt */
    diam(160, 52, 7, Ye);         /* emblem */

    /* Title banner */
    fr(54, 88, 212, 30, Ye);
    fr(56, 90, 208, 26, K);
    int tx = 64; unsigned short tc = Ye;
    /* B */ fr(tx,92,4,22,tc); fr(tx+4,92,8,6,tc); fr(tx+4,100,8,6,tc); fr(tx+4,108,8,7,tc); tx += 16;
    /* A */ fr(tx,96,4,18,tc); fr(tx+8,96,4,18,tc); fr(tx+4,92,4,5,tc); fr(tx+4,101,4,4,tc); tx += 16;
    /* T */ fr(tx,92,20,6,tc); fr(tx+8,92,4,22,tc); tx += 22;
    /* M */ fr(tx,92,4,22,tc); fr(tx+14,92,4,22,tc); fr(tx+4,94,5,7,tc); fr(tx+9,94,5,7,tc); tx += 20;
    /* A */ fr(tx,96,4,18,tc); fr(tx+8,96,4,18,tc); fr(tx+4,92,4,5,tc); fr(tx+4,101,4,4,tc); tx += 16;
    /* N */ fr(tx,92,4,22,tc); fr(tx+14,92,4,22,tc); fr(tx+4,94,6,5,tc); fr(tx+9,100,6,5,tc);

    /* Subtitle */
    fr(88, 122, 144, 10, Cy); fr(90, 123, 140, 8, K);
    for (int i = 0; i < 9; i++) fr(92 + i * 14, 125, 10, 4, Cy);

    /* Controls */
    fr(60, 136, 200, 1, MG);
    fr(80, 142, 16, 8, DG); dr(80, 142, 16, 8, LG);  /* W */
    fr(60, 142, 16, 8, DG); dr(60, 142, 16, 8, LG);  /* A */
    fr(100, 142, 16, 8, DG); dr(100, 142, 16, 8, LG); /* S */
    fr(120, 142, 16, 8, DG); dr(120, 142, 16, 8, LG); /* D */
    fr(150, 142, 38, 8, DG); dr(150, 142, 38, 8, LG); /* SHIFT */

    /* Blink prompt */
    unsigned short bc = (gm.fr & 32) ? Ye : Or;
    fr(78, 160, 164, 16, bc);
    fr(80, 162, 160, 12, K);
    /* "PRESS ENTER" approximation */
    for (int i = 0; i < 6; i++) fr(86 + i * 24, 165, 20, 6, bc);

    fr(60, 180, 200, 1, MG);
    diam(128, 196, 4, Ye);
    dnum(138, 192, pl.score, Ye);
}

static void drawPause(void) {
    dimHalf();
    fr(80, 90, 160, 60, DG); dr(80, 90, 160, 60, Ye);
    /* "PAUSED" bars */
    fr(120, 108, 12, 22, Ye);
    fr(148, 108, 12, 22, Ye);
    fr(100, 136, 120, 7, MG);
}

static void drawGameover(void) {
    dimHalf();
    fr(50, 70, 220, 110, K); dr(50, 70, 220, 110, Re);
    fr(70, 85, 180, 7, Re);        /* "GAME OVER" bar */
    dnum(120, 103, pl.score, Ye);
    dnum(120, 120, gm.lap, Cy);    /* laps */
    /* Blink prompt */
    if (gm.fr & 32) fr(90, 152, 140, 6, Wh);
}

/* ================================================================
 * SPAWN
 * ================================================================ */
static void spawn(void) {
    for (int i = 0; i < NEMAX; i++) {
        if (en[i].active) continue;
        int si = segidx(FRFP(gm.cam_z));
        int hw = road[si].width;
        en[i].active = true;
        en[i].lane = ir(0, NLANES - 1);
        en[i].loff = ir(-hw / 16, hw / 16);
        en[i].z_fp = ESPAWN_Z + TOFP(ir(0, 1200));
        en[i].spd_fp = TOFP(ir(8, 16));
        en[i].type = ir(0, 3);
        return;
    }
}

/* ================================================================
 * GAME INIT
 * ================================================================ */
static void ginit(void) {
    pl.x_fp = 0; pl.steer_fp = 0; pl.spd_fp = 0;
    pl.boost_t = 0; pl.hp = 5; pl.score = 0;
    pl.invuln = 0; pl.alive = true;
    for (int i = 0; i < NEMAX; i++) en[i].active = false;
    gm.cam_z = 0; gm.lap = 0; gm.spawn_cd = 24;
    gm.flash = false;
}

/* ================================================================
 * PHYSICS
 * ================================================================ */
static void updatePhysics(void) {
    if (!pl.alive) return;

    int mx = pl.boost_t > 0 ? BSTSPD : MAXSPD;

    if (kU) {
        int a = pl.boost_t > 0 ? BSTACC : ACC_FP;
        pl.spd_fp += a;
    } else if (kD) {
        pl.spd_fp -= pl.spd_fp > 0 ? BRK_FP : RVACC;
    } else {
        if (pl.spd_fp > 0) { pl.spd_fp -= DRAG; if (pl.spd_fp < 0) pl.spd_fp = 0; }
        else if (pl.spd_fp < 0) { pl.spd_fp += DRAG; if (pl.spd_fp > 0) pl.spd_fp = 0; }
    }

    if (kSh && pl.boost_t <= 0 && pl.spd_fp > TOFP(8)) pl.boost_t = BSTDUR;
    if (pl.boost_t > 0) {
        pl.boost_t--;
        if (pl.spd_fp < TOFP(12)) pl.spd_fp = TOFP(12);
    }
    pl.spd_fp = ic(pl.spd_fp, -MAXREV, mx);

    if (kL) pl.steer_fp -= SACC;
    if (kR) pl.steer_fp += SACC;
    pl.steer_fp = (pl.steer_fp * SDAMP_N) / SDAMP_D;
    pl.steer_fp = ic(pl.steer_fp, -SMAX, SMAX);
    pl.x_fp += pl.steer_fp;

    /* Centrifugal push from road curve */
    { int si = segidx(FRFP(gm.cam_z));
      pl.x_fp += road[si].curve * FRFP(pl.spd_fp) / 64; }

    /* Clamp to road */
    { int si = segidx(FRFP(gm.cam_z));
      int hw = road[si].width;
      int lim = TOFP(hw - EDGE_M);
      if (pl.x_fp > lim) { pl.x_fp = lim; if (pl.steer_fp > 0) pl.steer_fp = 0; }
      if (pl.x_fp < -lim) { pl.x_fp = -lim; if (pl.steer_fp < 0) pl.steer_fp = 0; }
    }

    gm.cam_z += pl.spd_fp;
    if (gm.cam_z < 0) gm.cam_z += TOFP(TRACKLEN);
    if (gm.cam_z >= TOFP(TRACKLEN)) { gm.cam_z -= TOFP(TRACKLEN); gm.lap++; }

    if (pl.invuln > 0) pl.invuln--;
    if (pl.spd_fp > 0) pl.score += FRFP(pl.spd_fp) / 7;
}

/* ================================================================
 * WORLD UPDATE
 * ================================================================ */
static void updateWorld(void) {
    if (--gm.spawn_cd <= 0) { spawn(); gm.spawn_cd = ir(SPAWN_LO, SPAWN_HI); }

    int sfp = pl.spd_fp;

    for (int i = 0; i < NEMAX; i++) {
        Enm *e = &en[i]; if (!e->active) continue;
        e->z_fp -= sfp;
        e->z_fp -= e->spd_fp / 4;
        if (e->z_fp < EDESPAWN) { e->active = false; continue; }

        /* Collision */
        int si = segidx(FRFP(gm.cam_z) + FRFP(e->z_fp));
        int hw = road[si].width;
        int lwx = lanex(e->lane, hw) + e->loff;
        int dx = lwx - FRFP(pl.x_fp);
        if (e->z_fp < COLL_ZW && e->z_fp > TOFP(20) && ia(dx) < COLL_XW && pl.invuln <= 0 && pl.alive) {
            e->active = false;
            pl.hp--;
            pl.spd_fp /= 2;
            pl.invuln = 90;
            gm.flash = true;
            if (pl.hp <= 0) { pl.hp = 0; pl.alive = false; gm.st = ST_OVER; kEnt = false; }
        }

        /* Keep in road */
        { int el = hw - EDGE_M;
          if (lwx > el) e->loff -= 8;
          if (lwx < -el) e->loff += 8; }
    }

    gm.flash = false;
}

/* ================================================================
 * RENDER SCENE
 * ================================================================ */
static void renderScene(void) {
    clr(K);
    drawSky();
    drawSkyline();
    drawRoad();
    drawLamps();
    drawRain();

    /* Z-sort enemies: draw far first */
    int ord[NEMAX], cnt = 0;
    for (int i = 0; i < NEMAX; i++) if (en[i].active) ord[cnt++] = i;
    for (int i = 0; i < cnt - 1; i++)
        for (int j = i + 1; j < cnt; j++)
            if (en[ord[i]].z_fp < en[ord[j]].z_fp)
                { int t = ord[i]; ord[i] = ord[j]; ord[j] = t; }
    for (int i = 0; i < cnt; i++) drawEnemy(&en[ord[i]]);

    drawPlayer();
    drawHUD();

    if (gm.st == ST_PAUSE) drawPause();
    if (gm.st == ST_OVER) drawGameover();

    /* Crash flash overlay */
    if (gm.flash) {
        for (int y = 0; y < H; y += 2) hl(0, y, W, sh(Re, 80));
    }

    blit();
}

/* ================================================================
 * MAIN
 * ================================================================ */
int main(void) {
    vinit();
    tinit();
    buildTrack();

    gm.st = ST_MENU;
    gm.fr = 0;
    ginit();
    kU = kD = kL = kR = kSh = kEnt = kEsc = false;

    while (1) {
        twait();
        gm.fr++;
        input();

        switch (gm.st) {
            case ST_MENU:
                if (kEnt) { ginit(); gm.st = ST_PLAY; kEnt = false; }
                drawMenu();
                blit();
                break;

            case ST_PLAY:
                if (kEsc) { gm.st = ST_PAUSE; kEsc = false; }
                else { updatePhysics(); updateWorld(); }
                renderScene();
                break;

            case ST_PAUSE:
                if (kEsc || kEnt) { gm.st = ST_PLAY; kEsc = kEnt = false; }
                renderScene();
                break;

            case ST_OVER:
                if (kEnt) { gm.st = ST_MENU; kEnt = false; }
                renderScene();
                break;
        }
    }
    return 0;
}

/*
 * ================================================================
 * COMPILE & RUN (CPUlator):
 *   1. Open https://cpulator.01xz.net/?sys=arm-de1soc
 *   2. Paste entire file into C editor
 *   3. Compile and Load
 *   4. Continue (F5)
 *   5. Click inside page for keyboard focus
 *   6. ENTER to start
 *
 * CONTROLS:
 *   W / ↑      Accelerate
 *   S / ↓      Brake / Reverse
 *   A / ←      Steer left
 *   D / →      Steer right
 *   L-SHIFT    Boost
 *   ESC        Pause / Resume
 *   ENTER      Start / Confirm
 * ================================================================
 */
