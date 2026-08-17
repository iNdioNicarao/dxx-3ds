/*
 * Bottom-screen (3DS) owner + RGB565 raw-framebuffer blitter.
 *
 * Single owner of the bottom LCD framebuffer. Driven by a mode state machine
 * (see bottom_screen.h): bottom_set_mode() selects what is drawn; the one and
 * only bottom_screen_present() call (from the main loop) does the actual draw
 * and swaps the bottom buffer. The top screen is picaGL's and is NEVER touched
 * here.
 *
 * The PrintConsole that previously owned the bottom (inferno.c consoleInit) is
 * retired; this module is the sole writer. Pixel format matches the bottom
 * screen setup: GSP_RGB565_OES (16-bit, one u16 per pixel).
 *
 * The 8x8 glyph table (font8x8_basic) is the well-known public-domain font
 * used widely in hobbyist/firmware projects. It covers ASCII 32..126.
 */

#include <3ds.h>
#include <string.h>
#include <stdio.h>
#include <GL/picaGL.h>   /* pglIsPoweredOff — bail bottom-screen flush on power-off */
#include "bottom_screen.h"
#include "gamefont.h"   /* GAME_FONT / Gamefonts[] — real Descent font for labels */
#include "gr.h"         /* grs_bitmap, gr_palette (6-bit), SWIDTH/SHEIGHT */
#include "pcx.h"        /* pcx_read_bitmap — load the marbled popup background */
#include "physfsx.h"    /* PHYSFSX_exists — find the marbled PCX on the SD */
/* 3DS power-off signal (set by the APT ONSUSPEND hook in arch/sdl/init.c);
 * the bottom-screen draw path checks it so it stops touching the framebuffer
 * the instant the system begins powering down. */
extern volatile int d1x_powering_off;

/* Top-screen game-font text colour we replicate on buttons (Abort game?
 * prompt = gr_set_fontcolor(29,29,47)). Declared once, before use. */
#ifndef BTN_TEXT_R
#define BTN_TEXT_R   29
#define BTN_TEXT_G   29
#define BTN_TEXT_B   47
#endif

/* Foreground colour for the game-font text blitter (file scope so the
 * renderer can see it). Defaults to the popup text colour (29,29,47);
 * draw_key overrides it to a dim grey for the disabled "DELETE PILOT" state. */
static int g_fg_r = BTN_TEXT_R, g_fg_g = BTN_TEXT_G, g_fg_b = BTN_TEXT_B;

/* Demo recorder toggle (3DS REC button). We only need the state var, the
 * state macros, and the two start/stop entry points — forward-declaring them
 * here avoids pulling in newdemo.h (which references the full `object` type
 * and would fail to compile without object.h in this translation unit). */
#define ND_STATE_NORMAL   0
#define ND_STATE_RECORDING 1
extern int Newdemo_state;
extern void newdemo_start_recording(void);
extern void newdemo_stop_recording(void);

/* Local copy of the font bit-packing helper (defined in 2d/font.c, not a
 * shared header). Glyph data is packed MSB-first at this byte-width. */
#ifndef BITS_TO_BYTES
#define BITS_TO_BYTES(x)    (((x) + 7) >> 3)
#endif

/* --- module state --- */
static u16 *g_bot_buf = NULL;
static int  g_bot_w = 0;
static int  g_bot_h = 0;
static bs_mode_t g_mode = BS_MODE_OFF;
static int  g_inited = 0;
static int  g_bottom_active = 0;   /* 1 once our framebuffer is owned+live */
static int  g_bg_painted = 0;     /* 1 once the marble base has been painted
                                     (paint ONCE, not per-button, so the three
                                     in-game buttons don't erase each other) */
static int  g_bottom_dirty = 0;    /* 1 when a repaint is pending (see present) */

/* APT (Home Menu / lid sleep) hook: on resume, the bottom framebuffer the OS
 * hands back is a DIFFERENT address than the one we cached at init, and the
 * top screen's GPU state is also re-initialised. Re-acquire both so the next
 * present/render draws to live memory instead of a dead framebuffer (black
 * screens). picaGL installs its own APTHOOK_ONRESTORE for the top; we add this
 * one for the bottom and also nudge picaGL so both recover in either order. */
static aptHookCookie g_bs_apt_cookie;
static void bottom_screen_apt_hook(APT_HookType type, void *param)
{
	switch (type) {
	case APTHOOK_ONRESTORE:
		bottom_screen_reacquire();
		pglReacquire();
		break;
	default:
		break;
	}
}

/* RGB565 packing helper. */
static inline u16 rgb565(int r, int g, int b) {
	return (u16)(((r & 0x1F) << 11) | ((g & 0x3F) << 5) | (b & 0x1F));
}

