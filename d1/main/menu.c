/*
THE COMPUTER CODE CONTAINED HEREIN IS THE SOLE PROPERTY OF PARALLAX
SOFTWARE CORPORATION ("PARALLAX").  PARALLAX, IN DISTRIBUTING THE CODE TO
END-USERS, AND SUBJECT TO ALL OF THE TERMS AND CONDITIONS HEREIN, GRANTS A
ROYALTY-FREE, PERPETUAL LICENSE TO SUCH END-USERS FOR USE BY SUCH END-USERS
IN USING, DISPLAYING,  AND CREATING DERIVATIVE WORKS THEREOF, SO LONG AS
SUCH USE, DISPLAY OR CREATION IS FOR NON-COMMERCIAL, ROYALTY OR REVENUE
FREE PURPOSES.  IN NO EVENT SHALL THE END-USER USE THE COMPUTER CODE
CONTAINED HEREIN FOR REVENUE-BEARING PURPOSES.  THE END-USER UNDERSTANDS
AND AGREES TO THE TERMS HEREIN AND ACCEPTS THE SAME BY USE OF THIS FILE.
COPYRIGHT 1993-1998 PARALLAX SOFTWARE CORPORATION.  ALL RIGHTS RESERVED.
*/

/*
 *
 * Inferno main menu.
 *
 */

#include <stdio.h>
#include <string.h>

#include "menu.h"
#include "inferno.h"
#include "game.h"
#include "gr.h"
#include "key.h"
#include "mouse.h"
#include "iff.h"
#include "u_mem.h"
#include "dxxerror.h"
#include "bm.h"
#include "screens.h"
#include "joy.h"
#include "vecmat.h"
#include "effects.h"
#include "slew.h"
#include "gamemine.h"
#include "gamesave.h"
#include "palette.h"
#include "args.h"
#include "newdemo.h"
#include "timer.h"
#include "sounds.h"
#include "gameseq.h"
#include "osk.h"   /* 3DS on-screen keyboard for pilot name entry */
#include "bottom_screen.h" /* 3DS bottom-screen UI (pilot delete button) */
#include "texmerge.h"      /* texmerge_flush() — wall-cache recovery after swkbd */
#include <GL/picaGL.h>     /* pglReacquire() — GPU recovery after swkbd */
#include "text.h"
#include "gamefont.h"
#include "newmenu.h"
#include "scores.h"
#include "playsave.h"
#include "kconfig.h"
#include "titles.h"
#include "credits.h"
#include "texmap.h"
#include "polyobj.h"
#include "state.h"
#include "mission.h"
#include "songs.h"
#include "jukebox.h"
#include "config.h"
#include "gauges.h"
#include "hudmsg.h" //for HUD_max_num_disp
#include "strutil.h"
#include "multi.h"
#include "vers_id.h"
#ifdef USE_UDP
#include "net_udp.h"
#endif
#ifdef EDITOR
#include "editor/editor.h"
#include "editor/kdefs.h"
#endif
#ifdef OGL
#include "ogl_init.h"
#endif


// Menu IDs...
enum MENUS
{
    MENU_NEW_GAME = 0,
    MENU_GAME,
    MENU_EDITOR,
    MENU_VIEW_SCORES,
    MENU_QUIT,
    MENU_LOAD_GAME,
    MENU_SAVE_GAME,
    MENU_DEMO_PLAY,
    MENU_CONFIG,
    MENU_REJOIN_NETGAME,
    MENU_DIFFICULTY,
    MENU_HELP,
    MENU_CONTROLS,
    MENU_NEW_PLAYER,
    #if defined(USE_UDP)
        MENU_MULTIPLAYER,
    #endif

    MENU_SHOW_CREDITS,
    MENU_ORDER_INFO,

    #ifdef USE_UDP
    MENU_START_UDP_NETGAME,
    MENU_JOIN_MANUAL_UDP_NETGAME,
    MENU_JOIN_LIST_UDP_NETGAME,
    #endif
    #ifndef RELEASE
    MENU_SANDBOX
    #endif
    #ifdef __3DS__
    MENU_RESUME_GAME,   /* 3DS only: return to a live game behind the menu overlay */
    MENU_CHEATS,        /* 3DS only: typed-cheat detection is disabled on 3DS, so
                           cheats are reachable only through this menu entry */
    #endif
};

//ADD_ITEM("Start netgame...", MENU_START_NETGAME, -1 );
//ADD_ITEM("Send net message...", MENU_SEND_NET_MESSAGE, -1 );

#define ADD_ITEM(t,value,key)  do { m[num_options].type=NM_TYPE_MENU; m[num_options].text=t; menu_choice[num_options]=value;num_options++; } while (0)

static window *menus[16] = { NULL };

// Function Prototypes added after LINTING
int do_option(int select);
int do_new_game_menu(void);
void do_multi_player_menu();
#ifndef RELEASE
void do_sandbox_menu();
#endif
extern void newmenu_free_background();
extern void ReorderPrimary();
extern void ReorderSecondary();

// Hide all menus
int hide_menus(void)
{
	window *wind;
	int i;

	if (menus[0])
		return 0;		// there are already hidden menus

	for (i = 0; (i < 15) && (wind = window_get_front()); i++)
	{
		menus[i] = wind;
		window_set_visible(wind, 0);
	}

	Assert(window_get_front() == NULL);
	menus[i] = NULL;

	return 1;
}

// Show all menus, with the front one shown first
// This makes sure EVENT_WINDOW_ACTIVATED is only sent to that window
void show_menus(void)
{
	int i;

	for (i = 0; (i < 16) && menus[i]; i++)
		if (window_exists(menus[i]))
			window_set_visible(menus[i], 1);

	menus[0] = NULL;
}

//pairs of chars describing ranges
char playername_allowed_chars[] = "azAZ09__--";

int MakeNewPlayerFile(int allow_abort)
{
	int x;
	char filename[PATH_MAX];
	newmenu_item m;
	char text[CALLSIGN_LEN+9]="";

	strncpy(text, Players[Player_num].callsign,CALLSIGN_LEN);

#ifdef __3DS__
	// 3DS has no physical keyboard, so prompt for the pilot name with the
	// bottom-screen on-screen keyboard (stylus). Falls back to "player" if
	// the user cancels (SELECT) or finishes with an empty callsign.
	osk_modal_loop(text, sizeof(text));
	/* swkbd took over the GPU; re-acquire picaGL's cached state so the
	 * next render/menu redraw doesn't data-abort. Same recovery as
	 * game_leave_menus(): bottom_screen_reacquire + pglReacquire +
	 * texmerge_flush, plus a full texture re-upload on the next draw. */
	bottom_screen_reacquire();
	pglReacquire();
	texmerge_flush();
	ogl_invalidate_textures();
	if (text[0] == 0) {
		char default_name[] = "player";
		int n = 1;
		snprintf(text, sizeof(text), "%s", default_name);
		for (;;) {
			memset(filename, '\0', PATH_MAX);
			snprintf(filename, PATH_MAX, GameArg.SysUsePlayersDir ? "Players/%s.plr" : "%s.plr", text);
			if (!PHYSFSX_exists(filename, 0))
				break;
			snprintf(text, sizeof(text), "%s%d", default_name, ++n);
		}
	}
	goto have_name;
#endif

try_again:
	m.type=NM_TYPE_INPUT; m.text_len = CALLSIGN_LEN; m.text = text;

	Newmenu_allowed_chars = playername_allowed_chars;
	x = newmenu_do( NULL, TXT_ENTER_PILOT_NAME, 1, &m, NULL, NULL );
	Newmenu_allowed_chars = NULL;

	if ( x < 0 ) {
		if ( allow_abort ) return 0;
		goto try_again;
	}

	if (text[0]==0)	//null string
		goto try_again;

#ifdef __3DS__
have_name:
	// On 3DS there is no physical keyboard, so we never reach the newmenu
	// input box below. If the entered name maps to a saved pilot, LOAD it
	// (this is how "Change Pilot" switches to an existing profile). If it
	// does not exist, create a fresh player file and save it.
	d_strlwr(text);

	memset(filename, '\0', PATH_MAX);
	snprintf(filename, PATH_MAX, GameArg.SysUsePlayersDir ? "Players/%s.plr" : "%s.plr", text);

	if (PHYSFSX_exists(filename, 0)) {
		if (read_player_file() == EZERO) {
			strncpy(Players[Player_num].callsign, text, CALLSIGN_LEN);
			return 1;           /* switched to existing pilot */
		}
		/* load failed: fall through and (re)create */
	}

	strncpy(Players[Player_num].callsign, text, CALLSIGN_LEN);
	d_strlwr(Players[Player_num].callsign);

	if (new_player_config())
		write_player_file();

	return 1;
#else
have_name:
	d_strlwr(text);

	memset(filename, '\0', PATH_MAX);
	snprintf( filename, PATH_MAX, GameArg.SysUsePlayersDir? "Players/%s.plr" : "%s.plr", text );

	if (PHYSFSX_exists(filename,0))
	{
		nm_messagebox(NULL, 1, TXT_OK, "%s '%s' %s", TXT_PLAYER, text, TXT_ALREADY_EXISTS );
		goto try_again;
	}

	if ( !new_player_config() )
		goto try_again;		// They hit Esc during New player config

	strncpy(Players[Player_num].callsign, text, CALLSIGN_LEN);
	d_strlwr(Players[Player_num].callsign);

	write_player_file();

	return 1;
#endif
}

void delete_player_saved_games(char * name);

int player_menu_keycommand( listbox *lb, d_event *event )
{
	char **items = listbox_get_items(lb);
	int citem = listbox_get_citem(lb);

	switch (event_key_get(event))
	{
		case KEY_CTRLED+KEY_D:
			if (citem > 0)
			{
				int x = 1;
				x = nm_messagebox( NULL, 2, TXT_YES, TXT_NO, "%s %s?", TXT_DELETE_PILOT, items[citem]+((items[citem][0]=='$')?1:0) );
				if (x==0)	{
					char * p;
					char plxfile[PATH_MAX], efffile[PATH_MAX], ngpfile[PATH_MAX];
					int ret;
					char name[PATH_MAX];

					p = items[citem] + strlen(items[citem]);
					*p = '.';

					strcpy(name, GameArg.SysUsePlayersDir ? "Players/" : "");
					strcat(name, items[citem]);

					ret = !PHYSFS_delete(name);
					*p = 0;

					if (!ret)
					{
						delete_player_saved_games( items[citem] );
						// delete PLX file
						sprintf(plxfile, GameArg.SysUsePlayersDir? "Players/%.8s.plx" : "%.8s.plx", items[citem]);
						if (PHYSFSX_exists(plxfile,0))
							PHYSFS_delete(plxfile);
						// delete EFF file
						sprintf(efffile, GameArg.SysUsePlayersDir? "Players/%.8s.eff" : "%.8s.eff", items[citem]);
						if (PHYSFSX_exists(efffile,0))
							PHYSFS_delete(efffile);
						// delete NGP file
						sprintf(ngpfile, GameArg.SysUsePlayersDir? "Players/%.8s.ngp" : "%.8s.ngp", items[citem]);
						if (PHYSFSX_exists(ngpfile,0))
							PHYSFS_delete(ngpfile);
					}

					if (ret)
						nm_messagebox( NULL, 1, TXT_OK, "%s %s %s", TXT_COULDNT, TXT_DELETE_PILOT, items[citem]+((items[citem][0]=='$')?1:0) );
					else
						listbox_delete_item(lb, citem);
				}

				return 1;
			}
			break;
	}

	return 0;
}

