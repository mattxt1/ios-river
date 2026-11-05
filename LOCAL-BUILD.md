# Local River XNU builds

`tools/river-local-build.sh` builds the **committed** River XNU patch series
locally, without registering a self-hosted GitHub Actions runner, packaging an
image, or accessing the phone. It runs the same pinned `darwin-xnu-build`
revision and River board setting as CI in a new work directory for each run.
An existing work directory or kernel is never silently reused. It uses a shared
ccache, but cache hits and build speed depend on paths and toolchain versions.
CI remains an independent reference.

## Requirements

- macOS with a full Xcode installation, its command-line tools, and sufficient
  free disk space. The first workspace setup on this Mac used several GiB;
  source, build intermediates, logs and repeated jobs require additional space.
- `git`, `jq`, `gum`, `cmake`, `ninja`, `ccache`, `xcrun`, Python 3, `tar`
  and `shasum`. The helper supplies an isolated `xcodes` presence-check stub,
  as CI does; it does not install or select Xcode.
- Apple's **Kernel Debug Kit 13.5, build 22G74**, extracted to a directory
  containing `System/Library/KernelSupport`. Set `KDKROOT` to that directory.
  In this session, the Apple-signed package was checked with `pkgutil` and the
  downloaded DMG matched its published SHA-256; it was expanded in the local
  workspace rather than installed into `/Library/Developer/KDKs`.
- Network access to retrieve the pinned build script and Apple's source
  dependencies on a fresh job. The helper uses the KDK supplied by the caller
  and does not automatically download or install it.

Xcode 27 beta on the current machine needed a local workaround for conflicting
platform deployment variables in libsyscall's MIG header script. The helper
applies this in its isolated build tree when it detects Xcode 27. It leaves the
system Xcode selection and the repository's XNU patches untouched. A linked
kernel is still experimental until checked and tested for the intended device.

## Run

Commit the intended patch changes first; the helper refuses uncommitted or
untracked files under `xnu/patches` because it builds from `HEAD`.

```sh
export KDKROOT=/absolute/path/to/extracted/KDK
export RIVER_LOCAL_BUILD_ROOT=/absolute/path/to/writable/river-builds
# Optional: choose an installed Xcode without changing xcode-select.
# export DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer
tools/river-local-build.sh
```

The command reports the source commit, isolated build directory, log, kernel
path and SHA-256. It fails if a numbered project patch was skipped or the built
kernel lacks River-specific strings. Record the Xcode version, KDK provenance,
full commit and hash with any experiment. Build success, symbols, and host tests
are not proof of a successful boot, correct battery readings, or safe hardware
control. Phone images also need device-specific private inputs described in
[README.md](README.md); keep them and KDK/Apple files out of Git.

The setup does not install a GitHub self-hosted runner. If one is added later,
its access to this Mac, repository secrets and untrusted pull-request code must
be considered before registration. A manual local build is the current path.
