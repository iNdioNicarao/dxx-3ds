/*
 * Tactical Bottom Screen Subsystem: Live Rear-View Mirror & Always-On Mini-Radar
 * For Nintendo 3DS (D1X-3DS)
 *
 * Provides real-time 360-degree situational awareness (Mini-Radar) and
 * live aft situational tracking (Rear-View Mirror) in the bottom-screen
 * center safe area during active flight.
 */

#ifndef _TACTICAL_BOTTOM_H_
#define _TACTICAL_BOTTOM_H_

#ifdef __3DS__

typedef enum {
	TACTICAL_MODE_TRI = 0,     /* Top: Rear-View Mirror, Bottom-Left: Radar, Bottom-Right: Automap Minimap */
	TACTICAL_MODE_FULL_REAR,   /* Full-screen Live Rear-View Mirror */
	TACTICAL_MODE_FULL_RADAR,  /* Full-screen Mini-Radar */
	TACTICAL_MODE_FULL_MAP     /* Full-screen Wireframe Minimap */
} tactical_mode_t;

/* Initialize / reset tactical state across level transitions */
void tactical_bottom_init(void);

/* Per-frame tick: updates radar, rear-view mirror, and minimap displays */
void tactical_bottom_tick(void);

/* Renders the actual 3D engine rear-view scene into the mirror buffer */
void tactical_render_rear_3d(void);

/* Adjust radar range independently (step: +1 = zoom in / closer range, -1 = zoom out / wider range) */
void tactical_radar_zoom(int step);

/* Adjust minimap range independently (step: +1 = zoom in / closer range, -1 = zoom out / wider range) */
void tactical_map_zoom(int step);

/* Adjust both ranges together (legacy) */
void tactical_zoom(int step);

/* Toggle between split and full-screen modes */
void tactical_toggle_mode(void);

/* Check if the tactical bottom screen is active */
int tactical_is_active(void);

#endif /* __3DS__ */

#endif /* _TACTICAL_BOTTOM_H_ */
