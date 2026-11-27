#!/usr/bin/env bash
# Host tests of every hardware-independent module, with address and undefined
# behaviour sanitizers. CITRO3D_DIR must point to a citro3d source checkout
# (only its maths library is compiled).
set -euo pipefail
cd "$(dirname "$0")/.."
: "${CITRO3D_DIR:?set CITRO3D_DIR to a citro3d source checkout}"
OUT="${OUT:-$(mktemp -d)}"
CC="${CC:-cc}"
FLAGS="-std=gnu11 -O1 -g -D_GNU_SOURCE -Itests -Iinclude/forza3ds -I$CITRO3D_DIR/include"
SAN="${SAN:--fsanitize=address,undefined -fno-sanitize-recover=undefined}"

PORTABLE="source/core/core.c source/core/app.c source/input/input.c source/physics/physics.c
source/physics/collision.c source/vehicle/vehicle.c source/vehicle/catalog.c source/world/track.c
source/world/terrain.c source/world/world.c source/world/props.c source/generation/noise.c
source/generation/gen_relief.c source/generation/gen_water.c source/generation/gen_sites.c
source/generation/gen_roads.c source/generation/gen_buildings.c source/generation/generate.c source/save/save_io.c source/save/settings.c source/save/garage.c
source/ui/workshop.c source/ui/canvas.c source/ui/screens.c source/render/camera.c
source/render/meshes.c source/render/scene.c source/audio/synth.c"

C3D_MATHS=""
for f in mtx_lookat mtx_multiply mtx_persptilt mtx_rotatex mtx_rotatey mtx_rotatez mtx_scale mtx_translate; do
	C3D_MATHS="$C3D_MATHS $CITRO3D_DIR/source/maths/$f.c"
done

# Third-party maths: built without our warning flags.
for f in $C3D_MATHS; do
	$CC $FLAGS $SAN -c "$f" -o "$OUT/c3d_$(basename "$f" .c).o"
done
$CC $FLAGS $SAN -Wall -Wextra -Werror $PORTABLE tests/test_*.c "$OUT"/c3d_*.o -lm -o "$OUT/forza3ds-tests"
"$OUT/forza3ds-tests"
