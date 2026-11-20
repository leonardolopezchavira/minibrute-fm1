#!/bin/sh
# SPDX-License-Identifier: MIT
# The editor page and its engine (app/editor/editor_wasm.c: mbvoice, mbfx and mbpatch.h, the firmware's own files)
# in WebAssembly, into OUT (default app/build/site/edit): clang --target=wasm32 with wasm-ld, no libc.
set -e
OUT="${1:-app/build/site/edit}"
mkdir -p "$OUT"
clang --target=wasm32 -O2 -std=gnu99 -nostdlib -ffreestanding -fvisibility=hidden -DMB_FASTMATH \
      -Iapp -Iapp/mbvoice -Ifm1/base/include \
      app/editor/editor_wasm.c app/mbvoice/mbvoice.c app/mbvoice/mbfx.c \
      -Wl,--no-entry -Wl,--export-dynamic -Wl,--initial-memory=4194304 -Wl,-z,stack-size=131072 \
      -o "$OUT/mb1_editor.wasm"
cp app/editor/index.html "$OUT/index.html"
echo "editor   $OUT ($(wc -c < "$OUT/mb1_editor.wasm") B wasm)"
