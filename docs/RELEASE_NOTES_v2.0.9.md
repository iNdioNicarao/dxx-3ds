D1X-Rebirth (Descent) port for the Nintendo 3DS — New 3DS recommended.

## What's new in 2.0.9
- **Bottom-screen minimap (live tactical radar).** The bottom screen now shows
  an always-on minimap of the current level while you play, separate from the
  full-screen `SELECT` automap (which remains the big study map). It draws room
  outlines (not every segment edge), gold doors, nearby object blips (hostages,
  power-ups, robots, players) with short labels, and a heading tick showing
  where you're facing. Drag on the map to rotate the view (confined to the map
  area so the edge buttons keep working); double-tap to recenter on your
  position. Only objects within a local radius are shown, to keep it readable.
  Updates at ~30 Hz.
- **Gentler stereoscopic 3D.** The 3DS 3D slider produced eye separation far too
  strong even at the minimum. Maximum separation is now much gentler (scaled
  0.35 -> 0.12) and hard-capped so depth stays comfortable at any slider
  position.
- **Music: no more MIDI extraction; user-supplied tracks.** The 3DS SDL_mixer
  port has no HMP/MIDI decoder, so the old `midi/` extraction was dead code. It
  is removed. In-game music now plays user-supplied audio from `mp3/`, `ogg/`,
  or `wav/` (named `gameNN`), trying `wav/` then `mp3/`. Get tracks from a
  DXX-Rebirth music AddOn pack, or convert the game's own HMP tracks from
  `descent.hog` with TiMidity++ + a soundfont (e.g.
  `timidity game01.hmp -Ow -o - | ffmpeg -i - -b:a 192k game01.mp3`). The old
  `midi/` folder can be deleted.

## Downloads
- d1x-3ds-2.0.9.cia  — install via FBI / a title manager
- d1x-3ds-2.0.9.3dsx — run from the homebrew menu (no install)

Title ID: 0x000400000FDDEB97
