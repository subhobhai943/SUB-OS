/*
 * SUB-OS "Sorry" -- an apology, rendered.
 *
 * The whole app is one idea: an apology should look like it took effort. So
 * nothing here is a static bitmap. The heart is the classic parametric heart
 * curve, scanline-filled every frame at whatever size the beat has it at; the
 * letter types itself out a character at a time; and a slow drift of small
 * hearts rises behind all of it.
 *
 * Two constraints shaped the implementation:
 *
 *   1. The kernel is built with -mno-sse and no floating point, so every
 *      curve, gradient, easing and physics step below is integer arithmetic.
 *      The heart outline is precomputed as a normalised integer polygon
 *      (half-width == 1000) so no trigonometry is needed at runtime.
 *
 *   2. The compositor only repaints when the scene is dirty, and otherwise
 *      ticks live content at ~10 Hz. That is too coarse for a heartbeat, so
 *      the app renews gui_desktop_request_animation_frame() on every paint
 *      and animates at the full frame rate for exactly as long as it is open.
 */

#include <gui/gui_apps.h>
#include <gui/gui_widgets.h>
#include <gui/gui_desktop.h>
#include <gui/gui_gfx.h>
#include <gui/gui_theme.h>
#include <kernel/ktime.h>
#include <mm/kmalloc.h>
#include <lib/string.h>
#include <lib/printf.h>

// ===========================================================================
// Palette
//
// The desktop theme is a cold slate blue, which is the wrong register for
// this entirely. The app paints its own warm rose over the client area.
// ===========================================================================
#define SORRY_BG_TOP        0xFF2B0E24  // deep plum
#define SORRY_BG_BOT        0xFF120611  // near-black wine
#define SORRY_ROSE          0xFFFF5C8A
#define SORRY_ROSE_LIGHT    0xFFFFA8C5
#define SORRY_ROSE_DEEP     0xFFB81D4E
#define SORRY_ROSE_MUTED    0xFF8E4A63  // the waiting stage: same heart, dimmer
#define SORRY_GOLD          0xFFF6C177
#define SORRY_TEXT          0xFFF9EAF0
#define SORRY_TEXT_SOFT     0xFFCBA4B5
#define SORRY_TEXT_FAINT    0xFF8C6376
#define SORRY_RULE          0xFF52243C

// ===========================================================================
// Heart geometry
//
// x = 16sin^3(t), y = 13cos(t) - 5cos(2t) - 2cos(3t) - cos(4t), sampled at 96
// points and normalised so the half-width is exactly 1000. In these units the
// lobes top out at y = +745, the notch between them dips to y = +313, and the
// bottom point reaches y = -1063 -- so a heart of half-width R occupies
// 1808*R/1000 pixels vertically.
// ===========================================================================
#define SORRY_HEART_PTS 96
#define HEART_TOP_Q     745   // lobe crown, in half-width units
#define HEART_BOT_Q     1063  // bottom point
#define HEART_SPAN_Q    1808  // total height == TOP + BOT

static const int16_t g_heart_pts[SORRY_HEART_PTS][2] = {
    {    0,   313}, {    0,   318}, {    2,   334}, {    7,   360}, {   17,   395}, {   33,   436},
    {   56,   482}, {   87,   530}, {  125,   579}, {  171,   625}, {  226,   666}, {  287,   700},
    {  354,   725}, {  425,   741}, {  499,   745}, {  575,   738}, {  650,   719}, {  721,   688},
    {  789,   647}, {  849,   597}, {  901,   538}, {  943,   472}, {  975,   402}, {  994,   327},
    { 1000,   250}, {  994,   172}, {  975,    94}, {  943,    17}, {  901,   -59}, {  849,  -133},
    {  789,  -205}, {  721,  -276}, {  650,  -344}, {  575,  -410}, {  499,  -475}, {  425,  -538},
    {  354,  -600}, {  287,  -661}, {  226,  -719}, {  171,  -775}, {  125,  -829}, {   87,  -878},
    {   56,  -924}, {   33,  -964}, {   17,  -998}, {    7, -1026}, {    2, -1046}, {    0, -1058},
    {    0, -1063}, {    0, -1058}, {   -2, -1046}, {   -7, -1026}, {  -17,  -998}, {  -33,  -964},
    {  -56,  -924}, {  -87,  -878}, { -125,  -829}, { -171,  -775}, { -226,  -719}, { -287,  -661},
    { -354,  -600}, { -425,  -538}, { -499,  -475}, { -575,  -410}, { -650,  -344}, { -721,  -276},
    { -789,  -205}, { -849,  -133}, { -901,   -59}, { -943,    17}, { -975,    94}, { -994,   172},
    {-1000,   250}, { -994,   327}, { -975,   402}, { -943,   472}, { -901,   538}, { -849,   597},
    { -789,   647}, { -721,   688}, { -650,   719}, { -575,   738}, { -499,   745}, { -425,   741},
    { -354,   725}, { -287,   700}, { -226,   666}, { -171,   625}, { -125,   579}, {  -87,   530},
    {  -56,   482}, {  -33,   436}, {  -17,   395}, {   -7,   360}, {   -2,   334}, {    0,   318},
};

