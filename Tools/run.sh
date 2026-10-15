#!/bin/bash
# Uruchamia scenę w oknie 1920×1080 (przez edytor, bez pakowania). Parametry jak w Sakura.ini, np.:
#   Tools/run.sh -channel=twojkanal -time=night -view=bridge
source "$(dirname "$0")/env.sh"
"$UE_EDITOR" "$UPROJECT" /Game/Sakura/Maps/SakuraRiver -game -windowed -ResX=1920 -ResY=1080 -nosplash -nocrashreports "$@"
