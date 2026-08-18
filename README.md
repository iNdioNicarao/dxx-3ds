# D1X 3DS

A Nintendo 3DS port of **Descent I** (D1X), built for the **New 3DS**
family (New 3DS / New 2DS XL). Hardware-rendered via **picaGL** on the
PICA200 GPU, with glasses-free **autostereoscopic 3D** as an opt-in,
experimental feature.

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

## What's new in v2.0.7

This release folds the stereoscopic-3D work into `master` and fixes a batch of
on-device bugs found during hardware testing. Since the previous published
release, the changes are:

- **Stereoscopic 3D is now in `master`** (was `stereo-3d`). Raise the 3DS 3D
  slider to enable depth; lower it for mono. Live controls: `START`+`ZR`
  toggles PARALLEL ↔ TOE-IN, `START`+`D-UP`/`D-DOWN` changes separation,
  `START`+`L`/`R` changes convergence. All overlays force mono so they never
  present to the hidden stereo bank.
- **Power-off crash fixed.** The NDSP audio thread is stopped *before*
  `pglSetPoweredOff()`, so closing the game / powering off no longer ARM11-crashes.
- **On-screen keyboard (OSK).** A 3DS software keyboard is now wired in, so you
  can enter a pilot name and create **multiple named pilots / save slots**
  (the previous release had no text entry, so named pilots / saved games weren't
  possible).
- **Named saves work now.** Pilot saves persist to the SD card; the prior release
  couldn't save them because there was no keyboard to name a pilot.
- **Demo playback & recording.** Demo playback from in-game and demo *recording*
  both work now — neither was supported in the previous release.
- **Demo → New Game banner fixed.** The "Prepare for Descent" banner after a
  demo / other stereo session no longer alternates with (or hides behind) the
  game view.
- **Automap overlay fixed.** Opening the automap over live gameplay with the 3D
  slider up no longer flickers/alternates with the game (it now forces mono for
  its duration).
- **Lid close/open fixed.** Closing the 3DS lid and reopening no longer leaves
  both screens black (stereo GPU state is reset on wake).
- **Quit → New Game fixes.** The simulator no longer stays frozen, flight
  controls are alive on the new game, and quick-load no longer shows black walls
  (texture handles are invalidated / re-uploaded).
- **Build hygiene.** Version is `2.0.7` and the CIA is named `d1x-3ds-2.0.7.cia`;
  the RSF pins a constant `Version : 10903` so the Title ID stays
  `0x000400000FDDEB97` (the build replaces the existing icon instead of adding a
  second one). The graphics backend (`libs/picaGL`) is a pinned submodule fork.

> **Known limitation carried to next release:** the in-game **Cheat menu is
> disabled in v2.0.7** — its checkbox widgets don't respond to touch on-device.
> It will return (fixed) in a later release.

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

**B. 3DSX (Homebrew Launcher, no install)** — use the `d1x-3ds.3dsx` +
`d1x-3ds.smdh` pair from a release:
- Copy **both** files into `sd:/3ds/D1X-3DS/` (keep them together — the
  `.smdh` is the icon/metadata the Homebrew Launcher needs).
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

1. **Get a music pack.** The upstream DXX-Rebirth project ships/supports
   community **OGG music AddOn packs** for Descent 1 & 2 (following the
   DOS song-naming rules). See the DXX-Rebirth site and its GitHub
   \u201cMusic Packs\u201d discussion for the current pack links.
2. **Or convert the game's own tracks.** The HMP tracks are already inside
   `descent.hog`. Render them with **TiMidity++** + a soundfont, then
   encode to MP3/OGG. Example (per track):
   ```
   timidity game01.hmp -Ow -o - | ffmpeg -i - -b:a 192k game01.mp3
   ```
3. **Install.** Drop the files in `/3ds/D1/mp3/` (or `/3ds/D1/ogg/`,
   `/3ds/D1/wav/`), named `game01.mp3`, `game02.mp3`, … matching the
   level songs. The 3DS fallback tries `wav/` first, then `mp3/`.

The `midi/` folder written by some older builds is **not used** for
playback (SDL_mixer can't decode it) — it can be deleted.
For custom music, list your own tracks in a `dxx.sng` song file in any
SDL_mixer-supported format (`.mp3`, `.ogg`, `.flac`); filenames must match
the song names in the list.

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
| Cycle cockpit view        | D-Pad Up (prev) / D-Pad Down (next)         |
| Weapon prev / next        | `D-LEFT` / `D-RIGHT`                        |
| Quick save               | `START` + `X`                                |
| Quick load               | `START` + `Y`                                |
| Controls help            | `START` + `SELECT`                           |
| Move / aim              | Circle Pad (analog) + C-Stick                 |
| Death screen dismiss     | any face button / `START`                     |

> **Cockpit views:** full cockpit → status bar → full screen (cycles).
> FPS is shown on the death screen regardless of cockpit mode.

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

---

## Known issues and missing features

- **Network / multiplayer.** Intentionally disabled (single-player 3DS build).
- **D1 end-of-level flythrough.** Skipped to avoid a crash (`endlevel.c`
  bails before the camera flythrough). Levels still advance correctly.
- **Stereoscopic 3D** — experimental but now **in `master`** (3D-slider-gated).
  Renders the scene twice per frame → roughly half FPS in heavy scenes with the
  slider up.
- **Original (Old) 3DS / 2DS** — untested; see below.
- **Cheat menu** — disabled in v2.0.7. Its `newmenu` checkbox widgets can't be
  toggled on-device (the cheat menu is on the top screen, which has no touch —
  the toggle relies on input the current build doesn't route to those widgets).
  Re-enabled in a later release once the toggle path is fixed.
- **Changing graphical settings** — toggling in-game graphical options (cockpit
  mode, transparency/lighting effects, etc.) may cause on-screen graphical
  corruption. Reload the level or restart the app to clear it.

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

This port — including the stereoscopic-3D work, the public-release history
scrub (removing copyrighted game data, debug artifacts, and the Descent II
tree), the documentation, and the build/release tooling — was developed with
the assistance of **Hermes Agent**, an AI coding assistant.

- **Assistant:** Hermes Agent ([Nous Research](https://nousresearch.com))
- **Model used:** `tencent/hy3:free` (via OpenRouter)
- **How it was used:** end-to-end — tracing the 3DS render/display path,
  root-causing and fixing the stereoscopic-3D bugs, preparing the repository
  for public release (copyright/large-file scrub with `git filter-repo`,
  branch organization, README + `docs/`), and building/packaging the CIA.



---

## License

See [COPYING.txt](COPYING.txt). DXX-Rebirth and this port are distributed
under the GNU General Public License. Descent game data is the property of its
respective owners and is **not** included in this repository.
