/*
 * Tactical Bottom Screen Subsystem: Live 3D Rear-View Mirror, Mini-Radar & Wireframe Minimap
 * For Nintendo 3DS (D1X-3DS)
 *
 * Provides real-time 360-degree situational awareness (Mini-Radar),
 * live 3D aft rendering from the game's actual engine (Rear-View Mirror),
 * and a local 3D wireframe room map (Minimap) in the bottom-screen center
 * safe area during active flight.
 */

#ifdef __3DS__

#include <3ds.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "inferno.h"
#include "game.h"
#include "object.h"
#include "segment.h"
#include "wall.h"
#include "vclip.h"
#include "timer.h"
#include "hudmsg.h"
#include "bottom_screen.h"
#include "tactical_bottom.h"
#include "vecmat.h"
#include "player.h"
#include "powerup.h"
#include "automap.h"
#include "screens.h"
#include "render.h"
#include "newdemo.h"
#include <GL/picaGL.h>

/* Safe area layout for the bottom screen (logical 320x240):
 * Top button row ends at y=24, bottom button row begins at y=216.
 * Center safe area: x in [8, 312] (w=304), y in [26, 214] (h=188). */
#define TACTICAL_AREA_X 8
#define TACTICAL_AREA_Y 26
#define TACTICAL_AREA_W 304
#define TACTICAL_AREA_H 188

/* RGB565 Colors */
#define COL_BLACK       0x0000
#define COL_DARK_GRAY   0x2104
#define COL_MID_GRAY    0x4208
#define COL_LIGHT_GRAY  0x8410
#define COL_GRID_CYAN   0x11B3
#define COL_BRIGHT_CYAN 0x07FF
#define COL_GREEN       0x07E0
#define COL_DIM_GREEN   0x03E0
#define COL_RED         0xF800
#define COL_DIM_RED     0x8000
#define COL_YELLOW      0xFFE0
#define COL_ORANGE      0xFD20
#define COL_BLUE        0x001F
#define COL_GOLD        0xFE00
#define COL_MAGENTA     0xF81F
#define COL_WHITE       0xFFFF

/* State */
static tactical_mode_t g_tactical_mode = TACTICAL_MODE_TRI;
static int g_radar_range_idx = 1; /* Default: 200m */
static const int g_radar_ranges[] = { 100, 200, 400, 800 };
#define NUM_RADAR_RANGES (int)(sizeof(g_radar_ranges)/sizeof(g_radar_ranges[0]))

static float g_radar_sweep_angle = 0.0f;
static int g_touch_down_prev = 0;
static int g_map_range = 120; /* Default 120m for clear local room wireframe */

/* Linear buffer for hardware GX DisplayTransfer capture of 3D rear-view mirror */
static uint16_t *g_rear_view_buf = NULL;
static int g_rear_view_ready = 0;

/* Bounded pixel plot helper */
static inline void tactical_pixel(int x, int y, uint16_t c, int bx, int by, int bw, int bh)
{
	if (x >= bx && x < bx + bw && y >= by && y < by + bh)
		bottom_set_pixel(x, y, c);
}

void tactical_bottom_init(void)
{
	g_radar_sweep_angle = 0.0f;
	g_touch_down_prev = 0;
	if (!g_rear_view_buf)
		g_rear_view_buf = (uint16_t *)linearAlloc(240 * 320 * sizeof(uint16_t));
	g_rear_view_ready = 0;
}

void tactical_radar_zoom(int step)
{
	if (step > 0) {
		if (g_radar_range_idx > 0) g_radar_range_idx--;
	} else if (step < 0) {
		if (g_radar_range_idx < NUM_RADAR_RANGES - 1) g_radar_range_idx++;
	}
	HUD_init_message(HM_DEFAULT, "Radar Range: %dm", g_radar_ranges[g_radar_range_idx]);
}

void tactical_map_zoom(int step)
{
	if (step > 0) {
		if (g_map_range > 60) g_map_range -= 30;
	} else if (step < 0) {
		if (g_map_range < 360) g_map_range += 30;
	}
	HUD_init_message(HM_DEFAULT, "Minimap Range: %dm", g_map_range);
}

