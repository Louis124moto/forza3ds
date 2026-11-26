# Forza3DS

Native 3D driving-game demo for **New Nintendo 3DS / New Nintendo 3DS XL**, written in C
with devkitARM, libctru and Citro3D.

> **Not yet validated on a physical console.** The game logic is covered by host tests and
> the 3DSX builds, but display, sound, controls and performance still have to be checked on
> a real New 3DS (checklist below).

## What is implemented

- Main menu: Mode Libre, Mode Progression (information screen only), settings, quit.
- Four cars (rear-wheel coupé, turbo 4x4, small front-wheel hatchback, V8), all available.
- 3D garage and workshop: engine and parts, turbo, rev limit, gearbox (up to 10 gears),
  three differentials, suspension per axle, six tyre types, brakes, paint and rim colour.
  Changes apply immediately to physics and sound; no tuning ceiling on boost or revs.
- Driving: tyre-force model with load transfer, asphalt / gravel / grass, handbrake,
  automatic or manual gearbox, optional ABS and traction control.
- Demo road with barriers, posts, crates, trees, torii gates, ramps and gravel zones.
- Chase camera (C-stick orbit, adjustable distance and height) and cockpit camera with
  steering wheel and dials.
- Lower screen: speed, rev gauge, gear, boost, surface, aid lamps, map.
- Procedural engine, wind and tyre sound; muffled in the cockpit; separate volumes.
- Settings (bindings, camera, aids, simulation rate, 30/60 FPS limit, FPS counter, HUD,
  draw distance, shadows, volumes) saved automatically; Mode Libre garage saved on request.
- Pause, and automatic pause when the lid is closed or HOME is pressed.

## Controls (default)

| Control | Action |
|---|---|
| Circle Pad | Steering |
| A | Accelerator (hold B at rest to reverse in automatic) |
| B | Brake |
| Y | Handbrake |
| L / R | Downshift / upshift (manual gearbox) |
| C-stick | Camera orbit |
| X | Chase / cockpit view |
| SELECT | Map |
| START | Pause |
| R (garage) | Rev the engine |

Driving buttons can be reassigned in the settings. In menus: D-pad or Circle Pad to move,
left/right to adjust, A to confirm, B to go back.

## Build

Install devkitPro with the 3DS packages (devkitARM, libctru, picasso, Citro3D), then:

```sh
make            # forza3ds.3dsx
make PROFILE=1  # same, with timing log in sdmc:/3ds/forza3ds/perf.csv
```

Copy `forza3ds.3dsx` to `sdmc:/3ds/` and start it from the Homebrew Launcher.
Sound needs the DSP firmware dump `sdmc:/3ds/dspfirm.cdc`.

## Test

```sh
git clone https://github.com/devkitPro/citro3d.git /tmp/citro3d
CITRO3D_DIR=/tmp/citro3d tests/run.sh
```

GitHub Actions builds the 3DSX (uploaded as an artifact) and runs these tests.

## Console checklist (to do on a New 3DS)

1. The game starts, both screens display, and it quits cleanly.
2. Menu → car → workshop → drive → pause → workshop.
3. Circle Pad steers both ways at low and high speed; braking, reverse, handbrake, collisions.
4. Chase and cockpit views; no geometry crossing the camera.
5. Engine sound without crackle; changes with parts, revs and cockpit view.
6. Close the lid while driving: the game stays paused on reopening.
7. Restart: settings and saved garage are restored.
8. With `make PROFILE=1`, drive a few laps and send back `perf.csv` (target: stable 30 FPS).

## Not implemented yet

Progression mode (collection, money, discoveries, races, AI), the procedural island,
biomes and day/night cycle, OBJ import, positional audio, reverb and biome ambiences,
texture quality (the game has no textures).

## Documentation

- [TODO.md](TODO.md) — scope and checklist
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)
- [docs/COORDINATE_CONVENTIONS.md](docs/COORDINATE_CONVENTIONS.md)
- [docs/ASSET_PIPELINE.md](docs/ASSET_PIPELINE.md) — planned OBJ pipeline