/* --- 8x8 public-domain font (font8x8_basic), ASCII 32..126 --- */
/* Each glyph is 8 bytes, MSB = top row, bit7 = leftmost pixel. */
static const uint8_t font[95][8] = {
{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* 32 space */
{0x18,0x3C,0x3C,0x18,0x18,0x00,0x18,0x00}, /* 33 ! */
{0x36,0x36,0x00,0x00,0x00,0x00,0x00,0x00}, /* 34 " */
{0x36,0x36,0x7F,0x36,0x7F,0x36,0x36,0x00}, /* 35 # */
{0x0C,0x3E,0x63,0x3E,0x6E,0x63,0x3E,0x0C}, /* 36 $ */
{0x60,0x66,0x0C,0x18,0x30,0x63,0x06,0x00}, /* 37 % */
{0x38,0x6C,0x6C,0x38,0x6D,0x66,0x3B,0x00}, /* 38 & */
{0x18,0x18,0x18,0x00,0x00,0x00,0x00,0x00}, /* 39 ' */
{0x0C,0x18,0x30,0x30,0x30,0x18,0x0C,0x00}, /* 40 ( */
{0x30,0x18,0x0C,0x0C,0x0C,0x18,0x30,0x00}, /* 41 ) */
{0x00,0x18,0x3C,0x7E,0x3C,0x18,0x00,0x00}, /* 42 * */
{0x00,0x18,0x18,0x7E,0x18,0x18,0x00,0x00}, /* 43 + */
{0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x30}, /* 44 , */
{0x00,0x00,0x00,0x7E,0x00,0x00,0x00,0x00}, /* 45 - */
{0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00}, /* 46 . */
{0x02,0x06,0x0C,0x18,0x30,0x60,0x40,0x00}, /* 47 / */
{0x3E,0x63,0x73,0x6B,0x67,0x63,0x3E,0x00}, /* 48 0 */
{0x0C,0x0E,0x0C,0x0C,0x0C,0x0C,0x3F,0x00}, /* 49 1 */
{0x1E,0x33,0x30,0x1C,0x06,0x33,0x3F,0x00}, /* 50 2 */
{0x1E,0x33,0x30,0x1C,0x30,0x33,0x1E,0x00}, /* 51 3 */
{0x38,0x3C,0x36,0x33,0x7F,0x30,0x30,0x00}, /* 52 4 */
{0x3F,0x03,0x1F,0x30,0x30,0x33,0x1E,0x00}, /* 53 5 */
{0x1C,0x06,0x03,0x1F,0x33,0x33,0x1E,0x00}, /* 54 6 */
{0x3F,0x33,0x30,0x18,0x0C,0x0C,0x0C,0x00}, /* 55 7 */
{0x1E,0x33,0x33,0x1E,0x33,0x33,0x1E,0x00}, /* 56 8 */
{0x1E,0x33,0x33,0x3E,0x30,0x18,0x0E,0x00}, /* 57 9 */
{0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x00}, /* 58 : */
{0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x30}, /* 59 ; */
{0x06,0x0C,0x18,0x30,0x18,0x0C,0x06,0x00}, /* 60 < */
{0x00,0x00,0x7E,0x00,0x7E,0x00,0x00,0x00}, /* 61 = */
{0x60,0x30,0x18,0x0C,0x18,0x30,0x60,0x00}, /* 62 > */
{0x1E,0x33,0x30,0x18,0x0C,0x00,0x0C,0x00}, /* 63 ? */
{0x3E,0x63,0x6F,0x6B,0x6E,0x60,0x3E,0x00}, /* 64 @ */
{0x3C,0x66,0x66,0x66,0x7E,0x66,0x66,0x00}, /* 65 A */
{0x3F,0x66,0x66,0x3E,0x66,0x66,0x3F,0x00}, /* 66 B */
{0x3C,0x66,0x63,0x63,0x63,0x66,0x3C,0x00}, /* 67 C */
{0x3F,0x66,0x66,0x66,0x66,0x66,0x3F,0x00}, /* 68 D */
{0x7F,0x63,0x63,0x7B,0x63,0x63,0x7F,0x00}, /* 69 E */
{0x7F,0x63,0x63,0x7B,0x63,0x63,0x63,0x00}, /* 70 F */
{0x3E,0x63,0x63,0x63,0x6B,0x66,0x3D,0x00}, /* 71 G */
{0x66,0x66,0x66,0x7E,0x66,0x66,0x66,0x00}, /* 72 H */
{0x3F,0x18,0x18,0x18,0x18,0x18,0x3F,0x00}, /* 73 I */
{0x1E,0x06,0x06,0x06,0x66,0x66,0x3C,0x00}, /* 74 J */
{0x66,0x6C,0x78,0x70,0x78,0x6C,0x66,0x00}, /* 75 K */
{0x63,0x63,0x63,0x63,0x63,0x63,0x7F,0x00}, /* 76 L */
{0x63,0x77,0x7F,0x6B,0x63,0x63,0x63,0x00}, /* 77 M */
{0x63,0x67,0x6F,0x7B,0x73,0x63,0x63,0x00}, /* 78 N */
{0x3C,0x66,0x63,0x63,0x63,0x66,0x3C,0x00}, /* 79 O */
{0x3F,0x66,0x66,0x3E,0x06,0x06,0x0F,0x00}, /* 80 P */
{0x3C,0x66,0x63,0x63,0x63,0x66,0x3C,0x78}, /* 81 Q */
{0x3F,0x66,0x66,0x3E,0x1E,0x36,0x63,0x00}, /* 82 R */
{0x1E,0x33,0x03,0x0E,0x30,0x33,0x1E,0x00}, /* 83 S */
{0x3F,0x2D,0x0C,0x0C,0x0C,0x0C,0x1E,0x00}, /* 84 T */
{0x66,0x66,0x66,0x66,0x66,0x66,0x3F,0x00}, /* 85 U */
{0x66,0x66,0x66,0x66,0x66,0x3C,0x18,0x00}, /* 86 V */
{0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00}, /* 87 W */
{0x66,0x66,0x3C,0x18,0x3C,0x66,0x66,0x00}, /* 88 X */
{0x66,0x66,0x66,0x3C,0x18,0x18,0x3F,0x00}, /* 89 Y */
{0x7F,0x63,0x30,0x18,0x0C,0x63,0x7F,0x00}, /* 90 Z */
{0x3F,0x33,0x33,0x33,0x33,0x33,0x3F,0x00}, /* 91 [ */
{0x40,0x60,0x30,0x18,0x0C,0x06,0x02,0x00}, /* 92 \ */
{0x3F,0x33,0x33,0x33,0x33,0x33,0x3F,0x00}, /* 93 ] */
{0x08,0x1C,0x36,0x63,0x36,0x1C,0x08,0x00}, /* 94 ^ */
{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF}, /* 95 _ */
{0x0C,0x0C,0x18,0x00,0x00,0x00,0x00,0x00}, /* 96 ` */
{0x00,0x00,0x1E,0x30,0x3E,0x33,0x6E,0x00}, /* 97 a */
{0x07,0x03,0x03,0x1F,0x33,0x33,0x1F,0x00}, /* 98 b */
{0x00,0x00,0x1E,0x33,0x03,0x33,0x1E,0x00}, /* 99 c */
{0x38,0x30,0x30,0x3E,0x33,0x33,0x6E,0x00}, /* 100 d */
{0x00,0x00,0x1E,0x33,0x3F,0x03,0x1E,0x00}, /* 101 e */
{0x1C,0x36,0x06,0x0F,0x06,0x06,0x0F,0x00}, /* 102 f */
{0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x1E}, /* 103 g */
{0x07,0x03,0x03,0x1F,0x33,0x33,0x33,0x00}, /* 104 h */
{0x00,0x00,0x0C,0x00,0x0C,0x0C,0x0C,0x00}, /* 105 i */
{0x00,0x00,0x30,0x00,0x30,0x30,0x30,0x1E}, /* 106 j */
{0x07,0x03,0x03,0x33,0x1B,0x3F,0x33,0x00}, /* 107 k */
{0x00,0x00,0x0C,0x0C,0x0C,0x0C,0x1E,0x00}, /* 108 l */
{0x00,0x00,0x1B,0x3F,0x37,0x33,0x33,0x00}, /* 109 m */
{0x00,0x00,0x1F,0x33,0x33,0x33,0x33,0x00}, /* 110 n */
{0x00,0x00,0x1E,0x33,0x33,0x33,0x1E,0x00}, /* 111 o */
{0x00,0x00,0x1F,0x33,0x33,0x1F,0x03,0x03}, /* 112 p */
{0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x30}, /* 113 q */
{0x00,0x00,0x1B,0x36,0x06,0x06,0x0F,0x00}, /* 114 r */
{0x00,0x00,0x1E,0x03,0x1E,0x30,0x1E,0x00}, /* 115 s */
{0x00,0x08,0x1E,0x08,0x08,0x2C,0x18,0x00}, /* 116 t */
{0x00,0x00,0x33,0x33,0x33,0x33,0x6E,0x00}, /* 117 u */
{0x00,0x00,0x33,0x33,0x33,0x1E,0x0C,0x00}, /* 118 v */
{0x00,0x00,0x33,0x33,0x37,0x3F,0x1B,0x00}, /* 119 w */
{0x00,0x00,0x33,0x1E,0x0C,0x1E,0x33,0x00}, /* 120 x */
{0x00,0x00,0x33,0x33,0x33,0x3E,0x30,0x1E}, /* 121 y */
{0x00,0x00,0x3F,0x18,0x0C,0x06,0x3F,0x00}, /* 122 z */
{0x0C,0x0C,0x0C,0x00,0x0C,0x0C,0x0C,0x00}, /* 123 { */
{0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x00}, /* 124 | */
{0x30,0x30,0x30,0x00,0x30,0x30,0x30,0x00}, /* 125 } */
{0x18,0x3C,0x66,0x00,0x00,0x00,0x00,0x00}, /* 126 ~ */
};

/* --- primitives ---
 *
 * COORDINATE SYSTEM: public draw calls (clear/rect/print) take LOGICAL
 * landscape coords on the physical bottom screen: x in [0,320) (width),
 * y in [0,240) (height), origin top-left.
 *
 * The framebuffer libctru returns is 240x320 (g_bot_w=240, g_bot_h=320),
 * confirmed at runtime (bs_dims.txt: bw=240 bh=320). The 3DS bottom LCD
 * does NOT rotate this buffer in hardware (verified visually on v2.2.0: the
 * test pattern appeared rotated 90deg CCW, proving the HW presents the
 * 240x320 buffer as-is). So we must rotate our logical landscape drawing
 * 90deg CLOCKWISE into the portrait buffer ourselves:
 *
 *     logical (x, y)  ->  buffer (fx, fy)
 *     fx = 239 - y             (240-tall logical y -> 240-wide buffer x)
 *     fy = x                  (320-wide logical x -> 320-tall buffer y)
 *     idx = fx + fy * g_bot_w  (g_bot_w == 240)
 *
 * This yields a full-screen, upright landscape image. A whole-screen
 * left-right mirror would be cancelled per-glyph, but the v2.2.0 footage
 * showed NO per-glyph mirror, so no glyph flip is needed here.
 */

static void bottom_set_px(int x, int y, uint16_t c)
{
	int fx, fy, idx;
	if (!g_bot_buf) return;
	if (x < 0 || x >= BS_LOGICAL_W || y < 0 || y >= BS_LOGICAL_H) return;
	fx = BS_LOGICAL_H - 1 - y;   /* 239 - y : rotate 90deg CW */
	fy = x;
	idx = fx + fy * g_bot_w;   /* g_bot_w == 240 == framebuffer stride */
	g_bot_buf[idx] = c;
}

void bottom_clear(uint16_t rgb565)
{
	int i;
	if (!g_bot_buf) return;
	for (i = 0; i < g_bot_w * g_bot_h; i++)
		g_bot_buf[i] = rgb565;
	g_bg_painted = 0;     /* base must be repainted before buttons redraw */
	g_bottom_dirty = 1;   /* a full repaint is now pending */
}

void bottom_fill_rect(int x, int y, int w, int h, uint16_t rgb565)
{
	int iy, ix;
	if (!g_bot_buf) return;
	if (x < 0) { w += x; x = 0; }
	if (y < 0) { h += y; y = 0; }
	if (x + w > BS_LOGICAL_W) w = BS_LOGICAL_W - x;
	if (y + h > BS_LOGICAL_H) h = BS_LOGICAL_H - y;
	if (w <= 0 || h <= 0) return;
	for (iy = y; iy < y + h; iy++)
		for (ix = x; ix < x + w; ix++)
			bottom_set_px(ix, iy, rgb565);
}

void bottom_print(int x, int y, const char *s, uint16_t rgb565)
{
	int cx = x;
	if (!g_bot_buf || !s) return;
	for (; *s; s++) {
		int c = (unsigned char)*s;
		int gx, gy;
		if (c < 32 || c > 126) c = 32;
		c -= 32;
		for (gy = 0; gy < 8; gy++) {
			uint8_t row = font[c][gy];
			/* Read font bits LSB->leftmost (gx=0 reads bit0). The 90deg-CW
			 * rotation in bottom_set_px (fx = 239 - y) flips the logical X
			 * axis on the portrait screen, so the glyph must be mirrored here
			 * to appear upright+un-mirrored. (The old MSB-first order produced
			 * individually X-flipped characters, confirmed on-device.) */
			for (gx = 0; gx < 8; gx++) {
				if (row & (0x01 << gx))
					bottom_fill_rect(cx + gx, y + gy, 1, 1, rgb565);
			}
		}
		cx += 8;
	}
}

/* Draw text using a REAL Descent game font (passed in) and replicate the
 * exact top-screen text styling — including its gradient/shading.
 *
 * How the top screen does it (gr_font_text -> ogl_ubitmapm_cs): the game font
 * is a 1-bit font whose glyphs are uploaded as a texture and then TINTED by a
 * foreground RGB via the GPU's LINEAR (bilinear) filter. The soft gradient on
 * e.g. "Abort game?" is that filtered edge anti-aliasing, tinted by the menu
 * prompt colour (29,29,47 — blue/purple). The bottom screen is a raw
 * framebuffer with no GPU filtering, so we reproduce the look by sampling a
 * 3x3 neighbourhood of each glyph's intensity bitmap and tinting (29,29,47) by
 * that normalised intensity. Lit glyph pixels store 63, transparent 255.
 *
 * f: the game font (MEDIUM1_FONT for the popup look, HUGE_FONT for the big
 *    bottom-screen text). fg_r/g/b are the foreground RGB (BTN_TEXT = 29/29/47
 *    to match "Abort game?"). Returns the pixel advance for caller positioning.
 */
/* Draw game-font glyphs at 1:1 (no downsample, no box-filter blur) so the
 * text is CRISP, tinted by g_fg_* (popup text = 29,29,47). The top-screen
 * "Abort game?" look is a 1-bit font tinted by the GPU; on the raw bottom
 * framebuffer we reproduce it by writing the tinted pixel for every lit glyph
 * bit. No intensity gradient, no smoothing — that was the cause of the fuzz. */
int bottom_print_gamefont(grs_font *f, int x, int y, const char *s)
{
	int cx = x;
	int gh, gw, letter;

	if (!g_bot_buf || !s || !f) return 0;

	gh = f->ft_h;
	for (; *s; s++) {
		unsigned char c = (unsigned char)*s;
		if (c < f->ft_minchar || c > f->ft_maxchar) { cx += f->ft_w; continue; }
		letter = c - f->ft_minchar;

		if (f->ft_flags & FT_PROPORTIONAL)
			gw = f->ft_widths[letter];
		else
			gw = f->ft_w;

		if (gh > 0 && gw > 0) {
			grs_bitmap *bmp = &f->ft_bitmaps[letter];
			grs_bitmap *par = &f->ft_parent_bitmap;
			int pw = par->bm_w;
			const ubyte *pdata = par->bm_data;
			int ox = bmp->bm_x, oy = bmp->bm_y;
			int gy, gx;
			uint16_t col = ((g_fg_r & 0x1F) << 11) |
			               ((g_fg_g & 0x3F) << 5) |
			               (g_fg_b & 0x1F);
			for (gy = 0; gy < gh; gy++) {
				for (gx = 0; gx < gw; gx++) {
					int iy = oy + gy, ix = ox + gx;
					if (iy < 0 || iy >= par->bm_h || ix < 0 || ix >= pw)
						continue;
					if (pdata[iy * pw + ix] == TRANSPARENCY_COLOR)
						continue;
					bottom_set_px(cx + gx, y + gy, col);
				}
			}
		}
		cx += gw;
	}
	return cx - x;
}

void bottom_get_dims(int *w, int *h)
{
	if (w) *w = BS_LOGICAL_W;
	if (h) *h = BS_LOGICAL_H;
}

/* --- lifecycle --- */

void bottom_screen_init(void)
{
	if (!g_inited) {
		u16 bw = 0, bh = 0;
		/* Single-buffered bottom: we own one framebuffer and just flush it
		 * each frame. Double buffering here caused the bottom to strobe
		 * (each gfxScreenSwapBuffers flips the drawable buffer and our
		 * per-frame re-fetch wrote to the wrong/back buffer). With one
		 * buffer there is nothing to swap — gfxFlushBuffers() pushes it. */
		gfxSetDoubleBuffering(GFX_BOTTOM, false);
		g_bot_buf = (u16 *)gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, &bw, &bh);
		g_bot_w = (int)bw;
		g_bot_h = (int)bh;
		g_mode = BS_MODE_OFF;
		g_inited = 1;
		g_bottom_active = 1;   /* our framebuffer is now owned + live */
		/* Register the suspend/resume hook so lid-close/reopen (and Home
		 * Menu) re-acquires the bottom framebuffer + picaGL top on wake. */
		aptHook(&g_bs_apt_cookie, bottom_screen_apt_hook, NULL);
	}
}

