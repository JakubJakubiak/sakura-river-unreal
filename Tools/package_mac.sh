#!/bin/bash
# Paczka dla macOS (Shipping): Dist/Mac/SakuraRiver.app + Sakura.ini obok.
# UE 5.8.3 + Xcode 27: krok „Touch UBT generated tiles” w xcodebuild uruchamianym przez UBT kończy się błędem
# (ten sam xcodebuild uruchomiony ręcznie przechodzi). Dlatego: UBT z UE_BUILD_FROM_XCODE=1 (pomija domykanie .app,
# ale zapisuje receipt celu), potem .app domykamy sami xcodebuildem, a UAT robi tylko cook/stage/pak (-skipbuild).
# Windows: na PC z UE 5.8 i Visual Studio 2022 — RunUAT.bat BuildCookRun ... -platform=Win64 (patrz README).
set -e
source "$(dirname "$0")/env.sh"
CONFIG="${CONFIG:-Shipping}"          # CONFIG=Development — paczka z logami do diagnozy
echo "== kompilacja ($CONFIG)"
UE_BUILD_FROM_XCODE=1 "$UE_BUILD" SakuraRiver Mac "$CONFIG" -project="$UPROJECT" -waitmutex 2>&1 | grep -E "error|Result" | tail -5
WS="$PROJECT_DIR/Intermediate/ProjectFiles/SakuraRiver_Mac_SakuraRiver.xcworkspace"
echo "== domknięcie .app (xcodebuild)"
UBT_NO_POST_DEPLOY=true xcodebuild build -workspace "$WS" -scheme SakuraRiver -configuration "$CONFIG" -destination generic/platform=macOS \
  CODE_SIGN_ALLOW_ENTITLEMENTS_MODIFICATION=YES UE_XCODE_BUILD_MODE=PostBuildSync -hideShellScriptEnvironment 2>&1 | grep -E "BUILD (SUCCEEDED|FAILED)"
echo "== cook / stage / pak / archive"
"$UE_UAT" BuildCookRun -project="$UPROJECT" -noP4 -platform=Mac -clientconfig="$CONFIG" -serverconfig="$CONFIG" \
  -skipbuild -cook -stage -pak -iostore -compressed -prereqs -archive -archivedirectory="$PROJECT_DIR/Dist/$CONFIG" -utf8output -nocompileeditor 2>&1 \
  | grep -E --line-buffered "BUILD SUCCESSFUL|BUILD FAILED|ERROR|Error:|COOK COMMAND|STAGE COMMAND|ARCHIVE COMMAND" | tail -30
APP=$(find "$PROJECT_DIR/Dist/$CONFIG" -maxdepth 3 -name "SakuraRiver*.app" | head -1)
[ -n "$APP" ] && cp "$PROJECT_DIR/Sakura.ini" "$(dirname "$APP")/Sakura.ini" && echo "Gotowe: $APP"
