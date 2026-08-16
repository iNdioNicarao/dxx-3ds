// Holds the main init and de-init functions for arch-related program parts

#ifdef __3DS__
#include <3ds.h>
#endif

#include <SDL/SDL.h>
#include "songs.h"
#include "key.h"
#include "digi.h"
#include "mouse.h"
#include "joy.h"
#include "gr.h"
#include "dxxerror.h"
#include "text.h"
#include "args.h"
#include "config.h"

#ifdef __3DS__
/* Set while the system is suspending/shutting down, so loops and GPU
 * flushes bail instead of touching hardware the OS is reclaiming. */
static aptHookCookie d1x_apt_cookie;
volatile int d1x_powering_off = 0;
/* 3DS APT hook: track suspend/power-off state.
 *
 * ONSUSPEND fires for power button, sleep, AND system applets (swkbd,
 * HOME). The flag lets modal loops and the GPU flush bail immediately.
 * It MUST be cleared on ONRESTORE/ONWAKEUP — applets suspend then resume,
 * and a stuck flag would make the main loop exit to the Home Menu. On a
 * real power-off ONEXIT follows ONSUSPEND and the process exits before
 * any restore can matter. One hook covers every app state.
 *
 * NOTE: the DSP audio callback is stopped at the real termination point
 * (inferno.c, after the main loop exits on power-off), NOT here — this
 * hook fires for HOME/sleep/applets too, and stopping/reopening audio
 * here broke game-start stereo and triggered a joystick-teardown crash. */
static void d1x_apt_hook(APT_HookType type, void *param)
{
	if (type == APTHOOK_ONSUSPEND || type == APTHOOK_ONEXIT) {
		d1x_powering_off = 1;
	} else if (type == APTHOOK_ONRESTORE || type == APTHOOK_ONWAKEUP) {
		d1x_powering_off = 0;
		/* 3DS: sleep/wake (lid close/open) loses GPU/display state. If the 3D
		 * slider was up at sleep, stereo_hw_on is stale == 1, so the next live
		 * frame skips re-issuing gfxSet3D/pglSetStereo and both screens stay
		 * black. Reset the stereo flags so the per-frame slider logic re-engages
		 * stereo and re-selects the visible bank (GFX_LEFT) on wake. */
		extern void stereo_resume(void);
		stereo_resume();
	}
}
#endif

void arch_close(void)
{
#ifdef __3DS__
	// Power-off fix: on 3DS, NEVER run the GPU/audio teardown here. main()
	// already calls gfxExit() on its normal path, and a forced exit (in-game
	// Power Off -> exit()) must not call SDL_Quit()->gfxExit() a second time
	// — that blocks forever in _queueWaitAndClear() once the display is off
	// (both screens go black but the console stays on, stuck). The OS reclaims
	// all GPU/audio resources on process exit regardless. The DSP audio
	// callback is stopped earlier, in the APT hook (d1x_apt_hook), so it does
	// not Data-Abort during OS teardown. So just bail.
	return;
#endif

	// Power-off trace: each step opens+flushes+closes so the last line
	// printed tells us exactly where the shutdown hangs.
	{
	}
	songs_uninit();
	{
	}

	gr_close();
	{
	}

	if (!GameArg.CtlNoJoystick)
		joy_close();

	mouse_close();

// 3DS: power-off is handled by the APTHOOK_ONEXIT hook (arch_init) which
// makes arch_close skip the blocking SDL_Quit GPU teardown, so the console
// powers off cleanly instead of hanging on two black screens.
#ifdef __3DS__
	printf("Press the home button to exit... (power off will terminate the app)\n");
#endif

	if (!GameArg.SndNoSound)
	{
		digi_close();
	}

	key_close();

	{
	}
#ifdef __3DS__
	// Power-off fix: on APTHOOK_ONEXIT the display is already off and
	// SDL_Quit() -> gfxExit() blocks forever in _queueWaitAndClear().
	// Skip it; the OS reclaims the GPU. On a normal (HOME) exit the
	// display is still on, so run the normal teardown.
	if (!d1x_powering_off)
#endif
	SDL_Quit();
	{
	}
}

void arch_init(void)
{
	int t;

#ifndef __3DS__
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0)
#else
	if (SDL_Init(SDL_INIT_JOYSTICK | SDL_INIT_AUDIO | SDL_INIT_TIMER) < 0)
#endif
		Error("SDL library initialisation failed: %s.",SDL_GetError());

	key_init();

	digi_select_system( GameArg.SndDisableSdlMixer ? SDLAUDIO_SYSTEM : SDLMIXER_SYSTEM );

	if (!GameArg.SndNoSound)
		digi_init();

	mouse_init();

	if (!GameArg.CtlNoJoystick)
		joy_init();

	if ((t = gr_init(0)) != 0)
		Error(TXT_CANT_INIT_GFX,t);

#ifdef __3DS__
	// Power-off fix: when the system is shutting down (APTHOOK_ONEXIT),
	// libctru's gfxExit() (reached via SDL_Quit) blocks forever in
	// _queueWaitAndClear() waiting for the GPU queue to drain -- but the
	// display is already off, so it never completes. The console then
	// hangs on two black screens (status lights stuck on) until a hard
	// power-off. We detect the exit and skip the blocking GPU teardown;
	// the OS reclaims the GPU regardless. Normal teardown (config save,
	// gr_close) still runs beforehand.
	aptHook(&d1x_apt_cookie, d1x_apt_hook, NULL);
#endif

	atexit(arch_close);
}

