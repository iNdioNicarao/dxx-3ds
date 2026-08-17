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
COPYRIGHT 1993-1999 PARALLAX SOFTWARE CORPORATION.  ALL RIGHTS RESERVED.
*/

/*
 *
 * Stuff for rendering the HUD
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "timer.h"
#include "pstypes.h"
#include "console.h"
#include "inferno.h"
#include "dxxerror.h"
#include "gr.h"
#include "palette.h"
#include "bm.h"
#include "player.h"
#include "render.h"
#include "menu.h"
#include "newmenu.h"
#include "screens.h"
#include "fix.h"
#include "robot.h"
#include "game.h"
#include "gauges.h"
#include "gamefont.h"

// Stereo 3D state: 1 when the 3DS slider requests depth this frame. Set in
// game_render_frame_mono; read by ogl_swap_buffers_internal() so gr_flip()
// skips its own present (the eye pair is presented inside the mono renderer).
int g_stereo_active = 0;
static int stereo_hw_on = 0;	// tracks whether gfxSet3D/pglSetStereo are currently engaged

// v99k forward decls (defined further below) so stereo_sep_load/save can use them
void stereo_cfg_load_all(void);
void stereo_cfg_save_all(void);

// v99k: live stereo-separation preset so the user can SAMPLE depth strength
// in-game without a rebuild. g_stereo_sep_pct is the max eye separation (as a
// world-unit scale) at full slider; eye = F1_0 * (pct*0.35) * slider.
// Cycled with the 3DS depth slider (hardware) and persisted to
// sdmc:/3ds/d1/stereo_sep.cfg so the choice survives reboots. Default 4.
int g_stereo_sep_pct = 4;
static const int g_sep_presets[] = { 1, 2, 3, 4, 5, 6 };
static const int g_sep_npresets = (int)(sizeof(g_sep_presets)/sizeof(g_sep_presets[0]));

void stereo_sep_load(void)
{
	stereo_cfg_load_all();
}

void stereo_sep_save(void)
{
	stereo_cfg_save_all();
}

// step = +1 (increase) or -1 (decrease). Shows a HUD message and saves.
void stereo_sep_cycle(int step)
{
	int idx = 0;
	for (int i = 0; i < g_sep_npresets; i++)
		if (g_sep_presets[i] == g_stereo_sep_pct) { idx = i; break; }
	idx += step;
	if (idx < 0) idx = 0;
	if (idx >= g_sep_npresets) idx = g_sep_npresets - 1;
	g_stereo_sep_pct = g_sep_presets[idx];
	stereo_sep_save();
	char msg[48];
	snprintf(msg, sizeof(msg), "Stereo separation: %d%%", g_stereo_sep_pct);
	HUD_init_message_literal(HM_DEFAULT, msg);
}

// Load/save separation only (parallel mode is the only stereo mode now).
void stereo_cfg_load_all(void)
{
	FILE *f = fopen("sdmc:/3ds/d1/stereo_sep.cfg", "r");
	if (f) {
		int a=0;
		if (fscanf(f, "%d", &a) == 1) { g_stereo_sep_pct = a; }
		fclose(f);
	}
}

void stereo_cfg_save_all(void)
{
	FILE *f = fopen("sdmc:/3ds/d1/stereo_sep.cfg", "w");
	if (f) {
		fprintf(f, "%d\n", g_stereo_sep_pct);
		fclose(f);
	}
}

#include "newdemo.h"
#include "text.h"
#include "multi.h"
#include "endlevel.h"
#include "cntrlcen.h"
#include "fuelcen.h"
#include "powerup.h"
#include "laser.h"
#include "playsave.h"
#include "automap.h"
#include "mission.h"
#include "gameseq.h"
#include "args.h"

#ifdef OGL
#include "ogl_init.h"
#endif

int netplayerinfo_on=0;

#ifdef __3DS__
int benchmark_active = 0;
static int bench_secs = 0, bench_min = 999, bench_max = 0, bench_sum = 0;
// Phase timing for the stereo-3D headroom analysis: world geometry render
// (render_mine) is the part that DOUBLES in stereo. Measured in fix64
// milliseconds, accumulated per second so we can average and estimate 2*W.
int bench_world_ms = 0;
int bench_world_acc = 0;
void benchmark_toggle(void)
{
	benchmark_active = !benchmark_active;
	if (benchmark_active) {
		bench_secs = 0; bench_min = 999; bench_max = 0; bench_sum = 0;
		HUD_init_message_literal(HM_DEFAULT, "Benchmark ON (START+ZL to stop)");
		con_printf(CON_URGENT, "BENCHMARK START (START+ZL to stop)\n");
	} else {
		HUD_init_message_literal(HM_DEFAULT, "Benchmark OFF");
		con_printf(CON_URGENT, "BENCHMARK STOP\n");
	}
}
#endif

#ifdef NETWORK
void game_draw_multi_message()
{

	if ( (Game_mode&GM_MULTI) && (multi_sending_message[Player_num]))	{
		gr_set_curfont(GAME_FONT);
		gr_set_fontcolor(BM_XRGB(0,63,0),-1);
		gr_printf(0x8000, (LINE_SPACING*5)+FSPACY(1), "%s: %s_", TXT_MESSAGE, Network_message );
	}

	if ( (Game_mode&GM_MULTI) && (multi_defining_message))	{
		gr_set_curfont(GAME_FONT);
		gr_set_fontcolor(BM_XRGB(0,63,0),-1);
		gr_printf(0x8000, (LINE_SPACING*5)+FSPACY(1), "%s #%d: %s_", TXT_MACRO, multi_defining_message, Network_message );
	}
}
#endif

void show_framerate()
{
	static int fps_count = 0, fps_rate = 0;
	int y = GHEIGHT;
	static fix64 fps_time = 0;

	fps_count++;
	if (timer_query() >= fps_time + F1_0)
	{
	fps_rate = fps_count;
	fps_count = 0;
	fps_time = timer_query();
	#ifdef __3DS__
	if (benchmark_active) {
		bench_secs++;
		bench_sum += fps_rate;
		if (fps_rate < bench_min) bench_min = fps_rate;
		if (fps_rate > bench_max) bench_max = fps_rate;
		int world_avg = f2i(bench_world_acc / (fps_rate > 0 ? fps_rate : 1));
		bench_world_acc = 0;
	}
	#endif
	}

	// On the death screen the user wants the FPS readable, so use the large
	// font and centre it near the top instead of the tiny top-right corner.
	if (Player_is_dead)
	{
		gr_set_curfont(HUGE_FONT);
		gr_set_fontcolor(BM_XRGB(0,63,0),-1);
		gr_printf(0x8000, FSPACY(2), "FPS: %i", fps_rate);
		return;
	}

	gr_set_curfont(GAME_FONT);
	gr_set_fontcolor(BM_XRGB(0,31,0),-1);

	if (PlayerCfg.CockpitMode[1] == CM_FULL_SCREEN) {
		if ((Game_mode & GM_MULTI) || (Newdemo_state == ND_STATE_PLAYBACK && Newdemo_game_mode & GM_MULTI))
			y -= LINE_SPACING * 10;
		else
			y -= LINE_SPACING * 4;
	} else if (PlayerCfg.CockpitMode[1] == CM_STATUS_BAR) {
		if ((Game_mode & GM_MULTI) || (Newdemo_state == ND_STATE_PLAYBACK && Newdemo_game_mode & GM_MULTI))
			y -= LINE_SPACING * 6;
		else
			y -= LINE_SPACING * 1;
	} else {
		if ((Game_mode & GM_MULTI) || (Newdemo_state == ND_STATE_PLAYBACK && Newdemo_game_mode & GM_MULTI))
			y -= LINE_SPACING * 7;
		else
			y -= LINE_SPACING * 2;
	}

	fps_count++;
	if (timer_query() >= fps_time + F1_0)
	{
		fps_rate = fps_count;
		fps_count = 0;
		fps_time = timer_query();
	}
	gr_printf(SWIDTH-(GameArg.SysMaxFPS>999?FSPACX(43):FSPACX(37)),y,"FPS: %i",fps_rate);
}

void show_observers() {
#ifdef NETWORK
	if(Netgame.max_numobservers == 0) {
		return;
	}
#endif

	int y = GHEIGHT;

	gr_set_curfont(GAME_FONT);
	gr_set_fontcolor(BM_XRGB(8,8,32),-1);

	if (PlayerCfg.CockpitMode[1] == CM_FULL_SCREEN) {
		if ((Game_mode & GM_MULTI) || (Newdemo_state == ND_STATE_PLAYBACK && Newdemo_game_mode & GM_MULTI))
			y -= LINE_SPACING * 10;
		else
			y -= LINE_SPACING * 4;
	} else if (PlayerCfg.CockpitMode[1] == CM_STATUS_BAR) {
		if ((Game_mode & GM_MULTI) || (Newdemo_state == ND_STATE_PLAYBACK && Newdemo_game_mode & GM_MULTI))
			y -= LINE_SPACING * 6;
		else
			y -= LINE_SPACING * 1;
	} else {
		if ((Game_mode & GM_MULTI) || (Newdemo_state == ND_STATE_PLAYBACK && Newdemo_game_mode & GM_MULTI))
			y -= LINE_SPACING * 7;
		else
			y -= LINE_SPACING * 2;
	}

	y -= LINE_SPACING*2; 

#ifdef NETWORK
	for(int i = 0; i < Netgame.numobservers; i++) {
		gr_printf(SWIDTH-FSPACX(strlen(Netgame.observers[i].callsign)*5 + 5),y,"%s",Netgame.observers[i].callsign);
		y -= LINE_SPACING; 
	}
#endif

	gr_set_fontcolor(BM_XRGB(8,8,32),-1);
	gr_printf(SWIDTH-FSPACX(37+15),y,"Observers:");
	y -= LINE_SPACING; 	
}

void set_font_present() { gr_set_fontcolor(BM_XRGB(25,25,25),-1); }
void set_font_absent() { gr_set_fontcolor(BM_XRGB(12,12,12),-1); }
void set_font_newline() { gr_set_fontcolor(255,-1); }
void draw_flag(char* string, int present, int x, int y) {
	if(present) { set_font_present(); }
	else        { set_font_absent();  }

	gr_printf(x,y,string); 
}
void set_font_presence(int i) { if(i) set_font_present(); else set_font_absent(); }

#ifdef NETWORK
void show_netplayerinfo()
{
	int x=0, y=0, i=0, color=0, eff=0;
	static const char *const eff_strings[]={"trashing","really hurting","seriously affecting","hurting","affecting","tarnishing"};

	gr_set_current_canvas(NULL);
	gr_set_curfont(GAME_FONT);
	gr_set_fontcolor(255,-1);

	x=(SWIDTH/2)-FSPACX(120);
	y=(SHEIGHT/2)-FSPACY(84);

	gr_settransblend(14, GR_BLEND_NORMAL);
	gr_setcolor( BM_XRGB(0,0,0) );
	gr_rect((SWIDTH/2)-FSPACX(120),(SHEIGHT/2)-FSPACY(84),(SWIDTH/2)+FSPACX(120),(SHEIGHT/2)+FSPACY(84));
	gr_settransblend(GR_FADE_OFF, GR_BLEND_NORMAL);

	// general game information
	y+=LINE_SPACING;
	gr_printf(0x8000,y,"%s",Netgame.game_name);
#ifndef SHAREWARE
	y+=LINE_SPACING;
	gr_printf(0x8000,y,"%s - lvl: %i",Netgame.mission_title,Netgame.levelnum);
#endif

	x+=FSPACX(8);
	y+=LINE_SPACING*2;
	unsigned gamemode = Netgame.gamemode;
	gr_printf(x,y,"game mode: %s",gamemode < (sizeof(GMNames) / sizeof(GMNames[0])) ? GMNames[gamemode] : "INVALID");

	
	int base_flags_left = SWIDTH/2 - FSPACX(15);
	int flags_x = base_flags_left + FSPACX(30);
	int letter_spacing = FSPACX(7); 
	int word_spacing = FSPACX(46); 


	if(Netgame.RetroProtocol) {
		draw_flag("RetroP2P", 1,                         						 base_flags_left + word_spacing*0, y); 
	} else if(Netgame.ShortPackets) {
		draw_flag("ShortPkt", 1,                         						 base_flags_left + word_spacing*0, y); 
	} else {
		draw_flag("LongPkt", 1,                         						 base_flags_left + word_spacing*0, y); 
	}

	char pps_string[16];
	sprintf(pps_string, "PPS %d", Netgame.PacketsPerSec); 
	draw_flag(pps_string, 1,                         						 base_flags_left + word_spacing*1, y);

	if(Netgame.SpawnStyle == SPAWN_STYLE_NO_INVUL ) {
		draw_flag("NoInvul", 1,                            base_flags_left + word_spacing*2, y); 
	} else if (Netgame.SpawnStyle == SPAWN_STYLE_SHORT_INVUL ) {
		draw_flag("ShortInv", 1, base_flags_left + word_spacing*2, y); 
	}  else if (Netgame.SpawnStyle == SPAWN_STYLE_LONG_INVUL ) {
		draw_flag("LongInv", 1,                            base_flags_left + word_spacing*2, y); 
	} else {
		draw_flag("Preview", 1,                            base_flags_left + word_spacing*2, y); 
	}
	

	set_font_newline(); 

	y+=LINE_SPACING;
	gr_printf(x,y,"difficulty: %s",MENU_DIFFICULTY_TEXT(Netgame.difficulty));

	draw_flag("ColorLgt", Netgame.AllowColoredLighting,                            base_flags_left + word_spacing*0, y); 
	draw_flag("BrtShips", Netgame.BrightPlayers,                                   base_flags_left + word_spacing*1, y); 	
	draw_flag("ConcResp", Netgame.RespawnConcs,                                    base_flags_left + word_spacing*2, y); 

	set_font_newline(); 
	y+=LINE_SPACING;
	gr_printf(x,y,"level time: %i:%02i:%02i",Players[Player_num].hours_level,f2i(Players[Player_num].time_level) / 60 % 60,f2i(Players[Player_num].time_level) % 60);

	char disp_string[16];
	sprintf(disp_string, "Guns x%d", Netgame.PrimaryDupFactor == 0 ? 1 : Netgame.PrimaryDupFactor);
	draw_flag(disp_string, Netgame.PrimaryDupFactor > 1,                           base_flags_left + word_spacing*0, y); 

	sprintf(disp_string, "Msls x%d", Netgame.SecondaryDupFactor == 0 ? 1 : Netgame.SecondaryDupFactor);
	draw_flag(disp_string, Netgame.SecondaryDupFactor > 1,                         base_flags_left + word_spacing*1, y); 	

	sprintf(disp_string, "Mcap %s", Netgame.SecondaryCapFactor == 0 ? "ALL" : (Netgame.SecondaryCapFactor == 1 ? "6" : "2"));
	draw_flag(disp_string, Netgame.SecondaryCapFactor > 0,                         base_flags_left + word_spacing*2, y); 	


	set_font_newline(); 
	y+=LINE_SPACING;
	gr_printf(x,y,"total time: %i:%02i:%02i",Players[Player_num].hours_total,f2i(Players[Player_num].time_total) / 60 % 60,f2i(Players[Player_num].time_total) % 60);




	set_font_newline(); 
	y+=LINE_SPACING;
	if (Netgame.KillGoal)
		gr_printf(x,y,"Kill goal: %d",Netgame.KillGoal*10);

	gr_printf(base_flags_left, y, "Items: "); 
	draw_flag("L", Netgame.AllowedItems & NETFLAG_DOLASER,     flags_x, y);  flags_x += letter_spacing; 
	draw_flag("Q", Netgame.AllowedItems & NETFLAG_DOQUAD,      flags_x, y);  flags_x += letter_spacing; 
	draw_flag("V", Netgame.AllowedItems & NETFLAG_DOVULCAN,    flags_x, y);  flags_x += letter_spacing; 
	draw_flag("A", Netgame.AllowedItems & NETFLAG_DOVULCANAMMO,flags_x, y);  flags_x += letter_spacing; 
	draw_flag("S", Netgame.AllowedItems & NETFLAG_DOSPREAD,    flags_x, y);  flags_x += letter_spacing; 
	draw_flag("P", Netgame.AllowedItems & NETFLAG_DOPLASMA,    flags_x, y);  flags_x += letter_spacing; 
	draw_flag("F", Netgame.AllowedItems & NETFLAG_DOFUSION,    flags_x, y);  flags_x += letter_spacing; 
	draw_flag("C", 1,                                          flags_x, y);  flags_x += letter_spacing; 
	draw_flag("H", Netgame.AllowedItems & NETFLAG_DOHOMING,    flags_x, y);  flags_x += letter_spacing; 
	draw_flag("P", Netgame.AllowedItems & NETFLAG_DOPROXIM,    flags_x, y);  flags_x += letter_spacing; 
	draw_flag("S", Netgame.AllowedItems & NETFLAG_DOSMART,     flags_x, y);  flags_x += letter_spacing; 
	draw_flag("M", Netgame.AllowedItems & NETFLAG_DOMEGA,      flags_x, y);  flags_x += letter_spacing; 
	draw_flag("C", Netgame.AllowedItems & NETFLAG_DOCLOAK,     flags_x, y);  flags_x += letter_spacing; 
	draw_flag("I", Netgame.AllowedItems & NETFLAG_DOINVUL,     flags_x, y);  flags_x += letter_spacing; 

	// player information (name, kills, ping, game efficiency)
	set_font_newline(); 	
	y+=LINE_SPACING*3;
	gr_printf(x,y,"player");
	if (Game_mode & GM_MULTI_COOP)
		gr_printf(x+FSPACX(8)*7,y,"score");
	else
	{
		gr_printf(x+FSPACX(8)*7,y,"kills");
		gr_printf(x+FSPACX(8)*12,y,"deaths");
	}
	gr_printf(x+FSPACX(8)*18,y,"ping");
	gr_printf(x+FSPACX(8)*23,y,"efficiency");

	if(Netgame.FairColors)
		selected_player_rgb = player_rgb_all_blue; 
	else if(Netgame.BlackAndWhitePyros) 
		selected_player_rgb = player_rgb_alt; 
	else
		selected_player_rgb = player_rgb;

	// process players table
	for (i=0; i<MAX_PLAYERS; i++)
	{
		if (!Players[i].connected)
			continue;

		y+=LINE_SPACING;

		//if (Game_mode & GM_TEAM)
		//	color=get_team(i);
		//else
		//	color=Netgame.players[i].color;//i;
		color = get_color_for_player(i, 0); 
		gr_set_fontcolor( BM_XRGB(selected_player_rgb[color].r,selected_player_rgb[color].g,selected_player_rgb[color].b),-1 );
		gr_printf(x,y,"%s\n",Players[i].callsign);
		if (Game_mode & GM_MULTI_COOP)
			gr_printf(x+FSPACX(8)*7,y,"%-6d",Players[i].score);
		else
		{
			gr_printf(x+FSPACX(8)*7,y,"%-6d",Players[i].net_kills_total);
			gr_printf(x+FSPACX(8)*12,y,"%-6d",Players[i].net_killed_total);
		}

		gr_printf(x+FSPACX(8)*18,y,"%-6d",Netgame.players[i].ping + Netgame.players[Player_num].ping);
		if (i != Player_num)
			gr_printf(x+FSPACX(8)*23,y,"%d/%d",kill_matrix[Player_num][i],kill_matrix[i][Player_num]);
	}

	y+=LINE_SPACING*2+(LINE_SPACING*(MAX_PLAYERS-N_players));

	// printf team scores
	if (Game_mode & GM_TEAM)
	{
		gr_set_fontcolor(255,-1);
		gr_printf(x,y,"team");
		gr_printf(x+FSPACX(8)*8,y,"score");
		y+=LINE_SPACING;
		gr_set_fontcolor(BM_XRGB(selected_player_rgb[0].r,selected_player_rgb[0].g,selected_player_rgb[0].b),-1 );
		gr_printf(x,y,"%s:",Netgame.team_name[0]);
		gr_printf(x+FSPACX(8)*8,y,"%i",team_kills[0]);
		y+=LINE_SPACING;
		gr_set_fontcolor(BM_XRGB(selected_player_rgb[1].r,selected_player_rgb[1].g,selected_player_rgb[1].b),-1 );
		gr_printf(x,y,"%s:",Netgame.team_name[1]);
		gr_printf(x+FSPACX(8)*8,y,"%i",team_kills[1]);
		y+=LINE_SPACING*2;
	}
	else
		y+=LINE_SPACING*4;

	gr_set_fontcolor(255,-1);

	// additional information about game - ranking
	eff=(int)((float)((float)PlayerCfg.NetlifeKills/((float)PlayerCfg.NetlifeKilled+(float)PlayerCfg.NetlifeKills))*100.0);
	if (eff<0)
		eff=0;

	if (!PlayerCfg.NoRankings)
	{
		gr_printf(0x8000,y,"Your lifetime efficiency of %d%% (%d/%d)",eff,PlayerCfg.NetlifeKills,PlayerCfg.NetlifeKilled);
		y+=LINE_SPACING;
		if (eff<60)
			gr_printf(0x8000,y,"is %s your ranking.",eff_strings[eff/10]);
		else
			gr_printf(0x8000,y,"is serving you well.");
		y+=LINE_SPACING;
		gr_printf(0x8000,y,"your rank is: %s",RankStrings[GetMyNetRanking()]);
	}
}
#endif

#ifndef NDEBUG

fix Show_view_text_timer = -1;

void draw_window_label()
{
	if ( Show_view_text_timer > 0 )
	{
		char *viewer_name,*control_name;

		Show_view_text_timer -= FrameTime;

		switch( Viewer->type )
		{
			case OBJ_FIREBALL:	viewer_name = "Fireball"; break;
			case OBJ_ROBOT:		viewer_name = "Robot"; break;
			case OBJ_HOSTAGE:		viewer_name = "Hostage"; break;
			case OBJ_PLAYER:		viewer_name = "Player"; break;
			case OBJ_WEAPON:		viewer_name = "Weapon"; break;
			case OBJ_CAMERA:		viewer_name = "Camera"; break;
			case OBJ_POWERUP:	viewer_name = "Powerup"; break;
			case OBJ_DEBRIS:		viewer_name = "Debris"; break;
			case OBJ_CNTRLCEN:	viewer_name = "Control Center"; break;
			default:					viewer_name = "Unknown"; break;
		}

		switch ( Viewer->control_type) {
			case CT_NONE:			control_name = "Stopped"; break;
			case CT_AI:				control_name = "AI"; break;
			case CT_FLYING:		control_name = "Flying"; break;
			case CT_SLEW:			control_name = "Slew"; break;
			case CT_FLYTHROUGH:	control_name = "Flythrough"; break;
			case CT_MORPH:			control_name = "Morphing"; break;
			default:					control_name = "Unknown"; break;
		}
		gr_set_curfont(GAME_FONT);
		gr_set_fontcolor(BM_XRGB(31,0,0),-1);
		gr_printf( 0x8000, (SHEIGHT/10), "%s View - %s",viewer_name,control_name );
	}
}
#endif

void render_countdown_gauge()
{
	if (!Endlevel_sequence && Control_center_destroyed  && (Countdown_seconds_left>-1) && (Countdown_seconds_left<127))	{
		gr_set_curfont(GAME_FONT);
		gr_set_fontcolor(BM_XRGB(0,63,0),-1);
		gr_printf(0x8000, (LINE_SPACING*6)+FSPACY(1), "T-%d s", Countdown_seconds_left );
	}
}

void game_draw_hud_stuff()
{
#ifdef CROSSHAIR
	if ( Viewer->type == OBJ_PLAYER )
		laser_do_crosshair(Viewer);
#endif
	
#ifndef NDEBUG
	draw_window_label();
#endif

#ifdef NETWORK
	game_draw_multi_message();
#endif

	if ((Newdemo_state == ND_STATE_PLAYBACK) || (Newdemo_state == ND_STATE_RECORDING)) {
		char message[128];
		int y;

		if (Newdemo_state == ND_STATE_PLAYBACK) {
			if (Newdemo_show_percentage) {
			  	sprintf(message, "%s (%d%% %s)", TXT_DEMO_PLAYBACK, newdemo_get_percent_done(), TXT_DONE);
			} else {
				sprintf (message, " ");
			}
		} else {
			//extern int Newdemo_num_written;
			//sprintf (message, "%s (%dK)", TXT_DEMO_RECORDING, (Newdemo_num_written / 1024));
			sprintf (message, "%s", TXT_DEMO_RECORDING);
		}

		gr_set_curfont( GAME_FONT );
		gr_set_fontcolor( BM_XRGB(27,0,0), -1 );

		y = GHEIGHT-(LINE_SPACING*2);

		if (PlayerCfg.CockpitMode[1] == CM_FULL_COCKPIT)
			y = grd_curcanv->cv_bitmap.bm_h / 1.2 ;
		if (PlayerCfg.CockpitMode[1] != CM_REAR_VIEW)
			gr_string(0x8000, y, message );
	}

	render_countdown_gauge();

	// Call show_framerate() when FPS is shown, on the death screen, OR when
	// the benchmark is active -- the benchmark's per-second logger lives
	// inside show_framerate and must tick even if FPSIndicator is off,
	// otherwise BENCH lines never hit the trace (the benchmark toggle's
	// console message would misleadingly suggest it's logging).
	if (GameCfg.FPSIndicator || Player_is_dead || benchmark_active)
		show_framerate();

	if ( (Game_mode & GM_MULTI) && (PlayerCfg.ObsShowObs)) {
		show_observers(); 
	}

	if (Newdemo_state == ND_STATE_PLAYBACK)
		Game_mode = Newdemo_game_mode;

	draw_hud();

	if (Newdemo_state == ND_STATE_PLAYBACK)
		Game_mode = GM_NORMAL | (Game_mode & GM_OBSERVER);

	if ( Player_is_dead )
		player_dead_message();
}

extern int gr_bitblt_dest_step_shift;
extern int gr_bitblt_double;
extern int force_cockpit_redraw;
void update_cockpits();

//render a frame for the game
void game_render_frame_mono(int flip)
{
	// UNCONDITIONAL entry marker (diagnose blank top: confirms this fn runs)
	{
		static int mono_entered = 0;
		if (mono_entered < 5)
		{
			/* pglIsPoweredOff() is declared by the 3DS GL headers
			 * (via ogl_init.h). Do NOT redeclare it here -- the real
			 * prototype differs and redeclaring breaks the build. */
			mono_entered++;
			}
			}

			gr_set_current_canvas(&Screen_3d_window);

			// 3DS: an overlay (in-game MENU, Save/Load, pause, options) is any window
	// in front of the live game window. While one is open we (a) freeze the
	// game sim in its handler, (b) force a single mono present on GFX_LEFT
	// (see the __3DS__ block below), and (c) suppress the per-window gr_flip
	// (line ~809) so event.c's single flip presents the menu on top instead
	// of alternating game-only / game+menu frames. Computed once here so both
	// the slider gate below and the gated gr_flip() can use it.
	extern window *window_get_front(void);
	int overlay_open = (Game_wind && window_get_front() != Game_wind);

