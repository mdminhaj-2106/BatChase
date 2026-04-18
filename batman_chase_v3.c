/*
 * batman_chase_v3.c
 * Batman: Gotham Chase — DE1-SoC / CPUlator  (v3 — FINAL)
 *
 * TARGET  : ARM Cortex-A9 · DE1-SoC · CPUlator
 * DISPLAY : VGA 320×240 RGB565
 * INPUT   : PS/2 keyboard
 *
 * ================================================================
 *  V3 ANTI-FLICKER ARCHITECTURE  (key change from v1/v2)
 * ================================================================
 *  Root cause of all previous flicker:
 *    Every draw call wrote directly to VGA hardware RAM at 0xC8000000.
 *    The display controller scans this memory continuously at 60 Hz.
 *    Erase → re-draw in one buffer = intermediate state always visible.
 *
 *  Solution — Software Framebuffer:
 *    static unsigned short fb[240 * 512];   ← in ARM SDRAM (fast)
 *    ALL drawing writes to fb[] (RAM, never to VGA).
 *    A single forward-only blit (fb → 0xC8000000) is done ONCE per
 *    frame AFTER the entire frame is finished.  The display reads the
 *    fully-completed previous frame at all times — zero tearing, zero
 *    flicker, no hardware vsync required.
 *
 *  Full-screen clear ("old screen cleanup"):
 *    clear_fb() is called at the top of every render pass.
 *    It fills fb with near-black sky colour using 32-bit writes (~1 ms).
 *    draw_background() then overwrites every pixel with the scene.
 *    No ghost pixels ("trains") remain from previous frames.
 *
 * ================================================================
 *  3-D PSEUDO EFFECT
 * ================================================================
 *  Road is drawn per-scan-line with a vanishing point at HORIZON_Y.
 *  DS(n,sc) scales any pixel dimension with depth factor sc (96..256).
 *  Sprites use DS() so they grow as they approach the player.
 *  Every sprite has a flat shadow drawn before it.
 *  Batmobile is drawn in 8 stacked layers (shadow → fins → hull →
 *  cockpit canopy → bat emblem → nose → headlights → exhaust).
 *
 * ================================================================
 *  BATMOBILE SHAPE (BTAS top-down, per search reference)
 * ================================================================
 *  - Blunt slatted nose at front (narrow)
 *  - Cockpit bubble in center-front, slides to canopy
 *  - Body widens toward rear
 *  - Two long horizontal tailfins flank a central turbine
 *  - Jet exhaust glow at very rear
 *  - Near-black / dark-charcoal body with cool blue cockpit
 *  - Yellow bat-wing emblem on hood
 *  - Red taillights flanking turbine
 */

#include <stdlib.h>
#include <stdbool.h>

/* ================================================================
 *  HARDWARE ADDRESSES
 * ================================================================ */
#define PIX_CTRL_BASE   0xFF203020u
#define PIX_BUF1        0xC8000000u   /* single VGA hardware buffer */
#define PS2_BASE        0xFF200100u
#define TIMER_BASE      0xFF202000u

/* ================================================================
 *  DISPLAY / ROAD CONSTANTS
 * ================================================================ */
#define SCR_W        320
#define SCR_H        240
#define PIX_STRIDE   512   /* VGA hardware row stride (shorts) */

#define HORIZON_Y    52    /* vanishing point Y                */
#define ROAD_CX      160   /* road centre X                    */
#define ROAD_FAR_H   18    /* half-width at horizon            */
#define ROAD_NEAR_H  82    /* half-width at bottom             */

/* Approximate flat-road bounds used for spawn clipping */
#define ROAD_L   (ROAD_CX - ROAD_NEAR_H)   /* 78 */
#define ROAD_R   (ROAD_CX + ROAD_NEAR_H)   /* 242 */
#define ROAD_W   (ROAD_R - ROAD_L)          /* 164 */

/* ================================================================
 *  RGB565 COLOUR PALETTE
 * ================================================================ */
#define C_BLACK      0x0000u
#define C_WHITE      0xFFFFu
#define C_RED        0xF800u
#define C_GREEN      0x07E0u
#define C_BLUE       0x001Fu
#define C_YELLOW     0xFFE0u
#define C_ORANGE     0xFCA0u
#define C_CYAN       0x07FFu
#define C_MAGENTA    0xF81Fu
#define C_DGREY      0x4208u
#define C_MGREY      0x8410u
#define C_LGREY      0xCE59u
#define C_HEALTH     0x07E0u
#define C_STUD       0xFFE0u

/* Batmobile-specific */
#define BAT_BODY     0x0842u   /* near-black charcoal body   */
#define BAT_BODY2    0x2945u   /* slightly lighter underside */
#define BAT_COCK     0x0019u   /* deep blue cockpit          */
#define BAT_COCK2    0x0017u   /* darker cockpit inner       */
#define BAT_EDGE     0x18C3u   /* fin/edge highlight         */

/* Environment */
#define C_SHADOW     0x0841u
#define C_SIDEWALK   0x4A49u   /* dark Gotham concrete       */
#define C_RAIN       0x4C99u
#define C_NEON_P     0xF81Fu   /* neon pink                  */
#define C_NEON_T     0x07EFu   /* neon teal                  */
#define C_BLDG       0x294Au   /* Gotham building dark       */
#define C_BLDG2      0x3186u   /* lighter building           */

/* ================================================================
 *  GAME CONSTANTS
 * ================================================================ */
#define MAX_ENEMIES     14
#define MAX_STUDS       24
#define MAX_OBSTACLES    8
#define MAX_POWERUPS     4

/* Base (full-scale) sprite sizes — used for collision */
#define PL_W        16
#define PL_H        24
#define EN_W        14
#define EN_H        18
#define STD_R        4
#define OBS_W       24
#define OBS_H       16
#define PWR_W       12
#define PWR_H       12
#define BOSS_W      48
#define BOSS_H      36
#define MISS_W       4
#define MISS_H       8

#define PL_MAX_HP       5
#define PL_BASE_SPD     2
#define BOSS_BASE_HP   10
#define SCROLL_BASE     1
#define INVULN_FRAMES  90
#define BOOST_FRAMES  180
#define MAGNET_FRAMES 300
#define BOSS_PERIOD   600
#define SPAWN_PERIOD  100

/* ================================================================
 *  ENUMERATIONS
 * ================================================================ */
typedef enum { MENU, PLAYING, PAUSED, GAMEOVER } GState;
typedef enum { E_SWAY, E_DIVE, E_ASSAULT, E_PROJECTILE } EType;
typedef enum { PW_MAGNET, PW_BOOST, PW_MISSILE } PwType;
typedef enum { BOSS_FREEZE, BOSS_RIDDLER, BOSS_JOKER } BossType;

/* ================================================================
 *  STRUCTS
 * ================================================================ */
typedef struct {
    int  x, y, vx, hp, invuln, score, studs, boost_t, magnet_t, missiles;
    bool alive, boosting;
} Player;

typedef struct {
    int  x, y, vx, vy, init_x, timer;
    EType type;
    bool active, activated, diving;
} Enemy;

typedef struct { int x, y; bool active, collected; } Stud;
typedef struct { int x, y, w, h; bool active; } Obstacle;
typedef struct { int x, y, anim; PwType type; bool active; } Powerup;
typedef struct { int x, y, vx, vy, life; bool active; } Missile;

typedef struct {
    int x, y, hp, max_hp, timer, atk_timer;
    BossType type;
    bool active, intro;
} Boss;

typedef struct {
    GState state;
    int frame, scroll, scroll_spd, spawn_t, boss_t, bosses;
    bool boss_active;
} Ctx;

/* ================================================================
 *  GLOBALS
 * ================================================================
 *  [V3-KEY] Software framebuffer.  Every draw call writes to fb[].
 *  blit_vga() copies fb → hardware once per frame, after render.
 */
static unsigned short fb[SCR_H * PIX_STRIDE];   /* 245,760 bytes in SDRAM */

static Player   pl;
static Enemy    enemies[MAX_ENEMIES];
static Stud     studs[MAX_STUDS];
static Obstacle obs[MAX_OBSTACLES];
static Powerup  pwrs[MAX_POWERUPS];
static Missile  rocket;
static Boss     boss;
static Ctx      ctx;

static bool k_left, k_right, k_up, k_enter, k_esc, k_up_prev;

/* ================================================================
 *  MATH
 * ================================================================ */