int player_menu_handler( listbox *lb, d_event *event, char **list )
{
	char **items = listbox_get_items(lb);
	int citem = listbox_get_citem(lb);

	switch (event->type)
	{
		case EVENT_KEY_COMMAND:
			return player_menu_keycommand(lb, event);
			break;

#ifdef __3DS__
		case EVENT_IDLE:
		{
			/* The PC build deletes a pilot with the Ctrl+D keyboard combo,
			 * which the 3DS lacks. While this listbox is on the TOP screen,
			 * draw a tappable "Delete Pilot" key on the BOTTOM screen and,
			 * on a fresh tap, feed the identical Ctrl+D command into the
			 * existing delete path (player_menu_keycommand) so the same
			 * confirmation box + file deletion runs. Disabled (grey, no tap)
			 * when the pseudo "Create New Player" entry (citem 0) is the
			 * selection, since there is nothing to delete. */
			int enable = (citem > 0);
			if (bottom_pilot_delete_tapped(enable)) {
				/* Synthesize the Ctrl+D key command and feed it into the
				 * existing delete path. d_event_keycommand is private to
				 * key.c, so declare a compatible local struct (stable ABI:
				 * { event_type type; int keycode; }). */
				struct { event_type type; int keycode; } del_evt;
				del_evt.type = EVENT_KEY_COMMAND;
				del_evt.keycode = KEY_CTRLED + KEY_D;
				player_menu_keycommand(lb, (d_event *)&del_evt);
			}
			bottom_screen_present();
			break;
		}
#endif

		case EVENT_NEWMENU_SELECTED:
			if (citem < 0)
				return 0;		// shouldn't happen
			else if (citem == 0)
			{
				// They selected 'create new pilot'
				return !MakeNewPlayerFile(1);
			}
			else
			{
				strncpy(Players[Player_num].callsign,items[citem] + ((items[citem][0]=='$')?1:0), CALLSIGN_LEN);
				d_strlwr(Players[Player_num].callsign);
			}
			break;

		case EVENT_WINDOW_CLOSE:
			if (read_player_file() != EZERO)
				return 1;		// abort close!

			WriteConfigFile();		// Update lastplr

#ifdef __3DS__
			/* Clear the pilot-list "Delete Pilot" key from the bottom screen
			 * so it doesn't linger after the listbox closes. */
			bottom_clear(0);
			bottom_screen_present();
#endif

			PHYSFS_freeList(list);
			d_free(items);
			break;

		default:
			break;
	}

	return 0;
}

//Inputs the player's name, without putting up the background screen
int RegisterPlayer()
{
	char **m;
	char **f;
	char **list;
	static const char *const types[] = { ".plr", NULL };
	int i = 0, NumItems;
	int citem = 0;
	int allow_abort_flag = 1;

	if ( Players[Player_num].callsign[0] == 0 )
	{
		if (GameCfg.LastPlayer[0]==0)
		{
			strncpy( Players[Player_num].callsign, "player", CALLSIGN_LEN );
			allow_abort_flag = 0;
		}
		else
		{
			// Read the last player's name from config file, not lastplr.txt
			strncpy( Players[Player_num].callsign, GameCfg.LastPlayer, CALLSIGN_LEN );
		}
	}

	// 3DS NOTE: the pilot list below is the normal newmenu listbox on the TOP
	// screen, shared byte-for-byte with the PC path — it must look and behave
	// EXACTLY like PC (same listbox, same navigation). The 3DS already drives
	// every other menu/listbox through newmenu, so no special-casing is needed
	// here. The bottom-screen keyboard's ONLY job is supplying name characters,
	// and that happens later inside MakeNewPlayerFile() when "Create New Player"
	// is chosen. We therefore run the identical PC listbox code that follows.

	list = PHYSFSX_findFiles(GameArg.SysUsePlayersDir ? "Players/" : "", types);
	if (!list)
		return 0;	// memory error
	if (!*list)
	{
		MakeNewPlayerFile(0);	// make a new player without showing listbox
		PHYSFS_freeList(list);
		return 0;
	}


	for (NumItems = 0; list[NumItems] != NULL; NumItems++) {}
	NumItems++;		// for TXT_CREATE_NEW

	MALLOC(m, char *, NumItems);
	if (m == NULL)
	{
		PHYSFS_freeList(list);
		return 0;
	}

	m[i++] = TXT_CREATE_NEW;

	for (f = list; *f != NULL; f++)
	{
		char *p;

		if (strlen(*f) > FILENAME_LEN-1 || strlen(*f) < 5) // sorry guys, can only have up to eight chars for the player name
		{
			NumItems--;
			continue;
		}
		m[i++] = *f;
		p = strchr(*f, '.');
		if (p)
			*p = '\0';		// chop the .plr
	}

	if (NumItems <= 1) // so it seems all plr files we found were too long. funny. let's make a real player
	{
		MakeNewPlayerFile(0);	// make a new player without showing listbox
		PHYSFS_freeList(list);
		return 0;
	}

	// Sort by name, except the <Create New Player> string
	qsort(&m[1], NumItems - 1, sizeof(char *), (int (*)( const void *, const void * ))string_array_sort_func);

	for ( i=0; i<NumItems; i++ )
		if (!d_stricmp(Players[Player_num].callsign, m[i]) )
			citem = i;

#ifdef __3DS__
	/* Fresh draw of the bottom-screen Delete Pilot key when the list opens,
	 * and clear the bottom to a clean panel (otherwise the menu background
	 * bleeds through and the key looks like a giant block). */
	bottom_pilot_delete_reset();
	bottom_clear(0);
	bottom_screen_present();
#endif

	newmenu_listbox1(TXT_SELECT_PILOT, NumItems, m, allow_abort_flag, citem, (int (*)(listbox *, d_event *, void *))player_menu_handler, list);

	return 1;
}

// Draw Copyright and Version strings
void draw_copyright()
{
	gr_set_current_canvas(NULL);
	gr_set_curfont(GAME_FONT);
	gr_set_fontcolor(BM_XRGB(6,6,6),-1);
	gr_string(0x8000,SHEIGHT-LINE_SPACING,TXT_COPYRIGHT);
	gr_set_fontcolor( BM_XRGB(25,0,0), -1);
	gr_string(0x8000,SHEIGHT-(LINE_SPACING*2),DESCENT_VERSION);
}

int main_menu_handler(newmenu *menu, d_event *event, int *menu_choice )
{
	newmenu_item *items = newmenu_get_items(menu);

	switch (event->type)
	{
		case EVENT_WINDOW_ACTIVATED:
			if ( Players[Player_num].callsign[0]==0 )
				RegisterPlayer();
			else
				keyd_time_when_last_pressed = timer_query();		// .. 20 seconds from now!
			break;

		case EVENT_KEY_COMMAND:
			// Don't allow them to hit ESC in the main menu.
			if (event_key_get(event)==KEY_ESC)
				return 1;
			break;

		case EVENT_MOUSE_BUTTON_DOWN:
		case EVENT_MOUSE_BUTTON_UP:
			// Don't allow mousebutton-closing in main menu.
			if (event_mouse_get_button(event) == MBTN_RIGHT)
				return 1;
			break;

		case EVENT_IDLE:
			if ( /*keyd_time_when_last_pressed+i2f(45) < timer_query() || */ GameArg.SysAutoDemo  )
			{
#ifdef __3DS__
				/* Don't auto-start a demo while a live game is running
				 * behind this menu overlay (opened via the 3DS MENU
				 * button). The player can still pick "View Demo" manually. */
				if (Game_wind != NULL)
					break;
#endif
				keyd_time_when_last_pressed = timer_query();		// Reset timer so that disk won't thrash if no demos.
				newdemo_start_playback(NULL);		// Randomly pick a file
			}
			break;

		case EVENT_NEWMENU_DRAW:
			draw_copyright();
			break;

		case EVENT_NEWMENU_SELECTED:
			return do_option(menu_choice[newmenu_get_citem(menu)]);
			break;

		case EVENT_WINDOW_CLOSE:
			d_free(menu_choice);
			d_free(items);
			break;

		default:
			break;
	}

	return 0;
}

//	-----------------------------------------------------------------------------
//	Create the main menu.
void create_main_menu(newmenu_item *m, int *menu_choice, int *callers_num_options)
{
	int	num_options;

	#ifndef DEMO_ONLY
	num_options = 0;

#ifdef __3DS__
	/* The 3DS MENU button opens this menu as an overlay on top of a live
	 * game (Game_wind is kept alive). Only show "Resume Game" when there is
	 * actually a game to return to. */
	if (Game_wind != NULL)
		ADD_ITEM("Resume Game", MENU_RESUME_GAME, -1);
#endif

	ADD_ITEM(TXT_NEW_GAME,MENU_NEW_GAME,KEY_N);

	ADD_ITEM(TXT_LOAD_GAME,MENU_LOAD_GAME,KEY_L);
#if defined(USE_UDP)
	ADD_ITEM(TXT_MULTIPLAYER_,MENU_MULTIPLAYER,-1);
#endif

	ADD_ITEM(TXT_OPTIONS_, MENU_CONFIG, -1 );
	ADD_ITEM(TXT_CHANGE_PILOTS,MENU_NEW_PLAYER,unused);
	ADD_ITEM(TXT_VIEW_DEMO,MENU_DEMO_PLAY,0);
	ADD_ITEM(TXT_VIEW_SCORES,MENU_VIEW_SCORES,KEY_V);
	ADD_ITEM(TXT_CREDITS,MENU_SHOW_CREDITS,-1);
	ADD_ITEM("Controls",MENU_CONTROLS,-1);
#ifdef __3DS__
	/* Typed-cheat detection (FinalCheats) is disabled on 3DS because the
	 * handheld has no keyboard and synthetic button ASCII was leaking into
	 * the cheat buffer. Cheats are reachable only through this menu entry. */
	ADD_ITEM("Cheats",MENU_CHEATS,-1);
#endif
	#endif
	ADD_ITEM(TXT_QUIT,MENU_QUIT,KEY_Q);

	#ifndef RELEASE
	if (!(Game_mode & GM_MULTI ))	{
		//m[num_options].type=NM_TYPE_TEXT;
		//m[num_options++].text=" Debug options:";

		#ifdef EDITOR
		ADD_ITEM("  Editor", MENU_EDITOR, KEY_E);
		#endif
	}
	ADD_ITEM("  SANDBOX", MENU_SANDBOX, -1);
	#endif

	*callers_num_options = num_options;
}

