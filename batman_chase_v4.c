/*
 * batman_chase_v5 (replaces batman_chase_v4.c)
 * Pseudo-3D retro racer for DE1-SoC / CPUlator
 *
 * Core features:
 * - Segment-based road model (distance z, curve, width)
 * - Perspective projection (world x,z -> screen x,y)
 * - Depth scaling for all world objects
 * - True sprite stacking for player + enemies
 * - 4-lane system with hard boundary clamping
 * - Smooth acceleration/brake/drag/steer/boost physics
 */

#include <stdbool.h>
#include <stdint.h>

/* ================================================================
 * Hardware
 * ================================================================ */
#define PIX_CTRL_BASE   0xFF203020u
#define PIX_BUF1        0xC8000000u
#define PS2_BASE        0xFF200100u
#define TIMER_BASE      0xFF202000u

/* ================================================================
 * Display
 * ================================================================ */
#define SCR_W       320
#define SCR_H       240
#define PIX_STRIDE  512
#define SCR_CX      (SCR_W / 2)

/* ================================================================
 * Fixed-point
 * ================================================================ */
#define FP          1024
#define TO_FP(x)    ((x) * FP)
#define FROM_FP(x)  ((x) / FP)

/* ================================================================
 * Projection and camera
 * ================================================================ */
#define HORIZON_Y       62
#define CAMERA_HEIGHT   860
#define CAMERA_DEPTH    168
#define PROJ_K          (CAMERA_HEIGHT * CAMERA_DEPTH)

/* ================================================================
 * Road / track
 * ================================================================ */
#define ROAD_SEG_COUNT      320
#define ROAD_SEG_LEN        220
#define TRACK_LEN           (ROAD_SEG_COUNT * ROAD_SEG_LEN)
#define LANE_COUNT          4
#define ROAD_EDGE_MARGIN    150

/* ================================================================
 * Gameplay
 * ================================================================ */
#define MAX_ENEMIES         18
#define SPAWN_COOLDOWN_MIN  18
#define SPAWN_COOLDOWN_MAX  48
#define ENEMY_SPAWN_Z       TO_FP(6600)
#define ENEMY_DESPAWN_Z     TO_FP(-320)
#define COLLISION_Z_WINDOW  TO_FP(220)
#define COLLISION_X_WINDOW  210

/* ================================================================
 * Player physics
 * ================================================================ */
#define MAX_SPEED_FP        TO_FP(28)
#define MAX_REVERSE_FP      TO_FP(10)
#define ACCEL_FP            390
#define BRAKE_FP            620
#define REVERSE_ACCEL_FP    260
#define DRAG_FP             150

#define BOOST_SPEED_FP      TO_FP(39)
#define BOOST_ACCEL_FP      700
#define BOOST_FRAMES        110

#define STEER_ACCEL_FP      270
#define STEER_DAMP_NUM      12
#define STEER_DAMP_DEN      16
#define STEER_MAX_FP        TO_FP(8)

/* ================================================================
 * Render constants
 * ================================================================ */
#define PLAYER_SCREEN_Y     206
#define PLAYER_BASE_W       68
#define PLAYER_BASE_H       58
#define ENEMY_BASE_W        56
#define ENEMY_BASE_H        48

/* ================================================================
 * Colors (RGB565)
 * ================================================================ */
#define C_BLACK         0x0000u
#define C_WHITE         0xFFFFu
#define C_DARK          0x18A3u
#define C_DARK2         0x1082u
#define C_GREY          0x6B4Du
#define C_LGREY         0xBDF7u
#define C_RED           0xF800u
#define C_ORANGE        0xFD20u
#define C_YELLOW        0xFFE0u
#define C_CYAN          0x07FFu
#define C_BLUE          0x001Fu
#define C_GREEN         0x07E0u
#define C_NEON          0x07F9u
#define C_MAGENTA       0xF81Fu

#define C_SKY_TOP       0x0002u
#define C_SKY_MID       0x0826u
#define C_HORIZON_GLOW  0x18A8u

#define C_ROAD_FAR      0x2124u
#define C_ROAD_NEAR     0x31A6u
#define C_GRASS_FAR     0x01C0u
#define C_GRASS_NEAR    0x02A0u
#define C_RUMBLE_A      0xF800u
#define C_RUMBLE_B      0xFFFFu
#define C_LANE          0xEF5Du