/* Re-acquire the bottom framebuffer after a system applet (e.g. swkbd) has
 * taken over the screen and returned. swkbd re-inits the GFX framebuffer on
 * exit, so the pointer cached in g_bot_buf may be stale; re-fetching it and
 * re-asserting single-buffering keeps bottom_screen_present() from touching
 * a dead/invalid LCD framebuffer. Safe to call even if nothing was taken
 * over. */
void bottom_screen_reacquire(void)
{
	u16 bw = 0, bh = 0;
	gfxSetDoubleBuffering(GFX_BOTTOM, false);
	g_bot_buf = (u16 *)gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, &bw, &bh);
	g_bot_w = (int)bw;
	g_bot_h = (int)bh;
	g_bottom_active = 1;
}

void bottom_set_mode(bs_mode_t m)
{
	g_mode = m;
	/* Paint static modes ONCE, here. NEVER repaint them every frame in
	 * present() — that was the global "CRT beam" (the whole bottom was being
	 * cleared + repainted 60x/sec in an older build). The OSK no longer uses
	 * this blitter — text entry goes through libctru's software keyboard
	 * (swkbd), which owns its own screen. The remaining modes are all static
	 * blanks drawn once. */
	switch (m) {
	case BS_MODE_OFF:
	case BS_MODE_CONSOLE:
	case BS_MODE_HUD:
	case BS_MODE_MAP:
		bottom_clear(0x0000);   /* static blank, drawn once */
		break;
	}
	gfxFlushBuffers();
}

