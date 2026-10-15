#!/bin/bash
# Generuje modele FBX w Tools/out (teren, Fuji, drzewa, budowle, efekty) — Blender bez okna, kilkanaście sekund.
set -e
source "$(dirname "$0")/env.sh"
for s in terrain trees props; do
  "$BLENDER" -b --factory-startup -P "$PROJECT_DIR/Tools/blender/$s.py" 2>&1 | grep -E "\[sakura\]|Error|Traceback" || true
done