#ifdef __3DS__
	// Stereo 3D driven by the 3DS slider: 0.0 = off (mono, no ghosting),
	// 1.0 = max depth. The hardware 3D mode and picaGL stereo path are
	// toggled dynamically so the slider is both on/off and depth adjuster.
	// HUD/gauges draw into the right eye for this first pass (per-eye HUD =
	// TODO). g_stereo_active tells gr_flip() to skip its own present.
	//
	// Slider via MCUHWC_Get3dSliderLevel (this devkitPro has no
	// osGet3DSliderState). All 3DS funcs forward-declared to avoid pulling
	// <3ds.h> (clashes with d1's own key.h).
	{
		extern void pglSelectScreen(unsigned display, unsigned side);
		extern void pglSetStereo(bool enable);
		extern int g_last_n_vertices;   // set by render_mine()
		extern int g_segs_rendered;     // OGL segments drawn by last render_mine()
		static int trace_frames = 0;
		unsigned char lvl = 0;
		MCUHWC_Get3dSliderLevel(&lvl);   // 0..255
		float slider = lvl / 255.0f;
		fix eye = 0;   // declared at outer scope so the trace below can log it

		// 3DS: if a modal overlay is open (overlay_open computed above),
		// force a MONO present on GFX_LEFT regardless of slider position.
		// DoMenu() runs its own event_process() loop, which calls this
		// function every frame; with the slider up the per-frame logic
		// below would re-engage stereo and leave g_stereo_active=1, making
		// the overlay's own present (event.c:433 gr_flip) a no-op so the
		// menu is never shown. Forcing mono while the overlay is up lets the
		// menu present; stereo re-engages on the next live gameplay frame.
		if (slider > 0.001f && !overlay_open)
		{
			if (!stereo_hw_on) { gfxSet3D(true); pglSetStereo(true); stereo_hw_on = 1; }
			// CRITICAL (v99e): reset g_stereo_active BEFORE the eye pair.
			// g_stereo_active is a global that persists across frames. It is
			// set to 1 at the END of this block (below) to suppress the outer
			// gr_flip()'s present. But the two eye-pair ogl_swap_buffers_internal()
			// calls also route through the SAME guard (gr.c:128
			// "if (g_stereo_active) return;"). Without resetting here, from
			// frame 2 onward g_stereo_active is still 1 from the previous
			// frame, so BOTH eye presents become no-ops AND the outer gr_flip
			// is suppressed -> nothing is presented after frame 1 -> the top
			// screen freezes on the first frame while the sim keeps running
			// (audio, weapons fire). This is THE briefing->game "frozen on
			// stale frame" bug. Reset to 0 so the eye presents go through,
			// then set to 1 only after the pair to suppress the outer flip.
			g_stereo_active = 0;
			// eye separation set by the 3DS hardware depth slider
			// (parallel-only stereo); the preset below is the max the
			// slider can reach. Persisted to sdmc:/3ds/d1/stereo_sep.cfg.
			// eye separation in WORLD units (not percent): presets
			// {1,2,3,4,5,6} are world-unit magnitudes. A /100 percent
			// scale made them ~0.2 units -> sub-pixel, invisible 3D.
			// Scale by 0.35 so pct=6 ~= 2.1 units (gentle max) and
			// pct=1 ~= 0.35 units (very subtle floor). Slider 0..1
			// scales depth continuously on top of the preset.
			eye = (fix)(F1_0 * (g_stereo_sep_pct * 0.35f) * slider);
			// LEFT eye (+offset) -> GFX_LEFT, then cockpit+HUD, then present
			pglSelectScreen(0/*GFX_TOP*/, 0/*GFX_LEFT*/);
			render_frame(eye);
			// v99i: draw cockpit + gauges INTO each eye's colorBuffer
			// BEFORE that eye is transferred/presented. Previously these
			// ran AFTER the whole eye-pair (line ~669), by which point
			// g_stereo_active=1 had suppressed the present and the cockpit
			// was drawn into the buffer the next frame's glClear wipes ->
			// no cockpit in stereo. Drawing per-eye puts it on screen.
			update_cockpits();
			if (PlayerCfg.CockpitMode[1]==CM_FULL_COCKPIT || PlayerCfg.CockpitMode[1]==CM_STATUS_BAR)
				render_gauges();
			else if (PlayerCfg.CockpitMode[1]==CM_FULL_SCREEN)
				game_draw_hud_stuff();
			ogl_swap_buffers_internal();
			// RIGHT eye (-offset) -> GFX_RIGHT, then cockpit+HUD, then present pair
			pglSelectScreen(0/*GFX_TOP*/, 1/*GFX_RIGHT*/);
			render_frame(-eye);
			update_cockpits();
			if (PlayerCfg.CockpitMode[1]==CM_FULL_COCKPIT || PlayerCfg.CockpitMode[1]==CM_STATUS_BAR)
				render_gauges();
			else if (PlayerCfg.CockpitMode[1]==CM_FULL_SCREEN)
				game_draw_hud_stuff();
			ogl_swap_buffers_internal();
			// Suppress the OUTER gr_flip() present: the eye pair above
			// already presented (LEFT then RIGHT) via pglSwapBuffers' stereo
			// branch. Set g_stereo_active AFTER the eye pair so these
			// two ogl_swap_buffers_internal() calls are NOT no-op'd by
			// the early-return guard in ogl_swap_buffers_internal().
			g_stereo_active = 1;
			}
		else
		{
			if (stereo_hw_on) { gfxSet3D(false); pglSetStereo(false); stereo_hw_on = 0; }
			g_stereo_active = 0;
			// v99g: reset the present target to GFX_LEFT. The stereo branch
			// above leaves pglState->display_side = GFX_RIGHT (set at the
			// RIGHT-eye present). This mono branch relies on the outer
			// gr_flip()->pglSwapBuffers, which presents to whatever
			// display_side is current -- but it never re-selects the screen.
			// With 3D hardware OFF the TOP LCD only shows the LEFT buffer, so
			// a leaked GFX_RIGHT sends the mono world to the invisible right
			// bank and the screen keeps showing the stale banner/last LEFT
			// frame. Symptom: slider all the way off -> 3D world vanishes,
			// only the PREPARE banner remains. Re-select LEFT every mono
			// frame so the world lands on the displayed bank.
			pglSelectScreen(0/*GFX_TOP*/, 0/*GFX_LEFT*/);
			render_frame(0);
		}

		trace_frames++;
	}
