# Forza3DS

Native 3D driving-game project for **New Nintendo 3DS**, written in C with devkitARM, libctru and Citro3D.

## Current state

The repository currently contains a minimal native homebrew bootstrap, project documentation, a development roadmap and a GitHub Actions build workflow. It is **not yet a playable driving game**: the 3D renderer, vehicle physics, OBJ importer and procedural world are planned work tracked in [TODO.md](TODO.md).

## Target

- New Nintendo 3DS / New Nintendo 3DS XL
- Native ARM11 homebrew (3DSX)
- C, devkitARM, libctru and Citro3D
- 30 FPS stable is the target to validate on real hardware
- First milestone: one car, one road and one camera

## Build locally

Install devkitPro with the 3DS development packages, including devkitARM, libctru and Citro3D. Then run from the repository root:

```sh
make
```

The expected output is `forza3ds.3dsx`. Set `DEVKITPRO` and `DEVKITARM` according to your devkitPro installation.

## GitHub Actions

The workflow in `.github/workflows/build.yml` builds the 3DSX in a pinned devkitPro devkitARM container and uploads the result as a workflow artifact when the build succeeds.

## Planned SD-card model packages

Custom OBJ models will eventually be imported by the game from:

```text
sdmc:/3ds/forza3ds/models/cars/<model-id>/
  model.obj
  model.mtl
  textures/
  vehicle.cfg
```

The game will parse and validate the model on the 3DS, convert it to an optimized Citro3D-compatible cache, then expose it in an in-game model browser for preview and vehicle assignment. This importer and menu are **not implemented yet**. OBJ geometry is visual data; vehicle physics and wheel attachment metadata remain separate.

## Documentation

- [TODO.md](TODO.md) — consolidated scope and implementation checklist
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) — planned runtime architecture and memory strategy
- [docs/ASSET_PIPELINE.md](docs/ASSET_PIPELINE.md) — OBJ package and conversion pipeline

See the official [devkitPro 3DS examples](https://github.com/devkitPro/3ds-examples) for API usage references.
