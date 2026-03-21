#!/usr/bin/env bash

set -xe

mkdir -p build
pushd build

sdl_version='2.32.10'
sdl_filename="SDL2-$sdl_version.dmg"
sdl_url="https://github.com/libsdl-org/SDL/releases/download/release-$sdl_version/$sdl_filename"

mount_point="$(mktemp -d)"

if [ ! -f "$sdl_filename" ]; then
	curl -sSf -L -O "$sdl_url"
fi

echo "4a7ac31640d70214e848f994be8a12849c0f97918a7e6c2e27a40036166d1a7f  $sdl_filename" | shasum -a 256 -c
hdiutil attach "$sdl_filename" -mountpoint "$mount_point" -quiet
cp -a "$mount_point/SDL2.framework" .
hdiutil detach "$mount_point"

popd
cmake -S . -B build
cmake --build build -j "$(sysctl -n hw.logicalcpu)"
pushd build

mkdir -p RecklessDrivin.app/Contents/Frameworks
cp -a SDL2.framework RecklessDrivin.app/Contents/Frameworks/
find RecklessDrivin.app -type d -name Headers -exec rm -rf {} +
find RecklessDrivin.app -type l -name Headers -exec rm -rf {} +

sw_version="$(sed -n 's/^project(RecklessDrivin VERSION \([0-9.]*\).*/\1/p' ../CMakeLists.txt)"

mv RecklessDrivin.app "Reckless Drivin'.app"
hdiutil create -fs HFS+ -srcfolder "Reckless Drivin'.app" -volname "Reckless Drivin' $sw_version" "Reckless-Drivin-$sw_version-mac.dmg"
