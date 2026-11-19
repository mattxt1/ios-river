#!/bin/sh
# Build only. Optional --with-ui compiles the existing UI fixes and the native
# bridge into the already injected SpringBoard library, preserving its path.
set -eu
repo=$(cd "$(dirname "$0")/.." && pwd -P)
out=${1:?usage: build-river-hardware.sh ABSOLUTE_OUTPUT_DIRECTORY}
case "$out" in /*) ;; *) echo 'output must be absolute' >&2; exit 2;; esac
mkdir -p "$out"
# SDKs omit this private framework. Export exactly the one weak symbol used
# by the bridge; retain its real iOS install name and do not permit arbitrary
# unresolved references through -undefined dynamic_lookup.
cat > "$out/BackBoardServices.tbd" <<'TBD'
--- !tapi-tbd
tbd-version: 4
targets: [ arm64-ios ]
install-name: '/System/Library/PrivateFrameworks/BackBoardServices.framework/BackBoardServices'
exports:
  - targets: [ arm64-ios ]
    symbols: [ _BKSDisplayBrightnessSet ]
...
TBD
xcrun --sdk iphoneos clang -arch arm64 -mios-version-min=16.0 -O2 -Wall -Wextra \
 -Wno-unused-parameter -fobjc-arc -dynamiclib \
 -install_name /usr/local/lib/libRiverHardware.dylib \
 -framework Foundation -framework UIKit -framework IOKit \
 -Wl,-weak_library,"$out/BackBoardServices.tbd" \
 "$repo/phone-tools/river-hardware.m" -o "$out/libRiverHardware.dylib"
codesign -f -s - "$out/libRiverHardware.dylib"
codesign --verify --strict "$out/libRiverHardware.dylib"
shasum -a 256 "$out/libRiverHardware.dylib"
if [ "${2:-}" = --with-ui ]; then
 xcrun --sdk iphoneos clang -arch arm64 -mios-version-min=16.0 -O2 -Wall \
  -Wno-deprecated-declarations -c "$repo/phone-tools/cgprobe.c" -o "$out/cgprobe.o"
 xcrun --sdk iphoneos clang -arch arm64 -mios-version-min=16.0 -O2 -Wall -Wextra \
  -Wno-unused-parameter -fobjc-arc -c "$repo/phone-tools/river-hardware.m" -o "$out/river-hardware.o"
 xcrun --sdk iphoneos clang -arch arm64 -mios-version-min=16.0 -dynamiclib \
  -install_name /usr/local/lib/libcgprobe.dylib \
  -framework Foundation -framework UIKit -framework CoreGraphics -framework IOKit \
  -Wl,-weak_library,"$out/BackBoardServices.tbd" \
  "$out/cgprobe.o" "$out/river-hardware.o" -o "$out/libcgprobe.dylib"
 codesign -f -s - "$out/libcgprobe.dylib"
 codesign --verify --strict "$out/libcgprobe.dylib"
 shasum -a 256 "$out/libcgprobe.dylib"
fi
