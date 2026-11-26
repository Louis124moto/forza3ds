# Architecture

## Target

Native New Nintendo 3DS application written in C (gnu11) with devkitARM, libctru and Citro3D.

## Layers

Everything except the files marked **3DS** is portable and runs in the host tests.

| Module | Files | Role |
|---|---|---|
| core | `core/core.c`, `core/app.c`, `core/platform.c` (3DS) | Fixed-step clock, screens and game flow, console check |
| input | `input/input.c`, `input/input_hw.c` (3DS) | Bindings, dead zone, sensitivity |
| vehicle | `vehicle/vehicle.c`, `vehicle/catalog.c` | Mechanical definition and runtime state; catalogue and visual dimensions |
| physics | `physics/physics.c`, `physics/collision.c` | Two-axle tyre-force model, ramps, collisions |
| world | `world/track.c`, `world/terrain.c` | Demo road, obstacles, trees, ramps, gravel |
| render | `render/camera.c`, `meshes.c`, `scene.c`, `renderer.c` (3DS), `vshader.v.pica` | Meshes built once at start, per-frame draw list, Citro3D submission |
| ui | `ui/workshop.c`, `canvas.c`, `screens.c` | Workshop items and software-drawn lower screen |
| audio | `audio/synth.c`, `audio/engine_audio.c` (3DS) | Procedural sound; NDSP streaming |
| save | `save/save_io.c`, `settings.c`, `garage.c` | Checksummed, versioned, atomic files with backup |

`source/main.c` (3DS) only connects the hardware to the application.
The `generation/` and `assets/` modules of the roadmap do not exist yet.

## Frame

1. Read input, then `app_update`: menus, and while driving a fixed simulation step
   (60 Hz, or 120 Hz in the precise profile) with a bounded catch-up.
   Physics subdivides each step to at most 1/240 s.
2. `app_view` produces an interpolated description of what to show.
3. `scene_build` culls road chunks and produces at most 128 draws; `renderer.c` submits them.
4. The lower screen is redrawn when the UI changes, and about 30 times a second while driving.

## Vehicle separation

- Visual data (`F3DSCarVisual`): dimensions used to build meshes and place cameras.
- Physical definition (`F3DSVehicleDef`): everything the simulation reads; edited by the workshop.
- Runtime state (`F3DSVehicleState`): pose, velocities, engine, suspension.

## Data on the SD card

`sdmc:/3ds/forza3ds/settings.bin` is saved automatically.
`sdmc:/3ds/forza3ds/saves/free_garage.bin` (Mode Libre) is saved only on request.
Each file has a magic number, version, size and CRC-32; it is written to a temporary file
then renamed, and the previous good version is kept as `.bak`.
Progression saves will use separate files.

## Tests and tools

- `tests/run.sh`: host tests with address and undefined-behaviour sanitizers.
- `tools/preview.sh`: renders both screens to images with a software rasteriser. It checks
  geometry and layout; it does not replace the console or an emulator.
- `make PROFILE=1`: logs one line of timings per second to `sdmc:/3ds/forza3ds/perf.csv`.

## Planned: procedural world

The full island will be deterministic logical data derived from a seed and a generator
version, with only nearby sectors converted to geometry. Sub-seeds stay separate for
terrain, biomes, roads, settlements, buildings and details.

## Planned: OBJ import

See [ASSET_PIPELINE.md](ASSET_PIPELINE.md). External files must be validated (sizes,
indices, finite numbers, paths); imported geometry never changes vehicle physics.
