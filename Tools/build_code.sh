#!/bin/bash
# Kompiluje moduły C++ (edytor). Konfiguracja: Development.
set -e
source "$(dirname "$0")/env.sh"
"$UE_BUILD" SakuraRiverEditor Mac Development -project="$UPROJECT" -waitmutex