#else
	render_frame(0);
#endif

	// v99i: in STEREO mode the cockpit + gauges were already drawn into
	// each eye above (per-eye, before each present). Don't redraw here or
	// it lands in the post-present buffer and gets wiped. In MONO mode the
	// outer gr_flip() below is the present, so draw cockpit/gauges now.
	if (!g_stereo_active)
	{
	update_cockpits();

	if (Newdemo_state == ND_STATE_PLAYBACK)
		Game_mode = Newdemo_game_mode;

	if (PlayerCfg.CockpitMode[1]==CM_FULL_COCKPIT || PlayerCfg.CockpitMode[1]==CM_STATUS_BAR)
		render_gauges();

	if (Newdemo_state == ND_STATE_PLAYBACK)
		Game_mode = GM_NORMAL | (Game_mode & GM_OBSERVER);
	}
	else
	{
	if (Newdemo_state == ND_STATE_PLAYBACK)
		Game_mode = Newdemo_game_mode;
	if (Newdemo_state == ND_STATE_PLAYBACK)
		Game_mode = GM_NORMAL | (Game_mode & GM_OBSERVER);
	}

	gr_set_current_canvas(&Screen_3d_window);
	game_draw_hud_stuff();

	// Present the finished frame. In mono this is the only swap; in stereo
	// g_stereo_active makes ogl_swap_buffers_internal() a no-op (the eye pair
	// is presented explicitly inside the __3DS__ block above).
	//
	// 3DS: when an overlay (menu/save/pause) is open we must NOT flip here.
	// event_process() dispatches EVENT_WINDOW_DRAW to ALL visible windows
	// each frame, so Game_wind (game) and the overlay both draw; if this
	// per-window gr_flip() also fires we get TWO presents per frame
	// (game-only, then game+overlay) which flicker/alternate on the screen.
	// Suppressing it here leaves the single present to event.c:433, which
	// shows the overlay on top of the frozen game in one flip. With no
	// overlay the per-window flip is the sole present, as before.
	if (!overlay_open)
		gr_flip();