bs_mode_t bottom_get_mode(void)
{
	return g_mode;
}

void bottom_screen_present(void)
{
	/* SAFETY: never touch the GPU unless (a) the bottom framebuffer is ours
	 * and live, (b) something actually changed, and (c) the system is NOT
	 * powering off. At power-off the OS tears down the GPU while this flush
	 * could still be mid-write, which is a Data Abort on the GPU/LCD
	 * register space. pglIsPoweredOff() is set by picaGL's
	 * APTHOOK_ONSUSPEND and by pglSetPoweredOff() in the exit path. */
	if (!g_bottom_active || !g_bot_buf)
		return;
	if (!g_bottom_dirty)
		return;
	if (pglIsPoweredOff())
		return;

	/* Re-check the power-off flag immediately before the GPU write. The
	 * APT hook can fire (on apt's own thread) between the guard above and
	 * gfxFlushBuffers(); gspWaitForVBlank() can also wedge once GSP is
	 * gone, so bail first. */
	if (pglIsPoweredOff())
		return;

	gspWaitForVBlank();
	gfxFlushBuffers();
	g_bottom_dirty = 0;
}

/* --- hit test --- */
int bottom_hit(int x, int y, int w, int h, const touchPosition *t)
{
	if (!t) return 0;
	if (!(hidKeysHeld() & KEY_TOUCH)) return 0;
	/* touchPosition.px/py are bottom-screen pixels already. */
	return (t->px >= x && t->px < x + w && t->py >= y && t->py < y + h) ? 1 : 0;
}

#ifdef __3DS__
/* Shared palette (3DS). RGB565.
 * Button TEXT styling is copied EXACTLY from the top-screen game font: the
 * "Abort game?" prompt (nm_messagebox) sets gr_set_fontcolor(
 * gr_find_closest_color_current(29,29,47)) and the GPU tints the font's
 * intensity bitmap by that RGB (the gradient/shading you see). We match it:
 * text = tinted (29,29,47) with anti-aliased gradient (see bottom_print_gamefont).
 * The button FILL is light so that medium-blue/purple text reads clearly. */
#define BTN_TEXT     0xFFFF   /* white fallback for the 8x8 blitter path */

