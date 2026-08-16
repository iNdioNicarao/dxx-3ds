/*
 * On-screen keyboard (3DS).
 *
 * Uses libctru's software keyboard (swkbd) system applet for all text
 * entry (pilot name, save name, demo filename, ...). swkbd handles
 * editing, backspace and confirm/cancel itself and renders on both
 * screens; the field's current contents are offered as editable text.
 *
 * swkbd runs its own applet thread and services HOME/POWER itself, so
 * osk_modal_loop() just calls swkbdInputText() and waits. On return the
 * app re-acquires the GPU state swkbd displaced (see below).
 */

#include "osk.h"
#include <3ds.h>
#include <SDL/SDL.h>
#include <string.h>
#include <stdio.h>
#include "gr.h"                   /* grs_bitmap (needed before texmerge.h) */
#include <GL/picaGL.h>          /* pglReacquire() — GPU state replay */
#include "bottom_screen.h"      /* bottom_screen_reacquire() — framebuffer rebind */
#include "texmerge.h"            /* texmerge_flush() — wall/texture cache reload */
#include "ogl_init.h"            /* ogl_invalidate_textures() */


/* Default prompt when a caller does not supply one. */
#define OSK_PROMPT_DEFAULT "Enter text"

/*
 * Blocking text entry. Fills buf (size maxlen, including NUL). Returns 1 on
 * confirm, 0 on cancel (buf cleared so the caller can fall back). The initial
 * contents of buf are offered as editable text. opts may be NULL.
 */
int osk_modal_loop_ex(char *buf, int maxlen, const osk_opts_t *opts)
{
	if (!buf || maxlen < 2) return 0;

	const char *prompt = (opts && opts->prompt) ? opts->prompt : OSK_PROMPT_DEFAULT;
	int numeric = opts && opts->numeric_only;
	int allow_empty = opts && opts->allow_empty;
	SwkbdState swkbd;
	SwkbdType type = numeric ? SWKBD_TYPE_NUMPAD : SWKBD_TYPE_QWERTY;
	swkbdInit(&swkbd, type, 1, (u16)(maxlen - 1));
	swkbdSetHintText(&swkbd, prompt);

	/* Prefill with the field's current value so it can be edited. */
	if (buf[0] != 0)
		swkbdSetInitialText(&swkbd, buf);

	/* Validation: block confirm-until-typed unless empty is allowed. */
	if (!allow_empty)
		swkbdSetValidation(&swkbd, SWKBD_NOTEMPTY, 0, 0);
	else
		swkbdSetValidation(&swkbd, 0, 0, 0);

	/* Darken the top screen and keep POWER allowed. SWKBD_ALLOW_HOME is
	 * deliberately NOT set: HOME during the keyboard triggers the APT
	 * suspend hook, and if the power-off flag is never cleared every
	 * later bottom_screen_present() bails and the screens freeze. */
	swkbdSetFeatures(&swkbd, SWKBD_DARKEN_TOP_SCREEN | SWKBD_ALLOW_POWER);

	char tmp[256];
	memset(tmp, 0, sizeof(tmp));

	/* Audio around swkbd: pause SDL's mixing for the duration of the
	 * applet, then unpause. The 3DS SDL audio driver owns NDSP (it
	 * registers the NDSP callback and a DSP hook itself), so NDSP must
	 * NOT be torn down or re-armed here: nulling or replacing its
	 * callback silences audio permanently, and ndspExit()/ndspInit()
	 * around the applet races the DSP teardown/re-init and crashes.
	 * libctru's NDSP layer handles the DSP sleep/wake caused by the
	 * applet internally. */
	if (SDL_WasInit(SDL_INIT_AUDIO)) {
		SDL_PauseAudio(1);
	}

	SwkbdButton res = swkbdInputText(&swkbd, tmp, sizeof(tmp));

	/* GPU recovery after the applet. Do NOT call gspExit()/gspInit()
	 * here: recreating libctru's GSP thread resets the framebuffer
	 * register mappings, which corrupts the bottom screen. Instead
	 * replay picaGL's cached state (same recovery APTHOOK_ONRESTORE /
	 * game_leave_menus() use) and re-grab the framebuffers. */
	pglReacquire();                  /* rebind picaGL queue + replay state */
	bottom_screen_reacquire();       /* rebind bottom framebuffer */
	texmerge_flush();                /* reload wall/texture cache */
	ogl_invalidate_textures();       /* drop GL handles: next draw re-uploads */

	if (SDL_WasInit(SDL_INIT_AUDIO)) {
		SDL_PauseAudio(0);
	}

	if (res == SWKBD_BUTTON_CONFIRM) {
		/* Confirmed (right/OK button). Copy back, clamped to maxlen-1. */
		tmp[sizeof(tmp) - 1] = 0;
		strncpy(buf, tmp, maxlen - 1);
		buf[maxlen - 1] = 0;
		if (buf[0] == 0 && !allow_empty) return 0;  /* guarded by NOTEMPTY */
		return 1;
	}

	/* Cancel / POWER / any non-confirm path -> empty name. */
	buf[0] = 0;
	return 0;
}

/*
 * Legacy pilot-name entry (source-compatible with earlier callers). Behaves
 * like osk_modal_loop_ex with a pilot-name prompt and default options.
 */
int osk_modal_loop(char *buf, int maxlen)
{
	osk_opts_t opts;
	memset(&opts, 0, sizeof(opts));
	opts.prompt = "Enter pilot name";
	return osk_modal_loop_ex(buf, maxlen, &opts);
}
