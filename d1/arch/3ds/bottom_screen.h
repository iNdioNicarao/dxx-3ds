/*
 * Bottom-screen (3DS) owner + mode state machine.
 *
 * The bottom LCD is a MODE-SWITCHED second display. All consumers
 * (debug console, HUD, automap) go through this single owner via
 * bottom_set_mode(); bottom_screen_present() is the one place that draws
 * per frame. This prevents two code paths from writing the same framebuffer
 * (which corrupts it).
 *
 * Text entry (pilot name) does NOT use this blitter — it uses libctru's
 * software keyboard (swkbd) via osk_modal_loop() in osk.c, which owns its own
 * screen. See osk.h / osk.c.
 */

#ifndef _BOTTOM_SCREEN_H_
#define _BOTTOM_SCREEN_H_

#include <3ds.h>

/* Logical bottom-screen dimensions (landscape): the OSK/layout space.
 * The raw framebuffer is 240x320 (portrait); bottom_set_px rotates it.
 * Shared here so osk.c and bottom_screen.c agree on the logical canvas. */
#define BS_LOGICAL_W 320
#define BS_LOGICAL_H 240

/* Display modes for the bottom screen. Only one is active at a time. */
typedef enum {
	BS_MODE_OFF = 0,   /* nothing drawn (blank) */
	BS_MODE_CONSOLE,  /* debug console (toggleable, not scrapped) */
	BS_MODE_HUD,      /* ship status HUD */
	BS_MODE_MAP       /* automap / overhead view */
} bs_mode_t;

/* Lifecycle */
void bottom_screen_init(void);
void bottom_screen_reacquire(void);   /* after swkbd/applet returns */
void bottom_screen_present(void);   /* call ONCE per frame from main loop */

/* Mode control */
void bottom_set_mode(bs_mode_t m);
bs_mode_t bottom_get_mode(void);

/* Glyph / primitive API (RGB565-aware). Implemented in bottom_screen.c. */
void bottom_clear(uint16_t rgb565);
void bottom_print(int x, int y, const char *s, uint16_t rgb565);
void bottom_fill_rect(int x, int y, int w, int h, uint16_t rgb565);
void bottom_get_dims(int *w, int *h);

/*
 * Hit-test: is the touch position inside the rect (x,y,w,h) in bottom-screen
 * pixel coordinates? Returns 1 if the touch is currently down AND inside the
 * rect, else 0. Coordinates are bottom-screen space (320x240). Touch-space
 * (touchPosition.px/py) is already in bottom-screen pixels, so no transform
 * is needed.
 *
 * Defined in bottom_screen.c (later phase); declared here so touch consumers
 * can be written now.
 */
int bottom_hit(int x, int y, int w, int h, const touchPosition *t);

/* Pilot-list "Delete Pilot" on-screen button (3DS only). Draws a tappable key
 * on the bottom screen while the pilot SELECT listbox is up; returns 1 on a
 * fresh tap. enable=0 draws a disabled (grey) state. The caller routes the tap
 * into the existing Ctrl+D delete path. reset() clears dirty-state for next open. */
int bottom_pilot_delete_tapped(int enable);
void bottom_pilot_delete_reset(void);
int bottom_demo_delete_tapped(int enable);
void bottom_demo_delete_reset(void);

/* In-game "MENU" on-screen button (3DS only). Shown persistently while a game
 * runs; returns 1 on a fresh tap so the caller can open the PAUSE menu
 * (do_game_pause) — NOT hard-exit the game. reset() clears dirty-state. */
int bottom_menu_tapped(void);
void bottom_menu_reset(void);

/* In-game "SAVE" on-screen button (3DS only). Shown persistently while a game
 * runs; returns 1 on a fresh tap so the caller can save progress via
 * state_save_all(0) - the 3DS equivalent of PC's F2. Must NOT be used from the
 * main menu (no game loaded... save only valid mid-level). reset() clears
 * dirty-state. */
int bottom_save_tapped(void);
void bottom_save_reset(void);

/* In-game "REC"/"STOP" on-screen button (3DS only). The 3DS has no F5 key, so
 * demo recording (which on PC is F5) is unreachable without this. Tapping it
 * toggles recording: starts newdemo_start_recording() when idle, stops it
 * (prompting for a save name) when already recording. The label flips between
 * REC and STOP to reflect Newdemo_state. reset() clears dirty-state. */
int bottom_rec_tapped(void);
void bottom_rec_reset(void);

/* High-scores "Reset Scores" on-screen button (3DS only). The scores screen
 * shows "Press CTRL+R to reset", which the 3DS cannot; this is the tappable
 * equivalent. enable=1 only when no score row is highlighted (matching the
 * PC combo's precondition). Returns 1 on a fresh tap; the caller feeds the
 * identical Ctrl+R command into scores_handler's existing reset path.
 * reset() clears dirty-state for the next open of the scores screen. */
int bottom_scores_rst_tapped(int enable);
void bottom_scores_rst_reset(void);

/* In-game "RESUME" on-screen button (3DS only). Shown while the game is
 * PAUSED; returns 1 on a fresh tap so the caller can close the pause window
 * and resume. reset() clears dirty-state. */
int bottom_resume_tapped(void);
void bottom_resume_reset(void);

/* Draw a string using the real Descent game font (GAME_FONT) into the bottom
 * buffer (3DS only). Falls back to nothing if the font isn't loaded yet.
 * Mirrors the 8x8 blitter's X-orientation so glyphs stay upright after the
 * bottom-screen 90deg-CW rotation. Returns pixel advance width. */
/* (internal to bottom_screen.c — defined there; not prototyped here) */

#endif /* _BOTTOM_SCREEN_H_ */