//returns number of item chosen
int DoMenu()
{
	int *menu_choice;
	newmenu_item *m;
	int num_options = 0;

#ifdef __3DS__
	/* Suspend stereo (force mono on GFX_LEFT) for ANY overlay opened via
	 * DoMenu -- both the in-game MENU (Game_wind live) AND the main menu
	 * reached after a demo/game ends (Game_wind NULL). Previously the
	 * suspend was gated on Game_wind, so when a demo ended and returned to
	 * the main menu the stereo-present flag (g_stereo_active) stayed 1
	 * from the last stereo frame; the main menu's gr_flip() then became a
	 * no-op and the menu was logically up but never drawn -- "I can press
	 * up/down and it works but the menu is invisible". Call unconditionally
	 * so every DoMenu overlay presents in mono. stereo_resume() + the
	 * per-frame slider logic re-engage stereo on the next live frame. */
	stereo_suspend();
#endif

	MALLOC(menu_choice, int, 25);
	if (!menu_choice)
		return -1;
	MALLOC(m, newmenu_item, 25);
	if (!m)
	{
		d_free(menu_choice);
		return -1;
	}

	memset(menu_choice, 0, sizeof(int)*25);
	memset(m, 0, sizeof(newmenu_item)*25);

	create_main_menu(m, menu_choice, &num_options); // may have to change, eg, maybe selected pilot and no save games.

	newmenu_do3( "", NULL, num_options, m, (int (*)(newmenu *, d_event *, void *))main_menu_handler, menu_choice, 0, Menu_pcx_name);

	return 0;
}

extern void show_order_form(void);	// John didn't want this in inferno.h so I just externed it.

//returns flag, true means quit menu
int do_option ( int select)
{
	switch (select) {
		case MENU_NEW_GAME:
		{
#ifdef __3DS__
			int was_in_game = (Game_wind != NULL);
#endif
			select_mission(0, "New Game\n\nSelect mission", do_new_game_menu);
#ifdef __3DS__
			if (was_in_game && d1x_defer_leave_menus) {
				return 0;
			}
#endif
			break;
		}
		case MENU_GAME:
			break;
		case MENU_DEMO_PLAY:
#ifdef __3DS__
			/* Demo playback replaces the current level state, and when it
			 * ends newdemo_stop_playback() closes the game window. Starting
			 * one from the in-game MENU overlay would therefore destroy the
			 * live game with nothing to return to. Block it while a game is
			 * running; exit to the main menu first if the demo should play. */
			if (Game_wind != NULL) {
				nm_messagebox(NULL, 1, TXT_OK, "View demos from the main menu.\nPlaying a demo ends the current game.");
				break;
			}
#endif
			select_demo();
			break;
		case MENU_LOAD_GAME:
		{
#ifdef __3DS__
			int was_in_game = (Game_wind != NULL);
#endif
			if (state_restore_all(0)) {
#ifdef __3DS__
				if (was_in_game) {
					d1x_defer_leave_menus = 1;
					return 0;
				}
#endif
			}
			break;
		}
		case MENU_SAVE_GAME:
			/* Normal save: opens the top-screen save-slot menu
			 * (state_get_savegame_filename, no keyboard needed). On 3DS this
			 * was previously unreachable — only the name-free quick-save button
			 * (START+X) existed. Now it pairs with Load Game in the main menu
			 * so real multi-slot saves work, not just the single quick slot. */
			state_save_all(0);
			break;
		#ifdef EDITOR
		case MENU_EDITOR:
			if (!Current_mission)
			{
				create_new_mine();
				SetPlayerFromCurseg();
			}

			hide_menus();
			init_editor();
			break;
		#endif
		case MENU_VIEW_SCORES:
			scores_view(NULL, -1);
			break;
#if 1 //def SHAREWARE
		case MENU_ORDER_INFO:
			show_order_form();
			break;
#endif
#ifdef __3DS__
		case MENU_RESUME_GAME:
			/* Return to the live game running behind the menu overlay.
			 * game_leave_menus() closes the menu windows above Game_wind
			 * and the game resumes where it left off. */
			game_leave_menus();
			break;
#endif
		case MENU_QUIT:
			#ifdef EDITOR
			if (! SafetyCheck()) break;
			#endif
#ifdef __3DS__
			/* With a live game behind the menu overlay, a bare return just
			 * closes the menu and the game resumes — identical to Resume
			 * Game. Quit must instead abort the running game, same flow as
			 * the PC ESC path: confirm, then close Game_wind. The close is
			 * DEFERRED to the game window's own handler (like the demo
			 * selector's deferred game_leave_menus): window_close(Game_wind)
			 * fires EVENT_WINDOW_CLOSED, whose longjmp would unwind this
			 * menu's modal loop mid-dispatch if called from here. */
			if (Game_wind != NULL) {
				if (nm_messagebox(NULL, 2, TXT_YES, TXT_NO, TXT_ABORT_GAME) == 0) {
					d1x_defer_quit_game = 1;
					return 0;
				}
				break;
			}
#endif
			return 0;

		case MENU_NEW_PLAYER:
			/* Change Pilot: show the pilot LISTBOX (existing pilots +
			 * "Create New Player"), exactly like the PC build. RegisterPlayer()
			 * drives newmenu_listbox1(TXT_SELECT_PILOT) on the top screen; only
			 * the "Create New Player" branch reaches MakeNewPlayerFile(), which
			 * uses the swkbd keyboard purely for the NAME text entry. The old
			 * 3DS path called MakeNewPlayerFile() directly here, which skipped
			 * the list entirely and dropped the user straight into the keyboard
			 * (no way to pick/switch an existing pilot). */
			RegisterPlayer();
			break;

#ifdef USE_UDP
		case MENU_START_UDP_NETGAME:
			multi_protocol = MULTI_PROTO_UDP;
			select_mission(1, TXT_MULTI_MISSION, net_udp_setup_game);
			break;
		case MENU_JOIN_MANUAL_UDP_NETGAME:
			multi_protocol = MULTI_PROTO_UDP;
			net_udp_manual_join_game();
			break;
		case MENU_JOIN_LIST_UDP_NETGAME:
			multi_protocol = MULTI_PROTO_UDP;
			net_udp_list_join_game();
			break;
#endif
#if defined(USE_UDP)
		case MENU_MULTIPLAYER:
			do_multi_player_menu();
			break;
#endif
		case MENU_CONFIG:
			do_options_menu();
			break;
		case MENU_SHOW_CREDITS:
			credits_show(NULL);
			break;
		case MENU_CONTROLS:
			show_controls_3ds();
			break;
#ifdef __3DS__
		case MENU_CHEATS:
			do_cheat_menu();
			break;
#endif
#ifndef RELEASE
		case MENU_SANDBOX:
			do_sandbox_menu();
			break;
#endif
		default:
			Error("Unknown option %d in do_option",select);
			break;
	}

	return 1;		// stay in main menu unless quitting
}

void delete_player_saved_games(char * name)
{
	int i;
	char filename[PATH_MAX];

	for (i=0;i<10; i++)
	{
		snprintf( filename, PATH_MAX, GameArg.SysUsePlayersDir? "Players/%s.sg%x" : "%s.sg%x", name, i );
		PHYSFS_delete(filename);
		snprintf( filename, PATH_MAX, GameArg.SysUsePlayersDir? "Players/%s.mg%x" : "%s.mg%x", name, i );
		PHYSFS_delete(filename);
	}
}

int demo_menu_keycommand( listbox *lb, d_event *event )
{
	char **items = listbox_get_items(lb);
	int citem = listbox_get_citem(lb);

	switch (event_key_get(event))
	{
		case KEY_CTRLED+KEY_D:
			if (citem >= 0)
			{
				int x = 1;
				x = nm_messagebox( NULL, 2, TXT_YES, TXT_NO, "%s %s?", TXT_DELETE_DEMO, items[citem]+((items[citem][0]=='$')?1:0) );
				if (x==0)
				{
					int ret;
					char name[PATH_MAX];

					strcpy(name, DEMO_DIR);
					strcat(name,items[citem]);

					ret = !PHYSFS_delete(name);

					if (ret)
						nm_messagebox( NULL, 1, TXT_OK, "%s %s %s", TXT_COULDNT, TXT_DELETE_DEMO, items[citem]+((items[citem][0]=='$')?1:0) );
					else
						listbox_delete_item(lb, citem);
				}

				return 1;
			}
			break;

		case KEY_CTRLED+KEY_C:
			{
				int x = 1;
				char bakname[PATH_MAX];

				// Get backup name
				change_filename_extension(bakname, items[citem]+((items[citem][0]=='$')?1:0), DEMO_BACKUP_EXT);
				x = nm_messagebox( NULL, 2, TXT_YES, TXT_NO,	"Are you sure you want to\n"
								  "swap the endianness of\n"
								  "%s? If the file is\n"
								  "already endian native, D1X\n"
								  "will likely crash. A backup\n"
								  "%s will be created", items[citem]+((items[citem][0]=='$')?1:0), bakname );
				if (!x)
					newdemo_swap_endian(items[citem]);

				return 1;
			}
			break;
	}

	return 0;
}

