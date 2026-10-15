#!/bin/bash
# Zrzuty testowe: Tools/shots.sh "day@river,sunset@river,night@river" [inne parametry]
source "$(dirname "$0")/env.sh"
SHOTS="${1:-day@river,sunset@river,night@river,day@bridge,night@pagoda}"; shift || true
DIR="$PROJECT_DIR/Saved/SakuraShots"; mkdir -p "$DIR"
LOG="$PROJECT_DIR/Saved/Logs/shots.log"
"$UE_EDITOR" "$UPROJECT" /Game/Sakura/Maps/SakuraRiver -game -windowed -ResX=1920 -ResY=1080 -nosplash -nocrashreports -unattended \
  -shots="$SHOTS" -shotdir="$DIR" -demo "$@" > "$LOG" 2>&1
grep -E "SakuraShot|SakuraBench|LogSakura|Error.*(Sakura|shader)|error:" "$LOG" | grep -v Callstack | tail -40
