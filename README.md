# D1X 3DS

A Nintendo 3DS port of **Descent I** (D1X), built for the **New 3DS**
family (New 3DS / New 2DS XL). Hardware-rendered via **picaGL** on the
PICA200 GPU, with glasses-free **autostereoscopic 3D** as an opt-in,
experimental feature.

This is a fork of [DXX-Switch](https://github.com/aagallag/DXX-Switch) /
[DXX-Retro](https://github.com/CDarrow/DXX-Retro), which is a fork of
[DXX-Rebirth](https://github.com/dxx-rebirth/dxx-rebirth), which is a fork
of the original engine by Parallax Software.

> **Scope of this public release:** **Descent I only.** Descent II (`d2/`)
> was part of earlier development but is **not** included here — it was never
> test-driven on hardware and is omitted to keep the release clean and
> honest about what works. See *Original (Old) 3DS* below for hardware notes.

---

## Branches

| Branch        | What it is                                                            |
|----------------|-------------------------------------------------------------------------|
| `master`      | **Stable, mono (2D) Descent I.** The playable, recommended build.  |
| `stereo-3d`  | **EXPERIMENTAL** Descent I **with stereoscopic 3D** enabled.   |

`master` is the one to build/install for normal play. `stereo-3d` carries
the 3D work documented in [`docs/STEREO_3D.md`](docs/STEREO_3D.md) —
it works (depth is perceptible on hardware) but is unfinished; use at your own
risk. The developer-side trace of that code lives in
[`docs/stereo-3d-logic-map.md`](docs/stereo-3d-logic-map.md).

---

## Install (Descent I)

You must **supply your own Descent I game data** — it is copyrighted and is
**not** included in this repo (and was scrubbed from history before publish).

1. Create `/3ds/D1/` on your SD card.
2. Copy **`descent.hog`** and **`descent.pig`** into it. These ship with a
   legitimate purchase of the game — tested with
   [Descent I on Steam](https://store.steampowered.com/app/273570/Descent/).
   (Other releases — GOG, CD-ROM — should work but are untested here.)
3. Install `d1x-3ds.cia` with FBI and launch from the home menu.

> **No `.cia` in the repo.** Build it yourself (see *Building*), or obtain a
> build from a release. The repo ships source + a Docker build flow, not binaries.

---

## Music

The 3DS can't play the game's HMP→MIDI music directly.

- On first run, MIDI files are copied to `/3ds/D1/midi/`.
- Convert them to WAV (e.g. TiMidity + FluidR3 soundfont) and drop the
  results in `/3ds/D1/wav/`.
- The game also plays MP3s from `/3ds/D1/mp3/` (e.g. the GOG release
  ships MP3s) — filenames must match the MIDI song names.
- Playback priority: `wav` → `mp3` → (then MIDI if a player is present).

---

## 3DS controls

`START` acts as the keyboard `Esc`/back; `SELECT` is the automap.

| Action                    | Button                                        |
|---------------------------|-----------------------------------------------|
| Fire primary              | `R`                                           |
| Fire secondary / missile  | `L`                                           |
| Accelerate (afterburner) | `X`                                           |
| Reverse / brake           | `B`                                           |
| Slide left               | `Y`                                           |
| Slide right              | `A`                                           |
| Drop bomb                | `L` (hold context)                            |
| Flare                    | `ZR`                                          |
| Rear view (hold)         | `ZL`                                          |
| Automap                  | `SELECT`                                      |
| Menu / pause / back      | `START` (Esc)                                |
| Cycle cockpit view        | `START` + `D-UP` (next) / `D-DOWN` (prev)   |
| Weapon prev / next        | `D-LEFT` / `D-RIGHT`                        |
| Quick save               | `START` + `X`                                |
| Quick load               | `START` + `Y`                                |
| Controls help            | `START` + `SELECT`                           |
| Move / aim              | Circle Pad (analog) + C-Stick                 |
| Death screen dismiss     | any face button / `START`                     |

> **Cockpit views:** full cockpit → status bar → full screen (cycles).
> FPS is shown on the death screen regardless of cockpit mode.

### Stereoscopic 3D controls (only on `stereo-3d`)
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
- **Cockpit view cycling** via `START` + `D-UP`/`D-DOWN` (no keyboard).
- **Bilinear texture filtering** forced on (PICA200 defaulted to nearest).
- **Level select** uses a d-pad number slider instead of keyboard text entry.
- **Automap boundaries** — picaGL has no `GL_LINES`; edges now draw as
  thin `GL_TRIANGLE_STRIP` quads.
- **Blank top screen / strobing / briefing-banner regressions** (a long chain
  of render-path fixes, v86–v99) — resolved; see commit history.

---

## Known issues and missing features

- **Multiple named saves.** Still a single `player.plr` slot. Named pilots /
  multiple save slots need a bottom-screen keyboard — not yet built.
- **Network / multiplayer.** Intentionally disabled (single-player 3DS build).
- **D1 end-of-level flythrough.** Skipped to avoid a crash (`endlevel.c`
  bails before the camera flythrough). Levels still advance correctly.
- **800×240 "high-res" mode.** Not beneficial — the PICA200 framebuffer is
  fixed at 400×240; the 800-wide top LCD upscales it.
- **Stereoscopic 3D** — experimental (see above). Renders the scene twice
  per frame → roughly half FPS in heavy scenes. **Not in `master`.**
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

## License

See [COPYING.txt](COPYING.txt). DXX-Rebirth and this port are distributed
under the GNU General Public License. Descent game data is the property of its
respective owners and is **not** included in this repository.