void tactical_zoom(int step)
{
	tactical_radar_zoom(step);
	tactical_map_zoom(step);
}

void tactical_toggle_mode(void)
{
	g_tactical_mode = (tactical_mode_t)((g_tactical_mode + 1) % 4);
	switch (g_tactical_mode) {
		case TACTICAL_MODE_TRI:
			HUD_init_message(HM_DEFAULT, "Tactical: Tri-View (Rear/Radar/Map)");
			break;
		case TACTICAL_MODE_FULL_REAR:
			HUD_init_message(HM_DEFAULT, "Tactical: Full Rear View");
			break;
		case TACTICAL_MODE_FULL_RADAR:
			HUD_init_message(HM_DEFAULT, "Tactical: Full Radar");
			break;
		case TACTICAL_MODE_FULL_MAP:
			HUD_init_message(HM_DEFAULT, "Tactical: Full Minimap");
			break;
	}
}

int tactical_is_active(void)
{
	return (Game_wind != NULL && Player_num >= 0 && Player_num < N_players &&
	        !Automap_active && !Player_is_dead && ConsoleObject != NULL);
}

/* Draw small high-contrast [-] and [+] touch buttons in the top-right of a panel */
static void draw_panel_zoom_buttons(int px, int py, int pw, int cx0, int cy0, int cw0, int ch0)
{
	int by = py + 2;
	int bh = 14;
	int bw = 16;
	int minus_x = px + pw - 38;
	int plus_x = px + pw - 19;

	/* [-] button */
	bottom_draw_rect(minus_x, by, bw, bh, COL_GRID_CYAN, cx0, cy0, cw0, ch0);
	bottom_draw_line(minus_x + 4, by + bh / 2, minus_x + bw - 5, by + bh / 2, COL_WHITE, cx0, cy0, cw0, ch0);

	/* [+] button */
	bottom_draw_rect(plus_x, by, bw, bh, COL_GRID_CYAN, cx0, cy0, cw0, ch0);
	bottom_draw_line(plus_x + 4, by + bh / 2, plus_x + bw - 5, by + bh / 2, COL_WHITE, cx0, cy0, cw0, ch0);
	bottom_draw_line(plus_x + bw / 2, by + 3, plus_x + bw / 2, by + bh - 4, COL_WHITE, cx0, cy0, cw0, ch0);
}

