#!/usr/bin/env bash
# Build committed River patches locally in a new isolated directory.
# Requires KDKROOT and RIVER_LOCAL_BUILD_ROOT; no phone access or flashing.
set -euo pipefail
repo=$(cd "$(dirname "$0")/.." && pwd -P)
: "${KDKROOT:?set KDKROOT to the extracted macOS 13.5 (22G74) KDK directory}"
: "${RIVER_LOCAL_BUILD_ROOT:?set RIVER_LOCAL_BUILD_ROOT to a writable build directory}"
case "$KDKROOT" in /*) ;; *) echo 'KDKROOT must be absolute' >&2; exit 2;; esac
case "$RIVER_LOCAL_BUILD_ROOT" in /*) ;; *) echo 'RIVER_LOCAL_BUILD_ROOT must be absolute' >&2; exit 2;; esac
[ -f "$KDKROOT/System/Library/KernelSupport/libVMAPPLE.os.DEVELOPMENT.a" ] || { echo 'KDKROOT lacks the expected KernelSupport libraries' >&2; exit 2; }
for tool in git jq gum cmake ninja ccache xcrun python3 tar shasum; do
 command -v "$tool" >/dev/null || { echo "missing required tool: $tool" >&2; exit 2; }
done
if [ -n "$(git -C "$repo" status --porcelain -- xnu/patches)" ]; then
 echo 'Commit or stash changes to xnu/patches; this script builds committed patches only.' >&2; exit 2
fi
rev=$(git -C "$repo" rev-parse --verify HEAD)
mkdir -p "$RIVER_LOCAL_BUILD_ROOT"
job=$(mktemp -d "$RIVER_LOCAL_BUILD_ROOT/river-${rev:0:12}-XXXXXX")
printf 'Source commit: %s\nBuild directory: %s\n' "$rev" "$job"
mkdir -p "$job/input" "$job/bin" "$job/ccache-bin"
git -C "$repo" archive "$rev" xnu/patches | tar -xf - -C "$job/input"
git init -q "$job/dxb"
git -C "$job/dxb" fetch -q --depth 1 https://github.com/blacktop/darwin-xnu-build.git 663906528d1cd5ea8fde7de6f16de73db9511549
git -C "$job/dxb" checkout -q --detach FETCH_HEAD
cp "$job/input"/xnu/patches/*.patch "$job/input"/xnu/patches/*.sh "$job/dxb/patches/"
# xcodes is only a presence check in the pinned build script; Xcode is already installed.
printf '#!/bin/sh\nexit 0\n' > "$job/bin/xcodes"
chmod 755 "$job/bin/xcodes"
for name in clang clang++; do
 compiler=$(xcrun -find "$name")
 printf '#!/bin/sh\nexec ccache %q "$@"\n' "$compiler" > "$job/ccache-bin/$name"
 chmod 755 "$job/ccache-bin/$name"
done
# Mirror the CI firehose and ccache configuration. Apply the workspace-only
# Xcode 27 deployment-target workaround before the local header build.
python3 - "$job/dxb/build.sh" <<'PY'
from pathlib import Path
import sys
p=Path(sys.argv[1]); s=p.read_text()
changes=[('ARCHS="x86_64 arm64e" VALID_ARCHS="x86_64 arm64e"', 'ARCHS="arm64" VALID_ARCHS="arm64" OTHER_CFLAGS="-mkernel -nostdinc -Wno-packed -mcpu=cortex-a53"'), ('export CC="$(xcrun -find clang)"','export CC="$CCACHE_WRAP/clang"'), ('export CXX="$(xcrun -find clang++)"','export CXX="$CCACHE_WRAP/clang++"')]
for a,b in changes:
 if s.count(a)!=1: raise SystemExit('Unexpected upstream build script: '+a)
 s=s.replace(a,b)
a='    patches\n    venv\n'
b='''    patches
    if [[ "$(xcodebuild -version | head -1)" == "Xcode 27."* ]]; then
        python3 - "$WORK_DIR/xnu/libsyscall/xcodescripts/mach_install_mig.sh" <<'COMPAT'
from pathlib import Path
import sys
p=Path(sys.argv[1]); s=p.read_text(); a='# build inside OBJROOT\\n'
if s.count(a)!=1: raise SystemExit('Unexpected MIG build script')
s=s.replace(a,'unset IPHONEOS_DEPLOYMENT_TARGET TVOS_DEPLOYMENT_TARGET WATCHOS_DEPLOYMENT_TARGET XROS_DEPLOYMENT_TARGET DRIVERKIT_DEPLOYMENT_TARGET\\n\\n'+a)
p.write_text(s)
COMPAT
    fi
    venv
'''
if s.count(a)!=1: raise SystemExit('Unexpected patch invocation')
p.write_text(s.replace(a,b))
PY
export PATH="$job/bin:$PATH" CCACHE_WRAP="$job/ccache-bin"
export CCACHE_DIR="$RIVER_LOCAL_BUILD_ROOT/ccache" CCACHE_BASEDIR="$job/dxb"
export MACOS_VERSION=13.5 KERNEL_CONFIG=DEVELOPMENT ARCH_CONFIG=ARM64 MACHINE_CONFIG=SWEET SWEET_BOARD=river
export DEVELOPER_DIR="${DEVELOPER_DIR:-$(xcode-select -p)}"
printf 'Build log: %s\n' "$job/build.log"
(cd "$job/dxb" && bash build.sh) > "$job/build.log" 2>&1 || { echo "Build failed; inspect $job/build.log" >&2; exit 1; }
if grep -E 'Skipping patch .*/patches/[0-9]{4}-' "$job/build.log" >/dev/null; then
 echo "A numbered project patch was skipped; inspect $job/build.log" >&2; exit 1
fi
kernel="$job/dxb/fakeroot/System/Library/Kernels/kernel.development.sweet"
[ -f "$kernel" ] || { echo 'Build completed without the expected kernel' >&2; exit 1; }
strings "$kernel" | grep -F 'RIVER GIC' >/dev/null || { echo 'Kernel does not contain River symbols' >&2; exit 1; }
shasum -a 256 "$kernel"
printf 'Kernel: %s\nBuild source: %s\n' "$kernel" "$rev"
echo 'Local build only: this does not package, boot or validate a phone image.'