#define BTN_BLUE    0x7BEF   /* light blue fill so (29,29,47) text reads clearly */
#define BTN_BLUE_HI 0x9DE7   /* lighter blue highlight (pressed) */
#define BTN_RED     0xF800   /* bright red fill (REC/stop recording) */
#define BTN_GREY    0x8C71   /* disabled/dim (light grey) */
/* Bevel edges match the top-screen "Abort game?" popup, drawn by
 * ui_draw_frame() (d1/ui/uidraw.c) with the colours from d1/ui/ui.c:
 *     CGREY   = gr_find_closest_color(45,45,45)
 *     CWHITE  = gr_find_closest_color(50,50,50)
 *     CBRIGHT = gr_find_closest_color(58,58,58)
 * i.e. all greys (45-58), never pure white. RGB565 equivalents below. */
#define POPUP_CGREY   0x2D85   /* (45,45,45) */
#define POPUP_CWHITE  0x3606   /* (50,50,50) */
#define POPUP_CBRIGHT 0x3B87   /* (58,58,58) */
#define NEAR_BLACK    0x0841   /* popup interior fill = BM_XRGB(1,1,1) */

#ifdef __3DS__
/* Bottom-screen background = the SAME marbled PCX the "Abort game?" / "Save
 * game" popups use (scores.pcx / scoresb.pcx, MENU_BACKGROUND_BITMAP in
 * newmenu.c). The popups draw it via show_fullscr()->gr_bitmap_scale_to(),
 * which SCALES the bitmap to fill the screen — that's why the popup is a
 * seamless slab with NO seam/lines. We must do the same: SCALE to fill
 * 320x240, never tile (tiling wraps at the PCX width and produced the seam;
 * and a full-screen frame rotated into 2 horizontal lines — both gone now).
 *
 * CPU-ONLY decoder (plain PHYSFS read + d_malloc). We do NOT use
 * gr/pcx_read_bitmap — that path allocates via gr_init_bitmap_alloc and,
 * under the -DOGL build, touches the GPU at load time, which Data-Aborted on
 * launch. Decoded once into a plain heap buffer; paint is idempotent. */
static ubyte *g_btn_bg_data = NULL;   /* rowsize * ph bytes of 8-bit indices */
static int   g_btn_bg_w = 0, g_btn_bg_h = 0, g_btn_bg_rs = 0;
static ubyte g_btn_bg_pal[768];       /* 256 * (r,g,b) 0..255 */
static int   g_btn_bg_loaded = 0;

static int bottom_decode_pcx(const char *fn, ubyte **out, int *ow, int *oh, int *ors, ubyte *pal)
{
	PHYSFS_file *f = PHYSFSX_openReadBuffered(fn);
	if (!f) return -1;
	ubyte hdr[128];
	if (PHYSFS_read(f, hdr, 1, 128) != 128) { PHYSFS_close(f); return -1; }
	if (hdr[0] != 10 || hdr[2] != 1 || hdr[3] != 8) { PHYSFS_close(f); return -1; }
	int xmin = hdr[4] | (hdr[5] << 8), xmax = hdr[8] | (hdr[9] << 8);
	int ymin = hdr[10] | (hdr[11] << 8), ymax = hdr[12] | (hdr[13] << 8);
	int w = xmax - xmin + 1, h = ymax - ymin + 1;
	int rs = w;
	ubyte *buf = (ubyte *)d_malloc((size_t)rs * h ? rs * h : 1);
	if (!buf) { PHYSFS_close(f); return -1; }
	int row, col, count;
	ubyte data;
	for (row = 0; row < h; row++) {
		ubyte *dst = buf + (size_t)row * rs;
		for (col = 0; col < w; ) {
			if (PHYSFS_read(f, &data, 1, 1) != 1) { d_free(buf); PHYSFS_close(f); return -1; }
			if ((data & 0xC0) == 0xC0) {
				count = data & 0x3F;
				if (PHYSFS_read(f, &data, 1, 1) != 1) { d_free(buf); PHYSFS_close(f); return -1; }
				while (count-- && col < w) { dst[col++] = data; }
			} else {
				dst[col++] = data;
			}
		}
	}
	if (PHYSFS_seek(f, PHYSFS_fileLength(f) - 769)) {
		ubyte p[769];
		if (PHYSFS_read(f, p, 1, 769) == 769 && p[0] == 0x0C)
			memcpy(pal, p + 1, 768);
	}
	PHYSFS_close(f);
	*out = buf; *ow = w; *oh = h; *ors = rs;
	return 0;
}

static void bottom_load_bg(void)
{
	if (g_btn_bg_loaded) return;
	g_btn_bg_loaded = 1;
	const char *fn = PHYSFSX_exists("scoresb.pcx", 1) ? "scoresb.pcx"
	                 : (PHYSFSX_exists("scores.pcx", 1) ? "scores.pcx" : NULL);
	if (!fn) return;
	bottom_decode_pcx(fn, &g_btn_bg_data, &g_btn_bg_w, &g_btn_bg_h, &g_btn_bg_rs, g_btn_bg_pal);
}

/* Paint the bottom screen base ONCE: the marbled PCX SCALED to fill the
 * whole 320x240 logical canvas (same as the popup's show_fullscr). No tiling,
 * no full-screen frame -> seamless slab, no seam, no horizontal lines. */
void bottom_fill_marble(void)
{
	if (g_bg_painted) return;   /* already painted — don't wipe the buttons */
	bottom_load_bg();
	if (!g_bot_buf) return;
	if (!g_btn_bg_data || g_btn_bg_w <= 0 || g_btn_bg_h <= 0) {
		bottom_clear(NEAR_BLACK);
		g_bg_painted = 1;
		return;
	}
	int pw = g_btn_bg_w, ph = g_btn_bg_h, prs = g_btn_bg_rs;
	/* SCALE: map each logical pixel to a source pixel (nearest-neighbour
	 * stretch), exactly like gr_bitmap_scale_to fills the screen. */
	for (int y = 0; y < BS_LOGICAL_H; y++) {
		int sy = (y * ph) / BS_LOGICAL_H;
		const ubyte *row = g_btn_bg_data + sy * prs;
		for (int x = 0; x < BS_LOGICAL_W; x++) {
			int sx = (x * pw) / BS_LOGICAL_W;
			int idx = row[sx];
			int r = g_btn_bg_pal[idx * 3 + 0];
			int g = g_btn_bg_pal[idx * 3 + 1];
			int b = g_btn_bg_pal[idx * 3 + 2];
			uint16_t c = (((r * 31) / 255) << 11) | (((g * 63) / 255) << 5) | ((b * 31) / 255);
			bottom_set_px(x, y, c);
		}
	}
	g_bg_painted = 1;
	g_bottom_dirty = 1;
}

