#!/bin/bash
# Paczka dla macOS (Shipping): Dist/Mac/SakuraRiver.app + Sakura.ini obok.
# Windows: ten sam projekt na PC z UE 5.8 i Visual Studio 2022 — RunUAT.bat BuildCookRun ... -platform=Win64 (patrz README).
set -e
source "$(dirname "$0")/env.sh"
"$UE_UAT" BuildCookRun -project="$UPROJECT" -noP4 -platform=Mac -clientconfig=Shipping -serverconfig=Shipping \
  -build -cook -stage -pak -iostore -compressed -prereqs -archive -archivedirectory="$PROJECT_DIR/Dist" -utf8output -nocompileeditor 2>&1 \
  | grep -E --line-buffered "BUILD SUCCESSFUL|BUILD FAILED|Error|error:|Cook:|LogCook: Display: Cooked|Archiving" | tail -40
APP=$(find "$PROJECT_DIR/Dist" -maxdepth 3 -name "SakuraRiver*.app" | head -1)
[ -n "$APP" ] && cp "$PROJECT_DIR/Sakura.ini" "$(dirname "$APP")/Sakura.ini" && echo "Gotowe: $APP"