#ifdef NETWORK
	if (netplayerinfo_on && Game_mode & GM_MULTI)
		show_netplayerinfo();
#endif
}

void toggle_cockpit()
{
	int new_mode=CM_FULL_SCREEN;

	if (Rear_view || Player_is_dead)
		return;

	switch (PlayerCfg.CockpitMode[1])
	{
		case CM_FULL_COCKPIT:
			new_mode = CM_STATUS_BAR;
			break;
		case CM_STATUS_BAR:
			new_mode = CM_FULL_SCREEN;
			break;
		case CM_FULL_SCREEN:
			new_mode = CM_FULL_COCKPIT;
			if(PlayerCfg.DisableCockpit) {
				new_mode = CM_STATUS_BAR; 
			}
			break;
	}

	select_cockpit(new_mode);
	HUD_clear_messages();
	PlayerCfg.CockpitMode[0] = new_mode;
	write_player_file();
}

// 3DS: cycle through the three main cockpit views (full cockpit -> status bar
// -> full screen -> back). Bound to SELECT + D-UP / D-DOWN in event.c so it
// doesn't collide with the plain SELECT (automap) tap or gameplay buttons.
void cycle_cockpit_next(void)
{
	int order[] = { CM_FULL_COCKPIT, CM_STATUS_BAR, CM_FULL_SCREEN };
	int i;
	if (Rear_view || Player_is_dead)
		return;
	for (i = 0; i < 3; i++)
		if (PlayerCfg.CockpitMode[1] == order[i]) {
			int next = order[(i+1)%3];
			if (next == CM_FULL_COCKPIT && PlayerCfg.DisableCockpit)
				next = CM_STATUS_BAR;
			select_cockpit(next);
			PlayerCfg.CockpitMode[0] = next;
			write_player_file();
			return;
		}
	select_cockpit(CM_STATUS_BAR);
	PlayerCfg.CockpitMode[0] = CM_STATUS_BAR;
	write_player_file();
}

