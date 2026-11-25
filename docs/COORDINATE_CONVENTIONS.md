# Coordinate and Unit Conventions
These conventions are authoritative for world, simulation, and configuration data.
## Coordinate system
The engine uses a right-handed world coordinate system.
| Axis | Positive direction | Meaning |
|---|---|---|
| +X | East | Horizontal east/west axis |
| +Y | Up | Vertical axis |
| +Z | North | Horizontal north/south axis |
Negative directions are -X West, -Y Down, and -Z South.
## World scale
- 1 world unit = 1 metre.
- Positions, distances, lengths, and speeds derived from world-space distances use SI units.
## Angles
- Gameplay angle values are expressed in degrees.
- Vehicle yaw, yaw rate, steering angle, and camera orbit angles use degrees; yaw rate is degrees/second.
- Convert to radians explicitly only at C math and Citro3D API boundaries.
- Never pass degree values directly to trigonometric functions or APIs expecting radians.
## Orientation
- Vehicle yaw = 0 faces +Z (north).
- Positive yaw rotates from +Z toward +X (east), around +Y.
- Positive steering produces positive yaw while moving forward.
- Rendering uses the same world axes; matrix/API angle conversion occurs at the renderer boundary.
