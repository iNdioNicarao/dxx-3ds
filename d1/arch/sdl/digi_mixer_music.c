/*
 * This is an alternate backend for the music system.
 * It uses SDL_mixer to provide a more reliable playback,
 * and allow processing of multiple audio formats.
 *
 *  -- MD2211 (2006-04-24)
 */

#include <SDL/SDL.h>
#include <SDL/SDL_mixer.h>
#include <SDL/SDL_thread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "args.h"
#include "hmp.h"
#include "digi_mixer_music.h"
#include "u_mem.h"
#include "console.h"

#ifdef _WIN32
extern int digi_win32_play_midi_song( char * filename, int loop );
#endif
Mix_Music *current_music = NULL;
static unsigned char *current_music_hndlbuf = NULL;

#ifdef __3DS__
typedef enum {
	PRELOAD_STATE_NONE = 0,
	PRELOAD_STATE_LOADING,
	PRELOAD_STATE_READY,
	PRELOAD_STATE_FAILED
} preload_state_t;

static unsigned char *s_preload_buf = NULL;
static unsigned int s_preload_size = 0;
static char s_preload_filename[PATH_MAX] = "";
static volatile preload_state_t s_preload_state = PRELOAD_STATE_NONE;
static SDL_Thread *s_preload_thread = NULL;

static int preload_worker_thread(void *arg)
{
	char *path = (char *)arg;
	PHYSFS_file *fh = NULL;
	unsigned char *buf = NULL;
	unsigned int len = 0;

	fh = PHYSFS_openRead(path);
	if (fh != NULL)
	{
		PHYSFS_sint64 flen = PHYSFS_fileLength(fh);
		if (flen > 0)
		{
			buf = (unsigned char *)d_malloc((size_t)flen);
			if (buf != NULL)
			{
				PHYSFS_uint32 total_read = 0;
				while (total_read < (PHYSFS_uint32)flen)
				{
					PHYSFS_uint32 chunk = (PHYSFS_uint32)flen - total_read;
					if (chunk > 65536)
						chunk = 65536;
					PHYSFS_uint32 n = PHYSFS_read(fh, buf + total_read, 1, chunk);
					if (n != chunk)
					{
						d_free(buf);
						buf = NULL;
						break;
					}
					total_read += n;
					// 3DS Performance: Yield briefly between 64KB chunks to keep main render thread at 60 FPS
					SDL_Delay(1);
				}
				if (buf != NULL)
					len = total_read;
			}
		}
		PHYSFS_close(fh);
	}

	if (!buf)
	{
		char full_path[PATH_MAX];
		if (PHYSFSX_getRealPath(path, full_path))
		{
			FILE *fp = fopen(full_path, "rb");
			if (fp != NULL)
			{
				fseek(fp, 0, SEEK_END);
				long sz = ftell(fp);
				fseek(fp, 0, SEEK_SET);
				if (sz > 0)
				{
					buf = (unsigned char *)d_malloc((size_t)sz);
					if (buf != NULL)
					{
						long total_read = 0;
						while (total_read < sz)
						{
							long chunk = sz - total_read;
							if (chunk > 65536)
								chunk = 65536;
							size_t n = fread(buf + total_read, 1, chunk, fp);
							if (n != (size_t)chunk)
							{
								d_free(buf);
								buf = NULL;
								break;
							}
							total_read += (long)n;
							SDL_Delay(1);
						}
						if (buf != NULL)
							len = (unsigned int)total_read;
					}
				}
				fclose(fp);
			}
		}
	}

	d_free(path);

	if (buf != NULL && len > 0)
	{
		s_preload_buf = buf;
		s_preload_size = len;
		s_preload_state = PRELOAD_STATE_READY;
	}
	else
	{
		s_preload_state = PRELOAD_STATE_FAILED;
	}
	return 0;
}

void digi_mixer_music_preload(const char *filename)
{
	if (!filename || !*filename)
		return;

	// Wait for any existing worker thread to finish
	if (s_preload_thread != NULL)
	{
		SDL_WaitThread(s_preload_thread, NULL);
		s_preload_thread = NULL;
	}

	// If already preloaded this exact filename, nothing to do
	if (s_preload_state == PRELOAD_STATE_READY && s_preload_buf != NULL && strcmp(s_preload_filename, filename) == 0)
		return;

	// Free any previously preloaded buffer
	if (s_preload_buf != NULL)
	{
		d_free(s_preload_buf);
		s_preload_buf = NULL;
		s_preload_size = 0;
	}

	strncpy(s_preload_filename, filename, sizeof(s_preload_filename) - 1);
	s_preload_filename[sizeof(s_preload_filename) - 1] = '\0';
	s_preload_state = PRELOAD_STATE_LOADING;

	char *arg_path = d_strdup(filename);
	s_preload_thread = SDL_CreateThread(preload_worker_thread, arg_path);
	if (!s_preload_thread)
	{
		d_free(arg_path);
		s_preload_state = PRELOAD_STATE_NONE;
	}
}