void cycle_cockpit_prev(void)
{
	int order[] = { CM_FULL_COCKPIT, CM_STATUS_BAR, CM_FULL_SCREEN };
	int i;
	if (Rear_view || Player_is_dead)
		return;
	for (i = 0; i < 3; i++)
		if (PlayerCfg.CockpitMode[1] == order[i]) {
			int prev = order[(i+2)%3];
			select_cockpit(prev);
			PlayerCfg.CockpitMode[0] = prev;
			write_player_file();
			return;
		}
	select_cockpit(CM_STATUS_BAR);
	PlayerCfg.CockpitMode[0] = CM_STATUS_BAR;
	write_player_file();
}

int last_drawn_cockpit = -1;
extern void ogl_loadbmtexture(grs_bitmap *bm);

// This actually renders the new cockpit onto the screen.
void update_cockpits()
{
	grs_bitmap *bm;
	PIGGY_PAGE_IN(cockpit_bitmap[PlayerCfg.CockpitMode[1]]);
	bm = &GameBitmaps[cockpit_bitmap[PlayerCfg.CockpitMode[1]].index];

	switch( PlayerCfg.CockpitMode[1] )	{
		case CM_FULL_COCKPIT:
			gr_set_current_canvas(NULL);
#ifdef OGL
			ogl_ubitmapm_cs (0, 0, -1, grd_curcanv->cv_bitmap.bm_h, bm,255, F1_0);
#else
			gr_ubitmapm(0,0, bm);
#endif
			break;
		case CM_REAR_VIEW:
			gr_set_current_canvas(NULL);
#ifdef OGL
			ogl_ubitmapm_cs (0, 0, -1, grd_curcanv->cv_bitmap.bm_h, bm,255, F1_0);
#else
			gr_ubitmapm(0,0, bm);
#endif
			break;
		case CM_FULL_SCREEN:
			break;
		case CM_STATUS_BAR:
			gr_set_current_canvas(NULL);
#ifdef OGL
			ogl_ubitmapm_cs (0, (HIRESMODE?(SHEIGHT*2)/2.6:(SHEIGHT*2)/2.72), -1, ((int) ((double) (bm->bm_h) * (HIRESMODE?(double)SHEIGHT/480:(double)SHEIGHT/200) + 0.5)), bm,255, F1_0);
#else
			gr_ubitmapm(0,SHEIGHT-bm->bm_h,bm);
#endif
			break;
		case CM_LETTERBOX:
			gr_set_current_canvas(NULL);
			break;
	}

	gr_set_current_canvas(NULL);

	if (PlayerCfg.CockpitMode[1] != last_drawn_cockpit)
		last_drawn_cockpit = PlayerCfg.CockpitMode[1];
	else
		return;

	if (PlayerCfg.CockpitMode[1]==CM_FULL_COCKPIT || PlayerCfg.CockpitMode[1]==CM_STATUS_BAR)
		init_gauges();
}