int demo_menu_handler( listbox *lb, d_event *event, void *userdata )
{
	char **items = listbox_get_items(lb);
	int citem = listbox_get_citem(lb);

	userdata = userdata;

	switch (event->type)
	{
		case EVENT_KEY_COMMAND:
			return demo_menu_keycommand(lb, event);
			break;

#ifdef __3DS__
		case EVENT_IDLE:
		{
			/* The PC build deletes a demo with the Ctrl+D keyboard combo,
			 * which the 3DS lacks. While this listbox is on the TOP screen,
			 * draw a tappable "Delete Demo" key on the BOTTOM screen and,
			 * on a fresh tap, feed the identical Ctrl+D command into the
			 * existing delete path (demo_menu_keycommand) so the same
			 * confirmation box + file deletion + listbox removal runs.
			 * Disabled (grey, no tap) when nothing is selected (citem < 0). */
			int enable = (citem >= 0);
			if (bottom_demo_delete_tapped(enable)) {
				/* Synthesize the Ctrl+D key command and feed it into the
				 * existing delete path. d_event_keycommand is private to
				 * key.c, so declare a compatible local struct (stable ABI:
				 * { event_type type; int keycode; }). */
				struct { event_type type; int keycode; } del_evt;
				del_evt.type = EVENT_KEY_COMMAND;
				del_evt.keycode = KEY_CTRLED + KEY_D;
				demo_menu_keycommand(lb, (d_event *)&del_evt);
			}
			bottom_screen_present();
			break;
		}
#endif

		case EVENT_NEWMENU_SELECTED:
			if (citem < 0)
				return 0;		// shouldn't happen

			newdemo_start_playback(items[citem]);
#ifdef __3DS__
			/* Demo playback runs inside the game window. When the demo list
			 * was opened from the in-game MENU overlay, newdemo_start_playback's
			 * "if (!Game_wind) hide_menus()" was skipped and the demo-list
			 * window stayed front — which blocks the game loop that drives
			 * playback, bouncing straight back to the list after the
			 * "Prepare for Descent" banner. Close the menu windows above
			 * Game_wind so the game loop takes over and plays the demo.
			 *
			 * Do NOT call game_leave_menus() here: it closes the demo-list
			 * window synchronously, which re-enters listbox_handler with a
			 * dangling event and Data-Aborts. Instead set a flag that
			 * game_handler() honours on the next event, running
			 * game_leave_menus() from the game-loop context after this
			 * handler has fully unwound. */
			d1x_defer_leave_menus = 1;
#endif
			return 0;		// close the demo selector; playback now runs

		case EVENT_WINDOW_CLOSE:
			PHYSFS_freeList(items);
			break;

		default:
			break;
	}

	return 0;
}

int select_demo(void)
{
	char **list;
	static const char *const types[] = { DEMO_EXT, NULL };
	int NumItems;

	list = PHYSFSX_findFiles(DEMO_DIR, types);
	if (!list)
		return 0;	// memory error
	if ( !*list )
	{
		nm_messagebox( NULL, 1, TXT_OK, "%s %s\n%s", TXT_NO_DEMO_FILES, TXT_USE_F5, TXT_TO_CREATE_ONE);
		PHYSFS_freeList(list);
		return 0;
	}

	for (NumItems = 0; list[NumItems] != NULL; NumItems++) {}

	// Sort by name
	qsort(list, NumItems, sizeof(char *), (int (*)( const void *, const void * ))string_array_sort_func);

	newmenu_listbox1(TXT_SELECT_DEMO, NumItems, list, 1, 0, demo_menu_handler, NULL);

	return 1;
}

int do_difficulty_menu()
{
	int s;
	newmenu_item m[5];

	m[0].type=NM_TYPE_MENU; m[0].text=MENU_DIFFICULTY_TEXT(0);
	m[1].type=NM_TYPE_MENU; m[1].text=MENU_DIFFICULTY_TEXT(1);
	m[2].type=NM_TYPE_MENU; m[2].text=MENU_DIFFICULTY_TEXT(2);
	m[3].type=NM_TYPE_MENU; m[3].text=MENU_DIFFICULTY_TEXT(3);
	m[4].type=NM_TYPE_MENU; m[4].text=MENU_DIFFICULTY_TEXT(4);

	s = newmenu_do1( NULL, TXT_DIFFICULTY_LEVEL, NDL, m, NULL, NULL, Difficulty_level);

	if (s > -1 )	{
		if (s != Difficulty_level)
		{
			PlayerCfg.DefaultDifficulty = s;
			write_player_file();
		}
		Difficulty_level = s;
		return 1;
	}
	return 0;
}

int do_new_game_menu()
{
	int new_level_num,player_highest_level;
#ifdef __3DS__
	int was_in_game = (Game_wind != NULL);
#endif

	new_level_num = 1;
#ifdef NDEBUG
	player_highest_level = get_highest_level();

	if (player_highest_level > Last_level)
#endif
		player_highest_level = Last_level;
	if (player_highest_level > 1) {
		newmenu_item m[4];
		char info_text[80];
		char num_text[10];
		int choice;
		int n_items;
		int valid = 0;

		while (!valid)
		{
			sprintf(info_text,"%s %d",TXT_START_ANY_LEVEL, player_highest_level);

			m[0].type=NM_TYPE_TEXT; m[0].text = info_text;
			m[1].type=NM_TYPE_NUMBER; m[1].value=1; m[1].min_value=1; m[1].max_value=player_highest_level; m[1].text="Level";
			n_items = 2;

#ifdef __3DS__
			snprintf(num_text, 10, "%d", player_highest_level);
#else
			strcpy(num_text,"1");
#endif

			choice = newmenu_do( NULL, TXT_SELECT_START_LEV, n_items, m, NULL, NULL );

			if (choice==-1)
				return 0;

			new_level_num = m[1].value;

			if (!(new_level_num>0 && new_level_num<=player_highest_level)) {
				m[0].text = TXT_ENTER_TO_CONT;
				nm_messagebox( NULL, 1, TXT_OK, TXT_INVALID_LEVEL);
				valid = 0;
			}
			else
				valid = 1;
		}
	}

	Difficulty_level = PlayerCfg.DefaultDifficulty;

	if (!do_difficulty_menu())
		return 0;

	StartNewGame(new_level_num);
#ifdef __3DS__
	if (was_in_game) {
		if (Game_wind) {
			window_set_visible(Game_wind, 1);
			window_select(Game_wind);
		}
		d1x_defer_leave_menus = 1;
	}
#endif

	return 1;	// exit mission listbox
}

void do_sound_menu();
void input_config();
void graphics_config();
void do_misc_menu();

int options_menuset(newmenu *menu, d_event *event, void *userdata)
{
	switch (event->type)
	{
		case EVENT_NEWMENU_CHANGED:
			break;

		case EVENT_NEWMENU_SELECTED:
			switch(newmenu_get_citem(menu))
			{
				case  0: do_sound_menu();		break;
				case  2: input_config();		break;
				case  4: graphics_config();		break;
				case  6: ReorderPrimary();		break;
				case  7: ReorderSecondary();		break;
				case  8: do_misc_menu();		break;
			}
			return 1;	// stay in menu until escape
			break;

		case EVENT_WINDOW_CLOSE:
		{
			newmenu_item *items = newmenu_get_items(menu);
			d_free(items);
			write_player_file();
			break;
		}

		default:
			break;
	}

	userdata = userdata;		//kill warning

	return 0;
}

void input_config_sensitivity()
{
	newmenu_item m[23];
	int i = 0, nitems = 0, joysens = 0, joydead = 0, joyunder = 0;

	m[nitems].type = NM_TYPE_TEXT; m[nitems].text = "Joystick Sensitivity:"; nitems++;
	joysens = nitems;
	m[nitems].type = NM_TYPE_SLIDER; m[nitems].text = TXT_TURN_LR; m[nitems].value = PlayerCfg.JoystickSens[0]; m[nitems].min_value = 0; m[nitems].max_value = 16; nitems++;
	m[nitems].type = NM_TYPE_SLIDER; m[nitems].text = TXT_PITCH_UD; m[nitems].value = PlayerCfg.JoystickSens[1]; m[nitems].min_value = 0; m[nitems].max_value = 16; nitems++;
	m[nitems].type = NM_TYPE_SLIDER; m[nitems].text = TXT_SLIDE_LR; m[nitems].value = PlayerCfg.JoystickSens[2]; m[nitems].min_value = 0; m[nitems].max_value = 16; nitems++;
	m[nitems].type = NM_TYPE_SLIDER; m[nitems].text = TXT_SLIDE_UD; m[nitems].value = PlayerCfg.JoystickSens[3]; m[nitems].min_value = 0; m[nitems].max_value = 16; nitems++;
	m[nitems].type = NM_TYPE_SLIDER; m[nitems].text = TXT_BANK_LR; m[nitems].value = PlayerCfg.JoystickSens[4]; m[nitems].min_value = 0; m[nitems].max_value = 16; nitems++;
	m[nitems].type = NM_TYPE_SLIDER; m[nitems].text = TXT_THROTTLE; m[nitems].value = PlayerCfg.JoystickSens[5]; m[nitems].min_value = 0; m[nitems].max_value = 16; nitems++;
	m[nitems].type = NM_TYPE_TEXT; m[nitems].text = ""; nitems++;
	m[nitems].type = NM_TYPE_TEXT; m[nitems].text = "Joystick Deadzone:"; nitems++;
	joydead = nitems;
	m[nitems].type = NM_TYPE_SLIDER; m[nitems].text = TXT_TURN_LR; m[nitems].value = PlayerCfg.JoystickDead[0]; m[nitems].min_value = 0; m[nitems].max_value = 16; nitems++;
	m[nitems].type = NM_TYPE_SLIDER; m[nitems].text = TXT_PITCH_UD; m[nitems].value = PlayerCfg.JoystickDead[1]; m[nitems].min_value = 0; m[nitems].max_value = 16; nitems++;
	m[nitems].type = NM_TYPE_SLIDER; m[nitems].text = TXT_SLIDE_LR; m[nitems].value = PlayerCfg.JoystickDead[2]; m[nitems].min_value = 0; m[nitems].max_value = 16; nitems++;
	m[nitems].type = NM_TYPE_SLIDER; m[nitems].text = TXT_SLIDE_UD; m[nitems].value = PlayerCfg.JoystickDead[3]; m[nitems].min_value = 0; m[nitems].max_value = 16; nitems++;
	m[nitems].type = NM_TYPE_SLIDER; m[nitems].text = TXT_BANK_LR; m[nitems].value = PlayerCfg.JoystickDead[4]; m[nitems].min_value = 0; m[nitems].max_value = 16; nitems++;
	m[nitems].type = NM_TYPE_SLIDER; m[nitems].text = TXT_THROTTLE; m[nitems].value = PlayerCfg.JoystickDead[5]; m[nitems].min_value = 0; m[nitems].max_value = 16; nitems++;
	m[nitems].type = NM_TYPE_TEXT; m[nitems].text = ""; nitems++;
	m[nitems].type = NM_TYPE_TEXT; m[nitems].text = "Joystick Undercalibration:"; nitems++;
	joyunder = nitems;
	m[nitems].type = NM_TYPE_SLIDER; m[nitems].text = TXT_TURN_LR; m[nitems].value = PlayerCfg.JoystickUndercalibrate[0]; m[nitems].min_value = 0; m[nitems].max_value = 16; nitems++;
	m[nitems].type = NM_TYPE_SLIDER; m[nitems].text = TXT_PITCH_UD; m[nitems].value = PlayerCfg.JoystickUndercalibrate[1]; m[nitems].min_value = 0; m[nitems].max_value = 16; nitems++;
	m[nitems].type = NM_TYPE_SLIDER; m[nitems].text = TXT_SLIDE_LR; m[nitems].value = PlayerCfg.JoystickUndercalibrate[2]; m[nitems].min_value = 0; m[nitems].max_value = 16; nitems++;
	m[nitems].type = NM_TYPE_SLIDER; m[nitems].text = TXT_SLIDE_UD; m[nitems].value = PlayerCfg.JoystickUndercalibrate[3]; m[nitems].min_value = 0; m[nitems].max_value = 16; nitems++;
	m[nitems].type = NM_TYPE_SLIDER; m[nitems].text = TXT_BANK_LR; m[nitems].value = PlayerCfg.JoystickUndercalibrate[4]; m[nitems].min_value = 0; m[nitems].max_value = 16; nitems++;
	m[nitems].type = NM_TYPE_SLIDER; m[nitems].text = TXT_THROTTLE; m[nitems].value = PlayerCfg.JoystickUndercalibrate[5]; m[nitems].min_value = 0; m[nitems].max_value = 16; nitems++;

	newmenu_do1(NULL, "SENSITIVITY & DEADZONE", nitems, m, NULL, NULL, 1);

	for (i = 0; i <= 5; i++)
	{
		PlayerCfg.JoystickSens[i] = m[joysens+i].value;
		PlayerCfg.JoystickDead[i] = m[joydead+i].value;
		PlayerCfg.JoystickUndercalibrate[i] = m[joyunder+i].value;
	}
}

