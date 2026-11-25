# Aim Trainer (C++17 + raylib)

Light 3D aim trainer. Bots wander to random points in the arena (never a fixed route), with random
speed, pauses and strafing. Everything visual and audible is configurable.

## Build

    cmake -B build
    cmake --build build -j
    ./build/aimtrainer        # assets are copied next to the binary
    ./build/sim_test          # logic tests, no GPU needed

Linux needs the usual raylib build deps (X11/GL dev packages). raylib 5.5 is downloaded automatically.

## Customize (no recompiling)

| What | Where |
|---|---|
| Sensitivity, cm/360, FOV, fps, vsync | `config.ini` `[window]` `[mouse]` |
| Weapon damage, rpm, spread, auto | `[weapon]` |
| Bot speed, hp, size, wander behaviour, colour | `[bots]` |
| Bot model | drop `models/bot.glb` |
| Aura: outline, rim glow, pulse, rainbow, through walls, hit flash | `[aura]` |
| Tracer | `[tracer]` |
| Hit marker, crosshair | `[hitmarker]` `[crosshair]` |
| Sky colours or panorama image | `[world]`, `sky/panorama.png` |
| Maps | `maps/*.map`, set `[world] map =` |
| Sounds | `sounds/shot hit headshot kill death` (.wav .ogg .mp3 .flac) |

F5 reloads config, models, sounds and map while the game runs.
