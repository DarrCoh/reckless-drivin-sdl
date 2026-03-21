#!/usr/bin/env bash

set -xe

mkdir -p build
pushd build

system_arch="$(uname -m)"

sw_version="$(sed -n 's/^project(RecklessDrivin VERSION \([0-9.]*\).*/\1/p' ../CMakeLists.txt)"

appimagetool_version="1.9.1"
appimagetool_filename="appimagetool-$system_arch.AppImage"
appimagetool_baseurl="https://github.com/AppImage/appimagetool/releases/download/$appimagetool_version"

runtime_version="20251108"
runtime_filename="runtime-$system_arch"
runtime_baseurl="https://github.com/AppImage/type2-runtime/releases/download/$runtime_version"

if [ "$system_arch" = "x86_64" ]; then
	appimagetool_hash="ed4ce84f0d9caff66f50bcca6ff6f35aae54ce8135408b3fa33abfc3cb384eb0"
	runtime_hash="2fca8b443c92510f1483a883f60061ad09b46b978b2631c807cd873a47ec260d"

elif [ "$system_arch" = "aarch64" ]; then
	appimagetool_hash="f0837e7448a0c1e4e650a93bb3e85802546e60654ef287576f46c71c126a9158"
	runtime_hash="00cbdfcf917cc6c0ff6d3347d59e0ca1f7f45a6df1a428a0d6d8a78664d87444"
fi

# Shared libraries as of Ubuntu 22.04
required_libs=(
	libbsd.so.0
	libdecor-0.so.0
	libmd.so.0
	libSDL2-2.0.so.0
	libXss.so.1
)

popd
cmake -S . -B build
cmake --build build -j "$(nproc)"
pushd build

mkdir -p RecklessDrivin.AppDir/usr/{bin,lib,share/{applications,assets}}

cp -a Data RecklessDrivin.AppDir/usr/share/assets/
cp -a RecklessDrivin RecklessDrivin.AppDir/usr/bin/

cp -a ../packaging/linux/{AppRun,RecklessDrivin.desktop} RecklessDrivin.AppDir/
cp -a ../packaging/linux/Icon_128x128.png RecklessDrivin.AppDir/RecklessDrivin.png
cp -a ../packaging/linux/RecklessDrivin.desktop RecklessDrivin.AppDir/usr/share/applications/

for lib in "${required_libs[@]}"; do
	cp -a -L "/lib/$system_arch-linux-gnu/$lib" RecklessDrivin.AppDir/usr/lib/
done

if [ ! -f "$appimagetool_filename" ]; then
	curl -sSf -L -O "$appimagetool_baseurl/$appimagetool_filename"
	chmod +x "$appimagetool_filename"
fi

if [ ! -f "$runtime_filename" ]; then
	curl -sSf -L -O "$runtime_baseurl/$runtime_filename"
	chmod +x "$runtime_filename"
fi

echo "$appimagetool_hash  $appimagetool_filename" | shasum -a 256 -c
echo "$runtime_hash  $runtime_filename" | shasum -a 256 -c

./"$appimagetool_filename" --no-appstream --runtime-file "runtime-$system_arch" RecklessDrivin.AppDir "Reckless-Drivin-$sw_version-linux-$system_arch.AppImage"

rm -r RecklessDrivin.AppDir