static int iabs(int x)                    { return x<0?-x:x; }
static int iclamp(int v,int lo,int hi)    { return v<lo?lo:v>hi?hi:v; }
static int imin(int a,int b)              { return a<b?a:b; }
static int imax(int a,int b)              { return a>b?a:b; }

static unsigned int rng = 0xACE1u;
static int irand(int lo, int hi) {
    rng ^= rng<<13; rng ^= rng>>17; rng ^= rng<<5;
    int r = hi-lo+1;
    return r<=0 ? lo : lo + (int)(rng&0x7FFFu)%r;
}

static const short SIN4[64] = {
      0, 25, 50, 75,100,125,150,175,
    198,222,245,267,289,310,330,350,
    368,385,401,416,429,441,452,462,
    471,478,484,489,492,494,495,495,
    494,492,489,484,478,471,462,452,
    441,429,416,401,385,368,350,330,
    310,289,267,245,222,198,175,150,
    125,100, 75, 50, 25,  0,-25,-50
};
static int isin(int a) {
    a&=255;
    if(a< 64) return  SIN4[a];
    if(a<128) return  SIN4[127-a];
    if(a<192) return -SIN4[a-128];
    return            -SIN4[255-a];
}

/* ================================================================
 *  PERSPECTIVE + DEPTH SCALE
 * ================================================================ */

/* Road left/right edge at scan-line y */
static int road_left(int y) {
    if(y<=HORIZON_Y) return ROAD_CX - ROAD_FAR_H;
    int t = ((y-HORIZON_Y)*256)/(SCR_H-HORIZON_Y);
    return ROAD_CX - ROAD_FAR_H - (t*(ROAD_NEAR_H-ROAD_FAR_H))/256;
}
static int road_right(int y) {
    if(y<=HORIZON_Y) return ROAD_CX + ROAD_FAR_H;
    int t = ((y-HORIZON_Y)*256)/(SCR_H-HORIZON_Y);
    return ROAD_CX + ROAD_FAR_H + (t*(ROAD_NEAR_H-ROAD_FAR_H))/256;
}

/*
 * depth_scale(y) → 96 (far/horizon) .. 256 (near/bottom)
 * Use DS(n,sc) to scale a pixel dimension.
 */
static int depth_scale(int y) {
    if(y<=HORIZON_Y) return 96;
    int t = ((y-HORIZON_Y)*160)/(SCR_H-HORIZON_Y);
    return 96+t;
}
#define DS(n,sc)  (imax(((n)*(sc))>>8, 1))

/* ================================================================
 *  SOFTWARE FB DRAWING PRIMITIVES  (all write to fb[], never VGA)
 * ================================================================ */
static inline void pset(int x,int y,unsigned short c) {
    if((unsigned)x<SCR_W && (unsigned)y<(unsigned)SCR_H)
        fb[y*PIX_STRIDE+x]=c;
}

static void frect(int x,int y,int w,int h,unsigned short c) {
    if(x<0){w+=x;x=0;} if(y<0){h+=y;y=0;}
    if(x+w>SCR_W)w=SCR_W-x; if(y+h>SCR_H)h=SCR_H-y;
    if(w<=0||h<=0) return;
    for(int j=y;j<y+h;j++)
        for(int i=x;i<x+w;i++)
            fb[j*PIX_STRIDE+i]=c;
}

static void hline(int x,int y,int l,unsigned short c){frect(x,y,l,1,c);}
static void vline(int x,int y,int l,unsigned short c){frect(x,y,1,l,c);}

static void drect(int x,int y,int w,int h,unsigned short c) {
    hline(x,y,w,c); hline(x,y+h-1,w,c);
    vline(x,y,h,c); vline(x+w-1,y,h,c);
}

static void fdiamond(int cx,int cy,int r,unsigned short c) {
    for(int dy=-r;dy<=r;dy++){int hw=r-iabs(dy); hline(cx-hw,cy+dy,hw*2+1,c);}
}

/* ================================================================
 *  DIGIT RENDERER (5×7)
 * ================================================================ */
