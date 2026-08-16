/*
 *
 * SDL library timer functions
 *
 */

#include <SDL/SDL.h>

#include "maths.h"
#include "timer.h"
#include "config.h"

static fix64 F64_RunTime = 0;

#ifdef __3DS__
#include <3ds.h>
// SYSCLOCK on 3DS is 268.123480 MHz
#define SYSCLOCK 268123480ULL
static u_int32_t get_3ds_ticks(void) {
    static u64 start_tick = 0;
    if (start_tick == 0) start_tick = svcGetSystemTick();
    u64 current_tick = svcGetSystemTick();
    return (u_int32_t)(((current_tick - start_tick) * 1000ULL) / SYSCLOCK);
}
#define GET_TICKS() get_3ds_ticks()
#else
#define GET_TICKS() SDL_GetTicks()
#endif

void timer_update(void)
{
	static ubyte init = 1;
	static fix64 last_tv = 0;
	fix64 cur_tv = GET_TICKS()*F1_0/1000;

	if (init)
	{
		last_tv = cur_tv;
		init = 0;
	}

	if (last_tv < cur_tv) // in case SDL_GetTicks wraps, don't update and have a little hickup
		F64_RunTime += (cur_tv - last_tv); // increment! this value will overflow long after we are all dead... so why bother checking?
	last_tv = cur_tv;
}

fix64 timer_query(void)
{
	return (F64_RunTime);
}

void timer_delay(fix seconds)
{
	SDL_Delay(f2i(fixmul(seconds, i2f(1000))));
}

// Replacement for timer_delay which considers calc time the program needs between frames (not reentrant)
void timer_delay2(int fps)
{
	static u_int32_t FrameStart=0;
	u_int32_t FrameLoop=0;
	int failsafe = 0;

	while (FrameLoop < 1000/(GameCfg.VSync?MAXIMUM_FPS:fps))
	{
		u_int32_t tv_now = GET_TICKS();
		if (FrameStart > tv_now)
			FrameStart = tv_now;
		if (!GameCfg.VSync)
			SDL_Delay(1);
		
#ifdef __3DS__
		// Yield the thread to prevent hard lock on 3DS if VSync is true
		if (GameCfg.VSync)
			svcSleepThread(1000000); // 1 millisecond
#endif

		FrameLoop=tv_now-FrameStart;

		failsafe++;
		if (failsafe > 2000) {
			// If we looped 2000 times, something is wrong, break to prevent freeze!
			break;
		}
	}

	FrameStart=GET_TICKS();
}
