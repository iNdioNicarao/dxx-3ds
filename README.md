# D1X 3DS

A Nintendo 3DS port of **Descent I** (D1X), built for the **New 3DS**
family (New 3DS / New 2DS XL). Hardware-rendered via **picaGL** on the
PICA200 GPU, with glasses-free **autostereoscopic 3D** seamlessly driven
by the console's 3D depth slider.

This is a fork of [DXX-Switch](https://github.com/aagallag/DXX-Switch) /
[DXX-Retro](https://github.com/CDarrow/DXX-Retro), which is a fork of
[DXX-Rebirth](https://github.com/dxx-rebirth/dxx-rebirth), which is a fork
of the original engine by Parallax Software.

---

## Branches

| Branch        | What it is                                                            |
|----------------|-------------------------------------------------------------------------|
| `master`      | **Stable Descent I with stereoscopic 3D.** The playable, recommended build. Stereo is opt-in and 3D-slider-gated (see below). |
| `stereo-3d`  | **DEPRECATED** — superseded by `master`. Kept only for history. |

> [!WARNING]
> **`stereo-3d` is deprecated.** Stereoscopic 3D was merged into `master`
> (slider-gated: raise the 3DS 3D slider to enable depth, lower it for mono).
> Build/install `master` for all current features. The `stereo-3d` branch is no
> longer maintained and, if built, may install as a separate app icon.

`master` is the one to build/install. The 3D work is documented in
[`docs/STEREO_3D.md`](docs/STEREO_3D.md); the developer-side trace of that
code lives in [`docs/stereo-3d-logic-map.md`](docs/stereo-3d-logic-map.md).

---

## What's new in v2.1.1

This release delivers a **comprehensive performance overhaul** that eliminates framerate drops and ensures a rock-solid 60 FPS flight experience on New 3DS:

- **Decoupled Rear-View Mirror Cadence**:
  - Live 3D rear-view mirror rendering is now smoothly decoupled to an alternate-frame cadence (30 FPS mirror, 60 FPS flight).
  - Maintains full tactical rear awareness while freeing up critical GPU fillrate and CPU cycles for the primary top-screen combat cockpit.
- **Rear-View Depth Culling**:
  - Implemented dynamic depth culling (`Render_depth` capped to 18) for the rear-view camera, completely eliminating unnecessary overdraw of distant geometry in the rear-view viewport.
- **Eliminated SD Card Diagnostic Stalls**:
  - Root-caused and resolved severe framerate drops (down to 36 FPS or lower), which particularly affected users with large SD cards (256GB / 512GB).
  - Removed synchronous per-frame SD card diagnostic file writes (`lum_trace.txt` and `pgl_trace.txt`) and frame-buffer pixel hashing loops from the low-level rendering pipeline.
- **Bottom-Screen Latency Decoupling**:
  - Removed redundant VBlank synchronization stalls during bottom-screen presentations, ensuring completely smooth frame pacing and zero top-screen stutter.
- **Compiler & Math Stability**:
  - Standardized clean `-O2` optimization flags across the codebase to ensure robust struct alignment, deterministic fixed-point math, and total stability.

---

## What's new in v2.1.0

This release introduces the **Tri-View Bottom Screen Tactical Dashboard**, **Hardware-Calibrated Gyro Aim Assist**, **Roland SC-55 High-Fidelity Audio**, and significant engine stability and visual upgrades:

- **Tri-View Bottom Screen Tactical Dashboard**:
  - **Live 3D Rear-View Mirror**: Full real-time 3D camera rendering looking out the back of the ship, perfectly centered on the top panel of the bottom screen.
  - **Tactical Mini-Radar**: Live 360-degree radar sweep showing nearby robots and powerups with dedicated in-box `+` and `−` zoom buttons.
  - **Mini Wireframe Automap**: Real-time 3D mine wireframe overview with independent in-box `+` and `−` zoom buttons.
  - **Tap to Fullscreen**: Tap either the radar or minimap square to expand it fullscreen on the bottom screen; tap again to return to tri-view.
  - **Touch Button Bar**: Quick-tap buttons for HUD toggle, Gyroscope toggle, Primary weapon cycle, Secondary weapon cycle, Save, and Menu.
  - **Custom Handheld Typography**: Compact, clean labels and status indicators designed specifically for legibility on the 3DS bottom screen.
- **Hardware-Calibrated Gyroscope Aim Assist**:
  - Handheld motion aiming: Tilt the console to pitch and yaw your ship with precision gyro aim assist.
  - Drift suppression & auto-calibration: Stationary auto-calibration and deadband filtering counteract resting tremors and handheld tilt bias.
  - Dedicated **Gyroscope Settings** menu (`OPTIONS -> CONTROLS -> GYROSCOPE SETTINGS`) with adjustable Deadzone (0–16), Sensitivity (1–16), and on-demand "Calibrate Zero Bias".
- **Visuals & Hardware Acceleration**:
  - **24-bit True Color & Dithering**: 24-bit framebuffer rendering with spatial color dithering to eliminate banding across dark mines and lighting gradients.
  - **Crisp Font Rendering**: Point/nearest-neighbor filtering enforced on font textures, eliminating fuzzy text across HUD and menus while 3D geometry retains smooth bilinear filtering.
  - **+10% Ambient Brightness Boost**: Tuned lighting curve to illuminate dark mine tunnels without washing out highlights.
  - **Sharper Top Automap**: 40% thinner wireframe lines for a cleaner, high-precision top-screen map.
  - **Stereo 3D Polish**: Smooth display buffer synchronization eliminates flashing banners during level transitions and demo playback.
- **Audio Overhaul & Jukebox**:
  - **Roland SC-55 Soundtrack**: High-fidelity Roland Sound Canvas SC-55 hardware MP3 soundtrack recorded by Brandon Blume (recommended by GBAtemp user **bakuDD**), with automatic fallback.
  - **In-Game / Menu Jukebox**: Track selector to preview and play any soundtrack piece on demand.
  - **Frame-Pacing & Buffer Fixes**: Audio buffer underrun fixes and elimination of sound BFS stalls in large rooms.
- **Controls & Navigation**:
  - **Top-Screen Precision Crosshairs**: Added crosshairs for cockpit aiming and dogfighting.
  - **Free-Cam Navigation**: Integrated free-camera exploration mode.
- **Save System & Mid-Game Lifecycle Improvements**:
  - **Visual Save Game Thumbnails**: Save slots now capture and display a live gameplay screenshot thumbnail directly in the save/load menu for instant visual recognition of your game state.
  - **Seamless Mid-Game Flow**: Starting a new game or loading an existing save from the in-game pause menu cleanly unwinds menu overlays and transitions directly into gameplay without requiring a manual game abort.
  - **System Stability & Sleep Support**: Full `aptMainLoop` pumping across all modal event loops ensures system sleep, HOME button suspend, and power-down operate cleanly without system freezes or GPU crashes.

---

## Install (Descent I)

You must **supply your own Descent I game data** — it is copyrighted and is
**not** included in this repo (and was scrubbed from history before publish).

1. Create `/3ds/D1/` on your SD card.
2. Copy **`descent.hog`** and **`descent.pig`** into it. These ship with a
   legitimate purchase of the game — tested with
   [Descent I on Steam](https://store.steampowered.com/app/273570/Descent/).
   (Other releases — GOG, CD-ROM — should work but are untested here.)

Then pick **one** install method:

**A. CIA (install to home menu)** — use the `d1x-3ds.cia` from a release:
- Copy `d1x-3ds.cia` to your SD card, install it with **FBI**, and launch
  from the home menu.

**B. 3DSX (Homebrew Launcher, no install)** — use the `d1x-3ds.3dsx` from a release:
- Copy `d1x-3ds.3dsx` into `sd:/3ds/D1X-3DS/`.
- Launch `D1X 3DS` from the **Homebrew Menu** (hold the appropriate
  exploit at boot, or use a forwarder).

> **Binaries come from Releases, not the repo.** The repo ships source + a
> Docker build flow. Grab the prebuilt `d1x-3ds.cia` / `d1x-3ds.3dsx` from
> the [Releases](https://github.com/iNdioNicarao/dxx-3ds/releases) page
> (or build them yourself — see *Building*).

---

## Music

The 3DS SDL_mixer port has **no HMP/MIDI decoder**, so the stock tracks
inside `descent.hog` cannot be played directly. To get in-game music you
must supply converted audio files (one-time setup):

1. **Download the Roland SC-55 MP3 Pack (Recommended).** Grab the authentic
   Roland Sound Canvas SC-55 hardware recordings by Brandon Blume from
   [Duke4.net SC-55 Archive](https://sc55.duke4.net/mp3/descent1_mp3.zip)
   (recommended by GBAtemp user **bakuDD**).
   Extract the 27 `.mp3` files directly into `/3ds/D1/mp3/` on your SD card.
   They are pre-named (`game01.mp3` through `game22.mp3`, `briefing.mp3`,
   `credits.mp3`, `descent.mp3`, `endgame.mp3`, `endlevel.mp3`) and work immediately.
2. **Or get an upstream OGG pack.** The upstream DXX-Rebirth project supports
   community **OGG music AddOn packs** for Descent 1 & 2. Drop the extracted
   tracks into `/3ds/D1/ogg/`.
3. **Or convert the game's own tracks.** The HMP tracks are already inside
   `descent.hog`. Render them with **TiMidity++** + a soundfont, then
   encode to MP3/OGG/WAV. Example (per track):
   ```
   timidity game01.hmp -Ow -o - | ffmpeg -i - -b:a 192k game01.mp3
   ```
4. **Install location.** Place the tracks on your SD card matching the format:
   - MP3: `/3ds/D1/mp3/<name>.mp3`
   - OGG: `/3ds/D1/ogg/<name>.ogg`
   - WAV: `/3ds/D1/wav/<name>.wav`
   The 3DS fallback chain automatically tries `wav/` → `mp3/` → `ogg/`.

The `midi/` folder written by some older builds is **not used** for
playback (SDL_mixer can't decode it) — it can be deleted.
For custom playlists, list your own tracks in a `descent.sng` song file in any
SDL_mixer-supported format (`.mp3`, `.ogg`, `.flac`); filenames must match
the song names in the list.

---

## 3DS controls

`START` acts as keyboard `Esc` / in-game menu; `SELECT` opens the full top-screen 3D automap.

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

> **Cockpit views:** full cockpit → status bar → full screen (cycles).
> Top-screen HUD includes precision crosshairs for aiming.

### Bottom-Screen Tri-View Tactical Dashboard

The bottom screen features a synchronized **Tri-View Tactical Dashboard** while you play:

1. **Live 3D Rear-View Mirror (Top Rectangle)**:
   - Full 3D camera rendering looking behind your ship in real time.
   - Centered with an unobstructed wide field of view.
2. **Tactical Mini-Radar (Bottom-Left Square)**:
   - Real-time 360-degree radar sweep showing nearby hostile robots (red), hostages (green), and power-ups (yellow).
   - In-box `+` and `−` touch buttons to independently zoom radar range (15m to 240m).
3. **Mini Wireframe Automap (Bottom-Right Square)**:
   - 3D wireframe overview of the local mine structure and visited rooms.
   - In-box `+` and `−` touch buttons to independently zoom map scale.
4. **Touch Interactions & Controls**:
   - **Tap-to-Fullscreen**: Tap either the radar or minimap square to instantly expand it fullscreen on the bottom screen; tap again to return to Tri-View.
   - **Top Quick Bar**: Tap `HUD` to toggle on-screen gauges, `GYRO` to toggle gyro aim assist on/off, `PRIMARY` to cycle primary weapons, and `SECONDARY` to cycle missiles/bombs.
   - **Bottom Bar**: `MENU` opens the in-game main menu overlay, `SAVE` opens the save-slot selection dialog, and `REC` toggles demo recording.

### Gyroscope Aim Assist & Settings

Gyro aim assist allows fine-tuned motion aiming by tilting the console:
- Toggle gyro on or off anytime via the bottom-screen `GYRO` touch button.
- Configure motion response under `OPTIONS -> CONTROLS -> GYROSCOPE SETTINGS`:
  - **Deadzone (0–16)**: Suppresses resting hand tremors and drift.
  - **Sensitivity (1–16)**: Adjusts rotational speed multiplier.
  - **Calibrate Zero Bias**: Re-centers the zero baseline on demand for your current grip angle.

### Stereoscopic 3D controls (3D-slider-gated, in `master`)
Hold `START` and press:

| Combo           | Action                                  |
|-----------------|-----------------------------------------|
| `START` + `D-UP` / `D-DOWN`   | Stereo separation strength up / down |
| `START` + `ZR`                   | Toggle method: PARALLEL ↔ TOE-IN    |
| `START` + `L` / `R`             | Convergence distance down / up        |

See [`docs/STEREO_3D.md`](docs/STEREO_3D.md) for what parallel vs
toe-in means and how to tune depth live (no rebuild). Settings persist to
the SD card.

---

## What was fixed in this fork

These were broken or missing in the original 3DS port and are now working:

- **`.cia` launch crash (8 MB stack).** Resolved via `__stacksize__` = 8 MB.
- **Death screen hang.** The "press any key" screen was keyboard-only and left
  you stuck after dying. Now dismissable with any controller button / `START`.
- **Quick-load input death.** `state_quick_load()`/`state_quick_save()` were
  called *inside* the event-poll loop, corrupting the window/event stack so
  all joystick input died after a load. Now deferred to run after the poll
  loop returns.
- **Cockpit view cycling** via bare `D-Pad Up`/`D-Pad Down` (no keyboard).
- **Bilinear texture filtering** forced on (PICA200 defaulted to nearest).
- **Level select** uses a d-pad number slider instead of keyboard text entry.
- **Automap boundaries** — picaGL has no `GL_LINES`; edges now draw as
  thin `GL_TRIANGLE_STRIP` quads.
- **Blank top screen / strobing / briefing-banner regressions** (a long chain
  of render-path fixes, v86–v99) — resolved; see commit history.
- **Cheat menu & on-device checkboxes** — accessible via pause menu (`Cheats`),
  with gamepad button `A` toggling checkbox and radio options in place.
- **Streamlined handheld options** — eliminated desktop-specific crash hazards
  in favor of a robust handheld menu (brightness slider, reticle, transparency,
  dynamic lighting, FPS counter, cockpit toggle).

---

## Known issues and missing features

- **Network / multiplayer.** Intentionally disabled (single-player 3DS build).
- **D1 end-of-level flythrough.** Skipped to avoid a crash (`endlevel.c`
  bails before the camera flythrough). Levels still advance correctly.
- **Stereoscopic 3D** — Fully integrated in `master` and hardware slider-gated
  (raise slider to enable, lower to return to mono). Due to rendering the scene
  twice per frame for stereo depth, framerates may modulate in particularly heavy
  geometric scenes.
- **Original (Old) 3DS / 2DS** — untested; see below.

---

## Original (Old) 3DS / 2DS

This port was developed and tested **only on New 3DS hardware**. The main
reasons original-3DS support was not pursued: the original has **fewer
physical buttons** (no ZL/ZR, single circle pad) so several combos would
need a remap pass, and it runs at a lower CPU clock (no `osSetSpeedupEnable`
boost). The two-pass stereo render is especially unlikely to be playable
there. If you want to attempt it, see
[`docs/ORIGINAL_3DS_NOTES.md`](docs/ORIGINAL_3DS_NOTES.md) for the
specific code paths to touch.

---

## Building (Docker)

### Prerequisites
- Docker + the `devkitpro/devkitarm` image (ARM11 cross-toolchain, libctru,
  libphysfs, SDL 1.2 3DS port).
- `bannertool` + `makerom` for `.cia` packaging (provided by the image /
  portlibs).

### Workflow (persistent build container)

A single long-lived container named `dxx-build` is reused so the toolchain
stays warm:

```bash
# one-time: create the build container
docker run -d --name dxx-build -v "$PWD":/src devkitpro/devkitarm sleep infinity

# build D1 (3dsx + elf), then package the CIA
docker exec dxx-build bash -c "cd /src/d1 && rm -rf build && make"
docker exec dxx-build bash -c "cd /src/d1/3ds_data && cp /src/d1/d1x-3ds.elf . && bash make_cia.sh"
```

Outputs: `d1/d1x-3ds.3dsx`, `d1/d1x-3ds.elf`,
`d1/3ds_data/d1x-3ds.cia`.

> **Verify builds by md5.** `makerom` can report `CIA_RC=0` even when
> `make` failed — always confirm `make` succeeded *and* check the `.cia` md5
> before shipping. (Also: under Docker-on-Windows, `make` may skip relinking
> `d1` against a rebuilt `libpicaGL.a` due to mtime skew — do a clean
> rebuild of both when picaGL changes, and verify the resulting ELF contains
> the expected strings.)

### Graphics backend
- **Rendering is hardware-accelerated via picaGL** (`libs/picaGL`, a git
  submodule) — a thin OpenGL-1.1-compatible layer that translates GL calls
  into native **PICA200 GPU** commands. This replaces the desktop OpenGL
  backend used by upstream DXX-Rebirth, which the 3DS has no driver for.
- **SDL is the app framework, not the renderer.** On 3DS, SDL (1.2 port) is
  used only for the event loop, input, audio, and timing. All drawing goes
  through picaGL/GPU.
- Because picaGL lacks `GL_LINES` and `glLineWidth` is a no-op stub, all
  line drawing must use triangle primitives (see the automap fix above).

---

## Repository layout

- `d1/` — Descent I source (`main/`, `arch/ogl/`, `arch/sdl/`, `2d/`,
  `3ds_data/`)
- `libs/picaGL/` — 3DS OpenGL backend (**git submodule**)
- `docs/` — `STEREO_3D.md`, `stereo-3d-logic-map.md`,
  `ORIGINAL_3DS_NOTES.md`
- `COPYING.txt` — license

---

## Acknowledgments

This port was developed by **Dennis Isaac Gutierrez Zeledon** with the assistance of AI coding assistants across its development milestones:

- **v2.0.7 – v2.1.1 (Tri-View Tactical Dashboard, Gyro Aim Assist, SC-55 Audio, Save System & Performance Overhaul):**
  - **Assistant:** **Gemini Antigravity** (Google DeepMind)
  - **Role:** Implementation of the live Tri-View bottom-screen dashboard (3D live rear-view mirror, tactical sweep radar, and wireframe minimap), hardware-calibrated gyroscope aim assist with dedicated options menu, save game screenshot thumbnail system, mid-game menu lifecycle resolution, stereo depth clamping and transitions, 24-bit color dithering, bilinear filtering, 30 FPS decoupled rear-view mirror cadence, SD diagnostic bottleneck elimination, and release hardening.

- **v1.0.0 – v2.0.7 (Initial Port, Stereoscopic 3D & CIA Packaging):**
  - **Assistant:** **Hermes Agent** (Nous Research)
  - **Model:** `tencent/hy3:free` (via OpenRouter)
  - **Role:** Tracing the initial 3DS render/display path, root-causing early stereoscopic-3D bugs, repository preparation and copyright scrub with `git filter-repo`, branch organization, and initial CIA packaging.

- **Community Contributors & Research:**
  - **CrashMidnick (GBAtemp)**: Sincere thanks for continued testing and valuable feedback!
  - **bakuDD (GBAtemp)**: Research and recommendation of the authentic Roland Sound Canvas SC-55 soundtrack recordings from [sc55.duke4.net](https://sc55.duke4.net/mp3/descent1_mp3.zip) by Brandon Blume, providing the definitive high-fidelity audio solution for the 3DS port. Sincere thanks for this great contribution!

---

## License

See [COPYING.txt](COPYING.txt). DXX-Rebirth and this port are distributed
under the GNU General Public License. Descent game data is the property of its
respective owners and is **not** included in this repository.