/* --- Touch Handler for Tactical Area --- */
static void handle_tactical_touch(void)
{
	touchPosition t;
	if (hidKeysHeld() & KEY_TOUCH) {
		hidTouchRead(&t);
		if (t.px >= TACTICAL_AREA_X && t.px < TACTICAL_AREA_X + TACTICAL_AREA_W &&
		    t.py >= TACTICAL_AREA_Y && t.py < TACTICAL_AREA_Y + TACTICAL_AREA_H) {
			if (!g_touch_down_prev) {
				g_touch_down_prev = 1;
				if (g_tactical_mode == TACTICAL_MODE_TRI) {
					/* Top area (Rear-View Mirror): py in [26, 98) */
					if (t.py < TACTICAL_AREA_Y + 72) {
						g_tactical_mode = TACTICAL_MODE_FULL_REAR;
						HUD_init_message(HM_DEFAULT, "Tactical: Full Rear View");
					} else if (t.px < TACTICAL_AREA_X + 152) {
						/* Bottom-left area (Radar): px in [8, 156), py in [100, 214) */
						int rx = TACTICAL_AREA_X, ry = TACTICAL_AREA_Y + 74, rw = 148;
						int mx_btn = rx + rw - 38;
						int px_btn = rx + rw - 19;
						if (t.py < ry + 24 && t.px >= mx_btn - 6 && t.px < mx_btn + 17) {
							tactical_radar_zoom(-1);
						} else if (t.py < ry + 24 && t.px >= px_btn - 2 && t.px < px_btn + 22) {
							tactical_radar_zoom(1);
						} else {
							g_tactical_mode = TACTICAL_MODE_FULL_RADAR;
							HUD_init_message(HM_DEFAULT, "Tactical: Full Radar");
						}
					} else {
						/* Bottom-right area (Minimap): px in [164, 312), py in [100, 214) */
						int ax = TACTICAL_AREA_X + 156, ay = TACTICAL_AREA_Y + 74, aw = 148;
						int mx_btn = ax + aw - 38;
						int px_btn = ax + aw - 19;
						if (t.py < ay + 24 && t.px >= mx_btn - 6 && t.px < mx_btn + 17) {
							tactical_map_zoom(-1);
						} else if (t.py < ay + 24 && t.px >= px_btn - 2 && t.px < px_btn + 22) {
							tactical_map_zoom(1);
						} else {
							g_tactical_mode = TACTICAL_MODE_FULL_MAP;
							HUD_init_message(HM_DEFAULT, "Tactical: Full Minimap");
						}
					}
				} else if (g_tactical_mode == TACTICAL_MODE_FULL_RADAR) {
					/* In full radar, check top-right zoom buttons */
					int mx_btn = TACTICAL_AREA_X + TACTICAL_AREA_W - 38;
					int px_btn = TACTICAL_AREA_X + TACTICAL_AREA_W - 19;
					if (t.py < TACTICAL_AREA_Y + 24 && t.px >= mx_btn - 6 && t.px < mx_btn + 17) {
						tactical_radar_zoom(-1);
					} else if (t.py < TACTICAL_AREA_Y + 24 && t.px >= px_btn - 2 && t.px < px_btn + 22) {
						tactical_radar_zoom(1);
					} else {
						g_tactical_mode = TACTICAL_MODE_TRI;
						HUD_init_message(HM_DEFAULT, "Tactical: Tri-View (Rear/Radar/Map)");
					}
				} else if (g_tactical_mode == TACTICAL_MODE_FULL_MAP) {
					/* In full minimap, check top-right zoom buttons */
					int mx_btn = TACTICAL_AREA_X + TACTICAL_AREA_W - 38;
					int px_btn = TACTICAL_AREA_X + TACTICAL_AREA_W - 19;
					if (t.py < TACTICAL_AREA_Y + 24 && t.px >= mx_btn - 6 && t.px < mx_btn + 17) {
						tactical_map_zoom(-1);
					} else if (t.py < TACTICAL_AREA_Y + 24 && t.px >= px_btn - 2 && t.px < px_btn + 22) {
						tactical_map_zoom(1);
					} else {
						g_tactical_mode = TACTICAL_MODE_TRI;
						HUD_init_message(HM_DEFAULT, "Tactical: Tri-View (Rear/Radar/Map)");
					}
				} else {
					/* Full rear view: tap returns to 3-way layout */
					g_tactical_mode = TACTICAL_MODE_TRI;
					HUD_init_message(HM_DEFAULT, "Tactical: Tri-View (Rear/Radar/Map)");
				}
			}
		}
	} else {
		g_touch_down_prev = 0;
	}
}

/* --- 3D Engine Rear-View Scene Render --- */
void tactical_render_rear_3d(void)
{
	if (!tactical_is_active() || !g_rear_view_buf)
		return;

	int saved_demo_state = Newdemo_state;
	if (Newdemo_state == ND_STATE_RECORDING)
		Newdemo_state = ND_STATE_NORMAL;

	Rear_view = 1;

	/* Render full 400x240 aft scene to the main screen canvas (no cockpit cutout) */
	gr_set_current_canvas(&grd_curscreen->sc_canvas);
	render_frame(0);

	Rear_view = 0;
	Newdemo_state = saved_demo_state;

	/* Capture the centered 320x240 region of the 400x240 buffer */
	pglCaptureRearView(g_rear_view_buf);
	g_rear_view_ready = 1;

	gr_set_current_canvas(NULL);
}

