#!/bin/sh
# make-hpkg.sh — build the `jackgraph` .hpkg on a running Haiku (x86_64).
#
# The Makefile links -ljack from /boot/system/non-packaged/lib, so build/install
# jack-port-haiku from source first (build-from-source.sh) to populate that tree.
set -e

HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/.." && pwd)
VERSION=0.1
REVISION=1
STAGE="$HERE/stage"

pkgman install -y gcc make haiku_devel || true

cd "$ROOT"
make clean
make

# Build the package tree by hand: a GUI app under apps/ plus a Deskbar menu entry.
rm -rf "$STAGE"
mkdir -p "$STAGE/apps" "$STAGE/data/deskbar/menu/Applications"
cp JackGraph "$STAGE/apps/JackGraph"
ln -sf ../../../../apps/JackGraph "$STAGE/data/deskbar/menu/Applications/JackGraph"

cp "$HERE/jackgraph.PackageInfo" "$STAGE/.PackageInfo"
OUT="$HERE/jackgraph-$VERSION-$REVISION-x86_64.hpkg"
package create -C "$STAGE" "$OUT"
echo ">> built $OUT"
