# DXX 3DS

A Nintendo 3DS port of **Descent I** & **Descent II**, built for the New 3DS
(N3DS) family. This is a fork of [DXX-Switch](https://github.com/aagallag/DXX-Switch)/[DXX-Retro](https://github.com/CDarrow/DXX-Retro), which is a fork of [DXX-Rebirth](https://github.com/dxx-rebirth/dxx-rebirth), which is a fork of the original engine by Parallax Software.

> **Status:** Actively maintained on the `working-3ds-port` branch. The `.cia`
> builds are working (stack-size crash resolved). See *Known issues* below for
> what is still open.

---

## Install instructions

### Descent I (D1X)
- Create the folder `/3ds/D1/` on your SD card.
- Copy `descent.hog` and `descent.pig` into it. These ship with a legitimate
  purchase of the game. Tested with [Descent I from Steam](https://store.steampowered.com/app/273570/Descent/).
- Install `d1x-3ds.cia` with your CIA installer (FBI) and launch from the home menu.

### Descent II (D2X)
- Create the folder `/3ds/D2/` on your SD card.
- Copy `descent2.ham`, `descent2.hog`, `descent2.s22`, and all `*.pig` files.
  Tested with [Descent II from Steam](https://store.steampowered.com/app/273580/Descent_2/).
- Install `d2x-3ds.cia` with FBI.

### Save games / pilot
- Pilot progress lives in `player.plr` on the SD card (single slot).
- **Quick save:** `START` + `X` → writes `.qusg1`.
- **Quick load:** `START` + `Y` → restores `.qusg1`.

---

## Music

- The 3DS cannot play the game's HMP→MIDI music directly.
- On first run, MIDI files are copied to `/3ds/D1/midi/` and `/3ds/D2/midi/`.
- Convert them to WAV (e.g. TiMidity + FluidR3 soundfont) and drop the results
  in `/3ds/D1/wav/` and `/3ds/D2/wav/`.
- The game also plays MP3s from `/3ds/D1/mp3/` and `/3ds/D2/mp3/` (e.g. the GoG
  release ships MP3s) — filenames must match the MIDI song names.
- Playback priority: `wav` → `mp3` → (then MIDI if a player is present).

---

## 3DS controls

The port maps a standard controller layout to the 3DS buttons. `START` acts as
the keyboard `Esc`/back; `SELECT` is the automap.

| Action | Button |
| --- | --- |
| Fire primary | `R` |
| Fire secondary / missile | `L` |
| Accelerate (afterburner) | `X` |
| Reverse / brake | `B` |
| Slide left | `Y` |
| Slide right | `A` |
| Drop bomb | `L` (hold context) |
| Flare | `ZR` |
| Rear view (hold) | `ZL` |
| Automap | `SELECT` |
| Menu / pause / back | `START` (Esc) |
| Cycle cockpit view | hold `SELECT` + `D-UP` (next) / `D-DOWN` (prev) |
| Weapon prev / next | `D-LEFT` / `D-RIGHT` |
| Quick save | `START` + `X` |
| Quick load | `START` + `Y` |
| Controls help | `START` + `SELECT` |
| Move / aim | Circle Pad (analog) + C-Stick |
| Death screen dismiss | any face button / `START` (was keyboard-only; now controller-dismissable) |

> **Cockpit views:** full cockpit → status bar → full screen (cycles). FPS is
> shown on the death screen regardless of cockpit mode.

---

## What has been fixed in this fork

These were broken or missing in the original 3DS port and are now working:

- **`.cia` crash (8 MB stack).** Resolved via `__stacksize__` = 8 MB; CIAs no
  longer crash on launch.
- **Death screen hang.** The "press any key" screen was keyboard-only and left
  you stuck after dying. Now dismissable with any controller button / `START`.
- **Death-screen FPS.** FPS indicator now renders on the death screen (it was
  suppressed when rear-view was active at death).
- **Quick-load input death.** `state_quick_load()`/`state_quick_save()` were
  called *inside* the event-poll loop, corrupting the window/event stack so all
  joystick input died after a load (only keyboard survived). They are now
  deferred and run after the poll loop returns. All buttons + sticks work after
  quick-load.
- **Cockpit view cycling.** Added `SELECT` + `D-UP`/`D-DOWN` to cycle cockpit
  modes (no keyboard needed).
- **Bilinear texture filtering.** The PICA200 defaulted to nearest (blocky);
  the 3DS now forces bilinear (`TexFilt=1`). Mipmaps intentionally left off
  (picaGL does not generate mip levels).
- **Level select.** The `levelwarp` cheat used keyboard text entry; it now uses
  a d-pad number slider (1..Last_level) so it works on 3DS.
- **Automap boundaries.** picaGL has **no `GL_LINES` support** (its
  `glDrawArrays` only handles TRIANGLES / TRIANGLE_FAN / TRIANGLE_STRIP, and
  `glLineWidth` is a no-op stub). Edge lines were drawn as degenerate invisible
  triangles. `ogl_draw_line_vec()` now emits each edge as a thin
  `GL_TRIANGLE_STRIP` quad, so segment boundaries render on the automap.
  (The HUD reticle is a bitmap and was unaffected.)

---

## Known issues and missing features

Open items (not all are blockers):

- **Multiple named saves.** Still a single `player.plr` slot. Named pilots /
  multiple save slots need text entry (a bottom-screen keyboard) — not yet built.
- **Original (classic) HUD.** The classic Descent HUD style is not yet verified
  as selectable/renderable on 3DS.
- **Network / multiplayer.** Intentionally disabled (single-player 3DS build).
- **D1 end-of-level flythrough.** Skipped to avoid a crash
  (`endlevel.c` bails before the camera flythrough). Levels still advance
  correctly; only the cinematic is missing.
- **D2 movies.** Disabled (they crashed on 3DS). D1 has no movies, so this only
  affects D2.
- **800×240 "high-res" mode.** Not beneficial — the PICA200 framebuffer is
  fixed at 400×240; the 800-wide top LCD upscales it. There is no true 2× render.
- **Stereoscopic 3D.** Feasible but expensive: requires rendering the scene
  twice (IPD-offset cameras) → roughly half FPS on the weak PICA200. Not planned
  for mainline.
- **Only Steam data files tested.** GoG / CD-ROM releases untested but expected
  to work.

---

## Building

### Prerequisites
- Docker + the `devkitpro/devkitarm` image (provides the ARM11 cross-toolchain,
  libctru, libphysfs, SDL 1.2 3DS port).
- `bannertool` + `makerom` (for `.cia`; included via the image / portlibs).

### Workflow used in this repo (Docker, persistent container)

A single long-lived container named `dxx-build` is reused (not `--rm`'d) so the
toolchain stays warm:

```bash
# one-time: create the build container
docker run -d --name dxx-build -v "$PWD":/src devkitpro/devkitarm sleep infinity

# build D1 (3dsx + elf), then package the CIA
docker exec dxx-build bash -c "cd /src/d1 && rm -rf build && make"
docker exec dxx-build bash -c "cd /src/d1/3ds_data && cp /src/d1/d1x-3ds.elf . && bash make_cia.sh"
```

Outputs: `d1/d1x-3ds.3dsx`, `d1/d1x-3ds.elf`, `d1/3ds_data/d1x-3ds.cia`
(and the D2 equivalents under `d2/`).

> **CIAs are verified by md5 after packaging** — `makerom` can report
> `CIA_RC=0` even when `make` failed, so always confirm the build succeeded
> (`MAKE_RC=0`) *and* check the `.cia` md5 before shipping.

### Graphics backend
- **Rendering is hardware-accelerated via picaGL** (`libs/picaGL`), a thin
  OpenGL-1.1-compatible layer that translates GL calls into native **PICA200 GPU**
  commands. The framebuffer is presented to the top screen (`pglSwapBuffers()`).
  This replaces the desktop OpenGL backend used by upstream DXX-Rebirth, which
  the 3DS has no driver for.
- **SDL is the app framework, not the renderer.** On 3DS, SDL (1.2 port) is used
  only for the **event loop, input (buttons/analog), audio, and timing**. It
  creates a dummy software surface purely to satisfy SDL init; all actual
  drawing goes through picaGL/GPU. (SDL's own software renderer is not used.)
- Because picaGL lacks `GL_LINES` and `glLineWidth` is a no-op stub, all line
  drawing must use triangle primitives (see the automap fix above).

---

## Repository layout

- `d1/` — Descent I source (`main/`, `arch/ogl/`, `arch/sdl/`, `2d/`, `3ds_data/`)
- `d2/` — Descent II source (mirrors `d1/`)
- `libs/picaGL/` — 3DS OpenGL backend (git submodule)
- `buildNotes.MD` — detailed per-build changelog (v29–v33) and debugging notes
- `tools_in_use.md` — dev tooling reference (crash-dump parser, toolchain, etc.)

---

## License

See [COPYING.txt](COPYING.txt). DXX-Rebirth and this port are distributed under
the terms of the GNU General Public License; Descent game data is property of
its respective owners and is not included.
