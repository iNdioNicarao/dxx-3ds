# D1X 3DS v2.1.1 — Release Notes

**D1X-Rebirth (Descent I)** port for the Nintendo 3DS family (New 3DS / New 2DS XL recommended).

---

## Highlights in v2.1.1

### Performance Overhaul & Fluid 60 FPS Flight
* **Decoupled Rear-View Mirror Cadence**:
  * The live 3D rear-view mirror rendering on the bottom tactical dashboard is now decoupled to an alternate-frame cadence (30 FPS mirror, 60 FPS flight).
  * Drastically reduces GPU fillrate demand and CPU scene traversal overhead while keeping rear situational awareness completely fluid.
* **Rear-View Depth Culling**:
  * Enforced an optimal depth culling limit (`Render_depth` capped to 18) for the rear-view viewport, eliminating costly overdraw of distant room segments.
* **Elimination of SD Card Diagnostic Logging Bottlenecks**:
  * Resolved severe framerate drops (down to 36 FPS or lower) reported by users with large SD cards (256GB / 512GB).
  * Removed synchronous per-frame diagnostic file operations (`lum_trace.txt` and `pgl_trace.txt`) and frame-buffer pixel hashing loops from the low-level rendering pipeline.
* **Bottom-Screen Latency Decoupling**:
  * Removed redundant VBlank synchronization stalls during bottom-screen presentations, ensuring clean frame pacing and jitter-free rendering on the top screen.
* **Compiler & Math Stability**:
  * Standardized clean `-O2` compiler optimizations to maintain strict struct alignment, avoid ABI mismatches, and preserve fixed-point vector math accuracy.

---

## Downloads

* `d1x-3ds-2.1.1.cia` — install via FBI / title manager (recommended for 3DS Home Menu)
* `d1x-3ds-2.1.1.3dsx` / `d1x-3ds.3dsx` — run directly via the Homebrew Launcher

**Title ID**: `0x000400000FDDEB97`

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

## Credits & Acknowledgments

* **Port & 3DS Enhancements**: Dennis Isaac Gutierrez Zeledon
* **AI Coding Partner**: Gemini Antigravity (Google DeepMind)
* **Initial 3DS Port Base & Stereo 3D**: Hermes Agent (Nous Research) & Parallax Software / DXX-Rebirth team
* **Community Contributors & Research**:
  * **CrashMidnick (GBAtemp)**: Sincere thanks for continued testing and valuable feedback!
  * **bakuDD (GBAtemp)**: Research and recommendation of the authentic Roland Sound Canvas SC-55 soundtrack recordings from [sc55.duke4.net](https://sc55.duke4.net/mp3/descent1_mp3.zip) by Brandon Blume, providing the definitive high-fidelity audio solution for the 3DS port. Sincere thanks for this great contribution!
