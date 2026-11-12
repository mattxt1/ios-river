#!/bin/sh
# Build a foreground iOS 16 audio test app with normal microphone consent.
# Usage: tools/build-river-audio-check.sh /absolute/output/directory
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
out=${1:?usage: build-river-audio-check.sh OUTPUT_DIRECTORY [BUNDLE_ID] [DISPLAY_NAME]}
audio_bundle_id=${2:-org.river.AudioCheck}
audio_display_name=${3:-River Audio Check}
mkdir -p "$out/RiverAudioCheck.app"
app="$out/RiverAudioCheck.app"
xcrun --sdk iphoneos clang -arch arm64 -mios-version-min=16.0 -O2 -Wall -Wextra -Werror \
 -Wno-deprecated-declarations -fobjc-arc -framework UIKit -framework AVFoundation -framework Foundation \
 "$repo/phone-tools/river-audio-check.m" -o "$app/RiverAudioCheck"
python3 - "$app/Info.plist" "$audio_bundle_id" "$audio_display_name" <<'PY'
import plistlib,sys
p={'CFBundleIdentifier':sys.argv[2],'CFBundleName':'RiverAudioCheck',
   'CFBundleDisplayName':sys.argv[3],'CFBundleExecutable':'RiverAudioCheck',
   'CFBundlePackageType':'APPL','CFBundleVersion':'1','CFBundleShortVersionString':'1.0',
   'CFBundleSupportedPlatforms':['iPhoneOS'],'MinimumOSVersion':'16.0','LSRequiresIPhoneOS':True,'UIDeviceFamily':[1],
   'UISupportedInterfaceOrientations':['UIInterfaceOrientationPortrait'],
   'NSMicrophoneUsageDescription':'Record short clips to test the River microphone audio path.'}
with open(sys.argv[1],'wb') as f:plistlib.dump(p,f,fmt=plistlib.FMT_BINARY)
PY
# Generated app bundles in synced folders may inherit Finder metadata.
xattr -cr "$app"
codesign -f -s - "$app"
COPYFILE_DISABLE=1 tar -C "$out" -cf "$out/RiverAudioCheck.tar" RiverAudioCheck.app
python3 - "$out" <<'PY'
from pathlib import Path
from zipfile import ZipFile,ZIP_DEFLATED
import sys
out=Path(sys.argv[1]); app=out/'RiverAudioCheck.app'
with ZipFile(out/'RiverAudioCheck.ipa','w',ZIP_DEFLATED) as z:
 for file in app.rglob('*'):
  if file.is_file():z.write(file, 'Payload/'+str(file.relative_to(out)))
PY
printf 'Built %s\n'  "$out/RiverAudioCheck.tar"
