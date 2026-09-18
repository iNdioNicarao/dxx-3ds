/*
 * D1X 3DS Roland SC-55 Soundtrack Jukebox
 * Author: Dennis Isaac Gutierrez Zeledon (Dennis)
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "physfsx.h"
#include "args.h"
#include "hudmsg.h"
#include "songs.h"
#include "digi_mixer_music.h"
#include "jukebox.h"
#include "dxxerror.h"
#include "console.h"
#include "config.h"
#include "window.h"
#include "game.h"
#include "maths.h"

const jukebox_track_info_t sc55_catalog[JUKEBOX_TOTAL_TRACKS] = {
	{ "01. Title (Descent)",              "descent"  },
	{ "02. Briefing",                     "briefing" },
	{ "03. Level 01: Lunar Outpost",      "game01"   },
	{ "04. Level 02: Lunar Sci-Lab",      "game02"   },
	{ "05. Level 03: Military Outpost",   "game03"   },
	{ "06. Level 04: Venus Atmospheric",  "game04"   },
	{ "07. Level 05: Venus Nickel-Iron",  "game05"   },
	{ "08. Level 06: Mercury Solar Lab",  "game06"   },
	{ "09. Level 07: Mercury Core",       "game07"   },
	{ "10. Level 08: Mars Processing",    "game08"   },
	{ "11. Level 09: Mars Military",      "game09"   },
	{ "12. Level 10: Mars Prison",        "game10"   },
	{ "13. Level 11: Callisto Tower",     "game11"   },
	{ "14. Level 12: Europa Mining",      "game12"   },
	{ "15. Level 13: Europa CO2 Mine",    "game13"   },
	{ "16. Level 14: Europa Sub-Surface", "game14"   },
	{ "17. Level 15: Ganymede Center",    "game15"   },
	{ "18. Level 16: Ganymede Ore",       "game16"   },
	{ "19. Level 17: Titan Mine",         "game17"   },
	{ "20. Level 18: Titan Nitrogen",     "game18"   },
	{ "21. Level 19: Hyperion Military",  "game19"   },
	{ "22. Level 20: Tethys H2O Mine",    "game20"   },
	{ "23. Level 21: Miranda Mine",       "game21"   },
	{ "24. Level 22: Oberon Platinum",    "game22"   },
	{ "25. Reactor Escape",               "endlevel" },
	{ "26. Victory (Endgame)",            "endgame"  },
	{ "27. Credits",                      "credits"  }
};

const char *const jukebox_exts[] = { SONG_EXT_HMP, SONG_EXT_MID, SONG_EXT_OGG, SONG_EXT_FLAC, SONG_EXT_MP3, NULL };

static const char *const s_jukebox_folders[] = {
	"soundtrack",
	"mp3",
	"ogg",
	"wav",
	""
};

static const char *const s_jukebox_extensions[] = {
	".mp3",
	".ogg",
	".wav",
	".hmp"
};

static int s_jukebox_current_track = 0;
static int s_jukebox_state = JUKEBOX_STATE_STOPPED;
static int s_jukebox_mode = JUKEBOX_MODE_LOOP;
static int s_jukebox_active = 0;
static volatile int s_jukebox_track_finished = 0;
static int s_jukebox_preloaded_track = -1;

static void jukebox_hook_finished(void);

static int jukebox_resolve_track_path(int track_idx, char *out_path, size_t out_len)
{
	int f, e, b;
	char test_path[PATH_MAX];
	const char *base;
	char alt_base[16];
	const char *bases[2];
	int num_bases = 1;

	if (track_idx < 0 || track_idx >= JUKEBOX_TOTAL_TRACKS)
		return 0;

	base = sc55_catalog[track_idx].basename;
	bases[0] = base;

	// Also support non-padded level track names (e.g. "game1.mp3" instead of "game01.mp3")
	if (strncmp(base, "game0", 5) == 0 && base[5] >= '1' && base[5] <= '9')
	{
		snprintf(alt_base, sizeof(alt_base), "game%c", base[5]);
		bases[1] = alt_base;
		num_bases = 2;
	}

	for (f = 0; f < (int)(sizeof(s_jukebox_folders) / sizeof(s_jukebox_folders[0])); f++)
	{
		for (b = 0; b < num_bases; b++)
		{
			for (e = 0; e < (int)(sizeof(s_jukebox_extensions) / sizeof(s_jukebox_extensions[0])); e++)
			{
				if (s_jukebox_folders[f][0] != '\0')
					snprintf(test_path, sizeof(test_path), "%s/%s%s", s_jukebox_folders[f], bases[b], s_jukebox_extensions[e]);
				else
					snprintf(test_path, sizeof(test_path), "%s%s", bases[b], s_jukebox_extensions[e]);

				if (PHYSFSX_exists(test_path, 1))
				{
					strncpy(out_path, test_path, out_len - 1);
					out_path[out_len - 1] = '\0';
					return 1;
				}
			}
		}
	}

	// Default fallback path for loader lookup
	snprintf(out_path, out_len, "soundtrack/%s.mp3", base);
	return 0;
}

static void jukebox_hook_finished(void)
{
	/* Audio callback from SDL_mixer running on audio thread:
	 * ONLY set the atomic notification flag. All file I/O, buffer allocations,
	 * and state transitions are deferred to jukebox_poll() on the main thread. */
	s_jukebox_track_finished = 1;
}

