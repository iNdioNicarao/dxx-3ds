/*
 * Pilot-name entry (3DS).
 *
 * Implemented with libctru's software keyboard (swkbd) — see osk.c. The system
 * applet pops up only when text is needed, handles editing/backspace/OK-Cancel
 * natively, and styles itself. It does NOT own the bottom screen as a permanent
 * fixture (that was the old hand-rolled grid, now removed).
 *
 * Pilot DELETION is a listbox-menu function (Ctrl+D in player_menu_keycommand),
 * not part of text entry, so it is unaffected by this module.
 */

#ifndef _OSK_H_
#define _OSK_H_

#include <3ds.h>

/*
 * Options for osk_modal_loop_ex(). All fields optional; zero-initialize and
 * set only what you need.
 */
typedef struct {
	int numeric_only;    /* 1 = numpad + digits only (ports, levels, etc.) */
	int allow_empty;     /* 1 = let the user confirm a blank string (else
	                      *     SWKBD_NOTEMPTY blocks confirm until typed) */
	const char *prompt;  /* hint text shown above the field (NULL = default) */
} osk_opts_t;

/*
 * Blocking text entry via libctru's software keyboard (swkbd). Fills buf
 * (size maxlen, including NUL) with the typed/edited text and returns 1 on
 * confirm, 0 on cancel (buf cleared so the caller can fall back). The existing
 * contents of buf are shown as the initial text so the field can be EDITED,
 * not just replaced. opts may be NULL for defaults (QWERTY, not-empty,
 * default prompt).
 */
int osk_modal_loop_ex(char *buf, int maxlen, const osk_opts_t *opts);

/*
 * Legacy pilot-name entry (kept for source compatibility). Equivalent to
 * osk_modal_loop_ex(buf, maxlen, NULL) with a fixed pilot-name prompt.
 */
int osk_modal_loop(char *buf, int maxlen);

#endif /* _OSK_H_ */
