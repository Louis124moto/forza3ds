# Coordinate and Unit Conventions

These conventions describe what the code does and are checked by the host tests.

## World

Right-handed coordinate system, 1 unit = 1 metre, SI units throughout.

| Axis | Meaning |
|---|---|
| +Y | Up |
| +Z | Reference "forward": heading of an object with yaw 0 |
| +X | To the **left** of an observer facing +Z (right-handed system) |

## Angles

- All stored angles are in **radians**; angular rates in rad/s.
- Yaw rotates around +Y. Yaw 0 faces +Z; increasing yaw turns **left** (+Z towards +X).
- `yaw_forward(yaw)` = (sin, 0, cos); `yaw_left(yaw)` = (cos, 0, -sin).

## Vehicle

- Body frame: +Z forward, +X left, +Y up. The cars are right-hand drive.
- Input steering is positive to the **right**. `steerAngle` and `yawRate` are positive to the **left**.
- `speed` is positive forwards; `lateralSpeed` is positive to the left.
- Positive `pitch` is nose up; positive `roll` is leaning to the right.
  In rendering, a positive X rotation lowers the nose and a positive Z rotation leans right.
- Wheel order: front left, front right, rear left, rear right.

## Citro3D / PICA200

- `C3D_FVec` is stored in memory as w, z, y, x: always use the `FVec*_New` helpers.
- `Mtx_PerspTilt` is used for the rotated top screen; depth is 1 at the near plane and
  0 at the far plane (`GPU_GEQUAL`, clear value 0).
- Front faces are counter-clockwise seen from outside (`GPU_CULL_BACK_CCW`).