/* Repaint JUST the marbled slab over a logical rect. Used to ERASE a previous
 * label before drawing a new one, so e.g. REC doesn't ghost under STOP when
 * the button toggles. Same scaled source as bottom_fill_marble(), so the
 * repainted patch is pixel-identical to the surrounding slab (no seam). */
static void bottom_repaint_marble_rect(int bx, int by, int bw, int bh)
{
	if (!g_bot_buf) return;
	if (!g_btn_bg_data || g_btn_bg_w <= 0 || g_btn_bg_h <= 0) return;
	int pw = g_btn_bg_w, ph = g_btn_bg_h, prs = g_btn_bg_rs;
	int y, x;
	for (y = by; y < by + bh && y < BS_LOGICAL_H; y++) {
		int sy = (y * ph) / BS_LOGICAL_H;
		const ubyte *row = g_btn_bg_data + sy * prs;
		for (x = bx; x < bx + bw && x < BS_LOGICAL_W; x++) {
			int sx = (x * pw) / BS_LOGICAL_W;
			int idx = row[sx];
			int r = g_btn_bg_pal[idx * 3 + 0];
			int g = g_btn_bg_pal[idx * 3 + 1];
			int b = g_btn_bg_pal[idx * 3 + 2];
			uint16_t c = (((r * 31) / 255) << 11) |
			             (((g * 63) / 255) << 5) |
			             ((b * 31) / 255);
			bottom_set_px(x, y, c);
		}
	}
	g_bottom_dirty = 1;
}

#else
/* (no-op stub kept for non-3DS builds that don't use the marble path) */
#endif

/* Width of a label as bottom_print_gamefont() will ACTUALLY draw it: the
 * sum of per-glyph advances at 1:1 (proportional widths when the font has
 * them). The downsample estimate used for positioning is NOT the drawn
 * width, so erasing based on it leaves glyph tails behind. */
static int gamefont_text_width(grs_font *f, const char *s)
{
	int w = 0;
	for (; *s; s++) {
		unsigned char c = (unsigned char)*s;
		if (c < f->ft_minchar || c > f->ft_maxchar) { w += f->ft_w; continue; }
		int letter = c - f->ft_minchar;
		w += (f->ft_flags & FT_PROPORTIONAL) ? f->ft_widths[letter] : f->ft_w;
	}
	return w;
}

/* Generic text-key drawing helper: paints the shared marbled slab ONCE
 * (g_bg_painted guard), then draws the label as BIG text in the popup's
 * (29,29,47) color. THE TEXT IS THE BUTTON — no rectangle, no fill, no
 * bezel is ever drawn around it. Drawn into the logical 320x240 space.
 *
 * last_bbox: caller-owned 4-int slot (x, y, w, h; -1 = none) holding the
 * TRUE drawn extent of the label previously drawn in this slot. Text can
 * overflow the button rect by a lot (STOP spans well past its 64px slot),
 * so the erase before each draw is the union of that previous extent and
 * the new label's extent — a rect-sized erase left the old tail visible
 * (STOP's trailing "P" next to REC). */
static void draw_key(int bx, int by, int bw, int bh,
                     uint16_t fill, const char *label, int *last_bbox)
{
	bottom_fill_marble();   /* paints the marble slab once */

	int disabled = (fill == BTN_GREY);
	/* Disabled = dim grey text ONLY (no box, no fill). */
	g_fg_r = disabled ? 110 : BTN_TEXT_R;
	g_fg_g = disabled ? 110 : BTN_TEXT_G;
	g_fg_b = disabled ? 130 : BTN_TEXT_B;

	g_bottom_dirty = 1;   /* button repaint pending */

	if (label && *label) {
		grs_font *f = (g_bot_buf && HUGE_FONT) ? HUGE_FONT : NULL;
		int fw = (int)strlen(label);
		int gt = 0, gh_draw = 0, tx = bx, ty = by;
		if (f && f->ft_w > 0) {
			/* The blitter (bottom_print_gamefont) draws at 1:1, so the
			 * TRUE drawn extent is gamefont_text_width(f,label) x f->ft_h.
			 * Center on THAT. The old code divided both by a width-fit
			 * estimate `div`, which varied per label width and shifted the
			 * vertical baseline — so "REC" landed higher than "STOP". */
			gt = gamefont_text_width(f, label);
			gh_draw = f->ft_h;
			tx = bx + (bw - gt) / 2;
			ty = by + (bh - gh_draw) / 2;
		} else {
			/* Fallback: 8x8 blitter. */
			gt = fw * 8;
			gh_draw = 8;
			tx = bx + (bw - gt) / 2;
			ty = by + (bh - 8) / 2;
		}

		/* TRUE drawn extent (1:1 glyphs), which is what must be erased. */
		int aw = (f && f->ft_w > 0) ? gamefont_text_width(f, label) : gt;
		int ah = (f && f->ft_w > 0) ? f->ft_h : gh_draw;

		/* Erase the UNION of the label previously drawn in this slot and
		 * the one about to be drawn. The old label's ink can overflow the
		 * button rect by a lot (STOP spans well past its 64px slot), and a
		 * rect-sized erase left its tail visible — the "RECP" ghost where
		 * STOP's trailing "P" survived under REC. last_bbox is -1 until the
		 * slot has drawn something. */
		int ex = tx, ey = ty, er = tx + aw, eb = ty + ah;
		if (last_bbox && last_bbox[2] > 0) {
			if (last_bbox[0] < ex) ex = last_bbox[0];
			if (last_bbox[1] < ey) ey = last_bbox[1];
			if (last_bbox[0] + last_bbox[2] > er) er = last_bbox[0] + last_bbox[2];
			if (last_bbox[1] + last_bbox[3] > eb) eb = last_bbox[1] + last_bbox[3];
		}
		if (ex < 0) ex = 0;
		if (ey < 0) ey = 0;
		if (er > BS_LOGICAL_W) er = BS_LOGICAL_W;
		if (eb > BS_LOGICAL_H) eb = BS_LOGICAL_H;
		bottom_repaint_marble_rect(ex, ey, er - ex, eb - ey);

		if (f && f->ft_w > 0)
			bottom_print_gamefont(f, tx, ty, label);
		else
			bottom_print(tx, ty, label, disabled ? 0x8410 : BTN_TEXT);

		/* Remember this slot's true drawn extent for the next erase. */
		if (last_bbox) {
			last_bbox[0] = tx; last_bbox[1] = ty;
			last_bbox[2] = aw; last_bbox[3] = ah;
		}
	} else {
		/* Empty label (disabled state): erase the union of the button rect
		 * and whatever this slot last drew, so an overflowing enabled label
		 * leaves no tail. */
		int ex = bx, ey = by, er = bx + bw, eb = by + bh;
		if (last_bbox && last_bbox[2] > 0) {
			if (last_bbox[0] < ex) ex = last_bbox[0];
			if (last_bbox[1] < ey) ey = last_bbox[1];
			if (last_bbox[0] + last_bbox[2] > er) er = last_bbox[0] + last_bbox[2];
			if (last_bbox[1] + last_bbox[3] > eb) eb = last_bbox[1] + last_bbox[3];
		}
		if (ex < 0) ex = 0;
		if (ey < 0) ey = 0;
		if (er > BS_LOGICAL_W) er = BS_LOGICAL_W;
		if (eb > BS_LOGICAL_H) eb = BS_LOGICAL_H;
		bottom_repaint_marble_rect(ex, ey, er - ex, eb - ey);
		if (last_bbox) {
			last_bbox[0] = last_bbox[1] = last_bbox[2] = last_bbox[3] = -1;
		}
	}
}