// 127 * sin(2*pi*i/16); used for the drifting hearts' sideways sway.
static const int8_t g_sin16[16] = {
    0, 49, 90, 117, 127, 117, 90, 49, 0, -49, -90, -117, -127, -117, -90, -49
};

/* Scratch for the scaled outline. The compositor paints from a single thread,
 * and heart_scan never recurses, so one shared buffer is enough and keeps a
 * few hundred bytes off the kernel stack. */
static int16_t s_hx[SORRY_HEART_PTS];
static int16_t s_hy[SORRY_HEART_PTS];

typedef void (*heart_span_fn)(void* ctx, int x0, int x1, int y, int t);

/*
 * Scanline-fill the heart at (cx, cy) with half-width `radius`, handing each
 * horizontal run to `fn`. `t` is the run's vertical position through the shape
 * scaled to 0..255, which is what makes the top-to-bottom gradient possible
 * without the caller having to know the geometry.
 *
 * Even-odd crossing counting is what preserves the notch between the lobes: on
 * those rows the scanline crosses four edges, not two, and the middle pair is
 * correctly left unpainted.
 */
static void heart_scan(int cx, int cy, int radius, heart_span_fn fn, void* ctx) {
    if (radius < 2 || !fn) return;

    int ymin = 1 << 30;
    int ymax = -(1 << 30);
    for (int i = 0; i < SORRY_HEART_PTS; i++) {
        s_hx[i] = (int16_t)(cx + ((int)g_heart_pts[i][0] * radius) / 1000);
        s_hy[i] = (int16_t)(cy - ((int)g_heart_pts[i][1] * radius) / 1000);
        if (s_hy[i] < ymin) ymin = s_hy[i];
        if (s_hy[i] > ymax) ymax = s_hy[i];
    }

    int height = ymax - ymin;
    if (height <= 0) height = 1;

    for (int y = ymin; y <= ymax; y++) {
        int xs[8];
        int n = 0;

        for (int i = 0; i < SORRY_HEART_PTS && n < 8; i++) {
            int j = (i + 1) % SORRY_HEART_PTS;
            int y0 = s_hy[i];
            int y1 = s_hy[j];

            // Half-open crossing test: the edge spans this scanline, and the
            // comparison also guarantees y1 != y0 below.
            if ((y0 > y) == (y1 > y)) continue;
            xs[n++] = s_hx[i] + ((y - y0) * (s_hx[j] - s_hx[i])) / (y1 - y0);
        }

        for (int a = 1; a < n; a++) {          // insertion sort; n is 2 or 4
            int v = xs[a];
            int b = a - 1;
            while (b >= 0 && xs[b] > v) { xs[b + 1] = xs[b]; b--; }
            xs[b + 1] = v;
        }

        int t = ((y - ymin) * 255) / height;
        for (int a = 0; a + 1 < n; a += 2) fn(ctx, xs[a], xs[a + 1], y, t);
    }
}

/*
 * Coverage mask.
 *
 * The glow is three widening copies of the heart drawn behind the real one,
 * but only the sliver of each that escapes past the body is ever seen -- the
 * rest is painted over immediately. Blending those hidden pixels was most of
 * the app's frame cost, so the body's spans are recorded first and the glow
 * layers skip anything they cover. Two spans a row is exactly enough: a
 * scanline crosses the outline twice, or four times where it cuts the notch,
 * and recording both keeps the glow correctly visible inside the notch.
 */
#define SORRY_ROW_MAX 400
static int16_t s_cov_l[SORRY_ROW_MAX][2];
static int16_t s_cov_r[SORRY_ROW_MAX][2];
static uint8_t s_cov_n[SORRY_ROW_MAX];
static int     s_cov_y0   = 0;
static int     s_cov_rows = 0;

static void heart_span_record(void* ctx, int x0, int x1, int y, int t) {
    (void)ctx; (void)t;

    int r = y - s_cov_y0;
    if (r < 0 || r >= s_cov_rows || s_cov_n[r] >= 2) return;

    s_cov_l[r][s_cov_n[r]] = (int16_t)x0;
    s_cov_r[r][s_cov_n[r]] = (int16_t)x1;
    s_cov_n[r]++;
}

/* Record the coverage of a heart without drawing it. heart_scan emits its
 * spans left to right, which the subtraction below relies on. */
