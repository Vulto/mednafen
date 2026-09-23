#!/bin/sh
set -eu

if [ "$#" -ne 1 ]; then
    echo "Usage: $0 /path/to/neodrift.zip" >&2
    exit 2
fi

GameRom=$1
if [ ! -f "$GameRom" ]; then
    echo "Game ROM not found: $GameRom" >&2
    exit 2
fi

MednafenBinary="${MEDNAFEN_BINARY:-./src/mednafen}"
if [ ! -x "$MednafenBinary" ]; then
    echo "Mednafen binary not found: $MednafenBinary" >&2
    exit 2
fi

MednafenHome="${MEDNAFEN_HOME:-$HOME/.mednafen}"
if [ ! -f "$MednafenHome/firmware/neogeo.zip" ]; then
    echo "Neo Geo BIOS not found: $MednafenHome/firmware/neogeo.zip" >&2
    exit 2
fi

TestDir="${TMPDIR:-/tmp}/mednafen-gngeo-neodrift"
rm -rf "$TestDir"
mkdir -p "$TestDir"

ReturnCode=0
timeout --signal=TERM --kill-after=5s 30s xvfb-run -a "$MednafenBinary" -sound 0 "$GameRom" >"$TestDir/runtime.log" 2>&1 || ReturnCode=$?

cat "$TestDir/runtime.log"

if [ "$ReturnCode" -ne 124 ]; then
    echo "Neo Geo runtime test failed: Mednafen exited before the 30 second acceptance window (rc=$ReturnCode)." >&2
    exit 1
fi

grep -q 'Using module: gngeo(Neo Geo (GnGeo))' "$TestDir/runtime.log"
grep -q 'Initializing video' "$TestDir/runtime.log"

if grep -Eiq 'segmentation fault|assertion failed|unable to load|fatal error|unhandled exception|missing ROM|Invalid instruction' "$TestDir/runtime.log"; then
    echo "Neo Geo runtime test found a fatal diagnostic." >&2
    exit 1
fi

echo "Neo Drift Out remained running in the Mednafen GnGeo core for 30 seconds."
