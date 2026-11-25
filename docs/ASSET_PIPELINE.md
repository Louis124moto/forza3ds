# 3D asset pipeline

## Runtime model location

Custom models are read from the SD card:

sdmc:/3ds/forza3ds/
  models/cars/<model-id>/model.obj
  models/cars/<model-id>/model.mtl
  models/cars/<model-id>/textures/
  models/cars/<model-id>/vehicle.cfg
  models/environment/<model-id>/model.obj
  cache/models/
  saves/

The exact directory and package schema are part of the project contract. The first release may support a documented subset of OBJ/MTL rather than every extension.

## OBJ support planned

- Vertex positions (v).
- Texture coordinates (vt).
- Vertex normals (vn).
- Object/group names (o, g).
- Material selection (usemtl, mtllib).
- Polygon faces (f) with positive and negative indices.
- Polygon triangulation and index validation.

Unsupported directives must be ignored safely when harmless or reported as an import error when they affect geometry/material correctness. The parser must not assume files are trusted.

## Vehicle package metadata

OBJ describes visual geometry; it does not define a safe or meaningful vehicle physics setup. A separate vehicle.cfg will define at minimum:

- Model identifier and display name.
- Unit scale and orientation correction.
- Visual origin / pivot.
- Wheel attachment positions and wheel radius.
- Optional separate mesh names for body and wheels.
- Optional collision bounds for preview and simple collision proxies.

The exact syntax will be specified before the importer is considered complete. Physics values remain in the vehicle definition managed by the game.

## Import and cache lifecycle

1. Scan packages and show valid/invalid status.
2. Parse into bounded temporary structures.
3. Validate references, counts, dimensions and materials.
4. Triangulate and convert to the engine's vertex format.
5. Generate a cache containing a format version and source fingerprint.
6. Load the cache on later launches only if version and source fingerprint match.
7. Allow cache deletion and rebuild from the model browser.

Replacing a visual model must not reset the car's mechanical tuning or save identity.

## Limitations to document

- New 3DS memory and GPU limits require model budgets.
- OBJ/MTL feature coverage will be explicitly listed in the importer documentation.
- Large or malformed models must fail gracefully.
- Texture formats and maximum dimensions will be fixed after testing the chosen libctru/Citro3D loading path.