#ifdef __3DS__
#include <3ds.h>
extern int g_gyro_enabled;
extern void gyro_calibrate_now(void);
extern void gyro_reset_calibration(void);

void gyro_config(void)
{
	int done = 0;
	while (!done) {
		newmenu_item m[10];
		int nitems = 0;
		int opt_gyro_on, opt_dead, opt_sens, opt_cal;

		opt_gyro_on = nitems;
		m[nitems].type = NM_TYPE_CHECK;
		m[nitems].text = "Enable Gyro Aim Assist";
		m[nitems].value = g_gyro_enabled;
		nitems++;

		m[nitems].type = NM_TYPE_TEXT; m[nitems].text = ""; nitems++;

		m[nitems].type = NM_TYPE_TEXT; m[nitems].text = "Gyro Deadzone (Default 8):"; nitems++;
		opt_dead = nitems;
		m[nitems].type = NM_TYPE_SLIDER;
		m[nitems].text = "Deadzone";
		m[nitems].value = PlayerCfg.GyroDeadzone;
		m[nitems].min_value = 0;
		m[nitems].max_value = 16;
		nitems++;

		m[nitems].type = NM_TYPE_TEXT; m[nitems].text = "Gyro Sensitivity (Default 8):"; nitems++;
		opt_sens = nitems;
		m[nitems].type = NM_TYPE_SLIDER;
		m[nitems].text = "Sensitivity";
		m[nitems].value = PlayerCfg.GyroSensitivity;
		m[nitems].min_value = 1;
		m[nitems].max_value = 16;
		nitems++;

		m[nitems].type = NM_TYPE_TEXT; m[nitems].text = ""; nitems++;

		opt_cal = nitems;
		m[nitems].type = NM_TYPE_MENU;
		m[nitems].text = "Calibrate Zero Bias (Hold Still)";
		nitems++;

		int choice = newmenu_do1(NULL, "GYROSCOPE SETTINGS", nitems, m, NULL, NULL, 0);

		int prev_enabled = g_gyro_enabled;
		g_gyro_enabled = m[opt_gyro_on].value;
		PlayerCfg.GyroDeadzone = m[opt_dead].value;
		PlayerCfg.GyroSensitivity = m[opt_sens].value;

		if (g_gyro_enabled != prev_enabled) {
			if (g_gyro_enabled) {
				gyro_reset_calibration();
				HIDUSER_EnableGyroscope();
			} else {
				HIDUSER_DisableGyroscope();
			}
		}

		if (choice == opt_cal) {
			gyro_calibrate_now();
			nm_messagebox(NULL, 1, TXT_OK, "Gyro zero-bias calibrated.\nHold console steady while playing.");
		} else {
			done = 1;
		}
	}
}
#endif

static int opt_ic_confjoy = 0, opt_ic_joymousesens = 0, opt_ic_stickyrear = 0, opt_ic_help0 = 0, opt_ic_help2 = 0;
#ifdef __3DS__
static int opt_ic_gyro = 0;
#endif

int input_config_menuset(newmenu *menu, d_event *event, void *userdata)
{
	newmenu_item *items = newmenu_get_items(menu);
	int citem = newmenu_get_citem(menu);

	userdata = userdata;

	switch (event->type)
	{
		case EVENT_NEWMENU_CHANGED:
			if (citem == opt_ic_stickyrear)			
				PlayerCfg.StickyRearview = items[citem].value;			
			break;

		case EVENT_NEWMENU_SELECTED:
			if (citem == opt_ic_confjoy)
				kconfig(1, "JOYSTICK");
			if (citem == opt_ic_joymousesens)
				input_config_sensitivity();
#ifdef __3DS__
			if (citem == opt_ic_gyro)
				gyro_config();
#endif
			if (citem == opt_ic_help0)
				show_controls_3ds();
			if (citem == opt_ic_help2)
				show_newdemo_help();
			return 1;		// stay in menu
			break;

		default:
			break;
	}

	return 0;
}

void input_config()
{
	newmenu_item m[10];
	int nitems = 0;

	PlayerCfg.ControlType |= CONTROL_USING_JOYSTICK;
	PlayerCfg.ControlType &= ~CONTROL_USING_MOUSE;

	opt_ic_confjoy = nitems;
	m[nitems].type = NM_TYPE_MENU; m[nitems].text = "CUSTOMIZE JOYSTICK"; nitems++;
	opt_ic_joymousesens = nitems;
	m[nitems].type = NM_TYPE_MENU; m[nitems].text = "SENSITIVITY & DEADZONE"; nitems++;
#ifdef __3DS__
	opt_ic_gyro = nitems;
	m[nitems].type = NM_TYPE_MENU; m[nitems].text = "GYROSCOPE SETTINGS"; nitems++;
#endif
	m[nitems].type = NM_TYPE_TEXT; m[nitems].text = ""; nitems++;
	opt_ic_stickyrear = nitems;
	m[nitems].type = NM_TYPE_CHECK; m[nitems].text= "Sticky Rearview"; m[nitems].value = PlayerCfg.StickyRearview; nitems++;
	m[nitems].type = NM_TYPE_TEXT; m[nitems].text = ""; nitems++;
	opt_ic_help0 = nitems;
	m[nitems].type = NM_TYPE_MENU; m[nitems].text = "GAME SYSTEM KEYS"; nitems++;
	opt_ic_help2 = nitems;
	m[nitems].type = NM_TYPE_MENU; m[nitems].text = "DEMO SYSTEM KEYS"; nitems++;

	newmenu_do1(NULL, TXT_CONTROLS, nitems, m, input_config_menuset, NULL, 0);

	PlayerCfg.ControlType |= CONTROL_USING_JOYSTICK;
	PlayerCfg.ControlType &= ~CONTROL_USING_MOUSE;
}

void reticle_config()
{
#ifdef OGL
	newmenu_item m[18];
#else
	newmenu_item m[17];
#endif
	int nitems = 0, i, opt_ret_type, opt_ret_rgba, opt_ret_size;
	
	m[nitems].type = NM_TYPE_TEXT; m[nitems].text = "Reticle Type:"; nitems++;
	opt_ret_type = nitems;
	m[nitems].type = NM_TYPE_RADIO; m[nitems].text = "Classic"; m[nitems].value = 0; m[nitems].group = 0; nitems++;
#ifdef OGL
	m[nitems].type = NM_TYPE_RADIO; m[nitems].text = "Classic Reboot"; m[nitems].value = 0; m[nitems].group = 0; nitems++;
#endif
	m[nitems].type = NM_TYPE_RADIO; m[nitems].text = "None"; m[nitems].value = 0; m[nitems].group = 0; nitems++;
	m[nitems].type = NM_TYPE_RADIO; m[nitems].text = "X"; m[nitems].value = 0; m[nitems].group = 0; nitems++;
	m[nitems].type = NM_TYPE_RADIO; m[nitems].text = "Dot"; m[nitems].value = 0; m[nitems].group = 0; nitems++;
	m[nitems].type = NM_TYPE_RADIO; m[nitems].text = "Circle"; m[nitems].value = 0; m[nitems].group = 0; nitems++;
	m[nitems].type = NM_TYPE_RADIO; m[nitems].text = "Cross V1"; m[nitems].value = 0; m[nitems].group = 0; nitems++;
	m[nitems].type = NM_TYPE_RADIO; m[nitems].text = "Cross V2"; m[nitems].value = 0; m[nitems].group = 0; nitems++;
	m[nitems].type = NM_TYPE_RADIO; m[nitems].text = "Angle"; m[nitems].value = 0; m[nitems].group = 0; nitems++;
	m[nitems].type = NM_TYPE_TEXT; m[nitems].text = ""; nitems++;
	m[nitems].type = NM_TYPE_TEXT; m[nitems].text = "Reticle Color:"; nitems++;
	opt_ret_rgba = nitems;
	m[nitems].type = NM_TYPE_SLIDER; m[nitems].text = "Red"; m[nitems].value = (PlayerCfg.ReticleRGBA[0]/2); m[nitems].min_value = 0; m[nitems].max_value = 16; nitems++;
	m[nitems].type = NM_TYPE_SLIDER; m[nitems].text = "Green"; m[nitems].value = (PlayerCfg.ReticleRGBA[1]/2); m[nitems].min_value = 0; m[nitems].max_value = 16; nitems++;
	m[nitems].type = NM_TYPE_SLIDER; m[nitems].text = "Blue"; m[nitems].value = (PlayerCfg.ReticleRGBA[2]/2); m[nitems].min_value = 0; m[nitems].max_value = 16; nitems++;
	m[nitems].type = NM_TYPE_SLIDER; m[nitems].text = "Alpha"; m[nitems].value = (PlayerCfg.ReticleRGBA[3]/2); m[nitems].min_value = 0; m[nitems].max_value = 16; nitems++;
	m[nitems].type = NM_TYPE_TEXT; m[nitems].text = ""; nitems++;
	opt_ret_size = nitems;
	m[nitems].type = NM_TYPE_SLIDER; m[nitems].text = "Reticle Size:"; m[nitems].value = PlayerCfg.ReticleSize; m[nitems].min_value = 0; m[nitems].max_value = 4; nitems++;

	i = PlayerCfg.ReticleType;
#ifndef OGL
	if (i > 1) i--;
#endif
	m[opt_ret_type+i].value=1;

	newmenu_do1( NULL, "Reticle Options", nitems, m, NULL, NULL, 1 );

#ifdef OGL
	for (i = 0; i < 9; i++)
		if (m[i+opt_ret_type].value)
			PlayerCfg.ReticleType = i;
#else
	for (i = 0; i < 8; i++)
		if (m[i+opt_ret_type].value)
			PlayerCfg.ReticleType = i;
	if (PlayerCfg.ReticleType > 1) PlayerCfg.ReticleType++;
#endif
	for (i = 0; i < 4; i++)
		PlayerCfg.ReticleRGBA[i] = (m[i+opt_ret_rgba].value*2);
	PlayerCfg.ReticleSize = m[opt_ret_size].value;
}

