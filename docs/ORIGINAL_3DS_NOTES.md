# Original Nintendo 3DS (Old 3DS / 2DS) Support — Hints for Future Maintainers

This port was developed and **tested only on the New Nintendo 3DS** (and
New 2DS XL). The stable `master` branch is therefore "New-3DS-only" in
practice. This document records *why*, and *what would have to change*
to bring the original (Old) 3DS / 2DS hardware up.

## Why it was scoped to New 3DS
Two practical reasons drove the decision to skip original-3DS support:

1. **Fewer physical buttons (the main reason).** The original 3DS has
   **one** C-stick-less circle pad and a smaller shoulder set (L/R only;
   no ZL/ZR). This port's control scheme leans on ZL/ZR for several
   combos (benchmark toggle, stereo method toggle, convergence adjust)
   and on the circle pad + C-stick for analog aim. On original hardware
   those bindings have nowhere to go without a remap pass. The maintainer
   chose not to take on that remap work — it is pure UI/input plumbing,
   not a capability gap.
2. **CPU clock.** `arch/sdl/init.c` calls
   `osSetSpeedupEnable(true)` to engage the New 3DS's higher clock. The
   Old 3DS cannot do this; it runs at the lower ~268 MHz. Whether the
   picaGL single-pass render (mono) holds 30–60 fps there is unknown
   and was never measured. The stereo (two-pass) path almost certainly
   will *not* on Old hardware — see below.

## What to change for original-3DS support
If you want to attempt it, the work is mostly input remapping plus a
performance reality-check:

- **Input (`d1/arch/sdl/event.c`, `key.c`, `joy.c`):** re-bind the
  ZL/ZR-dependent combos (benchmark, stereo method, convergence) onto
  buttons that exist on original hardware (e.g. START+SELECT, or L/R
  double-tap). The `btn_map[]` table and the START-combo block are the
  two places to touch. Be aware the cockpit-cycle (START+D-UP/DN) and
  weapon-cycle (D-L/R) already use only original-3DS buttons, so those
  are fine.
- **Clock (`d1/arch/sdl/init.c`):** `osSetSpeedupEnable(true)` is a
  no-op / returns error on Old 3DS — it won't crash, but you lose the
  boost. Consider gating it behind a `osGetSpeedupMode()` /
  `APT_GetAppCpuTime` check, or just accept the lower clock.
- **Performance:** mono mode is the only realistic target on Old hardware.
  The stereoscopic 3D path renders the scene **twice per frame**
  (see `docs/STEREO_3D.md`); on the weaker Old-3DS CPU+GPU that is
  likely unplayable. If you enable stereo there at all, expect to drop
  the render resolution and/or the separation strength substantially.
- **Memory / stack:** the CIA RSF requests an 8 MB stack
  (`d1/3ds_data/d1x-3ds.rsf`, `0x00800000`). Original-3DS has the
  same 128 MB FCRAM, so this is probably fine, but it was only ever
  validated on New hardware.

## What is NOT a blocker
- The picaGL library and citro3d calls used here are standard and work on
  all 3DS models. There is no New-3DS-only GPU feature in the render
  path.
- The autostereoscopic display (`gfxSet3D`) is hardware on every 3DS,
  so the *display* side of stereo is model-agnostic.

## Status
Untested. If you get it running on original hardware, a PR updating this
file with real fps numbers and the final button map would be welcome.
