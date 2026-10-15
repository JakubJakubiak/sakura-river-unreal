# Wspólne ścieżki dla skryptów (macOS). Nadpisz UE_ROOT / BLENDER, jeśli masz inne położenie.
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$HERE/.." && pwd)"
UPROJECT="$PROJECT_DIR/SakuraRiver.uproject"
UE_ROOT="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
BLENDER="${BLENDER:-/Applications/Blender.app/Contents/MacOS/Blender}"
UE_EDITOR="$UE_ROOT/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
UE_CMD="$UE_ROOT/Engine/Binaries/Mac/UnrealEditor-Cmd"
UE_BUILD="$UE_ROOT/Engine/Build/BatchFiles/Mac/Build.sh"
UE_UAT="$UE_ROOT/Engine/Build/BatchFiles/RunUAT.sh"
export SAKURA_OUT="$PROJECT_DIR/Tools/out"
QUIET_FLAGS=(-unattended -nosplash -nocrashreports -stdout -FullStdOutLogOutput)