/* Pilot-list "Delete Pilot" on-screen button.
 *
 * The PC build deletes a pilot via the Ctrl+D keyboard combo, which the 3DS
 * has no physical key for. While the pilot SELECT listbox is up (top screen),
 * the bottom screen is free, so we draw a tappable "Delete Pilot" key there.
 * Caller polls this each EVENT_IDLE; it returns 1 on a *fresh* tap (edge-
 * triggered so one touch = one delete request). The caller then feeds the tap
 * into the existing Ctrl+D delete path (player_menu_keycommand) so the SAME
 * confirmation box + file deletion runs — no duplicated logic.
 *
 * enable: pass 0 to draw a disabled (grey) state when there is nothing to
 * delete (e.g. the "Create New Player" pseudo-entry is selected).
 *
 * The button is only redrawn when its visual STATE changes (enabled<->disabled
 * or first show), not every idle tick — this prevents the "undulating" shimmer
 * that constant per-frame redraw caused on the single-buffered bottom.
 */
static int g_del_btn_prev = 0;
static int g_del_btn_state = -1;   /* -1 = not yet drawn */
static int g_del_btn_bbox[4] = {-1,-1,-1,-1};
/* Logical canvas is 320x240 but only x[0..239] is visible after the 90deg-CW
 * rotation (fy = x, so x>=240 wraps/clips). Three 74px buttons fit in the
 * visible 240-wide strip (4 + 74 + 2 + 74 + 2 + 74 = 230) and sit flush at
 * the bottom (by=210, bh=30 -> y 210..240). All share by=210 so their text
 * baselines are identical. */
static const int DEL_BX = 54, DEL_BY = 210, DEL_BW = 212, DEL_BH = 30;
int bottom_pilot_delete_tapped(int enable)
{
	/* Redraw only on state change (enable toggled, or never drawn yet). */
	if (enable != g_del_btn_state) {
		g_del_btn_state = enable;
		draw_key(DEL_BX, DEL_BY, DEL_BW, DEL_BH,
		         enable ? BTN_BLUE : BTN_GREY,
		         enable ? "DELETE PILOT" : "", g_del_btn_bbox);
	}

	/* Edge-triggered tap test. */
	touchPosition t;
	hidTouchRead(&t);
	int held = (hidKeysHeld() & KEY_TOUCH) && bottom_hit(DEL_BX, DEL_BY, DEL_BW, DEL_BH, &t);
	int tapped = held && !g_del_btn_prev;
	g_del_btn_prev = held;

	return tapped ? 1 : 0;
}

/* Reset dirty-state so the next open of the pilot list redraws fresh. */
void bottom_pilot_delete_reset(void)
{
	g_del_btn_state = -1;
	g_del_btn_prev = 0;
}

/* Demo-list "Delete Demo" on-screen button (3DS analogue of the pilot one
 * above). The PC build deletes a demo with the Ctrl+D combo, which the 3DS
 * has no key for. While the demo SELECT listbox is up (top screen), draw a
 * tappable "Delete Demo" key on the bottom; on a fresh tap feed the identical
 * Ctrl+D command into demo_menu_keycommand() so the SAME confirmation box +
 * file deletion + listbox item removal runs. enable=0 (disabled/grey) when
 * nothing is selected (citem < 0). Mirrors bottom_pilot_delete_tapped(). */
static int g_demo_del_btn_prev = 0;
static int g_demo_del_btn_state = -1;
static int g_demo_del_btn_bbox[4] = {-1,-1,-1,-1};
int bottom_demo_delete_tapped(int enable)
{
	if (enable != g_demo_del_btn_state) {
		g_demo_del_btn_state = enable;
		draw_key(DEL_BX, DEL_BY, DEL_BW, DEL_BH,
		         enable ? BTN_BLUE : BTN_GREY,
		         enable ? "DELETE DEMO" : "", g_demo_del_btn_bbox);
	}

	/* Edge-triggered tap test. */
	touchPosition t;
	hidTouchRead(&t);
	int held = (hidKeysHeld() & KEY_TOUCH) && bottom_hit(DEL_BX, DEL_BY, DEL_BW, DEL_BH, &t);
	int tapped = held && !g_demo_del_btn_prev;
	g_demo_del_btn_prev = held;

	return tapped ? 1 : 0;
}

/* Reset dirty-state so the next open of the demo list redraws fresh. */
void bottom_demo_delete_reset(void)
{
	g_demo_del_btn_state = -1;
	g_demo_del_btn_prev = 0;
}

/* In-game "MENU" button (bottom-center). Shown persistently while a game is
 * running so the player can open the in-game PAUSE menu (where Save/Load/
 * Options/Resume live) — the 3DS has no ESC/PAUSE key. The caller taps this
 * into do_game_pause() (NOT window_close: closing Game_wind would hard-exit
 * the game with no way back). Poll each EVENT_IDLE; returns 1 on a fresh tap. */
static int g_menu_btn_prev = 0;
static int g_menu_btn_state = -1;
static int g_menu_btn_bbox[4] = {-1,-1,-1,-1};
static const int MEN_BX = 216, MEN_BY = 210, MEN_BW = 96, MEN_BH = 30;
int bottom_menu_tapped(void)
{
	if (g_menu_btn_state != 1) {
		g_menu_btn_state = 1;
		draw_key(MEN_BX, MEN_BY, MEN_BW, MEN_BH, BTN_BLUE, "MENU", g_menu_btn_bbox);
	}

	touchPosition t;
	hidTouchRead(&t);
	int held = (hidKeysHeld() & KEY_TOUCH) && bottom_hit(MEN_BX, MEN_BY, MEN_BW, MEN_BH, &t);
	int tapped = held && !g_menu_btn_prev;
	g_menu_btn_prev = held;

	return tapped ? 1 : 0;
}

