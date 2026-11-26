# Aim Trainer (C++17 + raylib)

Developer: yatuma. All rights reserved, see LICENSE.

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
| Weapons (keys 1/2), damage, rpm, spread, pellets, ADS delay in ms | `[weapon1]` `[weapon2]` |
| Bot speed, hp, size, wander behaviour, colour | `[bots]` |
| Bot model | drop `models/bot.glb` |
| Aura: outline, rim glow, pulse, rainbow, through walls, hit flash | `[aura]` |
| Tracer | `[tracer]` |
| Hit marker, crosshair | `[hitmarker]` `[crosshair]` |
| Sky colours or panorama image | `[world]`, `sky/panorama.png` |
| Movement speed, accel/decel, A/D gap readout colours | `[movement]` |
| Maps | in-game editor (F1) or `maps/*.map`, set `[world] map =` |
| Sounds | `sounds/shot hit headshot kill death` (.wav .ogg .mp3 .flac) |

F5 reloads config, models, sounds and map while the game runs.

## Controls

WASD move (the A/D gap in ms is shown next to the crosshair), LMB fire, RMB aim down sights, 1/2 switch weapon,
R reset stats, F5 reload config, F1 map editor, ESC settings menu.

## Settings menu (ESC)

Left column: sensitivity, cm/360, mouse DPI, field of view, master volume, ADS delay of the current weapon, your
walk speed. Right column: number of bots, bot HP, bot walk speed (slowest / fastest) and respawn time. Hover a slider and use the mouse wheel for single steps. "Save settings" writes them to `config.ini`.

## Map editor (F1)

Top-down view. 1 = round pillar, 2 = rectangular block (Tab switches). LMB place, RMB delete, mouse wheel = size
(Shift = height, Ctrl = block depth), G snap to 0.5 m, Ctrl+Z undo, C clear, - / = arena size, WASD pan, Q/E zoom,
F6 save to the map file. Maps are plain text (`size`, `pillar`, `box` lines) so they are easy to share.

## Characters

The built-in bot is a jointed mannequin (head, torso, arms, legs, hands, shoes) that swings its limbs when it
walks. To use your own, drop a `.glb` as `models/bot.glb`: it is scaled to the bot height and keeps its textures.
