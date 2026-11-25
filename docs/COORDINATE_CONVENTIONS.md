# Coordinate and Unit Conventions

This document records the agreed world-coordinate conventions for Forza3DS.
These conventions are authoritative for simulation and world data.

## Coordinate system

The engine uses a **right-handed** world coordinate system:

| Axis | Positive direction | Meaning |
|---|---|---|
| +X | West | Horizontal west/east axis |
| +Y | Up | Vertical axis |
| +Z | North | Horizontal north/south axis |

The corresponding negative directions are:

- **-X:** East
- **-Y:** Down
- **-Z:** South

## World scale

- **1 world unit = 1 metre.**
- Positions, distances, lengths, and speeds derived from world-space distances must use this scale consistently.

## Angles

- Angles are expressed in **degrees**.
- Do not silently interpret degree-valued data as radians.
- Any conversion required by a math or graphics API must happen explicitly at the relevant boundary.

## Implementation notes

- Keep simulation coordinates consistent with this document.
- Any conversion between simulation coordinates and the Citro3D/rendering coordinate convention must be explicit and centralized.
- This document does not yet define vehicle-forward orientation, yaw sign, rotation order, or the renderer's matrix layout. Those conventions must be specified separately before code relies on them.