/* In-game \"SAVE\" button (bottom-left). Shown persistently while a game is
 * running so the player can save progress without a keyboard (PC uses F2).
 * Equivalent to state_save_all(0): opens the top-screen save-slot menu.
 * The caller taps this into state_save_all(0) — the SAME safe in-game path
 * the keyboard F2 uses; it must NOT run from the main menu (no game loaded
 * -> state_save_all crashes). Poll each EVENT_IDLE; returns 1 on a fresh tap. */
static int g_save_btn_prev = 0;
static int g_save_btn_state = -1;
static int g_save_btn_bbox[4] = {-1,-1,-1,-1};
static const int SAV_BX = 8, SAV_BY = 210, SAV_BW = 96, SAV_BH = 30;
int bottom_save_tapped(void)
{
	if (g_save_btn_state != 1) {
		g_save_btn_state = 1;
		draw_key(SAV_BX, SAV_BY, SAV_BW, SAV_BH, BTN_BLUE, "SAVE", g_save_btn_bbox);
	}

	touchPosition t;
	hidTouchRead(&t);
	int held = (hidKeysHeld() & KEY_TOUCH) && bottom_hit(SAV_BX, SAV_BY, SAV_BW, SAV_BH, &t);
	int tapped = held && !g_save_btn_prev;
	g_save_btn_prev = held;

	return tapped ? 1 : 0;
}

void bottom_save_reset(void)
{
	g_save_btn_state = -1;
	g_save_btn_prev = 0;
}

/* In-game "REC"/"STOP" toggle button (bottom-center-left). The 3DS has no F5
 * key, so demo recording (PC: F5) is otherwise unreachable. Tapping it starts
 * recording when idle, or stops it (which then prompts for a save name via the
 * now-OSK-wired NM_TYPE_INPUT dialog) when already recording. The label flips
 * REC<->STOP to reflect Newdemo_state. Poll each EVENT_IDLE; returns 1 on a
 * fresh tap so the caller performs the actual start/stop toggle. */
static int g_rec_btn_prev = 0;
static int g_rec_btn_state = -1;
static int g_rec_btn_bbox[4] = {-1,-1,-1,-1};
static const int REC_BX = 112, REC_BY = 210, REC_BW = 96, REC_BH = 30;
int bottom_rec_tapped(void)
{
	int recording = (Newdemo_state == ND_STATE_RECORDING);
	const char *label = recording ? "STOP" : "REC";

	if (g_rec_btn_state != (recording ? 1 : 0)) {
		g_rec_btn_state = recording ? 1 : 0;
		draw_key(REC_BX, REC_BY, REC_BW, REC_BH,
		         recording ? BTN_RED : BTN_BLUE, label, g_rec_btn_bbox);
	}

	touchPosition t;
	hidTouchRead(&t);
	int held = (hidKeysHeld() & KEY_TOUCH) && bottom_hit(REC_BX, REC_BY, REC_BW, REC_BH, &t);
	int tapped = held && !g_rec_btn_prev;
	g_rec_btn_prev = held;

	return tapped ? 1 : 0;
}

void bottom_rec_reset(void)
{
	g_rec_btn_state = -1;
	g_rec_btn_prev = 0;
}

/* High-scores "Reset Scores" on-screen button (3DS analogue of the PC
 * Ctrl+R combo). The scores screen says "Press CTRL+R to reset", which the
 * 3DS cannot. While the scores window is up (top screen), draw a tappable
 * "Reset Scores" key on the bottom; on a fresh tap the caller feeds the
 * identical Ctrl+R command into scores_handler's existing reset path (same
 * confirmation box + file delete + scores reload). enable=0 (grey) when a
 * score row is highlighted — the PC combo only works with none highlighted,
 * and the reset text is only shown then, so the button mirrors that. */
static int g_scores_rst_btn_prev = 0;
static int g_scores_rst_btn_state = -1;
static int g_scores_rst_btn_bbox[4] = {-1,-1,-1,-1};
int bottom_scores_rst_tapped(int enable)
{
	if (enable != g_scores_rst_btn_state) {
		g_scores_rst_btn_state = enable;
		draw_key(DEL_BX, DEL_BY, DEL_BW, DEL_BH,
		         enable ? BTN_BLUE : BTN_GREY,
		         enable ? "RESET SCORES" : "", g_scores_rst_btn_bbox);
	}

	/* Edge-triggered tap test. */
	touchPosition t;
	hidTouchRead(&t);
	int held = (hidKeysHeld() & KEY_TOUCH) && bottom_hit(DEL_BX, DEL_BY, DEL_BW, DEL_BH, &t);
	int tapped = held && !g_scores_rst_btn_prev;
	g_scores_rst_btn_prev = held;

	return tapped ? 1 : 0;
}

/* Reset dirty-state so the next open of the scores screen redraws fresh. */
void bottom_scores_rst_reset(void)
{
	g_scores_rst_btn_state = -1;
	g_scores_rst_btn_prev = 0;
}

void bottom_menu_reset(void)
{
	g_menu_btn_state = -1;
	g_menu_btn_prev = 0;
}

/* --- Top-row in-game button: HUD toggle. The 3D depth slider (hardware)
 * handles stereo separation; only PARALLEL mode is used (toe-in removed). */

#define TOP_BY 4
#define TOP_BH 30

/* HUD toggle button (far left). */
static int g_hud_btn_prev = 0;
static int g_hud_btn_state = -1;
static int g_hud_btn_bbox[4] = {-1,-1,-1,-1};
#define HUD_BX 8, HUD_BY, HUD_BW, HUD_BH
static const int HUD_BY = TOP_BY, HUD_BW = 52, HUD_BH = TOP_BH;
int bottom_hud_tapped(void)
{
	if (g_hud_btn_state != 1) {
		g_hud_btn_state = 1;
		draw_key(HUD_BX, BTN_BLUE, "HUD", g_hud_btn_bbox);
	}
	touchPosition t;
	hidTouchRead(&t);
	int held = (hidKeysHeld() & KEY_TOUCH) && bottom_hit(HUD_BX, &t);
	int tapped = held && !g_hud_btn_prev;
	g_hud_btn_prev = held;
	return tapped ? 1 : 0;
}
void bottom_hud_reset(void)
{
	g_hud_btn_state = -1;
	g_hud_btn_prev = 0;
}

/* --- HUD toggle (top-left) stays; stereo buttons removed: the 3D slider
 * handles separation, and only PARALLEL mode is used. --- */
#else
/* Non-3DS builds: no bottom-screen buttons; Ctrl+D / ESC are real keys there. */
static inline int bottom_pilot_delete_tapped(int enable) { (void)enable; return 0; }
static inline void bottom_pilot_delete_reset(void) {}
static inline int bottom_menu_tapped(void) { return 0; }
static inline void bottom_menu_reset(void) {}
#endif