/* --- Live 3D Rear-View Mirror Display --- */
static void render_rear_view_mirror(int mx, int my, int mw, int mh, int pulse)
{
	int cx0 = mx + 1, cy0 = my + 1, cw0 = mw - 2, ch0 = mh - 2;

	if (g_rear_view_ready && g_rear_view_buf) {
		/* Blit the 3D rendered rear-view from linear buffer */
		bottom_copy_rear_view(g_rear_view_buf, mx, my, mw, mh);
	} else {
		bottom_fill_rect(mx, my, mw, mh, COL_BLACK);
	}

	/* Outer border frame */
	bottom_draw_rect(mx, my, mw, mh, COL_DARK_GRAY, mx, my, mw, mh);

	/* Subtle corner title */
	bottom_print_clipped(mx + 4, my + 3, "REAR", COL_BRIGHT_CYAN, cx0, cy0, cw0, ch0);

	/* Threat detection in rear view */
	vms_vector rear_fvec;
	rear_fvec.x = -ConsoleObject->orient.fvec.x;
	rear_fvec.y = -ConsoleObject->orient.fvec.y;
	rear_fvec.z = -ConsoleObject->orient.fvec.z;

	int enemies_behind = 0;
	int closest_dist_meters = 999;
	int missile_alert = 0;

	int i;
	for (i = 0; i <= Highest_object_index; i++) {
		object *objp = &Objects[i];
		if (objp->type != OBJ_ROBOT && objp->type != OBJ_WEAPON) continue;
		if (objp == ConsoleObject) continue;

		vms_vector orel;
		orel.x = objp->pos.x - ConsoleObject->pos.x;
		orel.y = objp->pos.y - ConsoleObject->pos.y;
		orel.z = objp->pos.z - ConsoleObject->pos.z;

		fix rz = vm_vec_dot(&orel, &rear_fvec);
		if (rz <= f1_0 * 4) continue; /* must be behind ship */

		fix dist = vm_vec_mag(&orel);
		int dist_m = f2i(dist);

		if (objp->type == OBJ_WEAPON && objp->ctype.laser_info.parent_type == OBJ_ROBOT) {
			if (dist_m < 80)
				missile_alert = 1;
		} else if (objp->type == OBJ_ROBOT) {
			enemies_behind++;
			if (dist_m < closest_dist_meters)
				closest_dist_meters = dist_m;
		}
	}

	/* Status text in top-right */
	if (missile_alert) {
		bottom_print_clipped(mx + mw - 76, my + 3, "!MISSILE!", pulse ? COL_YELLOW : COL_RED, cx0, cy0, cw0, ch0);
		bottom_draw_rect(mx, my, mw, mh, COL_RED, mx, my, mw, mh);
	} else if (enemies_behind > 0) {
		char bstat[24];
		snprintf(bstat, sizeof(bstat), "AFT:%d (%dm)", enemies_behind, closest_dist_meters);
		bottom_print_clipped(mx + mw - 88, my + 3, bstat, (closest_dist_meters < 35 ? (pulse ? COL_RED : COL_YELLOW) : COL_ORANGE), cx0, cy0, cw0, ch0);
	}
}