#define C_BAT_BODY      0x0841u
#define C_BAT_MID       0x1082u
#define C_BAT_TOP       0x2104u
#define C_BAT_GLASS     0x0216u
#define C_BAT_ACCENT    0xFFE0u
#define C_BAT_LIGHT     0xFFFFu

#define C_ENEMY_0       0xF980u
#define C_ENEMY_1       0x07FFu
#define C_ENEMY_2       0xF81Fu
#define C_ENEMY_3       0x7BEFu

/* ================================================================
 * Types
 * ================================================================ */
typedef struct {
    int curve;
    int width;
    int rumble;
} RoadSegment;

typedef struct {
    int x_fp;
    int steer_fp;
    int speed_fp;
    int boost_timer;
    int hp;
    int score;
} Player;

typedef struct {
    bool active;
    int lane;
    int lane_offset;
    int z_fp;
    int speed_fp;
    int type;
} Enemy;

typedef enum {
    STATE_MENU,
    STATE_PLAY,
    STATE_PAUSE,
    STATE_GAMEOVER
} GameState;

typedef struct {
    GameState state;
    int frame;
    int camera_z_fp;
    int track_lap;
    int spawn_cooldown;
    bool crashed_flash;
} Game;

/* ================================================================
 * Globals
 * ================================================================ */
static volatile uint32_t *const g_ps2 = (volatile uint32_t *)PS2_BASE;
static volatile uint32_t *const g_tmr = (volatile uint32_t *)TIMER_BASE;
static volatile uint32_t *const g_pbc = (volatile uint32_t *)PIX_CTRL_BASE;

static uint16_t fb[SCR_H * PIX_STRIDE];

static RoadSegment g_road[ROAD_SEG_COUNT];
static int g_curve_prefix[ROAD_SEG_COUNT + 1];

static Player g_player;
static Enemy g_enemies[MAX_ENEMIES];
static Game g_game;

static bool k_up, k_down, k_left, k_right, k_shift;
static bool k_enter, k_esc;

static uint32_t rng_state = 0x93A123F1u;

/* ================================================================
 * Utility
 * ================================================================ */
static int iabs(int v) { return v < 0 ? -v : v; }

static int iclamp(int v, int lo, int hi) {
    return (v < lo) ? lo : ((v > hi) ? hi : v);
}

static int imax(int a, int b) { return a > b ? a : b; }

static uint32_t urand(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

static int irand(int lo, int hi) {
    if (hi <= lo) return lo;
    return lo + (int)(urand() % (uint32_t)(hi - lo + 1));
}

static uint16_t shade565(uint16_t c, int shade /* 0..255 */) {
    int r = (c >> 11) & 0x1F;
    int g = (c >> 5) & 0x3F;
    int b = c & 0x1F;

    r = (r * shade) >> 8;
    g = (g * shade) >> 8;
    b = (b * shade) >> 8;

    if (r > 31) r = 31;
    if (g > 63) g = 63;
    if (b > 31) b = 31;

    return (uint16_t)((r << 11) | (g << 5) | b);
}

/* ================================================================
 * Framebuffer primitives
 * ================================================================ */
static inline void pset(int x, int y, uint16_t c) {
    if ((unsigned)x < SCR_W && (unsigned)y < SCR_H)
        fb[y * PIX_STRIDE + x] = c;
}

static void hline(int x, int y, int len, uint16_t c) {
    if ((unsigned)y >= SCR_H || len <= 0) return;
    int x0 = x;
    int x1 = x + len - 1;
    if (x1 < 0 || x0 >= SCR_W) return;
    if (x0 < 0) x0 = 0;
    if (x1 >= SCR_W) x1 = SCR_W - 1;
    for (int i = x0; i <= x1; ++i) fb[y * PIX_STRIDE + i] = c;
}

static void frect(int x, int y, int w, int h, uint16_t c) {
    if (w <= 0 || h <= 0) return;
    int x0 = x;
    int y0 = y;
    int x1 = x + w;
    int y1 = y + h;
    if (x1 <= 0 || y1 <= 0 || x0 >= SCR_W || y0 >= SCR_H) return;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > SCR_W) x1 = SCR_W;
    if (y1 > SCR_H) y1 = SCR_H;

    for (int yy = y0; yy < y1; ++yy)
        for (int xx = x0; xx < x1; ++xx)
            fb[yy * PIX_STRIDE + xx] = c;
}

