# D1X 3DS v2.1.0 — Release Notes

**D1X-Rebirth (Descent I)** port for the Nintendo 3DS family (New 3DS / New 2DS XL recommended).

---

## Highlights in v2.1.0

### 1. Tri-View Bottom-Screen Tactical Dashboard
* **Real-time Live 3D Rear-View Mirror**: Full 3D camera rendering looking behind your ship in real time, centered in the top panel of the bottom screen with an unobstructed wide field of view.
* **Tactical Mini-Radar**: Live 360-degree radar sweep showing nearby hostile robots (red), hostages (green), and power-ups (yellow). Includes independent in-box `+` and `−` touch buttons to zoom radar range from 15m to 240m.
* **Mini Wireframe Automap**: Real-time 3D wireframe overview of the local mine structure with independent in-box `+` and `−` touch buttons.
* **Tap-to-Fullscreen**: Tap either the radar or minimap square to instantly expand it to full bottom-screen view; tap again to return to Tri-View.
* **Quick-Access Touch Buttons**: Touch bar with quick controls for `HUD` toggle, `GYRO` toggle, `PRIMARY` weapon cycle, `SECONDARY` weapon cycle, `SAVE`, and `MENU`.
* **Custom Handheld Typography**: Compact, clean labels and status indicators designed specifically for legibility on the 3DS bottom screen.

### 2. Hardware-Calibrated Gyroscope Aim Assist
* **Handheld Motion Aiming**: Tilt the 3DS console to pitch and yaw your ship with precision gyro aim assist.
* **Drift Suppression & Auto-Calibration**: Stationary-window auto-calibration and deadband filtering keep aim steady and counteract resting hand tremors and handheld tilt bias.
* **Dedicated Gyroscope Settings Menu**: Accessible under `OPTIONS -> CONTROLS -> GYROSCOPE SETTINGS`:
  * **Deadzone (0–16)**: Fine-tune deadband to eliminate resting drift and hand tremors.
  * **Sensitivity (1–16)**: Adjust rotation rate multiplier.
  * **Calibrate Zero Bias**: Action button to immediately zero out bias for your current grip angle.
* **Quick Toggle**: Toggle gyro on/off anytime during gameplay via the bottom-screen `GYRO` touch button.

### 3. Visual & Rendering Upgrades
* **24-Bit True Color & Dithering**: 24-bit framebuffer rendering with spatial color dithering to eliminate banding across dark mines and lighting gradients.
* **Crisp Font Rendering**: Point/nearest-neighbor filtering enforced on font textures, eliminating fuzzy text across HUD and menus while 3D geometry retains smooth bilinear filtering.
* **+10% Ambient Brightness Boost**: Tuned lighting model illuminates dark mine corridors while preserving contrast.
* **Sharper Top-Screen Automap**: 40% thinner wireframe lines for a clean, high-precision map.
* **Stereo 3D Polish**: Smooth display buffer synchronization eliminates flashing banners during level transitions and demo playback.

### 4. Audio Overhaul & Jukebox
* **High-Fidelity Roland SC-55 Soundtrack**: Support for authentic Roland Sound Canvas SC-55 hardware MP3 recordings by Brandon Blume (recommended by GBAtemp user **bakuDD**), featuring seamless fallback to OGG and WAV.
* **In-Game / Menu Jukebox**: Track selector to preview and play any soundtrack piece on demand.
* **Audio Buffer & Frame-Pacing**: Sound effects preloaded and sound BFS pathing stalls eliminated in large rooms, resolving micro-stutters during combat.

### 5. Controls, Camera & Navigation
* **Precision Crosshairs**: Added in-cockpit crosshairs on the top screen for gun and missile alignment.
* **Free-Cam Navigation**: Integrated free-camera exploration mode.

