/*
 *
 * SDL Event related stuff
 *
 *
 */

#include <stdio.h>
#ifdef __3DS__
#include <3ds.h>
#endif
#include "console.h"
#include <stdlib.h>
#include "event.h"
#include "key.h"
#include "mouse.h"
#include "window.h"
#include "timer.h"
#include "config.h"

#include "joy.h"

extern void key_handler(SDL_KeyboardEvent *event);
extern void mouse_button_handler(SDL_MouseButtonEvent *mbe);
extern void mouse_motion_handler(SDL_MouseMotionEvent *mme);
extern void mouse_cursor_autohide();
extern int state_quick_save(void);
extern int state_quick_load(void);
extern int Automap_active;
extern void show_controls_3ds(void);
extern void cycle_cockpit_next(void);
extern void cycle_cockpit_prev(void);

// Emit a synthetic key as a clean pulse (down then up) so no key state lingers.
// key_handler() keeps sticky keyd_pressed[] state, and modal dialogs (e.g. the
// abort confirm box) can pause the main loop and starve our keyup -- leaving
// the key "stuck down" and swallowing the next press. Pulsing avoids that.
static void send_key_pulse(SDLKey sym) {
	SDL_KeyboardEvent d, r;
	d.type = SDL_KEYDOWN; d.state = SDL_PRESSED;
	d.keysym.scancode = 0; d.keysym.sym = sym; d.keysym.unicode = 0;
	key_handler(&d);
	r.type = SDL_KEYUP; r.state = SDL_RELEASED;
	r.keysym.scancode = 0; r.keysym.sym = sym; r.keysym.unicode = 0;
	key_handler(&r);
}

static int initialised=0;

// Deferred quick-save/load flags. We must NOT call state_quick_save()/
// state_quick_load() from inside event_poll()'s SDL_PollEvent loop: those
// rebuild the level/window stack (StartNewLevelSub, window_set_visible, SD
// file reads) mid-dispatch, which corrupts event/window state and breaks
// input after a quick-load. So the HID loop only SETS these flags; they are
// acted on once, after event_poll() returns (see event_process).
static int pending_quick_save = 0;
static int pending_quick_load = 0;

typedef struct d_event_joystick_moved
{
	event_type	type;	// EVENT_JOYSTICK_MOVED
	int		axis;
	int 		value;
} d_event_joystick_moved;