void digi_mixer_music_cancel_preload(void)
{
	if (s_preload_thread != NULL)
	{
		SDL_WaitThread(s_preload_thread, NULL);
		s_preload_thread = NULL;
	}
	if (s_preload_buf != NULL)
	{
		d_free(s_preload_buf);
		s_preload_buf = NULL;
		s_preload_size = 0;
	}
	s_preload_state = PRELOAD_STATE_NONE;
	s_preload_filename[0] = '\0';
}
#endif


/*
 *  Plays a music file from an absolute path or a relative path
 */

int mix_play_file(char *filename, int loop, void (*hook_finished_track)())
{
	SDL_RWops *rw = NULL;
	PHYSFS_file *filehandle = NULL;
	char full_path[PATH_MAX];
	char *fptr;
	unsigned int bufsize = 0;

	mix_free_music();	// stop and free what we're already playing, if anything

	fptr = strrchr(filename, '.');

	if (fptr == NULL)
		return 0;

#ifdef __3DS__
	// Check if this track was already preloaded into RAM on the background thread
	if (s_preload_thread != NULL)
	{
		SDL_WaitThread(s_preload_thread, NULL);
		s_preload_thread = NULL;
	}

	if (s_preload_state == PRELOAD_STATE_READY && s_preload_buf != NULL)
	{
		if (strcmp(filename, s_preload_filename) == 0)
		{
			current_music_hndlbuf = s_preload_buf;
			bufsize = s_preload_size;
			s_preload_buf = NULL;
			s_preload_size = 0;
			s_preload_state = PRELOAD_STATE_NONE;
			s_preload_filename[0] = '\0';

			rw = SDL_RWFromConstMem(current_music_hndlbuf, bufsize);
			if (rw != NULL)
			{
				current_music = Mix_LoadMUSType_RW(rw, MUS_NONE, 1);
				if (!current_music)
				{
					d_free(current_music_hndlbuf);
					current_music_hndlbuf = NULL;
				}
			}
		}
	}
#endif

	// It's a .hmp!
	if (!d_stricmp(fptr, ".hmp"))
	{
		hmp2mid(filename, &current_music_hndlbuf, &bufsize);
		rw = SDL_RWFromConstMem(current_music_hndlbuf,bufsize*sizeof(char));
		current_music = Mix_LoadMUS_RW(rw);
	}

#ifdef __3DS__
	/* 3DS Performance Optimization:
	 * Preload the entire music file into RAM. Streaming compressed audio (MP3/OGG)
	 * directly from the FAT32 SD card causes periodic I/O latency spikes on the
	 * audio callback thread (waiting on fs:USER IPC / SDIO bus). This starves the
	 * 3DS DSP wavebuf queue, resulting in audio crackling and tempo slowdown.
	 * By preloading into RAM, decoding runs with zero filesystem overhead. */
	if (!current_music)
	{
		filehandle = PHYSFS_openRead(filename);
		if (filehandle != NULL)
		{
			PHYSFS_sint64 flen = PHYSFS_fileLength(filehandle);
			if (flen > 0)
			{
				current_music_hndlbuf = d_malloc((size_t)flen);
				if (current_music_hndlbuf != NULL)
				{
					bufsize = (unsigned int)PHYSFS_read(filehandle, current_music_hndlbuf, 1, (PHYSFS_uint32)flen);
					if (bufsize == (unsigned int)flen)
					{
						rw = SDL_RWFromConstMem(current_music_hndlbuf, bufsize);
						if (rw != NULL)
						{
							current_music = Mix_LoadMUSType_RW(rw, MUS_NONE, 1);
							if (!current_music)
							{
								d_free(current_music_hndlbuf);
								current_music_hndlbuf = NULL;
							}
						}
						else
						{
							d_free(current_music_hndlbuf);
							current_music_hndlbuf = NULL;
						}
					}
					else
					{
						d_free(current_music_hndlbuf);
						current_music_hndlbuf = NULL;
					}
				}
			}
			PHYSFS_close(filehandle);
		}
	}

	if (!current_music && PHYSFSX_getRealPath(filename, full_path))
	{
		FILE *fp = fopen(full_path, "rb");
		if (fp != NULL)
		{
			fseek(fp, 0, SEEK_END);
			long sz = ftell(fp);
			fseek(fp, 0, SEEK_SET);
			if (sz > 0)
			{
				current_music_hndlbuf = d_malloc((size_t)sz);
				if (current_music_hndlbuf != NULL)
				{
					if (fread(current_music_hndlbuf, 1, sz, fp) == (size_t)sz)
					{
						rw = SDL_RWFromConstMem(current_music_hndlbuf, (int)sz);
						if (rw != NULL)
						{
							current_music = Mix_LoadMUSType_RW(rw, MUS_NONE, 1);
							if (current_music)
								filename = full_path;
							else
							{
								d_free(current_music_hndlbuf);
								current_music_hndlbuf = NULL;
							}
						}
						else
						{
							d_free(current_music_hndlbuf);
							current_music_hndlbuf = NULL;
						}
					}
					else
					{
						d_free(current_music_hndlbuf);
						current_music_hndlbuf = NULL;
					}
				}
			}
			fclose(fp);
		}
	}
#endif

	// try loading music via given filename
	if (!current_music)
		current_music = Mix_LoadMUS(filename);

	// allow the shell convention tilde character to mean the user's home folder
	// chiefly used for default jukebox level song music referenced in 'descent.m3u' for Mac OS X
	if (!current_music && *filename == '~')
	{
		snprintf(full_path, PATH_MAX, "%s%s", PHYSFS_getUserDir(),
				 &filename[1 + (!strncmp(&filename[1], PHYSFS_getDirSeparator(), strlen(PHYSFS_getDirSeparator())) ? 
				 strlen(PHYSFS_getDirSeparator()) : 0)]);
		current_music = Mix_LoadMUS(full_path);
		if (current_music)
			filename = full_path;	// used later for possible error reporting
	}
		

	// no luck. so it might be in Searchpath. So try to build absolute path
	if (!current_music)
	{
		PHYSFSX_getRealPath(filename, full_path);
		current_music = Mix_LoadMUS(full_path);
		if (current_music)
			filename = full_path;	// used later for possible error reporting
	}

	// still nothin'? Let's open via PhysFS in case it's located inside an archive
	if (!current_music)
	{
		filehandle = PHYSFS_openRead(filename);
		if (filehandle != NULL)
		{
			PHYSFS_sint64 flen = PHYSFS_fileLength(filehandle);
			current_music_hndlbuf = d_realloc(current_music_hndlbuf, (size_t)flen);
			bufsize = PHYSFS_read(filehandle, current_music_hndlbuf, sizeof(char), (PHYSFS_uint32)flen);
			rw = SDL_RWFromConstMem(current_music_hndlbuf,bufsize*sizeof(char));
			PHYSFS_close(filehandle);
			current_music = Mix_LoadMUS_RW(rw);
		}
	}

	if (current_music)
	{
		Mix_PlayMusic(current_music, (loop ? -1 : 1));
		Mix_HookMusicFinished(hook_finished_track ? hook_finished_track : mix_free_music);
		return 1;
	}
	else
	{
		mix_stop_music();

#ifdef __3DS__
        #define NEW_PATH_BUFSIZE 0x100
	    char new_path_buffer[NEW_PATH_BUFSIZE + 1];
        char basename[NEW_PATH_BUFSIZE + 1] = {0};
        
        fptr = strrchr(filename, '.');
        
        // Extract base name without path and extension
        char *slash = strrchr(filename, '/');
        if (!slash) slash = strrchr(filename, '\\');
        if (!slash) slash = (char *)filename; else slash++;
        
        strncpy(basename, slash, NEW_PATH_BUFSIZE);
        char *ext = strrchr(basename, '.');
        if (ext) *ext = '\0';
        
        if (fptr && !d_stricmp(fptr, ".hmp")) 
        {
            /* 3DS SDL_mixer has no MIDI/HMP decoder; try a user-supplied
             * converted track in soundtrack/, mp3/, ogg/, or wav/. */
            snprintf(new_path_buffer, NEW_PATH_BUFSIZE, "soundtrack/%s.mp3", basename);
            if (mix_play_file(new_path_buffer, loop, hook_finished_track)) return 1;
            snprintf(new_path_buffer, NEW_PATH_BUFSIZE, "mp3/%s.mp3", basename);
            if (mix_play_file(new_path_buffer, loop, hook_finished_track)) return 1;
            snprintf(new_path_buffer, NEW_PATH_BUFSIZE, "ogg/%s.ogg", basename);
            if (mix_play_file(new_path_buffer, loop, hook_finished_track)) return 1;
            snprintf(new_path_buffer, NEW_PATH_BUFSIZE, "wav/%s.wav", basename);
            return mix_play_file(new_path_buffer, loop, hook_finished_track);
        }
        else if (fptr && !strchr(filename, '/') && !strchr(filename, '\\'))
        {
            /* If a bare filename without folder was passed and not found,
             * check standard soundtrack/ and format subdirectories. */
            if (!d_stricmp(fptr, ".mp3"))
            {
                snprintf(new_path_buffer, NEW_PATH_BUFSIZE, "soundtrack/%s.mp3", basename);
                if (mix_play_file(new_path_buffer, loop, hook_finished_track)) return 1;
                snprintf(new_path_buffer, NEW_PATH_BUFSIZE, "mp3/%s.mp3", basename);
                if (mix_play_file(new_path_buffer, loop, hook_finished_track)) return 1;
                snprintf(new_path_buffer, NEW_PATH_BUFSIZE, "ogg/%s.ogg", basename);
                return mix_play_file(new_path_buffer, loop, hook_finished_track);
            }
            else if (!d_stricmp(fptr, ".ogg"))
            {
                snprintf(new_path_buffer, NEW_PATH_BUFSIZE, "soundtrack/%s.ogg", basename);
                if (mix_play_file(new_path_buffer, loop, hook_finished_track)) return 1;
                snprintf(new_path_buffer, NEW_PATH_BUFSIZE, "ogg/%s.ogg", basename);
                if (mix_play_file(new_path_buffer, loop, hook_finished_track)) return 1;
                snprintf(new_path_buffer, NEW_PATH_BUFSIZE, "mp3/%s.mp3", basename);
                return mix_play_file(new_path_buffer, loop, hook_finished_track);
            }
            else if (!d_stricmp(fptr, ".wav"))
            {
                snprintf(new_path_buffer, NEW_PATH_BUFSIZE, "soundtrack/%s.wav", basename);
                if (mix_play_file(new_path_buffer, loop, hook_finished_track)) return 1;
                snprintf(new_path_buffer, NEW_PATH_BUFSIZE, "wav/%s.wav", basename);
                if (mix_play_file(new_path_buffer, loop, hook_finished_track)) return 1;
                snprintf(new_path_buffer, NEW_PATH_BUFSIZE, "soundtrack/%s.mp3", basename);
                if (mix_play_file(new_path_buffer, loop, hook_finished_track)) return 1;
                snprintf(new_path_buffer, NEW_PATH_BUFSIZE, "mp3/%s.mp3", basename);
                if (mix_play_file(new_path_buffer, loop, hook_finished_track)) return 1;
                snprintf(new_path_buffer, NEW_PATH_BUFSIZE, "ogg/%s.ogg", basename);
                return mix_play_file(new_path_buffer, loop, hook_finished_track);
            }
        }
#endif
	}

	return 0;
}

// What to do when stopping song playback
void mix_free_music()
{
	Mix_HaltMusic();
	if (current_music)
	{
		Mix_FreeMusic(current_music);
		current_music = NULL;
	}
	if (current_music_hndlbuf)
	{
		d_free(current_music_hndlbuf);
		current_music_hndlbuf = NULL;
	}
}

void mix_set_music_volume(int vol)
{
	vol *= MIX_MAX_VOLUME/8;
	Mix_VolumeMusic(vol);
}

void mix_stop_music()
{
	Mix_HaltMusic();
#ifdef __3DS__
	digi_mixer_music_cancel_preload();
#endif
	if (current_music_hndlbuf)
	{
		d_free(current_music_hndlbuf);
		current_music_hndlbuf = NULL;
	}
}

void mix_pause_music()
{
	Mix_PauseMusic();
}

void mix_resume_music()
{
	Mix_ResumeMusic();
}

void mix_pause_resume_music()
{
	if (Mix_PausedMusic())
		Mix_ResumeMusic();
	else if (Mix_PlayingMusic())
		Mix_PauseMusic();
}