### 6. Save System & Lifecycle Improvements
* **Visual Save Game Thumbnails**: Save slots now capture and display a live gameplay screenshot thumbnail directly in the save/load menu for instant visual recognition of your game state.
* **Seamless Mid-Game Flow**: Starting a new game or loading an existing save from the in-game pause menu cleanly unwinds menu overlays and transitions directly into gameplay without requiring a manual game abort.
* **Refueling & Spawner Smoothness**: Optimized energy center particle updates and robot spawner logic to eliminate micro-stutters.
* **System Stability & Sleep Support**: Full `aptMainLoop` pumping across all modal event loops and menus ensures system sleep, HOME button suspend, and power-down operate cleanly without system freezes or GPU crashes.
* **End-of-Level Statistics**: Resolved stats calculation and rendering glitches on level-clear screens.

---

## 3DS Controls Summary

| Action                    | Button / Input                                |
|---------------------------|-----------------------------------------------|
| Fire primary              | `R`                                           |
| Fire secondary / missile  | `L`                                           |
| Accelerate (forward engine)| `X`                                          |
| Reverse / brake           | `B`                                           |
| Slide left (strafe)       | `Y`                                           |
| Slide right (strafe)      | `A`                                           |
| Drop bomb                 | `L` (hold context)                            |
| Flare                     | `ZR`                                          |
| Rear view (hold)          | `ZL`                                          |
| Automap (full 3D map)     | `SELECT`                                      |
| Menu / pause / back       | `START` (Esc)                                 |
| Aim assist                | Gyroscope (tilt console to pitch/yaw)         |
| Flight steering           | Circle Pad (analog pitch/turn)                |
| Look / strafe vertical    | C-Stick (New 3DS)                             |
| Cycle cockpit view        | `D-PAD UP` (prev) / `D-PAD DOWN` (next)       |
| Weapon prev / next        | `D-PAD LEFT` / `D-PAD RIGHT`                  |
| Quick save                | `START` + `X`                                 |
| Quick load                | `START` + `Y`                                 |
| Controls help screen      | `START` + `SELECT`                            |
| Death screen dismiss      | any face button / `START`                     |

---

## Music Setup (Roland SC-55)

The 3DS SDL_mixer port does not decode stock HMP MIDI directly. For authentic Roland Sound Canvas music:

1. Download the Roland SC-55 MP3 Pack by Brandon Blume: [Duke4.net SC-55 Archive](https://sc55.duke4.net/mp3/descent1_mp3.zip) (recommended by GBAtemp user **bakuDD**).
2. Extract the 27 `.mp3` files directly into `/3ds/D1/mp3/` on your SD card (`game01.mp3` through `game22.mp3`, `briefing.mp3`, `credits.mp3`, `descent.mp3`, `endgame.mp3`, `endlevel.mp3`).
3. Audio fallback order: `/3ds/D1/wav/` → `/3ds/D1/mp3/` → `/3ds/D1/ogg/`.

---

## Downloads

* `d1x-3ds-2.1.0.cia` — install via FBI / title manager (recommended)
* `d1x-3ds-2.1.0.3dsx` — run from Homebrew Menu

**Title ID**: `0x000400000FDDEB97`

---

## Credits & Acknowledgments

* **Port & 3DS Enhancements**: Dennis Isaac Gutierrez Zeledon
* **AI Coding Partner**: Gemini Antigravity (Google DeepMind)
* **Initial 3DS Port Base & Stereo 3D**: Hermes Agent (Nous Research) & Parallax Software / DXX-Rebirth team
* **Community Contributors & Research**:
  * **CrashMidnick (GBAtemp)**: Invaluable hardware testing and feedback on N3DS and O3DS that guided multiple core fixes: reporting in-game screen tearing/texture shaking (resolved via GPU pipeline & bilinear filtering improvements), identifying overly aggressive default 3D depth settings (leading to softened, comfortable stereo depth scaling), highlighting MIDI extraction issues (leading to the elimination of redundant MIDI generation and implementation of the clean WAV/MP3/OGG audio fallback chain), and reporting Old 3DS compatibility issues. Sincere thanks for these outstanding contributions!
  * **bakuDD (GBAtemp)**: Research and recommendation of the authentic Roland Sound Canvas SC-55 soundtrack recordings from [sc55.duke4.net](https://sc55.duke4.net/mp3/descent1_mp3.zip) by Brandon Blume, providing the definitive high-fidelity audio solution for the 3DS port. Sincere thanks for this great contribution!