static void heart_record(int cx, int cy, int radius, int clip_y0, int clip_y1) {
    int y0 = cy - (radius * HEART_TOP_Q) / 1000 - 1;
    int y1 = cy + (radius * HEART_BOT_Q) / 1000 + 1;
    if (y0 < clip_y0) y0 = clip_y0;
    if (y1 > clip_y1) y1 = clip_y1;

    s_cov_y0   = y0;
    s_cov_rows = (y1 - y0 + 1);
    if (s_cov_rows < 0) s_cov_rows = 0;
    if (s_cov_rows > SORRY_ROW_MAX) s_cov_rows = SORRY_ROW_MAX;

    for (int i = 0; i < s_cov_rows; i++) s_cov_n[i] = 0;
    heart_scan(cx, cy, radius, heart_span_record, NULL);
}

/* How a single heart is painted: a vertical gradient, an overall opacity, and
 * an optional specular highlight on the upper-left lobe. The highlight is
 * applied inside the span callback so it is clipped to the shape for free. */
typedef struct {
    uint32_t top;
    uint32_t bottom;
    uint8_t  alpha;
    int      hx, hy, hr;   // highlight centre and radius; hr == 0 disables it
    bool     mask;         // skip pixels the recorded body already covers
    int      clip_x0, clip_y0, clip_x1, clip_y1;
} heart_paint_t;

static void heart_run(int x0, int x1, int y, uint32_t col, uint8_t alpha) {
    if (x1 < x0) return;
    if (alpha >= 255) gui_gfx_fill_rect(x0, y, x1 - x0 + 1, 1, col);
    else              gui_gfx_fill_rect_blend(x0, y, x1 - x0 + 1, 1, col, alpha);
}

static void heart_span_paint(void* ctx, int x0, int x1, int y, int t) {
    heart_paint_t* hp = (heart_paint_t*)ctx;

    if (y < hp->clip_y0 || y > hp->clip_y1) return;
    if (x0 < hp->clip_x0) x0 = hp->clip_x0;
    if (x1 > hp->clip_x1) x1 = hp->clip_x1;
    if (x1 < x0) return;

    uint32_t col = gui_color_alpha_blend(hp->top, hp->bottom, (uint8_t)t);

    int r = hp->mask ? y - s_cov_y0 : -1;
    if (r >= 0 && r < s_cov_rows && s_cov_n[r] > 0) {
        // Paint only what the body leaves uncovered on this row.
        for (int i = 0; i < s_cov_n[r]; i++) {
            if (s_cov_r[r][i] < x0 || s_cov_l[r][i] > x1) continue;
            if (s_cov_l[r][i] > x0) heart_run(x0, s_cov_l[r][i] - 1, y, col, hp->alpha);
            x0 = s_cov_r[r][i] + 1;
            if (x0 > x1) return;
        }
    }
    heart_run(x0, x1, y, col, hp->alpha);

    if (hp->hr <= 0) return;

    int dy  = y - hp->hy;
    int hr2 = hp->hr * hp->hr;
    if (dy * dy >= hr2) return;

    int gx0 = hp->hx - hp->hr; if (gx0 < x0) gx0 = x0;
    int gx1 = hp->hx + hp->hr; if (gx1 > x1) gx1 = x1;

    for (int x = gx0; x <= gx1; x++) {
        int dx = x - hp->hx;
        int d2 = dx * dx + dy * dy;
        if (d2 >= hr2) continue;
        // Quadratic falloff: brightest dead centre, nothing at the rim.
        gui_gfx_draw_pixel_blend(x, y, GUI_COLOR_WHITE,
                                 (uint8_t)((110 * (hr2 - d2)) / hr2));
    }
}

// ===========================================================================
// Heartbeat
// ===========================================================================

/* One asymmetric thump: a quick rise to `peak`, a slower relax back to zero. */
static int thump(int32_t p, int32_t rise, int32_t fall, int peak) {
    if (p < 0 || p >= rise + fall) return 0;
    if (p < rise) return (int)((p * peak) / rise);
    return peak - (int)(((p - rise) * peak) / fall);
}

/* A real heartbeat is two beats, not one: a strong systolic thump followed by
 * a softer echo. Returns a scale percentage, 100 being the resting size. */
static int heart_beat_pct(uint64_t ms, uint32_t period, int strength) {
    int32_t p = (int32_t)(ms % period);
    return 100 + thump(p, 90, 210, (strength * 15) / 100)
               + thump(p - 300, 70, 220, (strength * 8) / 100);
}

// ===========================================================================
// The letters
//
// The actual product. Everything above just carries these.
// ===========================================================================
typedef struct {
    const char*        heading;
    const char* const* lines;
    int                line_count;
} sorry_letter_t;

static const char* const g_letter1[] = {
    "I have started this a hundred times",
    "and deleted every single one of them.",
    "",
    "So, plainly, with nothing behind it:",
    "",
    "I am sorry.",
    "",
    "Not the quick kind that just wants",
    "the quiet back. The kind that finally",
    "understands what it cost you.",
};