static void drect(int x, int y, int w, int h, uint16_t c) {
    if (w <= 1 || h <= 1) return;
    hline(x, y, w, c);
    hline(x, y + h - 1, w, c);
    for (int yy = y; yy < y + h; ++yy) {
        pset(x, yy, c);
        pset(x + w - 1, yy, c);
    }
}

static void clearFrame(uint16_t c) {
    uint32_t packed = ((uint32_t)c << 16) | c;
    uint32_t *ptr = (uint32_t *)fb;
    int count = (PIX_STRIDE * SCR_H) >> 1;
    for (int i = 0; i < count; ++i) ptr[i] = packed;
}

/* ================================================================
 * Hardware IO
 * ================================================================ */
static void blitVGA(void) {
    volatile uint32_t *dst = (volatile uint32_t *)PIX_BUF1;
    uint32_t *src = (uint32_t *)fb;
    int count = (PIX_STRIDE * SCR_H) >> 1;
    for (int i = 0; i < count; ++i) dst[i] = src[i];
}

static void initVGA(void) {
    g_pbc[1] = PIX_BUF1;
    clearFrame(0);
    blitVGA();
}

static void initTimer(void) {
    g_tmr[2] = 0xB4B3u;
    g_tmr[3] = 0x000Cu;
    g_tmr[1] = 0x0006u;
}

static void waitFrame(void) {
    while ((g_tmr[0] & 1u) == 0u) {}
    g_tmr[0] = 0;
}

/* ================================================================
 * Road sampling
 * ================================================================ */
static int wrap_track(int z) {
    int m = z % TRACK_LEN;
    if (m < 0) m += TRACK_LEN;
    return m;
}

static int road_index_for_world_z(int world_z) {
    return wrap_track(world_z) / ROAD_SEG_LEN;
}

static int road_local_t_fp(int world_z) {
    int wz = wrap_track(world_z);
    int local = wz % ROAD_SEG_LEN;
    return (local * FP) / ROAD_SEG_LEN;
}

static void sample_road(int world_z, int *center_world_x, int *half_width, int *rumble) {
    int idx = road_index_for_world_z(world_z);
    int t_fp = road_local_t_fp(world_z);

    RoadSegment *s0 = &g_road[idx];
    RoadSegment *s1 = &g_road[(idx + 1) % ROAD_SEG_COUNT];

    int cx0 = g_curve_prefix[idx];
    int cx1 = g_curve_prefix[idx + 1];

    int center = cx0 + (((cx1 - cx0) * t_fp) / FP);
    int width = s0->width + (((s1->width - s0->width) * t_fp) / FP);

    *center_world_x = center;
    *half_width = width;
    *rumble = s0->rumble;
}

static int lane_center_x(int lane, int half_width) {
    int road_full = half_width * 2;
    int lane_w = road_full / LANE_COUNT;
    int left_edge = -half_width;
    return left_edge + lane_w * lane + lane_w / 2;
}

/* ================================================================
 * Input
 * ================================================================ */
void handleInput(void) {
    static bool ext = false;
    static bool release = false;

    while (1) {
        uint32_t d = *g_ps2;
        if ((d & 0x8000u) == 0u) break;

        uint8_t b = (uint8_t)(d & 0xFFu);
        if (b == 0xE0u) { ext = true; continue; }
        if (b == 0xF0u) { release = true; continue; }

        bool pressed = !release;
        release = false;

        if (ext) {
            if (b == 0x75u) k_up = pressed;
            else if (b == 0x72u) k_down = pressed;
            else if (b == 0x6Bu) k_left = pressed;
            else if (b == 0x74u) k_right = pressed;
            ext = false;
            continue;
        }

        if (b == 0x1Du) k_up = pressed;       /* W */
        else if (b == 0x1Bu) k_down = pressed;/* S */
        else if (b == 0x1Cu) k_left = pressed;/* A */
        else if (b == 0x23u) k_right = pressed;/* D */
        else if (b == 0x12u || b == 0x59u) k_shift = pressed; /* Shift */
        else if (b == 0x5Au) k_enter = pressed; /* Enter */
        else if (b == 0x76u) k_esc = pressed; /* Esc */
    }
}