void jukebox_poll(void)
{
	if (!s_jukebox_track_finished)
		return;

	s_jukebox_track_finished = 0;

	if (!s_jukebox_active || s_jukebox_state != JUKEBOX_STATE_PLAYING)
		return;

	switch (s_jukebox_mode)
	{
		case JUKEBOX_MODE_LOOP:
			jukebox_play_track(s_jukebox_current_track);
			break;

		case JUKEBOX_MODE_SEQUENTIAL:
		case JUKEBOX_MODE_SHUFFLE:
			jukebox_next();
			break;
	}
}

int jukebox_play_track(int track_idx)
{
	char resolved[PATH_MAX];

	if (track_idx < 0 || track_idx >= JUKEBOX_TOTAL_TRACKS)
		return 0;

	s_jukebox_current_track = track_idx;
	s_jukebox_track_finished = 0;

	GameCfg.MusicType = MUSIC_TYPE_CUSTOM;

	jukebox_resolve_track_path(track_idx, resolved, sizeof(resolved));

	int loop = (s_jukebox_mode == JUKEBOX_MODE_LOOP) ? 1 : 0;

	if (songs_play_file(resolved, loop, (loop ? NULL : jukebox_hook_finished)))
	{
		s_jukebox_state = JUKEBOX_STATE_PLAYING;
		s_jukebox_active = 1;

		if (Game_wind != NULL)
		{
			HUD_init_message(HM_DEFAULT, "Jukebox: %s", sc55_catalog[track_idx].title);
		}
		con_printf(CON_DEBUG, "Jukebox playing: %s (%s)\n", sc55_catalog[track_idx].title, resolved);

#ifdef __3DS__
		// Preload next track in background for seamless zero-stutter playback
		if (s_jukebox_mode != JUKEBOX_MODE_LOOP)
		{
			int upcoming = track_idx;
			if (s_jukebox_mode == JUKEBOX_MODE_SEQUENTIAL)
			{
				upcoming = (track_idx + 1) % JUKEBOX_TOTAL_TRACKS;
			}
			else if (s_jukebox_mode == JUKEBOX_MODE_SHUFFLE)
			{
				int attempts = 10;
				while (attempts-- > 0 && upcoming == track_idx)
				{
					upcoming = d_rand() % JUKEBOX_TOTAL_TRACKS;
				}
			}
			s_jukebox_preloaded_track = upcoming;
			char next_path[PATH_MAX];
			jukebox_resolve_track_path(upcoming, next_path, sizeof(next_path));
			digi_mixer_music_preload(next_path);
		}
		else
		{
			s_jukebox_preloaded_track = -1;
		}
#endif
		return 1;
	}
	else
	{
		char fallback[64];
		snprintf(fallback, sizeof(fallback), "%s.mp3", sc55_catalog[track_idx].basename);
		if (songs_play_file(fallback, loop, (loop ? NULL : jukebox_hook_finished)))
		{
			s_jukebox_state = JUKEBOX_STATE_PLAYING;
			s_jukebox_active = 1;
			if (Game_wind != NULL)
			{
				HUD_init_message(HM_DEFAULT, "Jukebox: %s", sc55_catalog[track_idx].title);
			}
			return 1;
		}
	}

	s_jukebox_state = JUKEBOX_STATE_STOPPED;
	s_jukebox_active = 0;
	return 0;
}

int jukebox_play_game_track(int track_idx, int repeat)
{
	char resolved[PATH_MAX];

	if (track_idx < 0 || track_idx >= JUKEBOX_TOTAL_TRACKS)
		return 0;

	s_jukebox_current_track = track_idx;
	s_jukebox_track_finished = 0;

	jukebox_resolve_track_path(track_idx, resolved, sizeof(resolved));

	if (songs_play_file(resolved, repeat, (repeat ? NULL : jukebox_hook_finished)))
	{
		s_jukebox_state = JUKEBOX_STATE_PLAYING;
		con_printf(CON_DEBUG, "Soundtrack playing: %s (%s)\n", sc55_catalog[track_idx].title, resolved);
		return 1;
	}
	else
	{
		char fallback[64];
		snprintf(fallback, sizeof(fallback), "%s.mp3", sc55_catalog[track_idx].basename);
		if (songs_play_file(fallback, repeat, (repeat ? NULL : jukebox_hook_finished)))
		{
			s_jukebox_state = JUKEBOX_STATE_PLAYING;
			return 1;
		}
	}

	s_jukebox_state = JUKEBOX_STATE_STOPPED;
	return 0;
}

int jukebox_play(void)
{
	if (s_jukebox_state == JUKEBOX_STATE_PAUSED)
	{
		songs_resume();
		s_jukebox_state = JUKEBOX_STATE_PLAYING;
		return 1;
	}
	return jukebox_play_track(s_jukebox_current_track);
}