/* --- Mini-Radar Renderer --- */
static void render_mini_radar(int rx, int ry, int rw, int rh, int pulse)
{
	int cx0 = rx + 1, cy0 = ry + 1, cw0 = rw - 2, ch0 = rh - 2;
	int cx = rx + rw / 2;
	int cy = ry + rh / 2;
	int rad = (rw < rh ? rw : rh) / 2 - 8;
	if (rad < 22) rad = 22;

	int current_range = g_radar_ranges[g_radar_range_idx];
	fix range_fix = i2f(current_range);

	/* Outer panel border */
	bottom_draw_rect(rx, ry, rw, rh, COL_DARK_GRAY, rx, ry, rw, rh);

	/* Radar Scope Rings */
	bottom_draw_circle(cx, cy, rad, COL_GRID_CYAN, cx0, cy0, cw0, ch0);
	bottom_draw_circle(cx, cy, rad / 2, COL_DARK_GRAY, cx0, cy0, cw0, ch0);

	/* Crosshairs */
	bottom_draw_line(cx - rad, cy, cx + rad, cy, COL_DARK_GRAY, cx0, cy0, cw0, ch0);
	bottom_draw_line(cx, cy - rad, cx, cy + rad, COL_DARK_GRAY, cx0, cy0, cw0, ch0);

	/* Cardinal markers */
	bottom_print_clipped(cx - 2, cy - rad + 2, "F", COL_LIGHT_GRAY, cx0, cy0, cw0, ch0);
	bottom_print_clipped(cx - 2, cy + rad - 8, "A", COL_LIGHT_GRAY, cx0, cy0, cw0, ch0);
	bottom_print_clipped(cx - rad + 2, cy - 3, "L", COL_LIGHT_GRAY, cx0, cy0, cw0, ch0);
	bottom_print_clipped(cx + rad - 8, cy - 3, "R", COL_LIGHT_GRAY, cx0, cy0, cw0, ch0);

	/* Rotating radar sweep line */
	{
		g_radar_sweep_angle += 0.08f;
		if (g_radar_sweep_angle > 6.283185f)
			g_radar_sweep_angle -= 6.283185f;
		int sx = cx + (int)(cosf(g_radar_sweep_angle) * rad);
		int sy = cy + (int)(sinf(g_radar_sweep_angle) * rad);
		bottom_draw_line(cx, cy, sx, sy, COL_DIM_GREEN, cx0, cy0, cw0, ch0);
	}

	/* Center player ship icon: green chevron pointing forward (UP) */
	bottom_draw_line(cx, cy - 4, cx - 3, cy + 3, COL_GREEN, cx0, cy0, cw0, ch0);
	bottom_draw_line(cx - 3, cy + 3, cx, cy + 1, COL_GREEN, cx0, cy0, cw0, ch0);
	bottom_draw_line(cx, cy + 1, cx + 3, cy + 3, COL_GREEN, cx0, cy0, cw0, ch0);
	bottom_draw_line(cx + 3, cy + 3, cx, cy - 4, COL_GREEN, cx0, cy0, cw0, ch0);

	/* Scan and plot all game objects */
	int i;
	for (i = 0; i <= Highest_object_index; i++) {
		object *objp = &Objects[i];
		if (objp->type == OBJ_NONE) continue;
		if (objp == ConsoleObject) continue;

		vms_vector rel;
		rel.x = objp->pos.x - ConsoleObject->pos.x;
		rel.y = objp->pos.y - ConsoleObject->pos.y;
		rel.z = objp->pos.z - ConsoleObject->pos.z;

		fix dist = vm_vec_mag(&rel);
		if (dist > range_fix) continue;

		/* Project relative to player's 6-DOF orientation */
		fix x_rel = vm_vec_dot(&rel, &ConsoleObject->orient.rvec);
		fix y_fwd = vm_vec_dot(&rel, &ConsoleObject->orient.fvec);
		fix z_elev = vm_vec_dot(&rel, &ConsoleObject->orient.uvec);

		int px = cx + (int)(((int64_t)x_rel * rad) / range_fix);
		int py = cy - (int)(((int64_t)y_fwd * rad) / range_fix);

		/* Clamp to circular radar scope */
		int dx = px - cx;
		int dy = py - cy;
		if (dx * dx + dy * dy > rad * rad) continue;

		/* Plot based on object type */
		switch (objp->type) {
			case OBJ_ROBOT: {
				uint16_t rcol = (pulse ? COL_RED : COL_ORANGE);
				bottom_draw_line(px - 1, py, px + 1, py, rcol, cx0, cy0, cw0, ch0);
				bottom_draw_line(px, py - 1, px, py + 1, rcol, cx0, cy0, cw0, ch0);
				if (z_elev > f1_0 * 10)
					tactical_pixel(px, py - 3, rcol, cx0, cy0, cw0, ch0);
				else if (z_elev < -f1_0 * 10)
					tactical_pixel(px, py + 3, rcol, cx0, cy0, cw0, ch0);
				break;
			}
			case OBJ_WEAPON: {
				if (objp->ctype.laser_info.parent_type == OBJ_ROBOT) {
					uint16_t wcol = (pulse ? COL_YELLOW : COL_RED);
					bottom_draw_rect(px - 2, py - 2, 4, 4, wcol, cx0, cy0, cw0, ch0);
				}
				break;
			}
			case OBJ_CNTRLCEN: {
				uint16_t ccol = (pulse ? COL_MAGENTA : COL_WHITE);
				bottom_draw_rect(px - 2, py - 2, 4, 4, ccol, cx0, cy0, cw0, ch0);
				bottom_print_clipped(px + 3, py - 3, "C", ccol, cx0, cy0, cw0, ch0);
				break;
			}
			case OBJ_HOSTAGE: {
				tactical_pixel(px, py, COL_BRIGHT_CYAN, cx0, cy0, cw0, ch0);
				bottom_print_clipped(px + 2, py - 3, "H", COL_BRIGHT_CYAN, cx0, cy0, cw0, ch0);
				break;
			}
			case OBJ_POWERUP: {
				if (objp->id == POW_KEY_RED)
					bottom_print_clipped(px - 2, py - 3, "K", COL_RED, cx0, cy0, cw0, ch0);
				else if (objp->id == POW_KEY_BLUE)
					bottom_print_clipped(px - 2, py - 3, "K", COL_BLUE, cx0, cy0, cw0, ch0);
				else if (objp->id == POW_KEY_GOLD)
					bottom_print_clipped(px - 2, py - 3, "K", COL_GOLD, cx0, cy0, cw0, ch0);
				else
					tactical_pixel(px, py, COL_GREEN, cx0, cy0, cw0, ch0);
				break;
			}
			case OBJ_PLAYER: {
				if (i != Players[Player_num].objnum)
					bottom_print_clipped(px - 2, py - 3, "P", COL_WHITE, cx0, cy0, cw0, ch0);
				break;
			}
			default:
				break;
		}
	}

	/* Clean labels in corners */
	bottom_print_clipped(rx + 4, ry + 3, "RADAR", COL_BRIGHT_CYAN, cx0, cy0, cw0, ch0);
	char rng[16];
	snprintf(rng, sizeof(rng), "%dm", current_range);
	bottom_print_clipped(rx + 46, ry + 3, rng, COL_LIGHT_GRAY, cx0, cy0, cw0, ch0);

	/* Top-right zoom buttons */
	draw_panel_zoom_buttons(rx, ry, rw, cx0, cy0, cw0, ch0);
}

