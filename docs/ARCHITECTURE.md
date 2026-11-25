# Architecture

## Target

Native New Nintendo 3DS application using devkitARM, libctru and Citro3D. The game is written in C. The first milestone is a small, measurable driving prototype, not the complete open world.

## Runtime modules

Planned module boundaries:

- core/: application lifecycle, timing, configuration and common utilities.
- input/: New 3DS controls and configurable bindings.
- render/: Citro3D state, cameras, meshes, materials and scene submission.
- physics/: deterministic vehicle simulation and collision queries.
- vehicle/: vehicle definitions, visual model references and mechanical setup.
- world/: logical island representation, sectors and streaming.
- generation/: deterministic seeded terrain, biome, road and settlement generation.
- assets/: OBJ/MTL parsing, validation, conversion and cache management.
- ui/: menus, model browser, garage and lower-screen HUD.
- audio/: engine and environmental audio.
- save/: versioned save/load and integrity checks.

These are planned boundaries. The current bootstrap only initializes libctru's graphics console.

## Coordinate and simulation conventions

Before implementing physics, define one world-unit scale, one coordinate convention, angle units and the mapping between simulation coordinates and Citro3D. Keep physics state independent from render transforms. Use a fixed simulation timestep with an accumulator, then cap catch-up work to avoid an unbounded frame.

## Vehicle separation

A vehicle has three distinct data groups:

1. Visual asset reference: mesh, materials, textures and visual attachment points.
2. Physical definition: mass, inertia, center of gravity, wheelbase, track width, suspension and tire parameters.
3. Runtime state: position, orientation, velocities, wheel state, engine state and damage.

Replacing an OBJ changes the visual asset reference only. Physical values remain attached to the vehicle definition unless the user explicitly selects a different vehicle setup. A model package must provide scale/origin and wheel attachment metadata; a visual mesh alone is not a physics definition.

## Procedural world and memory

The full island exists as deterministic logical data derived from a world seed and generator version. Only nearby sectors are converted to renderable geometry and resident in memory. Sector identities and generation order must not depend on frame timing or traversal order.

Use separate deterministic sub-seeds for terrain, biomes, road graph, settlements, buildings and decorative details. A generator change that alters the world requires a new generator version or a compatibility path for old saves.

## OBJ import pipeline

1. Discover model packages under sdmc:/3ds/forza3ds/models/.
2. Read and validate OBJ/MTL source files with strict size and count limits.
3. Resolve indices and triangulate polygon faces.
4. Convert coordinates, normals and UVs to the engine convention.
5. Build Citro3D-ready vertex/index data.
6. Write a versioned cache under sdmc:/3ds/forza3ds/cache/models/.
7. Let the model browser preview and assign the visual asset to a vehicle.

Never trust paths or numeric values from external model files. Reject path traversal, malformed indices, NaN/infinite values, unsupported directives and allocations above configured limits. Cache data must be revalidated before loading.

## Save data

Store a schema version, generator version, world seed and generation settings, player state, garage and vehicle setup, visual model assignments, currency, event records, discoveries and user settings. Procedurally derivable immutable world data should be regenerated; persist all non-derivable state. Use temporary files and a safe replacement strategy so a failed save does not destroy the last valid save.

## Performance policy

New 3DS hardware is the authority for performance decisions. Establish measured budgets before choosing final world scale, mesh limits, texture sizes, sector radius or AI count. The 30 FPS target is a goal, not a guarantee. Keep profiling counters for frame time, simulation time, draw submission, visible sectors and memory use.
