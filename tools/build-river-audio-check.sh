#!/bin/sh
# Build a foreground iOS 16 audio test app with normal microphone consent.
# Usage: tools/build-river-audio-check.sh /absolute/output/directory
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
out=${1:?usage: build-river-audio-check.sh OUTPUT_DIRECTORY}
mkdir -p "$out/RiverAudioCheck.app"
app="$out/RiverAudioCheck.app"
xcrun --sdk iphoneos clang -arch arm64 -mios-version-min=16.0 -O2 -Wall -Wextra -Werror \
 -Wno-deprecated-declarations -fobjc-arc -framework UIKit -framework AVFoundation -framework Foundation \
 "$repo/phone-tools/river-audio-check.m" -o "$app/RiverAudioCheck"
python3 - "$app/Info.plist" <<'PY'
import plistlib,sys
p={'CFBundleIdentifier':'org.river.AudioCheck','CFBundleName':'RiverAudioCheck',
   'CFBundleDisplayName':'River Audio Check','CFBundleExecutable':'RiverAudioCheck',
   'CFBundlePackageType':'APPL','CFBundleVersion':'1','CFBundleShortVersionString':'1.0',
   'MinimumOSVersion':'16.0','LSRequiresIPhoneOS':True,'UIDeviceFamily':[1],
   'UISupportedInterfaceOrientations':['UIInterfaceOrientationPortrait'],
   'NSMicrophoneUsageDescription':'Record short clips to test the River microphone audio path.'}
with open(sys.argv[1],'wb') as f:plistlib.dump(p,f,fmt=plistlib.FMT_BINARY)
PY
codesign -f -s - "$app"
COPYFILE_DISABLE=1 tar -C "$out" -cf "$out/RiverAudioCheck.tar" RiverAudioCheck.app
printf 'Built %s\n' "$out/RiverAudioCheck.tar"