static const char* const g_letter2[] = {
    "You handed me something soft to hold",
    "and I was careless with it.",
    "",
    "You waited. You left me room.",
    "And I filled that room with reasons",
    "instead of simply showing up.",
    "",
    "That part is mine. All of it.",
    "No 'but'. No small print. No excuse",
    "dressed up as an explanation.",
};

static const char* const g_letter3[] = {
    "I am not asking you to be fine yet.",
    "I am not asking you to hurry.",
    "",
    "I only needed you to know that I see",
    "it, that I own it, and that I would",
    "rather become someone worth",
    "forgiving than someone who is merely",
    "good at apologising.",
    "",
    "Quietly. Consistently. Starting now.",
};

static const sorry_letter_t g_letters[] = {
    { "the part I kept rewriting", g_letter1, (int)(sizeof(g_letter1) / sizeof(g_letter1[0])) },
    { "what I actually did",       g_letter2, (int)(sizeof(g_letter2) / sizeof(g_letter2[0])) },
    { "what happens from here",    g_letter3, (int)(sizeof(g_letter3) / sizeof(g_letter3[0])) },
};
#define SORRY_LETTER_COUNT ((int)(sizeof(g_letters) / sizeof(g_letters[0])))

static const char* const g_forgiven[] = {
    "You did not owe me that.",
    "",
    "I know what it costs to hand someone",
    "back the benefit of the doubt after",
    "they have already spent it.",
    "",
    "I will be careful with it this time.",
    "Not just today. From here on.",
};

static const char* const g_waiting[] = {
    "You do not have to be ready just",
    "because I happen to be sorry.",
    "",
    "So I will wait. No clock. No pressure.",
    "No sulking about it, either.",
    "",
    "I will be right here -- still sorry,",
    "still here -- for as long as it takes.",
};

static const sorry_letter_t g_letter_forgiven = {
    "thank you", g_forgiven, (int)(sizeof(g_forgiven) / sizeof(g_forgiven[0]))
};
static const sorry_letter_t g_letter_waiting = {
    "that is completely fair", g_waiting, (int)(sizeof(g_waiting) / sizeof(g_waiting[0]))
};

// ===========================================================================
// App state
// ===========================================================================
typedef enum {
    SORRY_APOLOGY = 0,   // reading the letters
    SORRY_FORGIVEN,      // they said yes
    SORRY_WAITING        // they said not yet, which is allowed
} sorry_stage_t;

#define SORRY_PETALS      26
#define SORRY_TYPE_CPS    52   // characters revealed per second
#define SORRY_LINE_PAUSE  3    // characters' worth of pause at each line break

typedef struct {
    int32_t  x, y;      // position, 1/16 px
    int32_t  vx, vy;    // velocity, 1/16 px per 100 ms
    uint8_t  size;      // half-width in px
    uint8_t  alpha;
    uint8_t  phase;     // sway phase offset
    uint8_t  sway;      // sway amplitude, px
    uint32_t color;
} petal_t;

typedef struct {
    uint64_t open_ms;      // when the window opened
    uint64_t stage_ms;     // when the current stage began
    uint64_t now_ms;
    uint64_t last_ms;      // previous frame, for dt
    sorry_stage_t stage;
    int      letter;       // which apology letter is on screen
    uint32_t typed;        // characters revealed so far
    bool     burst;        // celebration burst in flight
    uint32_t rng;
    petal_t  petals[SORRY_PETALS];
} sorry_data_t;

static uint32_t sorry_rand(sorry_data_t* sd) {
    sd->rng = sd->rng * 1664525u + 1013904223u;
    return sd->rng >> 8;
}

// ===========================================================================
// Drifting hearts
// ===========================================================================
static void petal_spawn(sorry_data_t* sd, petal_t* p, int w, int h, bool anywhere) {
    static const uint32_t tints[] = {
        SORRY_ROSE, SORRY_ROSE_LIGHT, SORRY_ROSE_DEEP, SORRY_GOLD
    };

    p->x     = (int32_t)(sorry_rand(sd) % (uint32_t)(w > 8 ? w : 8)) * 16;
    p->y     = anywhere ? (int32_t)(sorry_rand(sd) % (uint32_t)(h > 8 ? h : 8)) * 16
                        : (int32_t)(h + 6 + (int)(sorry_rand(sd) % 40)) * 16;
    p->vx    = 0;
    p->vy    = -(int32_t)(18 + sorry_rand(sd) % 34);
    p->size  = (uint8_t)(3 + sorry_rand(sd) % 5);
    p->alpha = (uint8_t)(38 + sorry_rand(sd) % 60);
    p->phase = (uint8_t)(sorry_rand(sd) & 15);
    p->sway  = (uint8_t)(3 + sorry_rand(sd) % 7);
    p->color = tints[sorry_rand(sd) % (sizeof(tints) / sizeof(tints[0]))];
}