/* ================================================================
 * Render helpers
 * ================================================================ */
static int project_y_from_z(int z_world) {
    if (z_world < 1) z_world = 1;
    return HORIZON_Y + (PROJ_K / z_world);
}

static int project_x_from_world(int world_x, int z_world) {
    if (z_world < 1) z_world = 1;
    return SCR_CX + ((world_x * CAMERA_DEPTH) / z_world);
}

static int depth_shade_from_z(int z_world) {
    int shade = 255 - (z_world / 32);
    return iclamp(shade, 80, 255);
}

static void drawSkyAndBackground(void) {
    for (int y = 0; y <= HORIZON_Y + 2; ++y) {
        int t = (y * 255) / (HORIZON_Y + 2);
        uint16_t c = shade565(C_SKY_TOP, 255 - t / 2);
        if (y > HORIZON_Y / 2) c = shade565(C_SKY_MID, 170 + t / 3);
        hline(0, y, SCR_W, c);
    }

    hline(0, HORIZON_Y, SCR_W, C_HORIZON_GLOW);
    hline(0, HORIZON_Y + 1, SCR_W, shade565(C_HORIZON_GLOW, 180));

    /* skyline silhouettes */
    int scroll = FROM_FP(g_game.camera_z_fp / 6) % 64;
    for (int i = 0; i < 18; ++i) {
        int x = (i * 22 - scroll + 320) % 320;
        int h = 10 + ((i * 17 + g_game.frame / 3) % 28);
        frect(x, HORIZON_Y - h, 16, h, shade565(C_DARK2, 180));
        if ((i + (g_game.frame >> 3)) & 1) {
            frect(x + 5, HORIZON_Y - h + 4, 3, 3, C_NEON);
        }
    }
}

static void drawRoadPerspective(void) {
    for (int y = HORIZON_Y + 2; y < SCR_H; ++y) {
        int dy = y - HORIZON_Y;
        if (dy <= 0) continue;

        int z_view = PROJ_K / dy;
        int world_z = FROM_FP(g_game.camera_z_fp) + z_view;

        int center_w = 0;
        int half_w = 0;
        int rumble_phase = 0;
        sample_road(world_z, &center_w, &half_w, &rumble_phase);

        int center_x = project_x_from_world(center_w - FROM_FP(g_player.x_fp), z_view);
        int road_half_px = (half_w * CAMERA_DEPTH) / z_view;
        road_half_px = imax(road_half_px, 2);

        int grass_shade = iclamp(70 + dy, 70, 220);
        int road_shade = iclamp(90 + dy, 90, 230);

        uint16_t grass = shade565((dy < 95) ? C_GRASS_FAR : C_GRASS_NEAR, grass_shade);
        uint16_t road = shade565((dy < 120) ? C_ROAD_FAR : C_ROAD_NEAR, road_shade);

        int left = center_x - road_half_px;
        int right = center_x + road_half_px;
        int curb_w = imax(road_half_px / 10, 1);

        hline(0, y, SCR_W, grass);

        int curb_left_x = left;
        int curb_right_x = right - curb_w;

        int stripe_tick = ((world_z / 160) + rumble_phase) & 1;
        uint16_t rumble_col = stripe_tick ? C_RUMBLE_A : C_RUMBLE_B;

        hline(curb_left_x, y, curb_w, rumble_col);
        hline(left + curb_w, y, (right - curb_w) - (left + curb_w), road);
        hline(curb_right_x, y, curb_w, rumble_col);

        for (int lane = 1; lane < LANE_COUNT; ++lane) {
            int lane_wx = lane_center_x(lane, half_w) - lane_center_x(0, half_w);
            int lane_x = project_x_from_world(center_w + lane_wx - FROM_FP(g_player.x_fp), z_view);
            int dash = ((world_z / 220) + lane) & 1;
            if (dash || dy > 120) {
                pset(lane_x, y, C_LANE);
                if (dy > 120) pset(lane_x + 1, y, shade565(C_LANE, 180));
            }
        }
    }
}

