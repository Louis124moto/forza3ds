#!/usr/bin/env bash
# Builds and runs the host preview. Usage: tools/preview.sh <output-dir>
# Needs CITRO3D_DIR (citro3d sources) and LIBCTRU_DIR (libctru sources, for the
# console font).
set -euo pipefail
cd "$(dirname "$0")/.."
: "${CITRO3D_DIR:?}" "${LIBCTRU_DIR:?}"
OUT="${1:?output directory}"
mkdir -p "$OUT"
SRC=$(ls source/core/core.c source/core/app.c source/input/input.c source/physics/*.c source/vehicle/*.c \
	source/world/*.c source/save/*.c source/ui/*.c source/render/camera.c \
	source/render/meshes.c source/render/scene.c source/audio/synth.c)
MATHS=$(ls "$CITRO3D_DIR"/source/maths/mtx_{lookat,multiply,persptilt,rotatex,rotatey,rotatez,scale,translate}.c)
cc -std=gnu11 -O2 -D_GNU_SOURCE -Itests -Iinclude/forza3ds -I"$CITRO3D_DIR/include" \
	$SRC $MATHS tools/preview.c -lm -o "$OUT/preview"
"$OUT/preview" "$LIBCTRU_DIR/data/default_font.bin" "$OUT"
