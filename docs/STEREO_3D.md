# Stereoscopic 3D on the 3DS (EXPERIMENTAL)

> **Status: EXPERIMENTAL.** This feature lives on the `stereo-3d` branch
> only. It is *not* in the stable `master` build. It works (verified on
> hardware: depth is perceptible, near objects pop), but it is unfinished
> and has known sharp edges documented under "Limitations" below. Use at
> your own risk.

This document explains **what the stereoscopic 3D does, the two different
methods it can use, and how to tune it live on-device** without
recompiling. The developer-side trace of the code paths lives in
`docs/stereo-3d-logic-map.md`.

## What it is
The 3DS has an autostereoscopic (glasses-free) top screen. To use it,
the game renders the scene **twice per frame** — once for the left eye and
once for the right eye — with the two eyes slightly offset (inter-pupillary
distance, "IPD"). The 3DS GPU presents both to the left/right banks of
the top screen and the parallax between them is what your brain fuses into
depth.

- The 3D **slider** on the side of the 3DS is the master on/off and
  fine-strength control. Slider down = mono (single image). Slider up =
  full stereo. (Off-detent reads a small non-zero value, so you are
  effectively always in the stereo path once the slider is nudged up.)
- `gfxSet3D(true)` / `pglSetStereo(true)` are engaged live when the
  slider goes up, and released when it goes down.

## The two eye-computation methods
This is the part most people don't realize they can choose between. The
"eye offset" can be applied in two mathematically different ways, and they
**look different**:

### 1. PARALLEL (default, method = 0)
Each eye is shifted sideways by a fixed world distance and **both look in the
same direction** (parallel axes).

- Signature: **near objects show strong 3D, far objects look flat.** This
  is expected — with parallel axes the parallax of an object is inversely
  proportional to its distance, so distant geometry barely separates.
- It also introduces a constant horizontal image shift you have to fuse.
- This is the "classic camera pair" look.

### 2. TOE-IN / CONVERGED (method = 1)
Each eye is **rotated toward a fixed convergence point** instead of (only)
being translated. Objects at the convergence distance land on the same
screen pixel (zero parallax there), so depth reads **more evenly from near
to far** — the classic game/cinema stereo look.

- This directly addresses the "far stuff looks flat" complaint about parallel.
- The convergence distance (`g_stereo_conv`) sets *where the flat anchor
  plane sits*.
- Implementation note: the per-eye yaw is computed with the project's own
  `fix_atan2()` (single-precision `atan2f` is NOT available in the
  linked newlib on this toolchain — using it produced a degenerate view
  matrix and hung the GPU). See `docs/stereo-3d-logic-map.md`.

**You can switch between these live and feel the difference immediately.**

## Live tuning (no rebuild)
While **in-game**, hold **START** and press:

| Combo            | Action                                             |
|------------------|----------------------------------------------------|
| START + D-PAD UP | Stereo separation strength: **up** (4→20%)       |
| START + D-PAD DOWN | Stereo separation strength: **down** (20→4%) |
| START + ZR       | Toggle method: **PARALLEL ↔ TOE-IN**                |
| START + L       | Convergence distance: **down** (800→50 world units) |
| START + R       | Convergence distance: **up** (50→800 world units)   |

- **Separation strength** presets: `4, 6, 8, 10, 12, 15, 20` (% of
  `F1_0` at full slider). Default **8%**. This is the max eye offset at
  full slider; the actual offset scales with the hardware slider.
- **Convergence** presets: `50, 100, 200, 400, 800` (world units).
  Default **200**. Only meaningful in TOE-IN mode.
- Every change shows a HUD message and is **persisted to the SD card**
  (`sdmc:/3ds/d1/stereo_sep.cfg`) so your choice survives reboots.

### Suggested starting recipe
1. Slider up.
2. **START + ZR** → TOE-IN.
3. **START + L/R** → convergence ~200.
4. **START + D-PAD UP/DN** → separation ~8–10%.
5. Compare against PARALLEL (START+ZR again) to feel the difference.

If TOE-IN looks **cross-eyed** (eyes converged the wrong way), that is a
one-line sign flip in `d1/main/render.c` (`fix_atan2(conv_fix, half)`
→ swap the arguments) — file an issue.

## How the separation scales
`eye_offset = F1_0 * (g_stereo_sep_pct / 100) * slider`

- At off-detent (slider ≈ 0.043) the eye offset is tiny → almost mono.
- At full slider (1.0) the offset is `g_stereo_sep_pct`% of `F1_0`.
- So strength + hardware slider compound: set a comfortable strength, then
  fine-tune the *feel* with the physical slider.

## Limitations / known issues
- **Performance:** two full scene renders per frame. On the New 3DS this
  holds up at the small 400×240 top resolution, but expect roughly half
  the framerate versus mono in heavy scenes.
- **TOE-IN default off:** ships as PARALLEL because it is the better-
  understood / more robust path; TOE-IN is the more "cinematic" option.
- **No separate D2 build:** stereoscopic 3D was only ever developed for
  Descent 1 (`d1/`). Descent 2 (`d2/`) is not part of this port's
  public release.
- **Hard lockup risk if math is wrong:** a degenerate view matrix can hang
  the PICA200 GPU (requiring a power-off). The current code clamps the
  computed eye yaw to ±15° as a guard. Do not bypass that clamp.
- **Not on original (Old) 3DS:** see `docs/ORIGINAL_3DS_NOTES.md` —
  the two-pass render is unlikely to be playable on the weaker Old-3DS CPU.

## For developers
- Code paths: `d1/main/gamerend.c` (`game_render_frame_mono`, the
  `g_stereo_*` globals + combo handlers' effects), `d1/main/render.c`
  (`render_frame`, the eye computation), `d1/arch/sdl/event.c` (the
  START+combo dispatch), `libs/picaGL/source/picaGL.c` (`pglSwapBuffers`
  stereo branch — single swap per eye-pair).
- Full logic map + stability rules: `docs/stereo-3d-logic-map.md`.