static void drawVehicleStack(int cx, int base_y, int w, int h,
                             uint16_t body0, uint16_t body1, uint16_t roof,
                             uint16_t glass, uint16_t accent,
                             int layers, int depth_shade) {
    if (w < 2 || h < 2) return;

    uint16_t b0 = shade565(body0, depth_shade);
    uint16_t b1 = shade565(body1, depth_shade);
    uint16_t rt = shade565(roof, depth_shade);
    uint16_t gl = shade565(glass, depth_shade);
    uint16_t ac = shade565(accent, depth_shade);

    int layer_h = imax(h / (layers + 2), 1);

    for (int i = 0; i < layers; ++i) {
        int wy = base_y - i * layer_h;
        int taper = (i * imax(w / (layers * 3), 1));
        int lw = imax(w - taper, 2);
        int lh = imax(layer_h + (i < 2 ? 1 : 0), 1);

        uint16_t col = (i < layers / 2) ? b0 : b1;
        if (i == layers - 1) col = rt;

        frect(cx - lw / 2, wy - lh, lw, lh, col);

        if (i == layers / 2) {
            int gw = imax((lw * 62) / 100, 2);
            int gh = imax(lh / 2, 1);
            frect(cx - gw / 2, wy - lh + 1, gw, gh, gl);
        }
    }

    int nose_y = base_y - layers * layer_h - 1;
    frect(cx - 2, nose_y, 4, 2, ac);
}

static void drawPlayer(void) {
    int player_z = PROJ_K / (PLAYER_SCREEN_Y - HORIZON_Y);

    int center_w = 0;
    int half_w = 0;
    int rumble = 0;
    sample_road(FROM_FP(g_game.camera_z_fp) + player_z, &center_w, &half_w, &rumble);

    int cx = project_x_from_world(center_w - FROM_FP(g_player.x_fp), player_z);

    int speed_ratio = (g_player.speed_fp * 100) / BOOST_SPEED_FP;
    int w = PLAYER_BASE_W + iclamp(speed_ratio / 8, 0, 8);
    int h = PLAYER_BASE_H + iclamp(speed_ratio / 10, 0, 6);

    frect(cx - w / 2 - 3, PLAYER_SCREEN_Y + 2, w + 6, 4, shade565(C_BLACK, 140));

    drawVehicleStack(cx, PLAYER_SCREEN_Y, w, h,
                     C_BAT_BODY, C_BAT_MID, C_BAT_TOP,
                     C_BAT_GLASS, C_BAT_ACCENT,
                     7, 255);

    frect(cx - w / 2 + 4, PLAYER_SCREEN_Y - h + 6, 6, 2, C_BAT_LIGHT);
    frect(cx + w / 2 - 10, PLAYER_SCREEN_Y - h + 6, 6, 2, C_BAT_LIGHT);

    if (g_player.boost_timer > 0) {
        int flame_h = 8 + ((g_game.frame >> 1) & 3);
        frect(cx - 6, PLAYER_SCREEN_Y + 2, 12, flame_h, C_ORANGE);
        frect(cx - 3, PLAYER_SCREEN_Y + 3, 6, flame_h - 2, C_YELLOW);
    }
}

static void drawEnemy(const Enemy *e) {
    int z = FROM_FP(e->z_fp);
    if (z < 80) z = 80;

    int world_z = FROM_FP(g_game.camera_z_fp) + z;
    int center_w = 0;
    int half_w = 0;
    int rumble = 0;
    sample_road(world_z, &center_w, &half_w, &rumble);

    int lane_x = lane_center_x(e->lane, half_w);
    int enemy_world_x = center_w + lane_x + e->lane_offset;

    int screen_x = project_x_from_world(enemy_world_x - FROM_FP(g_player.x_fp), z);
    int screen_y = project_y_from_z(z);

    if (screen_y <= HORIZON_Y + 1 || screen_y >= SCR_H + 20) return;

    int scale_num = CAMERA_DEPTH;
    int w = imax((ENEMY_BASE_W * scale_num) / z, 4);
    int h = imax((ENEMY_BASE_H * scale_num) / z, 4);

    int shade = depth_shade_from_z(z);

    uint16_t base = C_ENEMY_0;
    if (e->type == 1) base = C_ENEMY_1;
    if (e->type == 2) base = C_ENEMY_2;
    if (e->type == 3) base = C_ENEMY_3;

    frect(screen_x - w / 2 - 2, screen_y + 1, w + 4, imax(h / 7, 1), shade565(C_BLACK, 120));

    drawVehicleStack(screen_x, screen_y, w, h,
                     base, shade565(base, 200), shade565(base, 230),
                     C_BLUE, C_WHITE,
                     5, shade);

    if ((g_game.frame >> 3) & 1) {
        frect(screen_x - 2, screen_y - h + 1, 4, 2, shade565(C_RED, shade));
    }
}

