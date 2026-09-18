/*
 * D1X 3DS Roland SC-55 Soundtrack Jukebox
 * Author: Dennis Isaac Gutierrez Zeledon (Dennis)
 */

#ifndef __JUKEBOX_H__
#define __JUKEBOX_H__

#define JUKEBOX_TOTAL_TRACKS 27

// Playback modes
#define JUKEBOX_MODE_LOOP        0
#define JUKEBOX_MODE_SEQUENTIAL  1
#define JUKEBOX_MODE_SHUFFLE     2

// Playback states
#define JUKEBOX_STATE_STOPPED    0
#define JUKEBOX_STATE_PLAYING    1
#define JUKEBOX_STATE_PAUSED     2

typedef struct {
	const char *title;     // Friendly display title, e.g. "01. Title (Descent)"
	const char *basename;  // Audio file basename, e.g. "descent"
} jukebox_track_info_t;

extern const jukebox_track_info_t sc55_catalog[JUKEBOX_TOTAL_TRACKS];
extern const char *const jukebox_exts[];

// Core Jukebox API
void jukebox_init(void);
void jukebox_unload(void);
void jukebox_load(void);

// Playback control
int jukebox_play(void);
int jukebox_play_track(int track_idx);
int jukebox_play_game_track(int track_idx, int repeat);
void jukebox_stop(void);
void jukebox_pause_resume(void);
void jukebox_next(void);
void jukebox_prev(void);

// State queries & mode setting
int jukebox_get_current_track(void);
const char *jukebox_get_current_title(void);
int jukebox_get_state(void);
int jukebox_get_mode(void);
void jukebox_set_mode(int mode);
int jukebox_is_active(void);
void jukebox_set_active(int active);

// Deferred main-thread polling hook
void jukebox_poll(void);

// UI Menu
void do_jukebox_menu(void);

// Compatibility with legacy callers
char *jukebox_current(void);
int jukebox_is_loaded(void);
int jukebox_is_playing(void);
int jukebox_numtracks(void);
void jukebox_list(void);

#endif