int opt_gr_brightness, opt_gr_reticlemenu, opt_gr_alphafx, opt_gr_dynlightcolor, opt_gr_fpsindi, opt_gr_disablecockpit;
int graphics_config_menuset(newmenu *menu, d_event *event, void *userdata)
{
	newmenu_item *items = newmenu_get_items(menu);
	int citem = newmenu_get_citem(menu);

	userdata = userdata;

	switch (event->type)
	{
		case EVENT_NEWMENU_CHANGED:
			if ( citem == opt_gr_brightness)
				gr_palette_set_gamma(items[citem].value);
			break;

		case EVENT_NEWMENU_SELECTED:
			if (citem == opt_gr_reticlemenu)
				reticle_config();
			return 1;		// stay in menu
			break;

		default:
			break;
	}

	return 0;
}

void graphics_config()
{
	newmenu_item m[7];
	int nitems = 0;

	opt_gr_brightness = nitems;
	m[nitems].type = NM_TYPE_SLIDER; m[nitems].text = TXT_BRIGHTNESS; m[nitems].value = gr_palette_get_gamma(); m[nitems].min_value = 0; m[nitems].max_value = 16; nitems++;
	opt_gr_reticlemenu = nitems;
	m[nitems].type = NM_TYPE_MENU; m[nitems].text = "Reticle Options"; nitems++;
#ifdef OGL
	opt_gr_alphafx = nitems;
	m[nitems].type = NM_TYPE_CHECK; m[nitems].text = "Transparency Effects"; m[nitems].value = PlayerCfg.AlphaEffects; nitems++;
	opt_gr_dynlightcolor = nitems;
	m[nitems].type = NM_TYPE_CHECK; m[nitems].text = "Colored Dynamic Light"; m[nitems].value = PlayerCfg.DynLightColor; nitems++;
#endif
	opt_gr_fpsindi = nitems;
	m[nitems].type = NM_TYPE_CHECK; m[nitems].text="FPS Counter"; m[nitems].value = GameCfg.FPSIndicator; nitems++;

	opt_gr_disablecockpit = nitems;
	m[nitems].type = NM_TYPE_CHECK; m[nitems].text="Disable Cockpit View"; m[nitems].value = PlayerCfg.DisableCockpit; nitems++;

	newmenu_do1( NULL, "Graphics Options", nitems, m, graphics_config_menuset, NULL, 1 );

#ifdef OGL
	PlayerCfg.AlphaEffects = m[opt_gr_alphafx].value;
	PlayerCfg.DynLightColor = m[opt_gr_dynlightcolor].value;
	GameCfg.TexFilt = 1; // Locked to Bilinear on 3DS
#endif
	GameCfg.GammaLevel = m[opt_gr_brightness].value;
	GameCfg.FPSIndicator = m[opt_gr_fpsindi].value;
	PlayerCfg.DisableCockpit = m[opt_gr_disablecockpit].value; 
}

/* =========================================================================
 * 3DS Sound Effects, Music & Roland SC-55 Soundtrack Jukebox
 * Author: Dennis Isaac Gutierrez Zeledon (Dennis)
 * ========================================================================= */

static int opt_sm_digivol = -1;
static int opt_sm_musicvol = -1;
static int opt_sm_revstereo = -1;
static int opt_sm_mtype_none = -1;
static int opt_sm_mtype_builtin = -1;
static int opt_sm_mtype_jukebox = -1;
static int opt_sm_jukebox_menu = -1;

int sound_menuset(newmenu *menu, d_event *event, void *userdata)
{
	newmenu_item *items = newmenu_get_items(menu);
	int citem = newmenu_get_citem(menu);
	int replay = 0;
	(void)userdata;

	switch (event->type)
	{
		case EVENT_NEWMENU_CHANGED:
			if (citem == opt_sm_digivol)
			{
				GameCfg.DigiVolume = items[citem].value;
				digi_set_digi_volume((GameCfg.DigiVolume * 32768) / 8);
				digi_play_sample_once(SOUND_DROP_BOMB, F1_0);
			}
			else if (citem == opt_sm_musicvol)
			{
				GameCfg.MusicVolume = items[citem].value;
				songs_set_volume(GameCfg.MusicVolume);
			}
			else if (citem == opt_sm_revstereo)
			{
				GameCfg.ReverseStereo = items[citem].value;
			}
			else if (citem == opt_sm_mtype_none)
			{
				GameCfg.MusicType = MUSIC_TYPE_NONE;
				jukebox_stop();
				replay = 1;
			}
			else if (citem == opt_sm_mtype_builtin)
			{
				GameCfg.MusicType = MUSIC_TYPE_BUILTIN;
				jukebox_stop();
				replay = 1;
			}
			else if (citem == opt_sm_mtype_jukebox)
			{
				GameCfg.MusicType = MUSIC_TYPE_CUSTOM;
				jukebox_stop();
				replay = 1;
			}
			break;

		case EVENT_NEWMENU_SELECTED:
			if (citem == opt_sm_jukebox_menu)
			{
				do_jukebox_menu();
				return 1;
			}
			break;

		case EVENT_WINDOW_ACTIVATED:
			if (opt_sm_musicvol >= 0)
				items[opt_sm_musicvol].value = GameCfg.MusicVolume;
			if (opt_sm_mtype_none >= 0)
				items[opt_sm_mtype_none].value = (GameCfg.MusicType == MUSIC_TYPE_NONE);
			if (opt_sm_mtype_builtin >= 0)
				items[opt_sm_mtype_builtin].value = (GameCfg.MusicType == MUSIC_TYPE_BUILTIN);
			if (opt_sm_mtype_jukebox >= 0)
				items[opt_sm_mtype_jukebox].value = (GameCfg.MusicType == MUSIC_TYPE_CUSTOM);
			break;

		default:
			break;
	}

	if (replay)
	{
		songs_uninit();

		if (Game_wind)
			songs_play_level_song(Current_level_num, 0);
		else
			songs_play_song(SONG_TITLE, 1);
	}

	return 0;
}

void do_sound_menu(void)
{
	newmenu_item m[10];
	int nitems = 0;

	opt_sm_digivol = nitems;
	m[nitems].type = NM_TYPE_SLIDER; m[nitems].text = TXT_FX_VOLUME; m[nitems].value = GameCfg.DigiVolume; m[nitems].min_value = 0; m[nitems++].max_value = 8;

	opt_sm_musicvol = nitems;
	m[nitems].type = NM_TYPE_SLIDER; m[nitems].text = "Music Volume"; m[nitems].value = GameCfg.MusicVolume; m[nitems].min_value = 0; m[nitems++].max_value = 8;

	opt_sm_revstereo = nitems;
	m[nitems].type = NM_TYPE_CHECK; m[nitems].text = TXT_REVERSE_STEREO; m[nitems++].value = GameCfg.ReverseStereo;

	m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = "";
	m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = "Music Type:";

	opt_sm_mtype_none = nitems;
	m[nitems].type = NM_TYPE_RADIO; m[nitems].text = "No Music"; m[nitems].value = (GameCfg.MusicType == MUSIC_TYPE_NONE); m[nitems].group = 0; nitems++;

	opt_sm_mtype_builtin = nitems;
	m[nitems].type = NM_TYPE_RADIO; m[nitems].text = "Built-in / Addon Music"; m[nitems].value = (GameCfg.MusicType == MUSIC_TYPE_BUILTIN); m[nitems].group = 0; nitems++;

	opt_sm_mtype_jukebox = nitems;
	m[nitems].type = NM_TYPE_RADIO; m[nitems].text = "Soundtrack Jukebox"; m[nitems].value = (GameCfg.MusicType == MUSIC_TYPE_CUSTOM); m[nitems].group = 0; nitems++;

	m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = "";

	opt_sm_jukebox_menu = nitems;
	m[nitems].type = NM_TYPE_MENU; m[nitems].text = "Soundtrack Jukebox..."; m[nitems++].value = 0;

	newmenu_do1(NULL, "Sound Effects & Music", nitems, m, sound_menuset, NULL, 0);
}

/* =========================================================================
 * Roland SC-55 Soundtrack Jukebox Menu Implementation
 * ========================================================================= */

static char s_jb_status[48];
static char s_jb_track[64];
static char *s_jukebox_catalog_titles[JUKEBOX_TOTAL_TRACKS];

static void jukebox_update_display_strings(void)
{
	int state = jukebox_get_state();
	const char *title = jukebox_get_current_title();

	if (state == JUKEBOX_STATE_PLAYING)
		snprintf(s_jb_status, sizeof(s_jb_status), "Status: Playing");
	else if (state == JUKEBOX_STATE_PAUSED)
		snprintf(s_jb_status, sizeof(s_jb_status), "Status: Paused");
	else
		snprintf(s_jb_status, sizeof(s_jb_status), "Status: Stopped");

	if (title && title[0])
		snprintf(s_jb_track, sizeof(s_jb_track), "Track: %s", title);
	else
		snprintf(s_jb_track, sizeof(s_jb_track), "Track: None");
}

static int jukebox_track_select_handler(listbox *lb, d_event *event, void *userdata)
{
	int citem = listbox_get_citem(lb);
	(void)userdata;

	switch (event->type)
	{
		case EVENT_NEWMENU_SELECTED:
			if (citem >= 0 && citem < JUKEBOX_TOTAL_TRACKS)
			{
				jukebox_play_track(citem);
			}
			return 0; // Closes listbox window

		default:
			break;
	}

	return 0;
}

static int opt_jb_status = -1;
static int opt_jb_track = -1;
static int opt_jb_vol = -1;
static int opt_jb_select = -1;
static int opt_jb_play = -1;
static int opt_jb_pause = -1;
static int opt_jb_stop = -1;
static int opt_jb_prev = -1;
static int opt_jb_next = -1;
static int opt_jb_mode_loop = -1;
static int opt_jb_mode_seq = -1;
static int opt_jb_mode_shuf = -1;