static void drawHUD(void) {
    frect(0, 0, 84, 34, shade565(C_DARK, 180));
    drect(0, 0, 84, 34, C_GREY);

    int speed = FROM_FP(g_player.speed_fp);
    int hp = g_player.hp;

    frect(4, 4, 60, 8, shade565(C_DARK2, 200));
    int bar_w = iclamp((g_player.speed_fp * 58) / BOOST_SPEED_FP, 0, 58);
    frect(5, 5, bar_w, 6, (g_player.boost_timer > 0) ? C_ORANGE : C_CYAN);

    frect(4, 16, 60, 4, shade565(C_DARK2, 220));
    frect(4, 16, iclamp(hp, 0, 5) * 12, 4, C_GREEN);

    int score_block = iclamp((g_player.score / 10), 0, 14);
    for (int i = 0; i < score_block; ++i) {
        frect(4 + i * 5, 24, 3, 6, C_YELLOW);
    }

    frect(70, 4, 10, 26, shade565(C_DARK2, 220));
    frect(72, 28 - iclamp(speed / 2, 0, 22), 6, iclamp(speed / 2, 0, 22), C_NEON);

    if (g_game.state == STATE_PAUSE) {
        frect(112, 96, 96, 40, shade565(C_BLACK, 180));
        drect(112, 96, 96, 40, C_WHITE);
        frect(140, 106, 12, 20, C_WHITE);
        frect(168, 106, 12, 20, C_WHITE);
    }

    if (g_game.state == STATE_GAMEOVER) {
        frect(86, 90, 148, 58, shade565(C_BLACK, 200));
        drect(86, 90, 148, 58, C_RED);
        frect(102, 108, 116, 8, C_RED);
        frect(108, 126, imax((g_player.score / 2), 6), 6, C_YELLOW);
    }
}

void renderScene(void) {
    clearFrame(C_BLACK);
    drawSkyAndBackground();
    drawRoadPerspective();

    /* Far-to-near enemy draw order by z */
    int order[MAX_ENEMIES];
    int count = 0;
    for (int i = 0; i < MAX_ENEMIES; ++i) {
        if (g_enemies[i].active) order[count++] = i;
    }

    for (int i = 0; i < count - 1; ++i) {
        for (int j = i + 1; j < count; ++j) {
            if (g_enemies[order[i]].z_fp < g_enemies[order[j]].z_fp) {
                int t = order[i];
                order[i] = order[j];
                order[j] = t;
            }
        }
    }

    for (int i = 0; i < count; ++i) drawEnemy(&g_enemies[order[i]]);

    drawPlayer();
    drawHUD();

    blitVGA();
}

/* ================================================================
 * World and physics
 * ================================================================ */
static void buildTrack(void) {
    for (int i = 0; i < ROAD_SEG_COUNT; ++i) {
        g_road[i].curve = 0;
        g_road[i].width = 1500;
        g_road[i].rumble = i & 1;
    }

    for (int i = 20; i < 70; ++i) g_road[i].curve = 18;
    for (int i = 70; i < 110; ++i) g_road[i].curve = 32;
    for (int i = 110; i < 150; ++i) g_road[i].curve = -26;
    for (int i = 150; i < 200; ++i) g_road[i].curve = -36;
    for (int i = 200; i < 240; ++i) g_road[i].curve = 14;
    for (int i = 240; i < 285; ++i) g_road[i].curve = -10;

    for (int i = 0; i < ROAD_SEG_COUNT; ++i) {
        int w = 1450 + ((i * 53) % 260) - 130;
        g_road[i].width = iclamp(w, 1280, 1640);
    }

    g_curve_prefix[0] = 0;
    for (int i = 0; i < ROAD_SEG_COUNT; ++i) {
        g_curve_prefix[i + 1] = g_curve_prefix[i] + g_road[i].curve;
    }

    int drift = g_curve_prefix[ROAD_SEG_COUNT];
    if (drift != 0) {
        for (int i = 0; i <= ROAD_SEG_COUNT; ++i) {
            g_curve_prefix[i] -= (drift * i) / ROAD_SEG_COUNT;
        }
    }
}