/* Forgiveness gets a burst: every drifting heart is relaunched from the middle
 * of the heart, outward and upward, and then allowed to fall back. */
static void petals_burst(sorry_data_t* sd, int cx, int cy) {
    for (int i = 0; i < SORRY_PETALS; i++) {
        petal_t* p = &sd->petals[i];
        int a = (int)(sorry_rand(sd) & 15);

        p->x     = cx * 16;
        p->y     = cy * 16;
        p->vx    = (g_sin16[a] * (int32_t)(24 + sorry_rand(sd) % 46)) / 127;
        p->vy    = (g_sin16[(a + 4) & 15] * (int32_t)(24 + sorry_rand(sd) % 46)) / 127 - 30;
        p->size  = (uint8_t)(3 + sorry_rand(sd) % 6);
        p->alpha = (uint8_t)(90 + sorry_rand(sd) % 90);
        p->sway  = 0;
    }
    sd->burst = true;
}

static void petals_step(sorry_data_t* sd, int w, int h, uint32_t dt_ms) {
    if (dt_ms > 120) dt_ms = 120;   // never fast-forward across a stall

    bool all_gone = true;

    for (int i = 0; i < SORRY_PETALS; i++) {
        petal_t* p = &sd->petals[i];

        p->x += (p->vx * (int32_t)dt_ms) / 100;
        p->y += (p->vy * (int32_t)dt_ms) / 100;

        if (sd->burst) {
            p->vy += (int32_t)dt_ms / 3;                    // gravity
            p->vx -= (p->vx * (int32_t)dt_ms) / 900;        // air drag
            if (p->y < (h + 40) * 16) all_gone = false;
            continue;
        }

        // Off the top: send it back to the bottom to rise again.
        if (p->y < -((int32_t)p->size + 8) * 16) petal_spawn(sd, p, w, h, false);
    }

    // Once the burst has fallen out of sight, resume the gentle drift.
    if (sd->burst && all_gone) {
        sd->burst = false;
        for (int i = 0; i < SORRY_PETALS; i++) petal_spawn(sd, &sd->petals[i], w, h, true);
    }
}

static void petals_draw(sorry_data_t* sd, int ox, int oy, int w, int h) {
    for (int i = 0; i < SORRY_PETALS; i++) {
        petal_t* p = &sd->petals[i];

        int px = p->x / 16;
        int py = p->y / 16;

        if (p->sway) {
            int phase = (int)(((sd->now_ms / 90) + p->phase) & 15);
            px += (g_sin16[phase] * p->sway) / 127;
        }

        if (py < -20 || py > h + 20 || px < -20 || px > w + 20) continue;

        heart_paint_t hp = {
            .top = p->color, .bottom = p->color, .alpha = p->alpha, .hr = 0,
            .clip_x0 = ox, .clip_y0 = oy, .clip_x1 = ox + w - 1, .clip_y1 = oy + h - 1
        };
        heart_scan(ox + px, oy + py, p->size, heart_span_paint, &hp);
    }
}

// ===========================================================================
// Text helpers
// ===========================================================================

/* Draw at most `max_chars` characters, so a narrow window truncates cleanly
 * instead of spilling text across the window border. */
static void draw_clipped(int x, int y, const char* s, int max_chars, uint32_t col) {
    char buf[96];
    if (max_chars <= 0 || !s || !*s) return;
    if (max_chars > (int)sizeof(buf) - 1) max_chars = (int)sizeof(buf) - 1;

    int n = 0;
    while (s[n] && n < max_chars) { buf[n] = s[n]; n++; }
    buf[n] = '\0';
    gui_gfx_draw_string(x, y, buf, col);
}

static uint32_t letter_total_chars(const sorry_letter_t* L) {
    uint32_t total = 0;
    for (int i = 0; i < L->line_count; i++) {
        total += (uint32_t)strlen(L->lines[i]) + SORRY_LINE_PAUSE;
    }
    return total;
}

/*
 * Type the letter out. Returns true once the whole thing is on screen.
 *
 * `budget` characters are revealed in total, spent line by line, with a few
 * characters' worth of pause charged at each line break so the rhythm reads
 * like someone actually writing rather than a uniform ticker.
 */
static bool letter_draw(const sorry_letter_t* L, int x, int y, int line_h,
                        int max_chars, uint32_t budget, bool caret) {
    char buf[96];
    int  limit = max_chars > (int)sizeof(buf) - 1 ? (int)sizeof(buf) - 1 : max_chars;
    if (limit < 1) return true;

    for (int i = 0; i < L->line_count; i++) {
        int len = (int)strlen(L->lines[i]);
        int row = y + i * line_h;

        if (budget >= (uint32_t)len) {
            draw_clipped(x, row, L->lines[i], limit, SORRY_TEXT);
            budget -= (uint32_t)len;
            // Charge the line break, but never let it underflow the budget.
            budget = (budget > SORRY_LINE_PAUSE) ? budget - SORRY_LINE_PAUSE : 0;
            if (budget == 0 && i + 1 < L->line_count) return false;
            continue;
        }

        int n = (int)budget;
        if (n > limit) n = limit;
        for (int c = 0; c < n; c++) buf[c] = L->lines[i][c];
        buf[n] = '\0';
        gui_gfx_draw_string(x, row, buf, SORRY_TEXT);

        if (caret) gui_gfx_fill_rect(x + n * 8, row + 1, 6, 8, SORRY_ROSE);
        return false;
    }

    return true;
}