void game_render_frame()
{
	set_screen_mode( SCREEN_GAME );
	play_homing_warning();
	game_render_frame_mono(GameArg.DbgUseDoubleBuffer);
}

/* Stereo is a GAMEPLAY-only render mode. While a menu/pause/DoMenu overlay
 * is open on top of the live Game_wind, the event loop still dispatches
 * EVENT_WINDOW_DRAW to Game_wind, so game_render_frame_mono keeps running
 * behind the overlay with g_stereo_active=1. The eye-pair path leaves
 * picaGL's display_side on GFX_RIGHT and makes gr_flip() a no-op, so the
 * overlay's own present lands on the invisible RIGHT bank (black screen)
 * and the two present paths contend for picaGL state -> the render thread
 * wedges (gameplay frozen, NDSP audio keeps playing). Force stereo OFF for
 * the duration of any overlay; the per-frame slider logic re-engages it on
 * resume (stereo_hw_on reset), and the slider position is preserved. */
void stereo_suspend(void)
{
	/* Force stereo OFF so any menu/overlay opened over a live game presents
	 * in mono on the top screen. The stereo eye-pair path (when the 3D
	 * slider was up) leaves g_stereo_active=1 AND the display bank on
	 * GFX_RIGHT. Resetting g_stereo_active=0 alone would make gr_flip()
	 * present mono, but to whatever bank pglState->display_side currently
	 * points -- still GFX_RIGHT (invisible on a 3D-off top LCD). The menu
	 * then draws correctly but its gr_flip() presents to the hidden bank,
	 * so the menu is logically up (tap works) but never visible -- exactly
	 * the "I can tell the menu is up but can't see it" symptom after a
	 * demo (or any stereo session) ends. Re-select GFX_TOP/GFX_LEFT here so
	 * the overlay's present lands on the displayed bank. stereo_resume()
	 * resets stereo_hw_on=0 and the per-frame slider logic re-engages
	 * stereo on the next live frame if the slider is still up. */
	gfxSet3D(false);
	pglSetStereo(false);
	stereo_hw_on = 0;
	g_stereo_active = 0;
	pglSelectScreen(0/*GFX_TOP*/, 0/*GFX_LEFT*/);
}