static void spawnEnemy(void) {
    for (int i = 0; i < MAX_ENEMIES; ++i) {
        if (g_enemies[i].active) continue;

        int seg_idx = road_index_for_world_z(FROM_FP(g_game.camera_z_fp));
        int hw = g_road[seg_idx].width;

        g_enemies[i].active = true;
        g_enemies[i].lane = irand(0, LANE_COUNT - 1);
        g_enemies[i].lane_offset = irand(-hw / 16, hw / 16);
        g_enemies[i].z_fp = ENEMY_SPAWN_Z + TO_FP(irand(0, 1200));
        g_enemies[i].speed_fp = TO_FP(irand(8, 16));
        g_enemies[i].type = irand(0, 3);
        return;
    }
}

void updatePhysics(void) {
    if (g_game.state != STATE_PLAY) return;

    int target_max = (g_player.boost_timer > 0) ? BOOST_SPEED_FP : MAX_SPEED_FP;

    if (k_up) {
        int acc = (g_player.boost_timer > 0) ? BOOST_ACCEL_FP : ACCEL_FP;
        g_player.speed_fp += acc;
    } else if (k_down) {
        if (g_player.speed_fp > 0) g_player.speed_fp -= BRAKE_FP;
        else g_player.speed_fp -= REVERSE_ACCEL_FP;
    } else {
        if (g_player.speed_fp > 0) {
            g_player.speed_fp -= DRAG_FP;
            if (g_player.speed_fp < 0) g_player.speed_fp = 0;
        } else if (g_player.speed_fp < 0) {
            g_player.speed_fp += DRAG_FP;
            if (g_player.speed_fp > 0) g_player.speed_fp = 0;
        }
    }

    if (k_shift && g_player.boost_timer <= 0 && g_player.speed_fp > TO_FP(8)) {
        g_player.boost_timer = BOOST_FRAMES;
    }

    if (g_player.boost_timer > 0) {
        g_player.boost_timer--;
        if (g_player.speed_fp < TO_FP(12)) g_player.speed_fp = TO_FP(12);
    }

    g_player.speed_fp = iclamp(g_player.speed_fp, -MAX_REVERSE_FP, target_max);

    if (k_left) g_player.steer_fp -= STEER_ACCEL_FP;
    if (k_right) g_player.steer_fp += STEER_ACCEL_FP;

    g_player.steer_fp = (g_player.steer_fp * STEER_DAMP_NUM) / STEER_DAMP_DEN;
    g_player.steer_fp = iclamp(g_player.steer_fp, -STEER_MAX_FP, STEER_MAX_FP);

    g_player.x_fp += g_player.steer_fp;

    int seg_idx = road_index_for_world_z(FROM_FP(g_game.camera_z_fp));
    int half_w = g_road[seg_idx].width;
    int limit = TO_FP(half_w - ROAD_EDGE_MARGIN);

    if (g_player.x_fp > limit) {
        g_player.x_fp = limit;
        if (g_player.steer_fp > 0) g_player.steer_fp = 0;
    }
    if (g_player.x_fp < -limit) {
        g_player.x_fp = -limit;
        if (g_player.steer_fp < 0) g_player.steer_fp = 0;
    }

    g_game.camera_z_fp += g_player.speed_fp;
    if (g_game.camera_z_fp < 0) g_game.camera_z_fp += TO_FP(TRACK_LEN);
    if (g_game.camera_z_fp >= TO_FP(TRACK_LEN)) {
        g_game.camera_z_fp -= TO_FP(TRACK_LEN);
        g_game.track_lap++;
    }

    if (g_player.speed_fp > 0) g_player.score += FROM_FP(g_player.speed_fp) / 7;
}