// ===========================================================================
// Paint
// ===========================================================================
static void sorry_paint(gui_window_t* win) {
    sorry_data_t* sd = (sorry_data_t*)win->user_data;
    if (!sd) return;

    // Animate at the compositor's full rate rather than the 10 Hz heartbeat.
    // Renewed every frame, so it stops mattering the moment this app closes.
    gui_desktop_request_animation_frame();

    gui_widget_begin(win);
    int w  = gui_widget_client_width();
    int h  = gui_widget_client_height();
    int ox = win->x + 1;
    int oy = win->y + GUI_TITLEBAR_HEIGHT + 1;
    if (w < 40 || h < 40) { gui_widget_end(); return; }

    uint64_t now = ktime_ms();
    uint32_t dt  = (now > sd->last_ms) ? (uint32_t)(now - sd->last_ms) : 0;
    sd->now_ms   = now;
    sd->last_ms  = now;

    uint64_t in_stage = now - sd->stage_ms;

    // --- background -------------------------------------------------------
    gui_gfx_draw_gradient_v(ox, oy, w, h, SORRY_BG_TOP, SORRY_BG_BOT);

    petals_step(sd, w, h, dt);
    petals_draw(sd, ox, oy, w, h);

    // --- layout -----------------------------------------------------------
    // Give the letter the ~46 columns it is written for and let the heart have
    // the rest, bounded so it stays sensible in both a small window and a
    // maximised one.
    int panel_w = w - 380;
    if (panel_w < 150) panel_w = 150;
    if (panel_w > 300) panel_w = 300;
    if (panel_w > w - 120) panel_w = w - 120;
    if (panel_w < 60) panel_w = 60;

    int btn_row   = h - 38;
    int top       = 24;
    int caption_h = 36;   // the two lines of text under the heart

    // Fit to whichever of width or height runs out first, then take 100/120 of
    // that: the heart is sized at rest but drawn up to ~1.15x at the peak of a
    // beat, and the margin is what stops the lobes clipping the panel edge.
    int avail  = btn_row - top - caption_h;
    int r_by_w = (panel_w - 16) / 2;
    int r_by_h = (avail * 1000) / HEART_SPAN_Q;
    int radius = ((r_by_w < r_by_h ? r_by_w : r_by_h) * 100) / 120;
    if (radius > 120) radius = 120;
    if (radius < 0)   radius = 0;

    int beat_period   = (sd->stage == SORRY_WAITING) ? 1600 : 1150;
    int beat_strength = (sd->stage == SORRY_WAITING) ? 55
                      : (sd->stage == SORRY_FORGIVEN) ? 130 : 100;
    int beat = heart_beat_pct(now, (uint32_t)beat_period, beat_strength);

    // A short swell as the window opens, so the heart arrives rather than
    // simply being there.
    if (in_stage < 420 && sd->stage != SORRY_APOLOGY) {
        beat = beat + (int)((420 - in_stage) * 18 / 420);
    }

    int r_beat = (radius * beat) / 100;
    int cx     = panel_w / 2;

    // Centre the heart-plus-caption block in the column rather than pinning it
    // to the top, so the left side stays balanced at any window height. All of
    // it is measured at the peak of a beat (r_peak), which is the extent that
    // actually has to fit.
    int r_peak  = (radius * 120) / 100;
    int block_h = (r_peak * HEART_SPAN_Q) / 1000 + caption_h;
    int slack   = (btn_row - top) - block_h;
    if (slack < 0) slack = 0;
    int cy = top + slack / 2 + (r_peak * HEART_TOP_Q) / 1000;

    if (radius >= 14) {
        uint32_t top_col, bot_col;
        switch (sd->stage) {
        case SORRY_FORGIVEN: top_col = SORRY_ROSE_LIGHT; bot_col = SORRY_ROSE;       break;
        case SORRY_WAITING:  top_col = SORRY_ROSE_MUTED; bot_col = SORRY_ROSE_DEEP;  break;
        default:             top_col = SORRY_ROSE;       bot_col = SORRY_ROSE_DEEP;  break;
        }

        // Record where the heart itself will land, so the glow behind it can
        // skip every pixel it is about to be covered by.
        heart_record(ox + cx, oy + cy, r_beat, oy, oy + h - 1);

        heart_paint_t glow = {
            .top = top_col, .bottom = bot_col, .hr = 0, .mask = true,
            .clip_x0 = ox, .clip_y0 = oy, .clip_x1 = ox + w - 1, .clip_y1 = oy + h - 1
        };

        // Widening, fading copies behind the heart stand in for a real blur.
        // They are drawn outermost first and each one covers the ones before
        // it, so the accumulated alpha falls off smoothly towards the rim --
        // six thin layers rather than three thick ones is the difference
        // between a halo and a set of contour rings.
        static const int glow_scale[6] = { 131, 125, 119, 113, 108, 104 };
        for (int i = 0; i < 6; i++) {
            glow.alpha = (uint8_t)(sd->stage == SORRY_WAITING ? 6 : 12);
            heart_scan(ox + cx, oy + cy, (r_beat * glow_scale[i]) / 100,
                       heart_span_paint, &glow);
        }

        heart_paint_t body = {
            .top = top_col, .bottom = bot_col, .alpha = 255,
            .hx = ox + cx - (r_beat * 40) / 100,
            .hy = oy + cy - (r_beat * 36) / 100,
            .hr = (r_beat * 26) / 100,
            .clip_x0 = ox, .clip_y0 = oy, .clip_x1 = ox + w - 1, .clip_y1 = oy + h - 1
        };
        heart_scan(ox + cx, oy + cy, r_beat, heart_span_paint, &body);
    }

    // --- caption under the heart -----------------------------------------
    char buf[80];
    // Clear the tip at the *peak* of a beat, not at rest, or the heart dips
    // into the caption every time it thumps.
    int  cap_y = oy + cy + (r_peak * HEART_BOT_Q) / 1000 + 10;

    const char* caption =
        (sd->stage == SORRY_FORGIVEN) ? "and it is not going to stop" :
        (sd->stage == SORRY_WAITING)  ? "patient, for as long as it takes" :
                                        "still beating, still sorry";

    int cap_chars = (panel_w - 8) / 8;
    int cap_w     = (int)strlen(caption) * 8;
    if (cap_w > panel_w - 8) cap_w = cap_chars * 8;
    if (cap_y < oy + h - 12) {
        draw_clipped(ox + cx - cap_w / 2, cap_y, caption, cap_chars, SORRY_TEXT_SOFT);
    }

    // The beat count is derived from elapsed time, so it is a real count of
    // the beats that have actually been drawn since the window opened.
    uint64_t beats = (now - sd->open_ms) / (uint64_t)beat_period;
    snprintf(buf, sizeof(buf), "%llu beats so far", (unsigned long long)beats);
    int bw = (int)strlen(buf) * 8;
    if (cap_y + 14 < oy + h - 12) {
        draw_clipped(ox + cx - bw / 2, cap_y + 14, buf, cap_chars, SORRY_TEXT_FAINT);
    }

    // --- the letter -------------------------------------------------------
    int lx = panel_w + 10;
    int lw = w - lx - 14;
    int lc = lw / 8;
    if (lc < 8) lc = 8;

    const sorry_letter_t* L =
        (sd->stage == SORRY_FORGIVEN) ? &g_letter_forgiven :
        (sd->stage == SORRY_WAITING)  ? &g_letter_waiting  :
                                        &g_letters[sd->letter];

    uint32_t total   = letter_total_chars(L);
    uint32_t elapsed = (uint32_t)((in_stage * SORRY_TYPE_CPS) / 1000);
    if (sd->typed < elapsed) sd->typed = elapsed;
    if (sd->typed > total)   sd->typed = total;

    gui_gfx_draw_string_16_shadow(ox + lx, oy + 16, "I'm sorry.", SORRY_ROSE_LIGHT, 0xFF3A0F26);
    gui_gfx_fill_rect(ox + lx, oy + 38, lw, 1, SORRY_RULE);
    draw_clipped(ox + lx, oy + 46, L->heading, lc, SORRY_GOLD);

    bool done = letter_draw(L, ox + lx, oy + 68, 15, lc, sd->typed,
                            ((now / 420) & 1) != 0);

    int sig_y = oy + 68 + L->line_count * 15 + 10;
    if (done && sig_y < oy + btn_row - 26) {
        const char* sig = (sd->stage == SORRY_APOLOGY)
                        ? "-- and I mean every character of it."
                        : "-- SUB";
        draw_clipped(ox + lx, sig_y, sig, lc, SORRY_TEXT_FAINT);
    }

    // How long they have been kept waiting, counted honestly.
    if (sd->stage == SORRY_WAITING) {
        uint64_t secs = in_stage / 1000;
        snprintf(buf, sizeof(buf), "waiting patiently: %02llu:%02llu",
                 (unsigned long long)(secs / 60), (unsigned long long)(secs % 60));
        draw_clipped(ox + lx, oy + btn_row - 22, buf, lc, SORRY_GOLD);
    }

    // Which letter of the three is on screen.
    if (sd->stage == SORRY_APOLOGY) {
        for (int i = 0; i < SORRY_LETTER_COUNT; i++) {
            int dx = ox + lx + i * 14;
            int dy = oy + btn_row - 18;
            if (i == sd->letter) gui_gfx_fill_rect(dx, dy, 8, 3, SORRY_ROSE);
            else                 gui_gfx_fill_rect(dx, dy, 8, 3, SORRY_RULE);
        }
    }

    // --- controls ---------------------------------------------------------
    int bx = lx;
    int bh = 26;

    if (sd->stage == SORRY_APOLOGY) {
        if (gui_button_styled(1, bx, btn_row, 116, bh, "I forgive you",
                              SORRY_ROSE_DEEP, SORRY_ROSE, 0xFF8E1440,
                              SORRY_ROSE_LIGHT, SORRY_TEXT)) {
            sd->stage    = SORRY_FORGIVEN;
            sd->stage_ms = now;
            sd->typed    = 0;
            petals_burst(sd, cx, cy);
        }
        bx += 124;

        if (gui_button_styled(2, bx, btn_row, 104, bh, "Tell me more",
                              0xFF3A1730, 0xFF542444, 0xFF2A0F22,
                              SORRY_RULE, SORRY_TEXT_SOFT)) {
            sd->letter   = (sd->letter + 1) % SORRY_LETTER_COUNT;
            sd->stage_ms = now;
            sd->typed    = 0;
        }
        bx += 112;

        if (gui_button_styled(3, bx, btn_row, 76, bh, "Not yet",
                              0xFF3A1730, 0xFF542444, 0xFF2A0F22,
                              SORRY_RULE, SORRY_TEXT_SOFT)) {
            sd->stage    = SORRY_WAITING;
            sd->stage_ms = now;
            sd->typed    = 0;
        }
    } else if (sd->stage == SORRY_WAITING) {
        if (gui_button_styled(4, bx, btn_row, 116, bh, "I'm ready now",
                              SORRY_ROSE_DEEP, SORRY_ROSE, 0xFF8E1440,
                              SORRY_ROSE_LIGHT, SORRY_TEXT)) {
            sd->stage    = SORRY_FORGIVEN;
            sd->stage_ms = now;
            sd->typed    = 0;
            petals_burst(sd, cx, cy);
        }
        bx += 124;

        if (gui_button_styled(5, bx, btn_row, 104, bh, "Say it again",
                              0xFF3A1730, 0xFF542444, 0xFF2A0F22,
                              SORRY_RULE, SORRY_TEXT_SOFT)) {
            sd->stage    = SORRY_APOLOGY;
            sd->stage_ms = now;
            sd->typed    = 0;
        }
    } else {
        if (gui_button_styled(6, bx, btn_row, 116, bh, "Read it again",
                              0xFF3A1730, 0xFF542444, 0xFF2A0F22,
                              SORRY_RULE, SORRY_TEXT_SOFT)) {
            sd->stage    = SORRY_APOLOGY;
            sd->letter   = 0;
            sd->stage_ms = now;
            sd->typed    = 0;
        }
    }

    // Impatient readers can click the letter to finish the typing at once.
    // Placed last so the buttons above get first claim on the click.
    if (!done && gui_hitzone(9, lx, 46, lw, btn_row - 52)) sd->typed = total;

    gui_widget_end();
}