/* --- Mini Wireframe Automap Renderer --- */
static void render_mini_automap(int ax, int ay, int aw, int ah, int pulse)
{
	int cx0 = ax + 1, cy0 = ay + 1, cw0 = aw - 2, ch0 = ah - 2;
	int mcx = ax + aw / 2;
	int mcy = ay + ah / 2;
	int rad = (aw < ah ? aw : ah) / 2 - 8;
	if (rad < 22) rad = 22;

	fix range_fix = i2f(g_map_range);

	/* Outer panel border */
	bottom_draw_rect(ax, ay, aw, ah, COL_DARK_GRAY, ax, ay, aw, ah);

	/* Label in corner */
	bottom_print_clipped(ax + 4, ay + 3, "MAP", COL_BRIGHT_CYAN, cx0, cy0, cw0, ch0);
	char mstr[16];
	snprintf(mstr, sizeof(mstr), "%dm", g_map_range);
	bottom_print_clipped(ax + 32, ay + 3, mstr, COL_LIGHT_GRAY, cx0, cy0, cw0, ch0);

	/* Top-right zoom buttons */
	draw_panel_zoom_buttons(ax, ay, aw, cx0, cy0, cw0, ch0);

	/* Draw visited segments in 3D wireframe around player */
	int segnum;
	int segs_drawn = 0;

	for (segnum = 0; segnum <= Highest_segment_index && segs_drawn < 32; segnum++) {
		if (!Automap_visited[segnum]) continue;
		segment *seg = &Segments[segnum];

		vms_vector srel;
		srel.x = Vertices[seg->verts[0]].x - ConsoleObject->pos.x;
		srel.y = Vertices[seg->verts[0]].y - ConsoleObject->pos.y;
		srel.z = Vertices[seg->verts[0]].z - ConsoleObject->pos.z;

		if (vm_vec_mag(&srel) > range_fix) continue;

		int s;
		for (s = 0; s < 6; s++) {
			int n = seg->children[s];
			int wn = seg->sides[s].wall_num;
			int is_door = (wn >= 0 && Walls[wn].type == WALL_DOOR);
			if (n >= 0 && Automap_visited[n] && !is_door)
				continue;

			uint16_t wcol = is_door ? COL_GOLD : COL_GRID_CYAN;
			const sbyte *fv = Side_to_verts[s];
			int pts[4][2];
			int k;

			for (k = 0; k < 4; k++) {
				vms_vector vrel;
				vrel.x = Vertices[seg->verts[(int)fv[k]]].x - ConsoleObject->pos.x;
				vrel.y = Vertices[seg->verts[(int)fv[k]]].y - ConsoleObject->pos.y;
				vrel.z = Vertices[seg->verts[(int)fv[k]]].z - ConsoleObject->pos.z;

				fix rx = vm_vec_dot(&vrel, &ConsoleObject->orient.rvec);
				fix ry = vm_vec_dot(&vrel, &ConsoleObject->orient.fvec);
				fix rz = vm_vec_dot(&vrel, &ConsoleObject->orient.uvec);

				/* Tilted 6-DOF map projection zoomed in for clear room geometry */
				pts[k][0] = mcx + (int)(((int64_t)rx * rad * 2) / range_fix);
				pts[k][1] = mcy - (int)(((int64_t)(ry * 7 - rz * 3) * rad * 2) / (range_fix * 8));
			}

			for (k = 0; k < 4; k++) {
				int a = k, b = (k + 1) & 3;
				bottom_draw_line(pts[a][0], pts[a][1], pts[b][0], pts[b][1],
						wcol, cx0, cy0, cw0, ch0);
			}
		}
		segs_drawn++;
	}

	/* Object blips on minimap */
	int i;
	for (i = 0; i <= Highest_object_index; i++) {
		object *objp = &Objects[i];
		if (objp->type == OBJ_NONE) continue;
		if (objp == ConsoleObject) continue;

		vms_vector orel;
		orel.x = objp->pos.x - ConsoleObject->pos.x;
		orel.y = objp->pos.y - ConsoleObject->pos.y;
		orel.z = objp->pos.z - ConsoleObject->pos.z;

		if (vm_vec_mag(&orel) > range_fix) continue;

		fix rx = vm_vec_dot(&orel, &ConsoleObject->orient.rvec);
		fix ry = vm_vec_dot(&orel, &ConsoleObject->orient.fvec);
		fix rz = vm_vec_dot(&orel, &ConsoleObject->orient.uvec);

		int px = mcx + (int)(((int64_t)rx * rad * 2) / range_fix);
		int py = mcy - (int)(((int64_t)(ry * 7 - rz * 3) * rad * 2) / (range_fix * 8));

		if (px < cx0 + 6 || px >= cx0 + cw0 - 6 || py < cy0 + 6 || py >= cy0 + ch0 - 6)
			continue;

		switch (objp->type) {
			case OBJ_HOSTAGE:
				bottom_print_clipped(px - 2, py - 3, "H", COL_BRIGHT_CYAN, cx0, cy0, cw0, ch0);
				break;
			case OBJ_POWERUP:
				if (objp->id == POW_KEY_RED)
					bottom_print_clipped(px - 2, py - 3, "K", COL_RED, cx0, cy0, cw0, ch0);
				else if (objp->id == POW_KEY_BLUE)
					bottom_print_clipped(px - 2, py - 3, "K", COL_BLUE, cx0, cy0, cw0, ch0);
				else if (objp->id == POW_KEY_GOLD)
					bottom_print_clipped(px - 2, py - 3, "K", COL_GOLD, cx0, cy0, cw0, ch0);
				else
					tactical_pixel(px, py, COL_GREEN, cx0, cy0, cw0, ch0);
				break;
			case OBJ_ROBOT:
				tactical_pixel(px, py, (pulse ? COL_RED : COL_ORANGE), cx0, cy0, cw0, ch0);
				break;
			case OBJ_CNTRLCEN:
				bottom_print_clipped(px - 2, py - 3, "C", (pulse ? COL_MAGENTA : COL_WHITE), cx0, cy0, cw0, ch0);
				break;
			default:
				break;
		}
	}

	/* Player ship marker at center: white chevron with heading tick */
	bottom_draw_line(mcx, mcy - 5, mcx - 3, mcy + 3, COL_WHITE, cx0, cy0, cw0, ch0);
	bottom_draw_line(mcx - 3, mcy + 3, mcx, mcy + 1, COL_WHITE, cx0, cy0, cw0, ch0);
	bottom_draw_line(mcx, mcy + 1, mcx + 3, mcy + 3, COL_WHITE, cx0, cy0, cw0, ch0);
	bottom_draw_line(mcx + 3, mcy + 3, mcx, mcy - 5, COL_WHITE, cx0, cy0, cw0, ch0);
	bottom_draw_line(mcx, mcy - 5, mcx, mcy - 9, COL_YELLOW, cx0, cy0, cw0, ch0);
}