void updateWorld(void) {
    if (g_game.state != STATE_PLAY) return;

    if (--g_game.spawn_cooldown <= 0) {
        spawnEnemy();
        g_game.spawn_cooldown = irand(SPAWN_COOLDOWN_MIN, SPAWN_COOLDOWN_MAX);
    }

    int scroll_fp = g_player.speed_fp;

    for (int i = 0; i < MAX_ENEMIES; ++i) {
        Enemy *e = &g_enemies[i];
        if (!e->active) continue;

        e->z_fp -= scroll_fp;
        e->z_fp -= e->speed_fp / 4;

        if (e->z_fp < ENEMY_DESPAWN_Z) {
            e->active = false;
            continue;
        }

        int seg_idx = road_index_for_world_z(FROM_FP(g_game.camera_z_fp) + FROM_FP(e->z_fp));
        int half_w = g_road[seg_idx].width;

        int lane_x = lane_center_x(e->lane, half_w) + e->lane_offset;
        int dx = lane_x - FROM_FP(g_player.x_fp);

        if (e->z_fp < COLLISION_Z_WINDOW && e->z_fp > TO_FP(20) && iabs(dx) < COLLISION_X_WINDOW) {
            e->active = false;
            g_player.hp--;
            g_player.speed_fp = g_player.speed_fp / 2;
            g_game.crashed_flash = true;
            if (g_player.hp <= 0) {
                g_player.hp = 0;
                g_game.state = STATE_GAMEOVER;
                k_enter = false;
            }
        }

        int edge_limit = half_w - ROAD_EDGE_MARGIN;
        if (lane_x > edge_limit) e->lane_offset -= 8;
        if (lane_x < -edge_limit) e->lane_offset += 8;
    }

    if (g_game.crashed_flash && (g_game.frame & 1)) {
        frect(0, 0, SCR_W, SCR_H, shade565(C_RED, 48));
    }
    g_game.crashed_flash = false;
}

/* ================================================================
 * Game init/state
 * ================================================================ */
void init(void) {
    initVGA();
    initTimer();
    buildTrack();

    for (int i = 0; i < MAX_ENEMIES; ++i) g_enemies[i].active = false;

    g_player.x_fp = 0;
    g_player.steer_fp = 0;
    g_player.speed_fp = 0;
    g_player.boost_timer = 0;
    g_player.hp = 5;
    g_player.score = 0;

    g_game.state = STATE_MENU;
    g_game.frame = 0;
    g_game.camera_z_fp = 0;
    g_game.track_lap = 0;
    g_game.spawn_cooldown = 32;
    g_game.crashed_flash = false;

    k_up = k_down = k_left = k_right = k_shift = false;
    k_enter = k_esc = false;
}

static void renderMenu(void) {
    clearFrame(C_BLACK);
    drawSkyAndBackground();
    drawRoadPerspective();

    frect(56, 76, 208, 80, shade565(C_BLACK, 220));
    drect(56, 76, 208, 80, C_YELLOW);

    frect(72, 92, 176, 8, C_YELLOW);
    frect(72, 110, 132, 8, C_CYAN);
    frect(72, 126, 160, 6, C_WHITE);

    drawPlayer();

    if ((g_game.frame >> 4) & 1) {
        frect(94, 142, 132, 6, C_ORANGE);
    }

    blitVGA();
}

/* ================================================================
 * Main loop
 * ================================================================ */
int main(void) {
    init();

    while (1) {
        waitFrame();
        g_game.frame++;

        handleInput();

        if (g_game.state == STATE_MENU) {
            if (k_enter) {
                g_game.state = STATE_PLAY;
                g_player.hp = 5;
                g_player.score = 0;
                g_player.speed_fp = 0;
                g_player.x_fp = 0;
                g_game.camera_z_fp = 0;
                g_game.track_lap = 0;
                g_game.spawn_cooldown = 24;
                for (int i = 0; i < MAX_ENEMIES; ++i) g_enemies[i].active = false;
                k_enter = false;
            }
            renderMenu();
            continue;
        }

        if (g_game.state == STATE_PLAY) {
            if (k_esc) {
                g_game.state = STATE_PAUSE;
                k_esc = false;
            } else {
                updatePhysics();
                updateWorld();
            }
        } else if (g_game.state == STATE_PAUSE) {
            if (k_esc || k_enter) {
                g_game.state = STATE_PLAY;
                k_esc = false;
                k_enter = false;
            }
        } else if (g_game.state == STATE_GAMEOVER) {
            if (k_enter) {
                g_game.state = STATE_MENU;
                k_enter = false;
            }
        }

        renderScene();
    }

    return 0;
}
