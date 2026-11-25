# Forza3DS

Native 3D driving-game prototype for **New Nintendo 3DS / New Nintendo 3DS XL**, written in C with devkitARM, libctru and Citro3D.

## Current prototype

The project is past the initial renderer bootstrap and currently contains an early island free-driving slice:

- Native New 3DS startup and Citro3D scene rendering.
- A fixed island scene with ocean, irregular shoreline, low-poly forest clusters and distant peaks.
- A winding asphalt road around the island, with open off-road driving inside the shoreline.
- A prototype vehicle with acceleration, braking, steering, handbrake, automatic gears, basic airborne/ramp behavior and surface grip.
- Chase and cockpit camera modes; C-stick orbit, automatic recentering, and adjustable chase distance/height.
- Pause menu that freezes vehicle simulation.
- Adjustable steering sensitivity and Circle Pad deadzone.
- Rebindable accelerator, brake, handbrake and camera-view buttons.
- 30/60 FPS presentation limit and hideable HUD.
- Versioned settings file at `sdmc:/3ds/forza3ds/settings.bin`, loaded at startup and saved from the pause menu and on exit.
- Deterministic host-side engine tests and a GitHub Actions New 3DS build.

### Default controls

| Control | Action |
|---|---|
| Circle Pad | Steering |
| A | Accelerator |
| B | Brake |
| Y | Handbrake |
| C-stick | Camera orbit |
| X | Toggle chase/cockpit camera |
| D-pad | Adjust chase camera while driving |
| START | Pause / resume |
| SELECT | Exit |

The four face/shoulder-button actions listed as rebindable can be reassigned in the pause menu. START and SELECT remain reserved for pause and exit.

## Important limitations

This is **not yet the completed first demo milestone** and has not been validated on a physical New 3DS. CI proves that the current source compiles and the host tests pass; it does not prove console performance or playability.

Still outstanding for the first polished demo:

- Real-hardware launch, controls, shutdown and performance checks; 30 FPS stability measurement.
- Further visual and driving polish on the fixed island map.
- Full camera/settings edge-case testing and lid-close pause behavior.

Later roadmap systems are not implemented yet: the full procedural multi-peninsula world, sector streaming, OBJ/MTL model importer and model browser, advanced vehicle physics and tuning workshop, garage/collection, events and AI, audio system, and progression save data.

## Target and build

- New Nintendo 3DS / New Nintendo 3DS XL (ARM11).
- Native 3DSX output.
- C, devkitARM, libctru and Citro3D.
- Target: 30 FPS stable, to be measured on real hardware.

Install devkitPro with devkitARM, libctru and Citro3D, then run:

```sh
make
```

The expected output is `forza3ds.3dsx`. Set `DEVKITPRO` and `DEVKITARM` in the environment.

## GitHub Actions

The workflow builds the 3DSX using a devkitPro devkitARM container, runs deterministic host-side engine tests, and uploads the 3DSX as an artifact when the build succeeds.

## Planned SD-card model packages

Custom OBJ models are planned under:

```text
sdmc:/3ds/forza3ds/models/cars/<model-id>/
  model.obj
  model.mtl
  textures/
  vehicle.cfg
```

The importer and in-game model browser are not implemented yet. OBJ geometry will remain separate from vehicle physics and mechanical data.

## Documentation

- [TODO.md](TODO.md) — consolidated project scope and checklist
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) — engine architecture
- [docs/ASSET_PIPELINE.md](docs/ASSET_PIPELINE.md) — planned OBJ package and conversion pipeline
- [docs/COORDINATE_CONVENTIONS.md](docs/COORDINATE_CONVENTIONS.md) — coordinate conventions