static const unsigned char DGLYPH[10][7]={
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
static void draw_digit(int x,int y,int d,unsigned short c) {
    if(d<0||d>9) return;
    for(int row=0;row<7;row++){
        unsigned char bits=DGLYPH[d][row];
        for(int col=0;col<5;col++)
            if(bits&(0x10>>col)) pset(x+col,y+row,c);
    }
}
static void draw_num(int x,int y,int n,unsigned short c) {
    if(n<0)n=0;
    char buf[10]; int len=0;
    if(n==0){draw_digit(x,y,0,c);return;}
    while(n>0){buf[len++]=(char)(n%10);n/=10;}
    for(int i=len-1;i>=0;i--){draw_digit(x,y,(int)buf[i],c);x+=6;}
}

/* ================================================================
 *  SHARED SHADOW HELPER
 * ================================================================ */
static void draw_shadow(int cx,int cy,int bw,int bh,int sc) {
    int sw = DS(bw,sc);
    int sh = imax(DS(bh/5,sc),1);
    frect(cx-sw/2+DS(2,sc), cy+DS(bh/2,sc), sw, sh, C_SHADOW);
}

/* ================================================================
 *  BATMOBILE — 8-layer sprite stack  [V3-3D]
 *
 *  BTAS reference: blunt grill nose →  cockpit bubble → wide body
 *  → horizontal tailfins flanking central turbine → jet exhaust.
 * ================================================================ */
static void draw_player(void) {
    if(!pl.alive) return;
    if(pl.invuln>0 && (ctx.frame&3)) return;

    int cx=pl.x, cy=pl.y;
    int sc=depth_scale(cy);

    /* === LAYER 0: ground shadow (flat dark ellipse behind car) === */
    {
        int sw=DS(22,sc), sh=DS(4,sc);
        frect(cx-sw/2+DS(2,sc), cy+DS(13,sc), sw, sh, C_SHADOW);
    }

    /* === LAYER 1: exhaust jet / turbine glow (rearmost) === */
    if(pl.boosting) {
        frect(cx-DS(4,sc), cy+DS(13,sc), DS(8,sc), DS(10,sc), C_ORANGE);
        frect(cx-DS(2,sc), cy+DS(21,sc), DS(4,sc), DS(6,sc),  C_YELLOW);
    } else {
        unsigned short eg=(ctx.frame&8)?C_ORANGE:0xFC40u;
        frect(cx-DS(3,sc), cy+DS(12,sc), DS(6,sc), DS(4,sc), eg);
    }

    /* === LAYER 2: horizontal tailfins (widest, rear-most solid part) ===
     * Left fin stretches far left, right fin far right. */
    {
        int fin_y   = cy + DS(4,sc);
        int fin_h   = DS(7,sc);
        int body_hw = DS(5,sc);
        int fin_ext = DS(9,sc);    /* how far fin sticks out past body */
        /* left fin */
        frect(cx-body_hw-fin_ext, fin_y, fin_ext+1, fin_h,     BAT_BODY);
        hline(cx-body_hw-fin_ext, fin_y, fin_ext,              BAT_EDGE); /* fin top edge */
        /* right fin */
        frect(cx+body_hw,         fin_y, fin_ext,   fin_h,     BAT_BODY);
        hline(cx+body_hw,         fin_y, fin_ext,              BAT_EDGE);
        /* centre turbine housing between fins */
        frect(cx-body_hw, fin_y, body_hw*2, fin_h, BAT_BODY2);
        /* turbine circle (intake/exhaust ring) */
        int tr=DS(3,sc);
        fdiamond(cx, fin_y+fin_h/2, tr, C_DGREY);
        fdiamond(cx, fin_y+fin_h/2, imax(tr-1,1), 0x2945u);
    }

    /* === LAYER 3: main lower hull (rear body block) === */
    frect(cx-DS(6,sc), cy-DS(2,sc), DS(12,sc), DS(7,sc), BAT_BODY);

    /* === LAYER 4: stacked engine/mid body (slightly raised visual layer) === */
    frect(cx-DS(5,sc), cy-DS(8,sc), DS(10,sc), DS(7,sc), BAT_BODY);
    /* side-body detail lines (give thickness illusion) */
    vline(cx-DS(5,sc),   cy-DS(8,sc), DS(6,sc), BAT_EDGE);
    vline(cx+DS(5,sc)-1, cy-DS(8,sc), DS(6,sc), BAT_EDGE);

    /* === LAYER 5: upper body / hood (narrows toward front) === */
    frect(cx-DS(4,sc), cy-DS(14,sc), DS(8,sc), DS(7,sc), BAT_BODY);
    /* hood panel lines (BTAS slatted grill feel) */
    for(int sl=0;sl<3;sl++)
        hline(cx-DS(3,sc), cy-DS(13,sc)+DS(sl*2,sc),
              DS(6,sc), BAT_EDGE);

    /* === LAYER 6: nose tip (blunt grill front, BTAS key feature) === */
    frect(cx-DS(3,sc), cy-DS(18,sc), DS(6,sc), DS(5,sc), BAT_BODY);
    frect(cx-DS(2,sc), cy-DS(20,sc), DS(4,sc), DS(3,sc), BAT_BODY);
    /* Grill slats on nose */
    hline(cx-DS(2,sc), cy-DS(19,sc), DS(4,sc), BAT_EDGE);
    hline(cx-DS(2,sc), cy-DS(17,sc), DS(4,sc), BAT_EDGE);

    /* === LAYER 7: cockpit canopy (blue bubble, slides forward) === */
    frect(cx-DS(3,sc), cy-DS(11,sc), DS(6,sc), DS(4,sc), BAT_COCK);
    frect(cx-DS(2,sc), cy-DS(10,sc), DS(4,sc), DS(3,sc), BAT_COCK2);
    /* Canopy rim highlight */
    hline(cx-DS(3,sc), cy-DS(11,sc), DS(6,sc), 0x18C3u);
    hline(cx-DS(3,sc), cy-DS(8,sc),  DS(6,sc), 0x18C3u);

    /* === LAYER 8: bat emblem on hood (yellow, iconic wing+spike) === */
    /* centre spike up */
    frect(cx-DS(1,sc), cy-DS(7,sc), DS(2,sc), DS(3,sc), C_YELLOW);
    /* left wing */
    frect(cx-DS(5,sc), cy-DS(6,sc), DS(4,sc), DS(2,sc), C_YELLOW);
    /* right wing */
    frect(cx+DS(1,sc), cy-DS(6,sc), DS(4,sc), DS(2,sc), C_YELLOW);

    /* === LAYER 9: headlights at nose tip === */
    frect(cx-DS(4,sc), cy-DS(19,sc), DS(2,sc), DS(1,sc), C_WHITE);
    frect(cx+DS(2,sc), cy-DS(19,sc), DS(2,sc), DS(1,sc), C_WHITE);

    /* === LAYER 10: red taillights flanking turbine === */
    frect(cx-DS(7,sc), cy+DS(7,sc),  DS(2,sc), DS(1,sc), C_RED);
    frect(cx+DS(5,sc), cy+DS(7,sc),  DS(2,sc), DS(1,sc), C_RED);
}

/* ================================================================
 *  ENEMY CARS — per-type distinct shapes  [V3]
 * ================================================================ */

/* Shared car base: body + roof + glass + lights */
static void car_base(int cx,int cy,int sc,
                     unsigned short body,unsigned short roof,unsigned short glass) {
    draw_shadow(cx,cy,EN_W,EN_H,sc);
    /* lower body */
    frect(cx-DS(6,sc), cy-DS(8,sc), DS(12,sc), DS(16,sc), body);
    /* upper roof (stacked, slightly narrower) */
    frect(cx-DS(4,sc), cy-DS(6,sc), DS(8,sc),  DS(8,sc),  roof);
    /* windshield glass */
    frect(cx-DS(3,sc), cy-DS(5,sc), DS(6,sc),  DS(3,sc),  glass);
    /* rear window */
    frect(cx-DS(3,sc), cy+DS(1,sc), DS(6,sc),  DS(2,sc),  glass);
    /* headlights */
    frect(cx-DS(5,sc), cy-DS(8,sc), DS(2,sc),  DS(1,sc),  C_WHITE);
    frect(cx+DS(3,sc), cy-DS(8,sc), DS(2,sc),  DS(1,sc),  C_WHITE);
    /* taillights */
    frect(cx-DS(5,sc), cy+DS(6,sc), DS(2,sc),  DS(1,sc),  C_RED);
    frect(cx+DS(3,sc), cy+DS(6,sc), DS(2,sc),  DS(1,sc),  C_RED);
}

static void draw_enemy(const Enemy *e) {
    if(!e->active || !e->activated) return;
    int cx=e->x, cy=e->y;
    int sc=depth_scale(cy);

    switch(e->type) {

        case E_SWAY: {
            /* GCPD Police cruiser — white body, blue stripe, siren */
            car_base(cx,cy,sc, C_WHITE, C_WHITE, 0x4208u);
            /* Blue POLICE stripe across mid */
            frect(cx-DS(6,sc), cy-DS(1,sc), DS(12,sc), DS(2,sc), C_BLUE);
            /* Siren bar on roof — alternates red/blue */
            unsigned short sl=(ctx.frame&8)?C_RED:C_BLUE;
            frect(cx-DS(3,sc), cy-DS(7,sc), DS(3,sc), DS(2,sc), sl);
            frect(cx+DS(0,sc), cy-DS(7,sc), DS(3,sc), DS(2,sc), (sl==C_RED?C_BLUE:C_RED));
            break;
        }

        case E_DIVE: {
            /* Villain muscle car — deep orange + black hood stripe */
            car_base(cx,cy,sc, C_ORANGE, 0xFC40u, 0x2104u);
            /* Hood racing stripe (black) */
            frect(cx-DS(1,sc), cy-DS(8,sc), DS(2,sc), DS(16,sc), C_BLACK);
            /* Exhaust pipes at rear */
            frect(cx-DS(6,sc), cy+DS(5,sc), DS(2,sc), DS(2,sc), C_DGREY);
            frect(cx+DS(4,sc), cy+DS(5,sc), DS(2,sc), DS(2,sc), C_DGREY);
            break;
        }

        case E_ASSAULT: {
            /* Fast motorbike — narrow magenta, two visible wheels */
            draw_shadow(cx,cy,EN_W-6,EN_H,sc);
            /* Bike frame */
            frect(cx-DS(3,sc), cy-DS(7,sc), DS(6,sc), DS(14,sc), C_MAGENTA);
            /* Fairing / windscreen */
            frect(cx-DS(2,sc), cy-DS(6,sc), DS(4,sc), DS(4,sc), 0x7800u);
            frect(cx-DS(1,sc), cy-DS(5,sc), DS(2,sc), DS(2,sc), 0x2104u);
            /* Front wheel */
            frect(cx-DS(3,sc), cy-DS(8,sc), DS(6,sc), DS(2,sc), C_BLACK);
            frect(cx-DS(3,sc), cy-DS(8,sc), DS(6,sc), DS(2,sc), C_MGREY);
            hline(cx-DS(3,sc), cy-DS(8,sc), DS(6,sc), C_BLACK);
            /* Rear wheel */
            frect(cx-DS(3,sc), cy+DS(5,sc), DS(6,sc), DS(2,sc), C_BLACK);
            /* Rider silhouette */
            frect(cx-DS(2,sc), cy-DS(4,sc), DS(4,sc), DS(5,sc), C_DGREY);
            break;
        }

        case E_PROJECTILE: {
            /* Boss rocket — elongated red missile */
            draw_shadow(cx,cy,5,MISS_H+4,sc);
            frect(cx-DS(2,sc), cy-DS(6,sc), DS(4,sc), DS(12,sc), C_RED);
            frect(cx-DS(1,sc), cy-DS(8,sc), DS(2,sc), DS(3,sc),  C_ORANGE);
            frect(cx-DS(1,sc), cy+DS(5,sc), DS(2,sc), DS(2,sc),  C_YELLOW);
            break;
        }
    }
}

/* ================================================================
 *  STUD, OBSTACLE, POWERUP, BOSS, ROCKET
 * ================================================================ */

static void draw_stud(const Stud *s) {
    if(!s->active||s->collected) return;
    int sc=depth_scale(s->y);
    int r=imax(DS(STD_R,sc),1);
    /* subtle floating glow */
    if(r>1 && (ctx.frame&4)) fdiamond(s->x, s->y, r+1, 0x8440u);
    fdiamond(s->x, s->y, r, C_STUD);
    pset(s->x-1, s->y-r+1, C_WHITE);
}

static void draw_obstacle(const Obstacle *o) {
    if(!o->active) return;
    int cx=o->x+o->w/2, cy=o->y+o->h/2;
    int sc=depth_scale(cy);
    draw_shadow(cx,cy,o->w,o->h,sc);
    /* Two stacked layers (top-half lighter = crate lid) */
    frect(cx-DS(o->w/2,sc), cy,                DS(o->w,sc), DS(o->h/2,sc), C_MGREY); /* bottom */
    frect(cx-DS(o->w/2,sc), cy-DS(o->h/2,sc), DS(o->w,sc), DS(o->h/2,sc), C_LGREY); /* top lid */
    drect(cx-DS(o->w/2,sc), cy-DS(o->h/2,sc), DS(o->w,sc), DS(o->h,sc),   C_DGREY);
    /* X danger mark */
    int bx=cx-DS(o->w/2,sc), by=cy-DS(o->h/2,sc);
    int sw=DS(o->w,sc), sh=DS(o->h,sc);
    for(int i=0;i<imin(sw,sh);i++){pset(bx+i,by+i,C_RED); pset(bx+sw-1-i,by+i,C_RED);}
}

static void draw_powerup(const Powerup *p) {
    if(!p->active) return;
    int sc=depth_scale(p->y);
    unsigned short col,hi;
    switch(p->type){
        case PW_MAGNET:  col=C_BLUE;   hi=C_CYAN;   break;
        case PW_BOOST:   col=C_ORANGE; hi=C_YELLOW; break;
        case PW_MISSILE: col=C_GREEN;  hi=C_WHITE;  break;
        default:         col=C_WHITE;  hi=C_YELLOW; break;
    }
    unsigned short c=(p->anim&8)?hi:col;
    /* floating bob */
    int fy=p->y + (isin((ctx.frame*4+p->x*2)&0xFF)>>5);
    draw_shadow(p->x,fy,PWR_W,PWR_H,sc);
    frect(p->x-DS(PWR_W/2,sc), fy-DS(PWR_H/2,sc), DS(PWR_W,sc), DS(PWR_H,sc), c);
    drect(p->x-DS(PWR_W/2,sc), fy-DS(PWR_H/2,sc), DS(PWR_W,sc), DS(PWR_H,sc), C_WHITE);
}

static void draw_boss(void) {
    if(!boss.active) return;
    unsigned short col;
    switch(boss.type){
        case BOSS_FREEZE:  col=C_CYAN;    break;
        case BOSS_RIDDLER: col=C_GREEN;   break;
        case BOSS_JOKER:   col=C_MAGENTA; break;
        default:           col=C_RED;     break;
    }
    int bx=boss.x-BOSS_W/2, by=boss.y-BOSS_H/2;
    draw_shadow(boss.x,boss.y,BOSS_W,BOSS_H,256);
    /* Armoured truck: lower hull */
    frect(bx+2, by+BOSS_H/2, BOSS_W-4, BOSS_H/2, col);
    /* Upper cab */
    frect(bx,   by,          BOSS_W,   BOSS_H/2,  col);
    /* Turret */
    frect(bx+BOSS_W/2-6, by+4, 12, 12, C_DGREY);
    frect(bx+BOSS_W/2-2, by,   4,  5,  C_MGREY);  /* gun */
    drect(bx,   by,   BOSS_W, BOSS_H,  C_WHITE);
    drect(bx+2, by+2, BOSS_W-4, BOSS_H-4, 0x4208u);
    frect(bx+8,         by+8, 6,5, C_BLACK); pset(bx+10,        by+10, C_RED);
    frect(bx+BOSS_W-14, by+8, 6,5, C_BLACK); pset(bx+BOSS_W-12, by+10, C_RED);
    /* HP bar */
    frect(bx, by-7, BOSS_W, 4, C_DGREY);
    frect(bx, by-7, (BOSS_W*boss.hp)/boss.max_hp, 4, C_RED);
    drect(bx, by-7, BOSS_W, 4, C_WHITE);
}

static void draw_rocket(void) {
    if(!rocket.active) return;
    int sc=depth_scale(rocket.y);
    draw_shadow(rocket.x,rocket.y,MISS_W+2,MISS_H,sc);
    frect(rocket.x-DS(MISS_W/2,sc), rocket.y-DS(MISS_H/2,sc),
          DS(MISS_W,sc), DS(MISS_H,sc), C_GREEN);
    frect(rocket.x-DS(1,sc), rocket.y-DS(MISS_H/2+2,sc), DS(2,sc), DS(2,sc), C_WHITE);
    if(ctx.frame&2)
        frect(rocket.x-DS(1,sc), rocket.y+DS(MISS_H/2,sc), DS(2,sc), DS(3,sc), C_ORANGE);
}

/* ================================================================
 *  VGA INIT + BLIT  [V3-KEY anti-flicker]
 * ================================================================ */
static volatile unsigned int *pbc = (volatile unsigned int *)PIX_CTRL_BASE;

static void vga_init(void) {
    pbc[1] = PIX_BUF1;
    /* Clear fb + hardware buffer */
    unsigned int *fi=(unsigned int *)fb;
    volatile unsigned int *vga=(volatile unsigned int *)PIX_BUF1;
    int n=(PIX_STRIDE*SCR_H)>>1;
    for(int i=0;i<n;i++){fi[i]=0; vga[i]=0;}
}

/*
 * blit_vga — copy finished frame from fb (RAM) to VGA hardware.
 * Uses 32-bit writes for maximum throughput.
 * Called ONCE per frame after all drawing is complete.
 */
static void blit_vga(void) {
    volatile unsigned int *dst=(volatile unsigned int *)PIX_BUF1;
    unsigned int *src=(unsigned int *)fb;
    int n=(PIX_STRIDE*SCR_H)>>1;
    for(int i=0;i<n;i++) dst[i]=src[i];
}

static void swap_buffers(void) { blit_vga(); }

/* ================================================================
 *  TIMER
 * ================================================================ */
static volatile unsigned int *tmr=(volatile unsigned int *)TIMER_BASE;
static void timer_init(void){tmr[2]=0xB4B3u;tmr[3]=0x000Cu;tmr[1]=0x0006u;}
static void timer_wait(void){while(!(tmr[0]&1u));tmr[0]=0;}

/* ================================================================
 *  PS/2 INPUT
 * ================================================================ */
static volatile unsigned int *ps2=(volatile unsigned int *)PS2_BASE;
static void read_ps2(void){
    static bool ext=false,rel=false;
    while(1){
        unsigned d=*ps2;
        if(!(d&0x8000u)) break;
        unsigned b=d&0xFF;
        if(b==0xE0){ext=true;continue;}
        if(b==0xF0){rel=true;continue;}
        bool pressed=!rel; rel=false;
        if(ext){switch(b){case 0x6B:k_left=pressed;break;case 0x74:k_right=pressed;break;case 0x75:k_up=pressed;break;}ext=false;}
        else  {ext=false;switch(b){case 0x5A:k_enter=pressed;break;case 0x76:k_esc=pressed;break;}}
    }
}

/* ================================================================
 *  CLEAR FB  [V3 "old screen cleanup"]
 *  Fill entire software FB with near-black sky.
 *  32-bit writes — clears 240×512 shorts in ~61K iterations.
 * ================================================================ */
static void clear_fb(void) {
    unsigned int *p=(unsigned int *)fb;
    unsigned int fill=0x00010001u;  /* two near-black pixels packed */
    int n=(PIX_STRIDE*SCR_H)>>1;
    for(int i=0;i<n;i++) p[i]=fill;
}

/* ================================================================
 *  BAT SIGNAL (sky feature)
 * ================================================================ */
static void draw_bat_signal(void) {
    int bx=264, by=28;
    /* Searchlight cone */
    for(int y=6;y<HORIZON_Y;y++){
        int hw=2+(y-6)*3/(HORIZON_Y-6);
        hline(bx-hw,y,hw*2, 0x0103u);
    }
    /* Glow rings */
    fdiamond(bx,by,13, 0x3180u);
    fdiamond(bx,by,11, 0x8C00u);
    fdiamond(bx,by,9,  C_YELLOW);
    /* Bat silhouette punched out */
    frect(bx-5,  by-3, 10, 8, 0x0001u);  /* body */
    frect(bx-10, by-1,  6, 4, 0x0001u);  /* left wing */
    frect(bx+4,  by-1,  6, 4, 0x0001u);  /* right wing */
    frect(bx-3,  by-6,  2, 4, 0x0001u);  /* left ear */
    frect(bx+1,  by-6,  2, 4, 0x0001u);  /* right ear */
}

/* ================================================================
 *  SKY GRADIENT
 * ================================================================ */
static void draw_sky(void) {
    for(int y=0;y<HORIZON_Y+6;y++){
        unsigned short sky;
        if(y<HORIZON_Y/2){
            int b=1+(y>>2); sky=(unsigned short)b;
        } else {
            int b=3+((y-HORIZON_Y/2)>>2);
            int g=1+((y-HORIZON_Y/2)>>3);
            sky=(unsigned short)((g<<5)|b);
        }
        hline(0,y,SCR_W,sky);
    }
    draw_bat_signal();
}

/* ================================================================
 *  PARALLAX GOTHAM SKYLINE  [V3]
 * ================================================================ */
static void draw_gotham_skyline(void) {

    /* --- FAR layer — very dark, tiny buildings, ×1/4 scroll --- */
    {
        int off=(ctx.scroll/4)%320;
        unsigned int s=0xCAFE1234u;
        for(int b=0;b<18;b++){
            s=s*1664525u+1013904223u;
            int bw=10+(int)((s>>28)&0xF)*2;
            int bh= 7+(int)((s>>24)&0xF);
            int bx=(int)((s>>8)&0xFF)%320;
            bx=((bx-off)%320+320)%320;
            int by=HORIZON_Y+5-bh;
            frect(bx,by,bw,bh,0x0841u);
            /* gargoyle tips */
            if((s>>4)&1){pset(bx+1,by-1,0x0841u);pset(bx+bw-2,by-1,0x0841u);}
            if((s>>3)&1) pset(bx+bw/2,by+2,((s>>2)&1)?0x0040u:0x0003u);
        }
    }

    /* --- MID layer — medium buildings with neon ×1/2 scroll --- */
    {
        int off=(ctx.scroll/2)%320;
        unsigned int s=0xDEADBEEFu;
        for(int b=0;b<14;b++){
            s=s*1664525u+1013904223u;
            int bw=14+(int)((s>>28)&0xF)*3;
            int bh=12+(int)((s>>22)&0x1F);
            int bx=(int)((s>>8)&0x1FF)%320;
            bx=((bx-off)%320+320)%320;
            int by=HORIZON_Y+5-bh;
            if(by<2)by=2;
            frect(bx,by,bw,bh,C_BLDG);
            drect(bx,by,bw,bh,0x31A6u);
            /* art-deco steps at top */
            frect(bx+2,by-2,bw-4,3,C_BLDG);
            frect(bx+4,by-4,bw-8,3,C_BLDG);
            /* neon sign strip */
            unsigned short nc=((s>>1)&1)?C_NEON_P:C_NEON_T;
            frect(bx+2,by+bh-4,bw-4,2,nc);
            /* windows */
            for(int wi=0;wi<(bw-4)/5&&wi<3;wi++)
                for(int wj=0;wj<2;wj++)
                    if((s>>(wi+wj*3))&1)
                        frect(bx+3+wi*5,by+4+wj*5,3,3,C_YELLOW);
        }
    }

    /* --- NEAR-RIGHT buildings — right sidewalk, ×3/4 scroll --- */
    {
        int off=(ctx.scroll*3/4)%200;
        unsigned int s=0xABCD1234u;
        for(int b=0;b<6;b++){
            s=s*1664525u+1013904223u;
            int bw=22+(int)((s>>28)&0xF)*4;
            int bh=28+(int)((s>>22)&0x1F)*2;
            int bx=ROAD_R+2+(int)((s>>8)&0x3F)%(SCR_W-ROAD_R-bw-2);
            if(bx<ROAD_R+2)bx=ROAD_R+2;
            if(bx+bw>SCR_W)bx=SCR_W-bw;
            int by=HORIZON_Y+5-bh; if(by<2)by=2;
            /* vertical scroll */
            int voff=((b*47+off*2)%(bh+40))-20;
            by+=voff;
            if(by<2)by=2;
            frect(bx,by,bw,bh,C_BLDG2); drect(bx,by,bw,bh,C_MGREY);
            /* large neon billboard */
            unsigned short nc=((b^(ctx.frame/60))&1)?C_NEON_P:C_NEON_T;
            frect(bx+3,by+3,bw-6,5,C_BLACK);
            for(int si=0;si<(bw-8)/4;si++) frect(bx+4+si*4,by+4,2,3,nc);
            /* water tower */
            if((s>>5)&1){frect(bx+bw/2-3,by-8,6,8,C_MGREY);fdiamond(bx+bw/2,by-9,3,C_DGREY);}
        }
    }

    /* --- NEAR-LEFT buildings — left sidewalk, ×3/4 scroll --- */
    {
        int off=(ctx.scroll*3/4)%200;
        unsigned int s=0x5A5ABEADu;
        for(int b=0;b<6;b++){
            s=s*1664525u+1013904223u;
            int bw=22+(int)((s>>28)&0xF)*4;
            int bh=28+(int)((s>>22)&0x1F)*2;
            int bx=(int)((s>>8)&0x3F)%(ROAD_L-bw-2);
            if(bx<0)bx=0;
            if(bx+bw>ROAD_L-2)bx=ROAD_L-bw-2;
            int by=HORIZON_Y+5-bh; if(by<2)by=2;
            int voff=((b*53+off*2+80)%(bh+40))-20;
            by+=voff; if(by<2)by=2;
            frect(bx,by,bw,bh,C_BLDG2); drect(bx,by,bw,bh,C_MGREY);
            unsigned short nc=((b^(ctx.frame/60+1))&1)?C_ORANGE:C_CYAN;
            frect(bx+3,by+3,bw-6,5,C_BLACK);
            for(int si=0;si<(bw-8)/4;si++) frect(bx+4+si*4,by+4,2,3,nc);
            if((s>>5)&1){frect(bx+bw/2-3,by-8,6,8,C_MGREY);fdiamond(bx+bw/2,by-9,3,C_DGREY);}
        }
    }
}

/* ================================================================
 *  PERSPECTIVE ROAD SURFACE
 * ================================================================ */
static void draw_road(void) {
    /* Sidewalks */
    frect(0,     HORIZON_Y+5, ROAD_L,         SCR_H-HORIZON_Y-5, C_SIDEWALK);
    frect(ROAD_R,HORIZON_Y+5, SCR_W-ROAD_R,   SCR_H-HORIZON_Y-5, C_SIDEWALK);

    /* Per-scan-line road — darker at horizon, lighter near player */
    for(int y=HORIZON_Y;y<SCR_H;y++){
        int rl=road_left(y), rr=road_right(y);
        int t=((y-HORIZON_Y)*256)/(SCR_H-HORIZON_Y);
        int rv=4+(t*10)/256;  /* 4..14 stepping grey */
        unsigned short rc=(unsigned short)((rv<<11)|(rv*2<<5)|rv);
        hline(rl,y,rr-rl,rc);
        /* edge lines */
        if(rl>0) pset(rl-1,y,C_WHITE);
        pset(rr,y,C_WHITE);
    }

    /* Dashed centre line — scrolls with road, perspective-foreshortened */
    {
        int period=28, dash=16;
        int off=ctx.scroll%period;
        for(int y=HORIZON_Y;y<SCR_H;y++){
            if(((y-HORIZON_Y+off)%period)<dash){
                pset(ROAD_CX-1,y,C_WHITE);
                pset(ROAD_CX,  y,C_WHITE);
            }
        }
    }

    /* Kerb rumble strips — red/white alternating, depth-scaled */
    {
        int off=ctx.scroll%12;
        for(int y=HORIZON_Y;y<SCR_H;y++){
            unsigned short kc=(((y+off)/6)&1)?C_RED:C_WHITE;
            int rl=road_left(y), rr=road_right(y);
            int sc=depth_scale(y);
            int kw=imax(DS(3,sc),1);
            frect(rl,y,kw,1,kc);
            frect(rr-kw,y,kw,1,kc);
        }
    }
}

/* ================================================================
 *  STREET LAMPS  [V3 — immersive Gotham]
 * ================================================================ */
static void draw_street_lamps(void) {
    int spacing=90;
    int off=ctx.scroll%spacing;
    for(int yi=-spacing;yi<SCR_H+spacing;yi+=spacing){
        int y=((yi+off)%spacing + yi/spacing*spacing + off);
        /* simpler: iterate a few positions */
        y=yi+off;
        if(y<HORIZON_Y||y>=SCR_H) continue;
        int sc =depth_scale(y);
        int ph =DS(20,sc);
        int arm=DS(12,sc);
        int rl =road_left(y);
        int rr =road_right(y);

        /* Left lamp */
        {
            int lx=rl-DS(10,sc);
            int ty=y-ph;
            if(ty<0)ty=0;
            vline(lx,ty,ph,C_LGREY);
            hline(lx,ty,arm,C_LGREY);
            fdiamond(lx+arm,ty,DS(3,sc),C_YELLOW);
            /* light pool on road (faint yellow glow) */
            if(sc>150)
                frect(lx+arm-DS(5,sc),ty+DS(2,sc),DS(10,sc),DS(5,sc),0x0840u);
        }
        /* Right lamp */
        {
            int rx=rr+DS(10,sc);
            int ty=y-ph;
            if(ty<0)ty=0;
            vline(rx,ty,ph,C_LGREY);
            hline(rx-arm,ty,arm,C_LGREY);
            fdiamond(rx-arm,ty,DS(3,sc),C_YELLOW);
            if(sc>150)
                frect(rx-arm-DS(5,sc),ty+DS(2,sc),DS(10,sc),DS(5,sc),0x0840u);
        }
    }
}

/* ================================================================
 *  MANHOLE COVERS on road surface
 * ================================================================ */
static void draw_manholes(void) {
    int spacing=160;
    int off=ctx.scroll%spacing;
    for(int yi=-spacing;yi<SCR_H+spacing;yi+=spacing){
        int y=yi+off+60;
        if(y<HORIZON_Y||y>=SCR_H) continue;
        int sc=depth_scale(y);
        int rl=road_left(y),rr=road_right(y);
        /* one left of centre, one right */
        int mx1=rl+(rr-rl)/3;
        int mx2=rl+2*(rr-rl)/3;
        fdiamond(mx1,y,DS(3,sc),C_MGREY);
        drect(mx1-DS(3,sc),y-DS(2,sc),DS(6,sc),DS(4,sc),C_DGREY);
        fdiamond(mx2,y,DS(3,sc),C_MGREY);
        drect(mx2-DS(3,sc),y-DS(2,sc),DS(6,sc),DS(4,sc),C_DGREY);
    }
}

/* ================================================================
 *  RAIN EFFECT
 * ================================================================ */
static void draw_rain(void) {
    for(int i=0;i<80;i++){
        int rx=(i*73+ctx.frame*2)%SCR_W;
        int ry=(i*53+ctx.frame*5)%SCR_H;
        unsigned short rc=(i&3)==0?0x7BDFu:C_RAIN;
        pset(rx,ry,rc);
        if(rx>0&&ry>0)  pset(rx-1,ry-1,rc);          /* upward streak head */
        if(rx>1&&ry<SCR_H-1) pset(rx-2,ry+1,0x4A69u); /* tail fade */
    }
}

/* ================================================================
 *  MASTER BACKGROUND DRAW
 * ================================================================ */
static void draw_background(void) {
    clear_fb();            /* [V3] clear entire fb — full old-screen cleanup */
    draw_sky();            /* sky gradient + bat signal                       */
    draw_gotham_skyline(); /* 4-layer parallax Gotham buildings               */
    draw_road();           /* per-scanline perspective road + kerbs           */
    draw_street_lamps();   /* Gotham lamp posts with light pools              */
    draw_manholes();       /* road surface detail                             */
    draw_rain();           /* diagonal rain particles                         */
}

/* ================================================================
 *  HUD
 * ================================================================ */
static void draw_hud(void) {
    frect(0,0,ROAD_L-1,SCR_H,0x1082u);
    drect(0,0,ROAD_L-1,SCR_H,0x2104u);
    for(int i=0;i<pl.hp;i++)       fdiamond(12,16+i*14,5,C_HEALTH);
    for(int i=pl.hp;i<PL_MAX_HP;i++) drect(7,11+i*14,10,10,C_DGREY);
    if(pl.magnet_t>0){
        frect(4,90,12,5,C_BLUE);
        frect(4,90,12*pl.magnet_t/MAGNET_FRAMES,5,C_CYAN);
        drect(4,90,12,5,C_LGREY);
    }
    if(pl.boost_t>0){
        frect(4,100,12,5,C_DGREY);
        frect(4,100,12*pl.boost_t/BOOST_FRAMES,5,C_ORANGE);
        drect(4,100,12,5,C_LGREY);
    }
    if(pl.missiles>0){
        frect(4,112,12,5,C_GREEN);
        draw_num(2,120,pl.missiles,C_WHITE);
    }
    frect(ROAD_R+1,0,SCR_W-ROAD_R-1,SCR_H,0x1082u);
    drect(ROAD_R+1,0,SCR_W-ROAD_R-1,SCR_H,0x2104u);
    draw_num(ROAD_R+4,8,pl.score,C_YELLOW);
    fdiamond(ROAD_R+8,28,4,C_STUD);
    draw_num(ROAD_R+4,36,pl.studs,C_YELLOW);
    if(boss.active){
        int bw=ROAD_W*boss.hp/boss.max_hp;
        frect(ROAD_L,0,ROAD_W,4,C_DGREY);
        frect(ROAD_L,0,bw,4,C_RED);
        drect(ROAD_L,0,ROAD_W,4,C_WHITE);
    }
}

/* ================================================================
 *  OVERLAY DIMMING  [V3 — reads fb, halves channels in-place]
 * ================================================================ */
static void dim_screen_half(void) {
    int n=PIX_STRIDE*SCR_H;
    for(int i=0;i<n;i++){
        unsigned short p=fb[i];
        fb[i]=(unsigned short)(((p>>1)&0x7800u)|((p>>1)&0x03E0u)|((p>>1)&0x000Fu));
    }
}

/* ================================================================
 *  MENU / PAUSE / GAMEOVER SCREENS
 * ================================================================ */
static void draw_menu(void) {
    clear_fb();
    /* Full sky */
    for(int y=0;y<SCR_H;y++){
        unsigned short sky;
        if(y<80) {int b=3+(y>>3);int g=y>>4;sky=(unsigned short)((g<<5)|b);}
        else if(y<160){int b=13+((y-80)>>4);int g=5+((y-80)>>4);sky=(unsigned short)((g<<5)|b);}
        else{int r=(y-160)>>3;int g=3+((y-160)>>4);sky=(unsigned short)((r<<11)|(g<<5)|8);}
        hline(0,y,SCR_W,sky);
    }
    draw_bat_signal();
    draw_rain();

    /* Gotham silhouette buildings top */
    for(int b=0;b<12;b++){
        int bx=b*27; int bh=18+(b*7)%28;
        frect(bx,68-bh,22,bh,C_BLDG);
        frect(bx+2,68-bh-3,4,4,C_BLDG);
        frect(bx+15,68-bh-3,4,4,C_BLDG);
    }

    /* Batman silhouette + cape wings */
    frect(110,48,20,32,C_DGREY); /* left cape */
    frect(190,48,20,32,C_DGREY); /* right cape */
    frect(128,30,64,52,C_DGREY);
    frect(140,18,12,18,C_DGREY);
    frect(168,18,12,18,C_DGREY);
    frect(148,42,24,22,0x2945u);
    frect(148,60,24,5,C_YELLOW);
    fdiamond(160,52,7,C_YELLOW);
    pset(153,52,C_DGREY); pset(167,52,C_DGREY);

    /* Title banner */
    frect(54,90,212,28,C_YELLOW);
    frect(56,92,208,24,C_BLACK);
    int tx=64; unsigned short tc=C_YELLOW;
    /* B */  frect(tx,94,4,20,tc);frect(tx+4,94,8,5,tc);frect(tx+4,102,8,5,tc);frect(tx+4,109,8,6,tc);tx+=16;
    /* A */  frect(tx,98,4,16,tc);frect(tx+8,98,4,16,tc);frect(tx+4,94,4,5,tc);frect(tx+4,103,4,4,tc);tx+=16;
    /* T */  frect(tx,94,18,5,tc);frect(tx+7,94,4,20,tc);tx+=20;
    /* M */  frect(tx,94,4,20,tc);frect(tx+12,94,4,20,tc);frect(tx+4,96,4,6,tc);frect(tx+8,96,4,6,tc);tx+=18;
    /* A */  frect(tx,98,4,16,tc);frect(tx+8,98,4,16,tc);frect(tx+4,94,4,5,tc);frect(tx+4,103,4,4,tc);tx+=16;
    /* N */  frect(tx,94,4,20,tc);frect(tx+12,94,4,20,tc);frect(tx+4,96,5,5,tc);frect(tx+8,102,5,5,tc);

    /* Subtitle */
    frect(88,122,144,10,C_CYAN);frect(90,123,140,8,C_BLACK);
    for(int i=0;i<9;i++) frect(92+i*14,125,10,4,C_CYAN);

    /* Controls icon row */
    frect(60,140,200,1,C_MGREY);
    frect(92,148,18,8,C_DGREY);  drect(92,148,18,8,C_LGREY);
    frect(114,148,18,8,C_DGREY); drect(114,148,18,8,C_LGREY);
    frect(136,148,18,8,C_DGREY); drect(136,148,18,8,C_LGREY);
    frect(170,148,34,8,C_DGREY); drect(170,148,34,8,C_LGREY);

    /* Blink prompt */
    unsigned short bc=(ctx.frame&32)?C_YELLOW:C_ORANGE;
    frect(78,162,164,16,bc);
    frect(80,164,160,12,C_BLACK);
    for(int i=0;i<6;i++) frect(88+i*24,167,18,6,bc);

    frect(60,182,200,1,C_MGREY);
    fdiamond(130,196,4,C_STUD);
    draw_num(140,192,pl.score,C_YELLOW);
}

static void draw_pause(void) {
    dim_screen_half();
    frect(80,90,160,60,C_DGREY);
    drect(80,90,160,60,C_YELLOW);
    frect(100,108,60,7,C_YELLOW);
    frect(100,122,120,7,C_MGREY);
}

static void draw_gameover(void) {
    dim_screen_half();
    frect(50,70,220,100,C_BLACK);
    drect(50,70,220,100,C_RED);
    frect(70,85,180,7,C_RED);
    draw_num(130,103,pl.score,C_YELLOW);
    draw_num(110,120,pl.studs,C_STUD);
    if(ctx.frame&32) frect(90,145,140,6,C_WHITE);
}

/* ================================================================
 *  SPAWN HELPERS
 * ================================================================ */
static void spawn_studs(int cx,int cy,int n){
    for(int i=0;i<MAX_STUDS&&n>0;i++)
        if(!studs[i].active){studs[i].active=true;studs[i].collected=false;
            studs[i].x=cx+irand(-26,26);studs[i].y=cy;n--;}
}
static void spawn_enemy(EType t){
    for(int i=0;i<MAX_ENEMIES;i++)
        if(!enemies[i].active){
            Enemy *e=&enemies[i];
            e->active=true;e->activated=false;e->diving=false;e->type=t;
            e->x=irand(ROAD_L+EN_W/2+4,ROAD_R-EN_W/2-4);
            e->y=-EN_H;e->init_x=e->x;e->vx=0;e->timer=0;
            switch(t){case E_SWAY:e->vy=1;e->timer=irand(0,255);break;
                       case E_DIVE:e->vy=1;e->timer=irand(60,140);break;
                       case E_ASSAULT:e->vy=2;break;
                       case E_PROJECTILE:e->vy=3;e->vx=irand(-2,2);break;}
            return;
        }
}
static void spawn_proj(int x,int y){
    for(int i=0;i<MAX_ENEMIES;i++)
        if(!enemies[i].active){
            enemies[i].active=true;enemies[i].activated=true;
            enemies[i].type=E_PROJECTILE;
            enemies[i].x=x;enemies[i].y=y;
            enemies[i].vx=irand(-2,2);enemies[i].vy=3;enemies[i].timer=0;return;
        }
}
static void spawn_obstacle(void){
    for(int i=0;i<MAX_OBSTACLES;i++)
        if(!obs[i].active){obs[i].active=true;
            obs[i].x=irand(ROAD_L+4,ROAD_R-OBS_W-4);
            obs[i].y=-OBS_H;obs[i].w=OBS_W;obs[i].h=OBS_H;return;}
}
static void spawn_powerup(void){
    for(int i=0;i<MAX_POWERUPS;i++)
        if(!pwrs[i].active){pwrs[i].active=true;
            pwrs[i].x=irand(ROAD_L+PWR_W,ROAD_R-PWR_W);
            pwrs[i].y=-PWR_H;pwrs[i].type=(PwType)irand(0,2);pwrs[i].anim=0;return;}
}

/* ================================================================
 *  GAME INIT
 * ================================================================ */
static void game_init(void){
    pl.x=ROAD_CX;pl.y=SCR_H-38;
    pl.vx=0;pl.hp=PL_MAX_HP;pl.invuln=0;
    pl.score=0;pl.studs=0;pl.alive=true;
    pl.boosting=false;pl.boost_t=0;pl.magnet_t=0;pl.missiles=0;
    for(int i=0;i<MAX_ENEMIES;  i++) enemies[i].active=false;
    for(int i=0;i<MAX_STUDS;    i++) studs[i].active=false;
    for(int i=0;i<MAX_OBSTACLES;i++) obs[i].active=false;
    for(int i=0;i<MAX_POWERUPS; i++) pwrs[i].active=false;
    rocket.active=false;boss.active=false;
    ctx.scroll=0;ctx.scroll_spd=SCROLL_BASE;
    ctx.frame=0;ctx.spawn_t=SPAWN_PERIOD;ctx.boss_t=BOSS_PERIOD;
    ctx.bosses=0;ctx.boss_active=false;
}

/* ================================================================
 *  GAME UPDATE FUNCTIONS
 * ================================================================ */
static bool aabb(int ax,int ay,int aw,int ah,int bx,int by,int bw,int bh){
    return !(ax+aw<=bx||bx+bw<=ax||ay+ah<=by||by+bh<=ay);
}
static void update_player(void){
    if(!pl.alive)return;
    int spd=pl.boosting?PL_BASE_SPD+2:PL_BASE_SPD;
    if(k_left) pl.vx-=1; else if(k_right) pl.vx+=1; else pl.vx=pl.vx*3/4;
    pl.vx=iclamp(pl.vx,-spd*2,spd*2);
    pl.x+=pl.vx;
    pl.x=iclamp(pl.x,ROAD_L+PL_W/2+1,ROAD_R-PL_W/2-1);
    if(pl.invuln>0)pl.invuln--;
    if(pl.boost_t>0){pl.boost_t--;if(!pl.boost_t){pl.boosting=false;ctx.scroll_spd=SCROLL_BASE;}}
    if(pl.magnet_t>0)pl.magnet_t--;
    if(ctx.frame%20==0)pl.score++;
}
static void fire_rocket(void){
    if(pl.missiles>0&&!rocket.active){
        rocket.active=true;rocket.x=pl.x;rocket.y=pl.y-PL_H/2;
        rocket.vx=0;rocket.vy=-4;rocket.life=150;pl.missiles--;
    }
}
static void update_rocket(void){
    if(!rocket.active)return;
    if(--rocket.life<=0){rocket.active=false;return;}
    int tx=-1,ty=-1;
    if(boss.active){tx=boss.x;ty=boss.y;}
    else{int best=99999;for(int i=0;i<MAX_ENEMIES;i++){if(!enemies[i].active)continue;
        int d=iabs(enemies[i].x-rocket.x)+iabs(enemies[i].y-rocket.y);
        if(d<best){best=d;tx=enemies[i].x;ty=enemies[i].y;}}}
    if(tx>=0){int dx=tx-rocket.x,dy=ty-rocket.y;
        int d=imax(iabs(dx)+iabs(dy),1);
        rocket.vx=iclamp(rocket.vx+dx*3/d,-5,5);
        rocket.vy=iclamp(rocket.vy+dy*3/d,-7,3);}
    rocket.x+=rocket.vx;rocket.y+=rocket.vy;
    if(boss.active&&iabs(rocket.x-boss.x)<BOSS_W/2&&iabs(rocket.y-boss.y)<BOSS_H/2){
        rocket.active=false;boss.hp--;pl.score+=10;
        if(boss.hp<=0){spawn_studs(boss.x,boss.y,12);boss.active=false;
            ctx.boss_active=false;ctx.boss_t=BOSS_PERIOD;ctx.bosses++;pl.score+=100;}
        return;}
    for(int i=0;i<MAX_ENEMIES;i++){if(!enemies[i].active)continue;
        if(iabs(rocket.x-enemies[i].x)<EN_W&&iabs(rocket.y-enemies[i].y)<EN_H){
            enemies[i].active=false;rocket.active=false;
            pl.score+=5;spawn_studs(enemies[i].x,enemies[i].y,3);return;}}
    if(rocket.y<-10||rocket.x<ROAD_L||rocket.x>ROAD_R)rocket.active=false;
}
static void update_enemies(void){
    for(int i=0;i<MAX_ENEMIES;i++){
        Enemy *e=&enemies[i];if(!e->active)continue;
        e->y+=ctx.scroll_spd;
        if(!e->activated&&e->y>=0)e->activated=true;
        if(e->activated){switch(e->type){
            case E_SWAY: e->timer++;
                e->x=e->init_x+(isin(e->timer*2&0xFF)*35)/256;
                e->x=iclamp(e->x,ROAD_L+EN_W/2+2,ROAD_R-EN_W/2-2);break;
            case E_DIVE:
                if(!e->diving){if(--e->timer<=0){e->diving=true;
                    int dx=pl.x-e->x,dy=pl.y-e->y;
                    int d=imax(iabs(dx)+iabs(dy),1);e->vx=dx*3/(d/10+1);}}
                else{e->x+=e->vx;e->y+=1;}
                e->x=iclamp(e->x,ROAD_L+EN_W/2,ROAD_R-EN_W/2);break;
            case E_ASSAULT: e->y+=1;break;
            case E_PROJECTILE: e->x+=e->vx;
                if(e->x<ROAD_L||e->x>ROAD_R)e->vx=-e->vx;break;}}
        if(e->y>SCR_H+EN_H)e->active=false;
    }
}
static void update_studs(void){
    for(int i=0;i<MAX_STUDS;i++){Stud *s=&studs[i];if(!s->active)continue;
        s->y+=ctx.scroll_spd;
        if(pl.magnet_t>0){int dx=pl.x-s->x,dy=pl.y-s->y;
            if(iabs(dx)+iabs(dy)<80){s->x+=dx/4;s->y+=dy/4;}}
        if(!s->collected&&iabs(s->x-pl.x)<(pl.magnet_t>0?14:9)&&iabs(s->y-pl.y)<(pl.magnet_t>0?14:9))
            {s->collected=true;pl.studs++;pl.score++;}
        if(s->y>SCR_H+8||s->collected)s->active=false;
    }
}
static void update_obstacles(void){
    for(int i=0;i<MAX_OBSTACLES;i++){Obstacle *o=&obs[i];if(!o->active)continue;
        o->y+=ctx.scroll_spd;
        if(pl.invuln<=0&&pl.alive)
            if(aabb(pl.x-PL_W/2,pl.y-PL_H/2,PL_W,PL_H,o->x,o->y,o->w,o->h))
                {pl.hp--;pl.invuln=INVULN_FRAMES;if(pl.hp<=0){pl.alive=false;ctx.state=GAMEOVER;}}
        if(o->y>SCR_H+OBS_H)o->active=false;}
}
static void update_powerups(void){
    for(int i=0;i<MAX_POWERUPS;i++){Powerup *p=&pwrs[i];if(!p->active)continue;
        p->y+=ctx.scroll_spd;p->anim++;
        if(iabs(p->x-pl.x)<PWR_W&&iabs(p->y-pl.y)<PWR_H){
            switch(p->type){case PW_MAGNET:pl.magnet_t=MAGNET_FRAMES;break;
                case PW_BOOST:pl.boosting=true;pl.boost_t=BOOST_FRAMES;ctx.scroll_spd=SCROLL_BASE*2;break;
                case PW_MISSILE:if(pl.missiles<5)pl.missiles++;break;}
            p->active=false;}
        if(p->y>SCR_H+PWR_H)p->active=false;}
}
static void update_boss(void){
    if(!boss.active)return;boss.timer++;
    if(!boss.intro){if(boss.y<45)boss.y+=2;else boss.intro=true;return;}
    boss.x=ROAD_CX+(isin(boss.timer&0xFF)*40)/256;
    int period=(boss.hp<boss.max_hp/2)?25:50;
    if(boss.timer%period==0)spawn_proj(boss.x,boss.y+BOSS_H/2+2);
    if(boss.hp<=boss.max_hp/4&&boss.timer%80==0)
        {spawn_proj(boss.x-20,boss.y+BOSS_H/2);spawn_proj(boss.x+20,boss.y+BOSS_H/2);}
}
static void start_boss(void){
    ctx.boss_active=true;boss.active=true;
    boss.hp=BOSS_BASE_HP+ctx.bosses*2;boss.max_hp=boss.hp;
    boss.x=ROAD_CX;boss.y=-BOSS_H;
    boss.type=(BossType)(ctx.bosses%3);boss.timer=0;boss.intro=false;boss.atk_timer=0;
    for(int i=0;i<MAX_ENEMIES;  i++)enemies[i].active=false;
    for(int i=0;i<MAX_OBSTACLES;i++)obs[i].active=false;
}
static void check_player_hits(void){
    if(pl.invuln>0||!pl.alive)return;
    int px=pl.x-PL_W/2,py=pl.y-PL_H/2;
    for(int i=0;i<MAX_ENEMIES;i++){Enemy *e=&enemies[i];
        if(!e->active||!e->activated)continue;
        if(aabb(px,py,PL_W,PL_H,e->x-EN_W/2,e->y-EN_H/2,EN_W,EN_H))
            {pl.hp--;pl.invuln=INVULN_FRAMES;e->active=false;
             if(pl.hp<=0){pl.alive=false;ctx.state=GAMEOVER;}return;}}
    if(boss.active&&boss.intro)
        if(aabb(px,py,PL_W,PL_H,boss.x-BOSS_W/2,boss.y-BOSS_H/2,BOSS_W,BOSS_H))
            {pl.hp--;pl.invuln=INVULN_FRAMES;
             if(pl.hp<=0){pl.alive=false;ctx.state=GAMEOVER;}}
}
static void handle_spawning(void){
    ctx.scroll=(ctx.scroll+ctx.scroll_spd)%512;
    if(--ctx.spawn_t<=0){
        ctx.spawn_t=imax(SPAWN_PERIOD-ctx.bosses*8,40);
        if(!ctx.boss_active){
            int r=irand(0,99);
            if(r<40)spawn_enemy(E_SWAY);
            else if(r<70||ctx.bosses>=1)spawn_enemy(E_DIVE);
            else spawn_enemy(E_ASSAULT);
            if(irand(0,2)==0)spawn_studs(irand(ROAD_L+20,ROAD_R-20),-8,irand(3,8));
            if(irand(0,3)==0)spawn_obstacle();
            if(irand(0,5)==0)spawn_powerup();
        }
    }
    if(!ctx.boss_active&&--ctx.boss_t<=0)start_boss();
}

/* ================================================================
 *  TOP-LEVEL UPDATE + RENDER
 * ================================================================ */
static void update(void){
    ctx.frame++;read_ps2();
    switch(ctx.state){
        case MENU:
            if(k_enter){game_init();ctx.state=PLAYING;k_enter=false;}
            break;
        case PLAYING:
            update_player();handle_spawning();update_enemies();
            update_studs();update_obstacles();update_powerups();
            update_boss();update_rocket();check_player_hits();
            if(k_up&&!k_up_prev)fire_rocket();
            k_up_prev=k_up;
            if(k_esc){ctx.state=PAUSED;k_esc=false;}
            break;
        case PAUSED:
            if(k_esc||k_enter){ctx.state=PLAYING;k_esc=k_enter=false;}
            break;
        case GAMEOVER:
            if(k_enter){ctx.state=MENU;k_enter=false;}
            break;
    }
}

static void render(void){
    switch(ctx.state){
        case MENU:
            draw_menu();
            break;
        case PLAYING:
        case PAUSED:
            /* draw_background() calls clear_fb() internally — full repaint */
            draw_background();
            /* Back-to-front draw order for pseudo-3D depth */
            for(int i=0;i<MAX_STUDS;    i++) draw_stud(&studs[i]);
            for(int i=0;i<MAX_OBSTACLES;i++) draw_obstacle(&obs[i]);
            for(int i=0;i<MAX_POWERUPS; i++) draw_powerup(&pwrs[i]);
            for(int i=0;i<MAX_ENEMIES;  i++) draw_enemy(&enemies[i]);
            draw_boss();
            draw_rocket();
            draw_player();
            draw_hud();
            if(ctx.state==PAUSED) draw_pause();
            break;
        case GAMEOVER:
            draw_background();
            for(int i=0;i<MAX_ENEMIES;i++) draw_enemy(&enemies[i]);
            draw_player();
            draw_hud();
            draw_gameover();
            break;
    }
    /* [V3-KEY] Single blit: completed fb → VGA hardware. Zero tearing. */
    blit_vga();
}

/* ================================================================
 *  ENTRY POINT
 * ================================================================ */
int main(void){
    vga_init();
    timer_init();
    ctx.state=MENU;ctx.frame=0;
    k_left=k_right=k_up=k_enter=k_esc=false;k_up_prev=false;

    while(1){
        timer_wait();   /* ~60 fps pacing (no blocking vsync needed) */
        update();       /* input + game state                        */
        render();       /* paint fb, then single-blit to VGA         */
    }
    return 0;
}