/* --- Per-Frame Tick --- */
static tactical_mode_t g_last_rendered_mode = (tactical_mode_t)-1;

void tactical_bottom_tick(void)
{
	if (!tactical_is_active()) return;

	handle_tactical_touch();

	/* Flashing pulse state (alternates roughly every 0.25 sec) */
	int pulse = ((timer_query() >> 14) & 1);

	/* If mode changed, clear whole tactical area once */
	if (g_last_rendered_mode != g_tactical_mode) {
		bottom_clear_rect(TACTICAL_AREA_X, TACTICAL_AREA_Y, TACTICAL_AREA_W, TACTICAL_AREA_H);
		g_last_rendered_mode = g_tactical_mode;
	}

	switch (g_tactical_mode) {
		case TACTICAL_MODE_TRI: {
			/* Top long rectangle: 3D Rear-View Mirror (304 x 70, self-clearing/blitted) */
			render_rear_view_mirror(TACTICAL_AREA_X, TACTICAL_AREA_Y, TACTICAL_AREA_W, 70, pulse);

			/* Bottom-left square: Mini-Radar (148 x 114) */
			bottom_clear_rect(TACTICAL_AREA_X, TACTICAL_AREA_Y + 74, 148, 114);
			render_mini_radar(TACTICAL_AREA_X, TACTICAL_AREA_Y + 74, 148, 114, pulse);

			/* Bottom-right square: Wireframe Minimap (148 x 114) */
			bottom_clear_rect(TACTICAL_AREA_X + 156, TACTICAL_AREA_Y + 74, 148, 114);
			render_mini_automap(TACTICAL_AREA_X + 156, TACTICAL_AREA_Y + 74, 148, 114, pulse);
			break;
		}
		case TACTICAL_MODE_FULL_REAR: {
			render_rear_view_mirror(TACTICAL_AREA_X, TACTICAL_AREA_Y, TACTICAL_AREA_W, TACTICAL_AREA_H, pulse);
			break;
		}
		case TACTICAL_MODE_FULL_RADAR: {
			bottom_clear_rect(TACTICAL_AREA_X, TACTICAL_AREA_Y, TACTICAL_AREA_W, TACTICAL_AREA_H);
			render_mini_radar(TACTICAL_AREA_X, TACTICAL_AREA_Y, TACTICAL_AREA_W, TACTICAL_AREA_H, pulse);
			break;
		}
		case TACTICAL_MODE_FULL_MAP: {
			bottom_clear_rect(TACTICAL_AREA_X, TACTICAL_AREA_Y, TACTICAL_AREA_W, TACTICAL_AREA_H);
			render_mini_automap(TACTICAL_AREA_X, TACTICAL_AREA_Y, TACTICAL_AREA_W, TACTICAL_AREA_H, pulse);
			break;
		}
	}
}

#endif /* __3DS__ */