int jukebox_menuset(newmenu *menu, d_event *event, void *userdata)
{
	newmenu_item *items = newmenu_get_items(menu);
	int citem = newmenu_get_citem(menu);
	(void)userdata;

	switch (event->type)
	{
		case EVENT_NEWMENU_CHANGED:
			if (citem == opt_jb_vol)
			{
				GameCfg.MusicVolume = items[citem].value;
				songs_set_volume(GameCfg.MusicVolume);
			}
			else if (citem == opt_jb_mode_loop)
			{
				jukebox_set_mode(JUKEBOX_MODE_LOOP);
			}
			else if (citem == opt_jb_mode_seq)
			{
				jukebox_set_mode(JUKEBOX_MODE_SEQUENTIAL);
			}
			else if (citem == opt_jb_mode_shuf)
			{
				jukebox_set_mode(JUKEBOX_MODE_SHUFFLE);
			}
			jukebox_update_display_strings();
			break;

		case EVENT_NEWMENU_SELECTED:
			if (citem == opt_jb_select)
			{
				int t;
				listbox *lb;
				window *w;

				for (t = 0; t < JUKEBOX_TOTAL_TRACKS; t++)
					s_jukebox_catalog_titles[t] = (char *)sc55_catalog[t].title;

				lb = newmenu_listbox1("ROLAND SC-55 CATALOG", JUKEBOX_TOTAL_TRACKS, s_jukebox_catalog_titles, 1, jukebox_get_current_track(), jukebox_track_select_handler, NULL);
				if (lb)
				{
					w = listbox_get_window(lb);
					while (window_exists(w))
						event_process();
				}
				jukebox_update_display_strings();
				return 1;
			}
			else if (citem == opt_jb_play)
			{
				jukebox_play();
				jukebox_update_display_strings();
				return 1;
			}
			else if (citem == opt_jb_pause)
			{
				jukebox_pause_resume();
				jukebox_update_display_strings();
				return 1;
			}
			else if (citem == opt_jb_stop)
			{
				jukebox_stop();
				jukebox_update_display_strings();
				return 1;
			}
			else if (citem == opt_jb_prev)
			{
				jukebox_prev();
				jukebox_update_display_strings();
				return 1;
			}
			else if (citem == opt_jb_next)
			{
				jukebox_next();
				jukebox_update_display_strings();
				return 1;
			}
			break;

		case EVENT_WINDOW_ACTIVATED:
			jukebox_update_display_strings();
			if (opt_jb_vol >= 0)
				items[opt_jb_vol].value = GameCfg.MusicVolume;
			if (opt_jb_mode_loop >= 0)
				items[opt_jb_mode_loop].value = (jukebox_get_mode() == JUKEBOX_MODE_LOOP);
			if (opt_jb_mode_seq >= 0)
				items[opt_jb_mode_seq].value = (jukebox_get_mode() == JUKEBOX_MODE_SEQUENTIAL);
			if (opt_jb_mode_shuf >= 0)
				items[opt_jb_mode_shuf].value = (jukebox_get_mode() == JUKEBOX_MODE_SHUFFLE);
			break;

		case EVENT_IDLE:
			jukebox_update_display_strings();
			break;

		default:
			break;
	}

	return 0;
}

void do_jukebox_menu(void)
{
	newmenu_item m[14];
	int nitems = 0;

	jukebox_update_display_strings();

	opt_jb_status = nitems;
	m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = s_jb_status;

	opt_jb_track = nitems;
	m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = s_jb_track;

	opt_jb_vol = nitems;
	m[nitems].type = NM_TYPE_SLIDER; m[nitems].text = "Music Volume"; m[nitems].value = GameCfg.MusicVolume; m[nitems].min_value = 0; m[nitems++].max_value = 8;

	m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = "";

	opt_jb_select = nitems;
	m[nitems].type = NM_TYPE_MENU; m[nitems++].text = "Select Track from Catalog...";

	opt_jb_play = nitems;
	m[nitems].type = NM_TYPE_MENU; m[nitems++].text = "Play Track";

	opt_jb_pause = nitems;
	m[nitems].type = NM_TYPE_MENU; m[nitems++].text = "Pause / Resume";

	opt_jb_stop = nitems;
	m[nitems].type = NM_TYPE_MENU; m[nitems++].text = "Stop";

	opt_jb_prev = nitems;
	m[nitems].type = NM_TYPE_MENU; m[nitems++].text = "Previous Track";

	opt_jb_next = nitems;
	m[nitems].type = NM_TYPE_MENU; m[nitems++].text = "Next Track";

	m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = "";

	opt_jb_mode_loop = nitems;
	m[nitems].type = NM_TYPE_RADIO; m[nitems].text = "Loop Current Track"; m[nitems].value = (jukebox_get_mode() == JUKEBOX_MODE_LOOP); m[nitems].group = 0; nitems++;

	opt_jb_mode_seq = nitems;
	m[nitems].type = NM_TYPE_RADIO; m[nitems].text = "Play Sequentially"; m[nitems].value = (jukebox_get_mode() == JUKEBOX_MODE_SEQUENTIAL); m[nitems].group = 0; nitems++;

	opt_jb_mode_shuf = nitems;
	m[nitems].type = NM_TYPE_RADIO; m[nitems].text = "Shuffle / Random"; m[nitems].value = (jukebox_get_mode() == JUKEBOX_MODE_SHUFFLE); m[nitems].group = 0; nitems++;

	newmenu_do1(NULL, "Soundtrack Jukebox", nitems, m, jukebox_menuset, NULL, opt_jb_select);
}

#define ADD_CHECK(n,txt,v)  do { m[n].type=NM_TYPE_CHECK; m[n].text=txt; m[n].value=v;} while (0)

int menu_misc_options_handler ( newmenu *menu, d_event *event, void *userdata );

void print_ship_color(char* color_string, int color_value) {
	char color[10];
	switch(color_value) {
		case 0:  sprintf(color, "%s", "Blue"); break;
		case 1:  sprintf(color, "%s", "Red"); break;
		case 2:  sprintf(color, "%s", "Green"); break;
		case 3:  sprintf(color, "%s", "Pink"); break;
		case 4:  sprintf(color, "%s", "Orange"); break;
		case 5:  sprintf(color, "%s", "Purple"); break;
		case 6:  sprintf(color, "%s", "White"); break;
		case 7:  sprintf(color, "%s", "Yellow"); break;
		case 8:  sprintf(color, "%s", "None"); break;
		default: sprintf(color, "%s", "???"); 
	}

	sprintf( color_string, "Wing Color: %s", color);
}

void print_missile_color(char* color_string, int color_value) {
	char color[11];
	switch(color_value) {
		case 0:  sprintf(color, "%s", "Blue"); break;
		case 1:  sprintf(color, "%s", "Red"); break;
		case 2:  sprintf(color, "%s", "Green"); break;
		case 3:  sprintf(color, "%s", "Pink"); break;
		case 4:  sprintf(color, "%s", "Orange"); break;
		case 5:  sprintf(color, "%s", "Purple"); break;
		case 6:  sprintf(color, "%s", "White"); break;
		case 7:  sprintf(color, "%s", "Yellow"); break;
		case 8:  sprintf(color, "%s", "Match Ship"); break;
		default: sprintf(color, "%s", "???"); 
	}

	sprintf( color_string, "Missiles/Guns: %s", color);
}

void do_misc_menu()
{
	newmenu_item m[15];
	int i = 0;

	do {
		ADD_CHECK(0, "Ship auto-leveling", PlayerCfg.AutoLeveling);
		ADD_CHECK(1, "Persistent Debris",PlayerCfg.PersistentDebris);
		ADD_CHECK(2, "Screenshots w/o HUD",PlayerCfg.PRShot);
		ADD_CHECK(3, "No redundant pickup messages",PlayerCfg.NoRedundancy);
		ADD_CHECK(4, "Show D2-style Prox. Bomb Gauge",PlayerCfg.BombGauge);
		ADD_CHECK(5, "Free Flight controls in Automap",PlayerCfg.AutomapFreeFlight);
		ADD_CHECK(6, "No Weapon Autoselect when firing",PlayerCfg.NoFireAutoselect);		
		ADD_CHECK(7, "Autoselect after firing",PlayerCfg.SelectAfterFire);
		ADD_CHECK(8, "Only Cycle Autoselect Weapons",PlayerCfg.CycleAutoselectOnly);		
		ADD_CHECK(9, "Ammo Warnings",PlayerCfg.VulcanAmmoWarnings);
		ADD_CHECK(10, "Shield Warnings",PlayerCfg.ShieldWarnings);
		ADD_CHECK(11, "Automatically Start Demos",PlayerCfg.AutoDemo);
		
		char preferred_color[30];
		print_ship_color(preferred_color, PlayerCfg.ShipColor); 
		m[12].type = NM_TYPE_SLIDER; 
		m[12].value= PlayerCfg.ShipColor; 
		m[12].text= preferred_color; 
		m[12].min_value=0; 
		m[12].max_value=8; 

		char missile_color[30];
		print_missile_color(missile_color, PlayerCfg.MissileColor); 
		m[13].type = NM_TYPE_SLIDER; 
		m[13].value= PlayerCfg.MissileColor; 
		m[13].text = missile_color; 
		m[13].min_value=0; 
		m[13].max_value=8; 		

		ADD_CHECK(14, "Show Custom Ship Colors", PlayerCfg.ShowCustomColors);

		i = newmenu_do1( NULL, "Misc Options", sizeof(m)/sizeof(*m), m, menu_misc_options_handler, NULL, i );

		PlayerCfg.AutoLeveling			= m[0].value;
		PlayerCfg.PersistentDebris		= m[1].value;
		PlayerCfg.PRShot 			= m[2].value;
		PlayerCfg.NoRedundancy 			= m[3].value;
		PlayerCfg.BombGauge 			= m[4].value;
		PlayerCfg.AutomapFreeFlight		= m[5].value;
		PlayerCfg.NoFireAutoselect		= m[6].value;
		PlayerCfg.SelectAfterFire       = m[7].value;  if(PlayerCfg.SelectAfterFire) { PlayerCfg.NoFireAutoselect = 1; }
		PlayerCfg.CycleAutoselectOnly		= m[8].value;
		PlayerCfg.VulcanAmmoWarnings = m[9].value; 
		PlayerCfg.ShieldWarnings = m[10].value; 
		PlayerCfg.AutoDemo = m[11].value;
		PlayerCfg.ShowCustomColors = m[14].value;

	} while( i>-1 );

}

