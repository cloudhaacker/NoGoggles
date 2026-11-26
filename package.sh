#!/bin/sh
# Builds the driver and makes the release zip: out/NoGoggles-v<version>.zip
set -e
cd "$(dirname "$0")"
VERSION=$(grep "^\$version = " dist/tools/install.ps1 | sed "s/.*'\(.*\)'.*/\1/")
./build.sh
rm -rf out && mkdir -p out/NoGoggles
cp -r dist/. out/NoGoggles/
rm -f out/NoGoggles/nogoggles/bin/win64/.gitkeep
cp build/driver_nogoggles.dll out/NoGoggles/nogoggles/bin/win64/
mkdir -p out/NoGoggles/source
cp -r src shim build.sh out/NoGoggles/source/
(cd out && zip -qr "NoGoggles-v$VERSION.zip" NoGoggles)
echo "Made out/NoGoggles-v$VERSION.zip"
