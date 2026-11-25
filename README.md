# Forza3DS

Native 3D driving-game prototype for **New Nintendo 3DS / New Nintendo 3DS XL**, written in C with devkitARM, libctru and Citro3D.

## Current implementation state

The code now contains an integrated **Mode Libre → vehicle → 3D garage/workshop → driving** path, but this iteration has not yet been compiled or validated on a physical New 3DS.

Implemented in source:

- Main menu with Mode Libre, Mode Progression information and exit.
- Mode Libre vehicle selection. The current in-game catalogue contains one prototype car; the menu does not claim that a larger catalogue exists.
- A low-poly 3D inspection garage used by the vehicle and workshop screens.
- Mechanical workshop pages for engine, turbo, intake, exhaust, camshaft, injectors, pistons, RPM, gearbox, differentials, suspension and tires.
- Automatic/manual gearbox selection. In manual mode, L/R request down/up shifts.
- Vehicle parameters are passed directly into the driving simulation: engine torque and boost, gear ratios/final drive, tire grip, differential settings, and suspension spring/damper/ride-height values.
- Procedural engine audio generated through libctru NDSP. RPM, engine load, cylinder count, exhaust/intake/piston settings and turbo configuration affect the generated sound.
- Pause navigation back to the workshop, vehicle selection or main menu.
- Existing chase/cockpit camera, Circle Pad steering configuration, button remapping, FPS limiter, HUD toggle and settings file remain available.

The vehicle model is still a compact real-time approximation rather than a complete multi-body simulation. Its suspension is a simplified sprung-mass/roll approximation, and the current prototype is not a validated physical vehicle model.

## Default controls

| Control | Action |
|---|---|
| Circle Pad | Steering |
| A | Accelerator |
| B | Brake |
| Y | Handbrake |
| L / R | Downshift / upshift in manual mode |
| C-stick | Camera orbit |
| X | Toggle chase/cockpit camera |
| D-pad | Adjust chase camera while driving |
| START | Pause / resume |
| SELECT | Exit |

Accelerator, brake, handbrake and camera-view buttons can be reassigned in the pause menu. START and SELECT remain reserved.

## Validation status and limitations

**Do not treat this iteration as finished yet.** The source edits have been committed to the main branch, but no successful build or test run for the latest commits has been confirmed in this session. A successful compile would still not prove console playability.

Still required:

- Compile the current source with devkitARM/libctru/Citro3D and fix any compiler/linker errors.
- Run the deterministic host tests, including the new tuning and manual-gearbox tests.
- Test the menu → car → workshop → driving → workshop loop on a physical New 3DS.
- Measure frame time and confirm the 30 FPS target on real hardware.
- Test NDSP audio stability, buffer lifetime and sound quality on the console.
- Add more vehicles; the current selectable catalogue has one prototype.
- Implement the playable progression mode, separate progression/free-mode save data and persistent workshop builds.
- Extend the vehicle simulation and validate its behavior; the current dynamics remain simplified.
- Add visual customization, a full vehicle collection and the later procedural island/world systems.

The OBJ/MTL importer, model browser, procedural multi-peninsula world, sector streaming, races and AI are still roadmap work.

## Target and build

- New Nintendo 3DS / New Nintendo 3DS XL (ARM11).
- Native 3DSX output.
- C, devkitARM, libctru and Citro3D.
- Target: 30 FPS stable, to be measured on real hardware.

Install devkitPro with devkitARM, libctru and Citro3D, then run:

~~~sh
make
~~~

The expected output is forza3ds.3dsx. Set DEVKITPRO and DEVKITARM in the environment.

## GitHub Actions

The workflow is configured to build the 3DSX and run deterministic host-side engine tests. The latest source changes still need a confirmed successful run.

## Planned SD-card model packages

Custom OBJ models are planned under:

~~~text
sdmc:/3ds/forza3ds/models/cars/<model-id>/
  model.obj
  model.mtl
  textures/
  vehicle.cfg
~~~

The importer and in-game model browser are not implemented yet. OBJ geometry will remain separate from vehicle physics and mechanical data.

## Documentation

- [TODO.md](TODO.md) — consolidated project scope and checklist
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) — engine architecture
- [docs/ASSET_PIPELINE.md](docs/ASSET_PIPELINE.md) — planned OBJ package and conversion pipeline
- [docs/COORDINATE_CONVENTIONS.md](docs/COORDINATE_CONVENTIONS.md) — coordinate conventions