static void sorry_event(gui_window_t* win, const gui_event_t* ev) {
    gui_widget_feed_event(win, ev);

    if (ev->type == GUI_EVENT_CLOSE && win->user_data) {
        kfree(win->user_data);
        win->user_data = NULL;
    }
}

void gui_app_sorry_launch(int x, int y, int w, int h) {
    gui_window_t* win = gui_wm_create_window("I'm Sorry", x, y,
                                             (w > 0) ? w : 660, (h > 0) ? h : 470);
    if (!win) return;

    sorry_data_t* sd = (sorry_data_t*)kzalloc(sizeof(sorry_data_t));
    if (!sd) {
        gui_wm_destroy_window(win->id);
        return;
    }

    uint64_t now = ktime_ms();
    sd->open_ms  = now;
    sd->stage_ms = now;
    sd->last_ms  = now;
    sd->stage    = SORRY_APOLOGY;
    sd->rng      = (uint32_t)(now * 2654435761u) | 1u;

    // Scatter the first drift across the window so it does not start empty.
    for (int i = 0; i < SORRY_PETALS; i++) {
        petal_spawn(sd, &sd->petals[i], win->width - 2,
                    win->height - GUI_TITLEBAR_HEIGHT - 2, true);
    }

    win->user_data    = sd;
    win->paint        = sorry_paint;
    win->handle_event = sorry_event;
}
