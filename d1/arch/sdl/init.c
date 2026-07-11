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
// Power-off fix: set when the system is shutting down so arch_close can
// skip the blocking GPU teardown (SDL_Quit -> gfxExit -> _queueWaitAndClear).
static aptHookCookie d1x_apt_cookie;
volatile int d1x_powering_off = 0;
static void d1x_apt_hook(APT_HookType type, void *param)
{
	if (type == APTHOOK_ONEXIT) {
		d1x_powering_off = 1;
		FILE *pf = fopen("sdmc:/3ds/d1/pwrtrace.txt", "a");
		if (pf) { fprintf(pf, "APTHOOK_ONEXIT fired (powering_off=1)\n"); fclose(pf); }
	}
}
#endif

void arch_close(void)
{
#ifdef __3DS__
	// Power-off: the display is already off, so EVERY teardown call
	// (songs_uninit, gr_close, SDL_Quit->gfxExit, even the config SD
	// write) can block forever or abort on the dead GPU. The OS reclaims
	// all resources on process exit, so just bail. This is reached either
	// via the APTHOOK_ONEXIT flag or via inferno.c setting the flag before
	// returning. (We no longer svcExitProcess -- that aborts on the live
	// GPU and trips the "an error has occurred" exception screen.)
	if (d1x_powering_off)
		return;
#endif

	// Power-off trace: each step opens+flushes+closes so the last line
	// printed tells us exactly where the shutdown hangs.
	{
		FILE *pf = fopen("sdmc:/3ds/d1/pwrtrace.txt", "a");
		if (pf) { fprintf(pf, "arch_close entered\n"); fclose(pf); }
	}
	songs_uninit();
	{
		FILE *pf = fopen("sdmc:/3ds/d1/pwrtrace.txt", "a");
		if (pf) { fprintf(pf, "songs_uninit done\n"); fclose(pf); }
	}

	gr_close();
	{
		FILE *pf = fopen("sdmc:/3ds/d1/pwrtrace.txt", "a");
		if (pf) { fprintf(pf, "gr_close done\n"); fclose(pf); }
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
		FILE *pf = fopen("sdmc:/3ds/d1/pwrtrace.txt", "a");
		if (pf) { fprintf(pf, "before SDL_Quit\n"); fclose(pf); }
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
		FILE *pf = fopen("sdmc:/3ds/d1/pwrtrace.txt", "a");
		if (pf) { fprintf(pf, "arch_close done (exiting)\n"); fclose(pf); }
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

