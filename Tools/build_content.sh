#!/bin/bash
# Buduje materiały, importuje modele i zapisuje mapę (Tools/ue/build_content.py) w edytorze bez okna.
# SAKURA_ONLY=materials | meshes | map — tylko wybrane kroki.
set -e
source "$(dirname "$0")/env.sh"
LOG="$PROJECT_DIR/Saved/Logs/build_content.log"; mkdir -p "$(dirname "$LOG")"
"$UE_CMD" "$UPROJECT" -run=pythonscript -script="$PROJECT_DIR/Tools/ue/build_content.py" "${QUIET_FLAGS[@]}" "$@" > "$LOG" 2>&1 || true
grep -E "\[sakura\]|LogPython: Error|Error: .*Sakura|error:" "$LOG" | grep -v "Callstack" | tail -60