void jukebox_stop(void)
{
	songs_stop_all();
#ifdef __3DS__
	digi_mixer_music_cancel_preload();
#endif
	s_jukebox_preloaded_track = -1;
	s_jukebox_state = JUKEBOX_STATE_STOPPED;
	s_jukebox_active = 0;
	s_jukebox_track_finished = 0;
}

void jukebox_pause_resume(void)
{
	if (s_jukebox_state == JUKEBOX_STATE_PLAYING)
	{
		songs_pause();
		s_jukebox_state = JUKEBOX_STATE_PAUSED;
	}
	else if (s_jukebox_state == JUKEBOX_STATE_PAUSED)
	{
		songs_resume();
		s_jukebox_state = JUKEBOX_STATE_PLAYING;
	}
	else if (s_jukebox_state == JUKEBOX_STATE_STOPPED)
	{
		jukebox_play_track(s_jukebox_current_track);
	}
}

void jukebox_next(void)
{
	if (s_jukebox_mode == JUKEBOX_MODE_SHUFFLE)
	{
		int next_tr = s_jukebox_preloaded_track;
		if (next_tr < 0 || next_tr >= JUKEBOX_TOTAL_TRACKS || next_tr == s_jukebox_current_track)
		{
			next_tr = s_jukebox_current_track;
			if (JUKEBOX_TOTAL_TRACKS > 1)
			{
				int attempts = 10;
				while (attempts-- > 0 && next_tr == s_jukebox_current_track)
					next_tr = d_rand() % JUKEBOX_TOTAL_TRACKS;
			}
		}
		jukebox_play_track(next_tr);
	}
	else
	{
		int next_tr = (s_jukebox_current_track + 1) % JUKEBOX_TOTAL_TRACKS;
		jukebox_play_track(next_tr);
	}
}

void jukebox_prev(void)
{
	if (s_jukebox_mode == JUKEBOX_MODE_SHUFFLE)
	{
		int next_tr = s_jukebox_current_track;
		if (JUKEBOX_TOTAL_TRACKS > 1)
		{
			int attempts = 10;
			while (attempts-- > 0 && next_tr == s_jukebox_current_track)
				next_tr = d_rand() % JUKEBOX_TOTAL_TRACKS;
		}
		jukebox_play_track(next_tr);
	}
	else
	{
		int prev_tr = (s_jukebox_current_track - 1 + JUKEBOX_TOTAL_TRACKS) % JUKEBOX_TOTAL_TRACKS;
		jukebox_play_track(prev_tr);
	}
}

int jukebox_get_current_track(void)
{
	return s_jukebox_current_track;
}

const char *jukebox_get_current_title(void)
{
	return sc55_catalog[s_jukebox_current_track].title;
}

int jukebox_get_state(void)
{
	return s_jukebox_state;
}

int jukebox_get_mode(void)
{
	return s_jukebox_mode;
}

void jukebox_set_mode(int mode)
{
	if (mode >= JUKEBOX_MODE_LOOP && mode <= JUKEBOX_MODE_SHUFFLE)
	{
		s_jukebox_mode = mode;
#ifdef __3DS__
		if (s_jukebox_state == JUKEBOX_STATE_PLAYING && mode != JUKEBOX_MODE_LOOP)
		{
			int upcoming = s_jukebox_current_track;
			if (mode == JUKEBOX_MODE_SEQUENTIAL)
			{
				upcoming = (s_jukebox_current_track + 1) % JUKEBOX_TOTAL_TRACKS;
			}
			else if (mode == JUKEBOX_MODE_SHUFFLE)
			{
				int attempts = 10;
				while (attempts-- > 0 && upcoming == s_jukebox_current_track)
				{
					upcoming = d_rand() % JUKEBOX_TOTAL_TRACKS;
				}
			}
			s_jukebox_preloaded_track = upcoming;
			char next_path[PATH_MAX];
			jukebox_resolve_track_path(upcoming, next_path, sizeof(next_path));
			digi_mixer_music_preload(next_path);
		}
#endif
	}
}

int jukebox_is_active(void)
{
	return s_jukebox_active;
}

void jukebox_set_active(int active)
{
	s_jukebox_active = active ? 1 : 0;
}

void jukebox_init(void)
{
	s_jukebox_track_finished = 0;
}

void jukebox_unload(void)
{
	s_jukebox_track_finished = 0;
}

void jukebox_load(void)
{
	s_jukebox_track_finished = 0;
}

char *jukebox_current(void)
{
	return (char *)sc55_catalog[s_jukebox_current_track].basename;
}

int jukebox_is_loaded(void)
{
	return 1;
}

int jukebox_is_playing(void)
{
	return (s_jukebox_state == JUKEBOX_STATE_PLAYING);
}

int jukebox_numtracks(void)
{
	return JUKEBOX_TOTAL_TRACKS;
}

void jukebox_list(void)
{
	int i;
	for (i = 0; i < JUKEBOX_TOTAL_TRACKS; i++)
		con_printf(CON_DEBUG, "* %s\n", sc55_catalog[i].title);
}