void stereo_resume(void)
{
	/* Let game_render_frame_mono's per-frame slider logic re-engage stereo:
	 * reset the hardware-on flag so the next gameplay frame re-reads the
	 * physical slider and re-issues gfxSet3D/pglSetStereo if it is still up.
	 * (Calling gfxSet3D/pglSetStereo here, during the menu->game transition,
	 * left stereo disabled at game start in testing, so the per-frame path
	 * is the reliable re-enable.)
	 *
	 * Crucially, re-select GFX_TOP/GFX_LEFT here. The stereo eye-pair path
	 * (slider up during the prior session -- e.g. a demo) leaves the display
	 * bank on GFX_RIGHT. If we only reset the flags, the first gameplay
	 * frame's per-frame logic hasn't run yet, but show_boxed_message's
	 * "Prepare for Descent" banner does its OWN direct gr_flip() -- which
	 * would present to the stale GFX_RIGHT (hidden) bank, then event.c's
	 * present flips to GFX_LEFT: the banner alternates/flickers over the
	 * game view. Re-selecting LEFT here makes the banner (and the first
	 * mono gameplay frames) present on the visible bank immediately, so
	 * resume-after-demo shows a clean, stable banner. */
	stereo_hw_on = 0;
	g_stereo_active = 0;
	pglSelectScreen(0/*GFX_TOP*/, 0/*GFX_LEFT*/);
}