void event_poll()
{
	SDL_Event event;
	int clean_uniframe=1;
	window *wind = window_get_front();
	int idle = 1;
	
	// If the front window changes, exit this loop, otherwise unintended behavior can occur
	// like pressing 'Return' really fast at 'Difficulty Level' causing multiple games to be started
	while ((wind == window_get_front()) && SDL_PollEvent(&event))
	{
		switch(event.type) {
			case SDL_KEYDOWN:
			case SDL_KEYUP:
				if (clean_uniframe)
					memset(unicode_frame_buffer,'\0',sizeof(unsigned char)*KEY_BUFFER_SIZE);
				clean_uniframe=0;
				key_handler((SDL_KeyboardEvent *)&event);
				idle = 0;
				break;
			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP:
				mouse_button_handler((SDL_MouseButtonEvent *)&event);
				idle = 0;
				break;
			case SDL_MOUSEMOTION:
				mouse_motion_handler((SDL_MouseMotionEvent *)&event);
				idle = 0;
				break;
#ifndef __3DS__
			case SDL_JOYBUTTONDOWN:
			case SDL_JOYBUTTONUP:
				joy_button_handler((SDL_JoyButtonEvent *)&event);
				idle = 0;
				break;
#endif
			case SDL_JOYAXISMOTION:
#ifndef __3DS__
				if (joy_axis_handler((SDL_JoyAxisEvent *)&event))
					idle = 0;
#endif
				break;
			case SDL_JOYHATMOTION:
				joy_hat_handler((SDL_JoyHatEvent *)&event);
				idle = 0;
				break;
			case SDL_JOYBALLMOTION:
				break;
			case SDL_QUIT: {
				d_event qevent = { EVENT_QUIT };
				call_default_handler(&qevent);
				idle = 0;
			} break;
		}
	}

#ifdef __3DS__
	hidScanInput();
	static u32 old_keys = 0;
	u32 current_keys = hidKeysHeld();
	u32 kDown = current_keys & ~old_keys;
	u32 kUp = ~current_keys & old_keys;
	old_keys = current_keys;

	extern window *Game_wind;
	int in_game = (window_get_front() == Game_wind) && (Game_wind != NULL);

	// New 3DS input mapping (final scheme).
	// Gameplay actions map to the REAL kc_joystick button numbers (kc_joystick[],
	// d1/main/kconfig.c): 0=Slide right,3=Slide left,2=Accelerate,1=Reverse,
	// 5=Fire primary,6=Drop bomb,7=Fire flare,8=Rear view,12=Slide up,13=Slide down,9=Automap.
	// C-Stick -> Slide axes (see kconfig axis remap). Circle Pad -> Turn/Pitch axes.
	// Menus only understand keyboard KEY_* commands (see newmenu.c), so menu
	// navigation keys are emitted where needed. START emits Enter: this both
	// skips briefing screens (titles.c advances on KEY_ENTER) and opens the
	// in-game pause/menu. Weapon cycle uses the standard Descent keys X/Y.
	struct { u32 mask; int joy_btn; SDLKey menu_key; } btn_map[] = {
		{ (1<<0),  0, SDLK_RETURN },  // A      -> Slide right (game) / Select (menu)
		{ (1<<11), 3, SDLK_UNKNOWN },  // Y      -> Slide left
		{ (1<<10), 2, SDLK_UNKNOWN },  // X      -> Accelerate
		{ (1<<1),  1, SDLK_ESCAPE },   // B      -> Reverse (game) / Back (menu)
		{ (1<<8),  5, SDLK_UNKNOWN },  // R      -> Fire primary
		{ (1<<9),  6, SDLK_UNKNOWN },  // L      -> Drop bomb
		{ (1<<15), 7, SDLK_UNKNOWN },  // ZR     -> Fire flare
		{ (1<<14), 8, SDLK_UNKNOWN },  // ZL     -> Rear view
		{ (1<<6), 12, SDLK_UP    },    // D-UP   -> Slide up (vertical) / menu up
		{ (1<<7), 13, SDLK_DOWN  },    // D-DOWN -> Slide down (vertical) / menu down
		{ (1<<4), -1, SDLK_RIGHT },    // D-RIGHT-> Next weapon (keys X) / menu right
		{ (1<<5), -1, SDLK_LEFT  },    // D-LEFT -> Prev weapon (keys Y) / menu left
		{ (1<<2),  9, SDLK_UNKNOWN },  // SELECT -> Automap
		{ (1<<3), -1, SDLK_UNKNOWN },   // START  -> handled below (Esc, in-game + menus)
		{ 0, -1, SDLK_UNKNOWN }
	};

	// D-RIGHT/D-LEFT weapon cycling via Descent's standard cycle keys (X/Y).
	// START -> Esc: opens the in-game "Abort Game?" dialog and backs out of
	// menus. Emitted as a pulse so it works every press (and through modals).
	// START+X = name-free quick save, START+Y = name-free quick load (no kbd).
	for (int i = 0; btn_map[i].mask != 0; i++) {
		if (kDown & btn_map[i].mask) {
			if (btn_map[i].mask == (1<<3)) {
				if (current_keys & (1<<10))
					pending_quick_save = 1;
				else if (current_keys & (1<<11))
					pending_quick_load = 1;
				else if (current_keys & (1<<9))
					show_controls_3ds();
				else
					send_key_pulse(SDLK_ESCAPE);
				idle = 0;
				} else if (btn_map[i].mask == (1<<4)) {
					send_key_pulse(SDLK_x); idle = 0;
				} else if (btn_map[i].mask == (1<<5)) {
					send_key_pulse(SDLK_y); idle = 0;
				}
				// Cockpit view-cycle: hold START + tap R (next) / L (prev).
				// SELECT alone stays Automap; SELECT+dpad is unused to avoid
				// the automap-vs-cycle conflict.
				if (btn_map[i].mask == (1<<3)) {
					if (current_keys & (1<<8))		// START + R = next view
						cycle_cockpit_next();
					else if (current_keys & (1<<9))	// START + L = prev view
						cycle_cockpit_prev();
						}
						}
						}

	for (int i = 0; btn_map[i].mask != 0; i++) {
		if (kDown & btn_map[i].mask) {
			// On the automap, L/R act as F9/F10 (zoom) instead of their
			// gameplay actions (drop bomb / fire primary), which are unused
			// in the modal map window.
			if (Automap_active && (btn_map[i].mask == (1<<9))) {  // L
				send_key_pulse(SDLK_F9); idle = 0; continue;
			}
			if (Automap_active && (btn_map[i].mask == (1<<8))) {  // R
				send_key_pulse(SDLK_F10); idle = 0; continue;
			}
			if (btn_map[i].joy_btn != -1) {
				SDL_JoyButtonEvent jbe;
				jbe.type = SDL_JOYBUTTONDOWN;
				jbe.which = 0;
				jbe.button = btn_map[i].joy_btn;
				jbe.state = SDL_PRESSED;
				joy_button_handler(&jbe);
			}
			// Menu navigation keys (A=Enter, B=Esc, D-pad arrows) are pulsed
			// so they register once per press and never stick.
			if (btn_map[i].menu_key != SDLK_UNKNOWN && !in_game) {
				send_key_pulse(btn_map[i].menu_key);
			}
			idle = 0;
		}
		if (kUp & btn_map[i].mask) {
			if (Automap_active && (btn_map[i].mask == (1<<9) || btn_map[i].mask == (1<<8)))
				continue;  // no joy-button release needed when repurposed
			if (btn_map[i].joy_btn != -1) {
				SDL_JoyButtonEvent jbe;
				jbe.type = SDL_JOYBUTTONUP;
				jbe.which = 0;
				jbe.button = btn_map[i].joy_btn;
				jbe.state = SDL_RELEASED;
				joy_button_handler(&jbe);
			}
		}
	}

	circlePosition cpad, cstick;
	hidCircleRead(&cpad);
	hidCstickRead(&cstick);

	int axes[4] = { cpad.dx, -cpad.dy, cstick.dx, -cstick.dy };
	static int old_axes[4] = { 0, 0, 0, 0 };

	for (int i = 0; i < 4; i++) {
		int val = (axes[i] * 127) / 154;
		if (val < -127) val = -127;
		if (val > 127) val = 127;
		
		if (val > -20 && val < 20) val = 0;

		if (val != old_axes[i]) {
			d_event_joystick_moved ev;
			ev.type = EVENT_JOYSTICK_MOVED;
			ev.axis = i;
			ev.value = val;
			event_send((d_event *)&ev);
			old_axes[i] = val;
			idle = 0;
		}
	}
#endif

	// Send the idle event if there were no other events
	if (idle)
	{
		d_event ievent;
		
		ievent.type = EVENT_IDLE;
		event_send(&ievent);
	}
	else
		event_reset_idle_seconds();
	
	mouse_cursor_autohide();
}