int menu_misc_options_handler ( newmenu *menu, d_event *event, void *userdata )
{
	
	newmenu_item *menus = newmenu_get_items(menu);
	int citem = newmenu_get_citem(menu);
	
	if (event->type == EVENT_NEWMENU_CHANGED)
	{
		if (citem == 12) {
			PlayerCfg.ShipColor = menus[12].value;
			print_ship_color(menus[12].text, PlayerCfg.ShipColor);			
		} else if (citem == 13) {
			PlayerCfg.MissileColor = menus[13].value;
			print_missile_color(menus[13].text, PlayerCfg.MissileColor);			
		}		
	}
	
	return 0;
}

#if defined(USE_UDP)
static int multi_player_menu_handler(newmenu *menu, d_event *event, int *menu_choice)
{
	newmenu_item *items = newmenu_get_items(menu);

	switch (event->type)
	{
		case EVENT_NEWMENU_SELECTED:
			// stay in multiplayer menu, even after having played a game
			return do_option(menu_choice[newmenu_get_citem(menu)]);

		case EVENT_WINDOW_CLOSE:
			d_free(menu_choice);
			d_free(items);
			break;

		default:
			break;
	}

	return 0;
}

void do_multi_player_menu()
{
	int *menu_choice;
	newmenu_item *m;
	int num_options = 0;

	MALLOC(menu_choice, int, 3);
	if (!menu_choice)
		return;

	MALLOC(m, newmenu_item, 3);
	if (!m)
	{
		d_free(menu_choice);
		return;
	}

#ifdef USE_UDP
	m[num_options].type=NM_TYPE_MENU; m[num_options].text="HOST GAME"; menu_choice[num_options]=MENU_START_UDP_NETGAME; num_options++;
#ifdef USE_TRACKER
	m[num_options].type=NM_TYPE_MENU; m[num_options].text="FIND LAN/ONLINE GAMES"; menu_choice[num_options]=MENU_JOIN_LIST_UDP_NETGAME; num_options++;
#else
	m[num_options].type=NM_TYPE_MENU; m[num_options].text="FIND LAN GAMES"; menu_choice[num_options]=MENU_JOIN_LIST_UDP_NETGAME; num_options++;
#endif
	m[num_options].type=NM_TYPE_MENU; m[num_options].text="JOIN GAME MANUALLY"; menu_choice[num_options]=MENU_JOIN_MANUAL_UDP_NETGAME; num_options++;
#endif

	newmenu_do3( NULL, TXT_MULTIPLAYER, num_options, m, (int (*)(newmenu *, d_event *, void *))multi_player_menu_handler, menu_choice, 0, NULL );
}
#endif

void do_options_menu()
{
	newmenu_item *m;

	MALLOC(m, newmenu_item, 9);
	if (!m)
		return;

	m[ 0].type = NM_TYPE_MENU;   m[ 0].text="Sound effects & music...";
	m[ 1].type = NM_TYPE_TEXT;   m[ 1].text="";
	m[ 2].type = NM_TYPE_MENU;   m[ 2].text=TXT_CONTROLS_;
	m[ 3].type = NM_TYPE_TEXT;   m[ 3].text="";
	m[ 4].type = NM_TYPE_MENU;   m[ 4].text="Graphics Options...";
	m[ 5].type = NM_TYPE_TEXT;   m[ 5].text="";
	m[ 6].type = NM_TYPE_MENU;   m[ 6].text="Primary autoselect ordering...";
	m[ 7].type = NM_TYPE_MENU;   m[ 7].text="Secondary autoselect ordering...";
	m[ 8].type = NM_TYPE_MENU;   m[ 8].text="Misc Options...";

	// Fall back to main event loop
	newmenu_do3( NULL, TXT_OPTIONS, 9, m, options_menuset, NULL, 0, NULL );
}

#ifndef RELEASE
int polygon_models_viewer_handler(window *wind, d_event *event)
{
	static int view_idx = 0;
	int key = 0;
	static vms_angvec ang;

	switch (event->type)
	{
		case EVENT_WINDOW_ACTIVATED:
			key_toggle_repeat(1);
			view_idx = 0;
			ang.p = ang.b = 0;
			ang.h = F0_5-1;
			break;
		case EVENT_KEY_COMMAND:
			key = event_key_get(event);
			switch (key)
			{
				case KEY_ESC:
					window_close(wind);
					break;
				case KEY_SPACEBAR:
					view_idx ++;
					if (view_idx >= N_polygon_models) view_idx = 0;
					break;
				case KEY_BACKSP:
					view_idx --;
					if (view_idx < 0 ) view_idx = N_polygon_models - 1;
					break;
				case KEY_A:
					ang.h -= 100;
					break;
				case KEY_D:
					ang.h += 100;
					break;
				case KEY_W:
					ang.p -= 100;
					break;
				case KEY_S:
					ang.p += 100;
					break;
				case KEY_Q:
					ang.b -= 100;
					break;
				case KEY_E:
					ang.b += 100;
					break;
				case KEY_R:
					ang.p = ang.b = 0;
					ang.h = F0_5-1;
					break;
				default:
					break;
			}
			return 1;
		case EVENT_WINDOW_DRAW:
			timer_delay(F1_0/60);
			draw_model_picture(view_idx, &ang);
			gr_set_curfont(GAME_FONT);
			gr_set_fontcolor(BM_XRGB(255,255,255), -1);
			gr_printf(FSPACX(1), FSPACY(1), "ESC: leave\nSPACE/BACKSP: next/prev model (%i/%i)\nA/D: rotate y\nW/S: rotate x\nQ/E: rotate z\nR: reset orientation",view_idx,N_polygon_models-1);
			break;
		case EVENT_WINDOW_CLOSE:
			key_toggle_repeat(0);
			break;
		default:
			break;
	}
	
	return 0;
}

void polygon_models_viewer()
{
	window *wind = window_create(&grd_curscreen->sc_canvas, 0, 0, SWIDTH, SHEIGHT, (int (*)(window *, d_event *, void *))polygon_models_viewer_handler, NULL);
	if (!wind)
	{
		d_event event = { EVENT_WINDOW_CLOSE };
		polygon_models_viewer_handler(NULL, &event);
		return;
	}

#ifdef __3DS__
	/* 3DS: if APT requests power-off/sleep, bail the modal menu loop so the
	 * OS can shut us down instead of waiting for the user to dismiss it. */
	extern volatile int d1x_powering_off;
	while (window_exists(wind) && !d1x_powering_off)
#else
	while (window_exists(wind))
#endif
		event_process();
}

int gamebitmaps_viewer_handler(window *wind, d_event *event)
{
	static int view_idx = 0;
	int key = 0;
#ifdef OGL
	float scale = 1.0;
#endif
	bitmap_index bi;
	grs_bitmap *bm;
	extern int Num_bitmap_files;

	switch (event->type)
	{
		case EVENT_WINDOW_ACTIVATED:
			key_toggle_repeat(1);
			view_idx = 0;
			break;
		case EVENT_KEY_COMMAND:
			key = event_key_get(event);
			switch (key)
			{
				case KEY_ESC:
					window_close(wind);
					break;
				case KEY_SPACEBAR:
					view_idx ++;
					if (view_idx >= Num_bitmap_files) view_idx = 0;
					break;
				case KEY_BACKSP:
					view_idx --;
					if (view_idx < 0 ) view_idx = Num_bitmap_files - 1;
					break;
				default:
					break;
			}
			return 1;
		case EVENT_WINDOW_DRAW:
			bi.index = view_idx;
			bm = &GameBitmaps[view_idx];
			timer_delay(F1_0/60);
			PIGGY_PAGE_IN(bi);
			gr_clear_canvas( BM_XRGB(0,0,0) );
#if defined(OGL) && !defined(__3DS__)
				scale = (bm->bm_w > bm->bm_h)?(SHEIGHT/bm->bm_w)*0.8:(SHEIGHT/bm->bm_h)*0.8;
				ogl_ubitmapm_cs((SWIDTH/2)-(bm->bm_w*scale/2),(SHEIGHT/2)-(bm->bm_h*scale/2),bm->bm_w*scale,bm->bm_h*scale,bm,-1,F1_0);
#else
				gr_bitmap((SWIDTH/2)-(bm->bm_w/2), (SHEIGHT/2)-(bm->bm_h/2), bm);
#endif
			gr_set_curfont(GAME_FONT);
			gr_set_fontcolor(BM_XRGB(255,255,255), -1);
			gr_printf(FSPACX(1), FSPACY(1), "ESC: leave\nSPACE/BACKSP: next/prev bitmap (%i/%i)",view_idx,Num_bitmap_files-1);
			break;
		case EVENT_WINDOW_CLOSE:
			key_toggle_repeat(0);
			break;
		default:
			break;
	}
	
	return 0;
}

void gamebitmaps_viewer()
{
	window *wind = window_create(&grd_curscreen->sc_canvas, 0, 0, SWIDTH, SHEIGHT, (int (*)(window *, d_event *, void *))gamebitmaps_viewer_handler, NULL);
	if (!wind)
	{
		d_event event = { EVENT_WINDOW_CLOSE };
		gamebitmaps_viewer_handler(NULL, &event);
		return;
	}

#ifdef __3DS__
	/* 3DS: if APT requests power-off/sleep, bail the modal menu loop so the
	 * OS can shut us down instead of waiting for the user to dismiss it. */
	extern volatile int d1x_powering_off;
	while (window_exists(wind) && !d1x_powering_off)
#else
	while (window_exists(wind))
#endif
		event_process();
}

int sandbox_menuset(newmenu *menu, d_event *event, void *userdata)
{
	switch (event->type)
	{
		case EVENT_NEWMENU_CHANGED:
			break;

		case EVENT_NEWMENU_SELECTED:
			switch(newmenu_get_citem(menu))
			{
				case  0: polygon_models_viewer(); break;
				case  1: gamebitmaps_viewer(); break;
			}
			return 1; // stay in menu until escape
			break;

		case EVENT_WINDOW_CLOSE:
		{
			newmenu_item *items = newmenu_get_items(menu);
			d_free(items);
			break;
		}

		default:
			break;
	}

	userdata = userdata; //kill warning

	return 0;
}

void do_sandbox_menu()
{
	newmenu_item *m;

	MALLOC(m, newmenu_item, 2);
	if (!m)
		return;

	m[ 0].type = NM_TYPE_MENU;   m[ 0].text="Polygon_models viewer";
	m[ 1].type = NM_TYPE_MENU;   m[ 1].text="GameBitmaps viewer";

	newmenu_do3( NULL, "Coder's sandbox", 2, m, sandbox_menuset, NULL, 0, NULL );
}
#endif