//show a message in a nice little box
void show_boxed_message(char *msg, int RenderFlag)
{
	int w,h,aw;
	int x,y;
	
	gr_set_current_canvas(NULL);

	// v99f: clear the whole screen to black BEFORE drawing the boxed
	// message. Historically this was not needed because gr_flip() did a
	// glClear(COLOR) AFTER every present, so by the time show_boxed_message
	// ran the buffer was already black and the "PREPARE FOR DESCENT" banner
	// appeared on black. v98 removed that post-present clear (it caused the
	// 30 Hz strobe: the buffer was blacked, then event.c's per-frame second
	// gr_flip presented black). Without a clear here, the banner now draws
	// over the STALE briefing screen (and later composites with the 3D
	// world). Clearing explicitly in the 2D path restores the black
	// background WITHOUT reintroducing the strobe (there is no second flip
	// in this path). Both banks are cleared (see the RenderFlag flip path
	// below plus the game-start clear) so no stale briefing survives.
	gr_clear_canvas(BM_XRGB(0,0,0));

	gr_set_curfont( MEDIUM1_FONT );
	gr_set_fontcolor(BM_XRGB(31, 31, 31), -1);
	gr_get_string_size(msg,&w,&h,&aw);
	
	x = (SWIDTH-w)/2;
	y = (SHEIGHT-h)/2;
	
	nm_draw_background(x-BORDERX,y-BORDERY,x+w+BORDERX,y+h+BORDERY);
	
	gr_string( 0x8000, y, msg );
	
	// If we haven't drawn behind it, need to flip
	if (!RenderFlag)
	{
#ifdef __3DS__
		{
			extern int g_stereo_active;
			extern int stereo_hw_on;
			/* pglIsPoweredOff() declared by 3DS GL headers (via ogl_init.h);
			 * do NOT redeclare (conflicts with header prototype). Bail on
			 * power-off so fopen() can't data-abort during FS teardown. */
		}
		extern void pglSetStereo(bool enable);
		extern void pglSelectScreen(unsigned display, unsigned side);
		extern int g_stereo_active;
		pglSetStereo(false);
		g_stereo_active = 0;
		pglSelectScreen(0/*GFX_TOP*/, 0/*GFX_LEFT*/);
#endif
		gr_flip();
	}
}

