#!/bin/sh
# build-from-source.sh — build and install JackGraph from source on Haiku.
# Build/install jack-port-haiku FIRST (the Makefile links -ljack from the
# non-packaged tree).
set -e

HERE=$(cd "$(dirname "$0")" && pwd)

pkgman install -y gcc make haiku_devel

cd "$HERE"
make clean
make

mkdir -p /boot/system/non-packaged/apps
cp JackGraph /boot/system/non-packaged/apps/JackGraph
echo ">> Installed JackGraph to /boot/system/non-packaged/apps/JackGraph"
echo ">> Launch it from Deskbar (Applications) or run ./JackGraph"