void event_flush()
{
	SDL_Event event;
	
	while (SDL_PollEvent(&event));
}

int event_init()
{
	// We should now be active and responding to events.
	initialised = 1;

	return 0;
}

int (*default_handler)(d_event *event) = NULL;

void set_default_handler(int (*handler)(d_event *event))
{
	default_handler = handler;
}

int call_default_handler(d_event *event)
{
	if (default_handler)
		return (*default_handler)(event);
	
	return 0;
}

void event_send(d_event *event)
{
	window *wind;
	int handled = 0;

	for (wind = window_get_front(); wind != NULL && !handled; wind = window_get_prev(wind))
		if (window_is_visible(wind))
		{
			handled = window_send_event(wind, event);

			if (!window_exists(wind)) // break away if necessary: window_send_event() could have closed wind by now
				break;
			if (window_is_modal(wind))
				break;
		}
	
	if (!handled)
		call_default_handler(event);
}

// Process the first event in queue, sending to the appropriate handler
// This is the new object-oriented system
// Uses the old system for now, but this may change
void event_process(void)
{
	d_event event;
	#ifdef __3DS__
	// con_printf(CON_URGENT, "[TITLE-BUILD-23 event_process enter\n");
	#endif
	window *wind = window_get_front();

	timer_update();

	event_poll();	// send input events first

	// Process deferred quick-save/load HERE, after event_poll() has returned,
	// so state_restore_all_sub()/state_save_all_sub() never run mid-dispatch
	// inside the poll loop (which corrupted the window/event stack and broke
	// input after a quick-load).
	if (pending_quick_save) { pending_quick_save = 0; state_quick_save(); }
	if (pending_quick_load) { pending_quick_load = 0; state_quick_load(); }

	// Doing this prevents problems when a draw event can create a newmenu,
	// such as some network menus when they report a problem
	if (window_get_front() != wind)
		return;
	
	event.type = EVENT_WINDOW_DRAW;	// then draw all visible windows
	wind = window_get_first();
	while (wind != NULL)
	{
		window *prev = window_get_prev(wind);
		if (window_is_visible(wind))
			window_send_event(wind, &event);
		if (!window_exists(wind))
		{
			if (!prev) // well there isn't a previous window ...
				break; // ... just bail out - we've done everything for this frame we can.
			wind = window_get_next(prev); // the current window seemed to be closed. so take the next one from the previous which should be able to point to the one after the current closed
		}
		else
			wind = window_get_next(wind);
	}

	gr_flip();
}

void event_toggle_focus(int activate_focus)
{
#ifndef __3DS__
	if (activate_focus && GameCfg.Grabinput)
		SDL_WM_GrabInput(SDL_GRAB_ON);
	else
		SDL_WM_GrabInput(SDL_GRAB_OFF);
#endif
	mouse_toggle_cursor(!activate_focus);
}

static fix64 last_event = 0;

void event_reset_idle_seconds()
{
	last_event = timer_query();
}

fix event_get_idle_seconds()
{
	return (timer_query() - last_event)/F1_0;
}

