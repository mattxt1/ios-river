# XNU build and patch series

The River kernel is built from Apple's open-source XNU 8796 generation with
this repository's ordered patches and scripts. The GitHub Actions workflow
[`.github/workflows/xnu.yml`](../.github/workflows/xnu.yml) pins its
[blacktop/darwin-xnu-build](https://github.com/blacktop/darwin-xnu-build)
revision and configures the build environment.

The workflow dispatch defaults to `board=topaz`; **select `board=river`** for
the Moto G7. Push-triggered workflow runs also use the default when no board
input is provided. A successful workflow alone therefore does not identify a
River kernel: verify the requested board, commit, build output and River-only
symbols. Runner selection and availability depend on GitHub Actions; consult
the workflow rather than assuming an ARM host.

The usual development artifact for the SWEET machine configuration is
`kernel.development.sweet`.

### Local Builds (Recommended)

`tools/river-local-build.sh` builds the committed patch series locally on macOS
using the macOS 13.5 (22G74) KDK and pinned `darwin-xnu-build`. It enforces
that changes to `xnu/patches` are committed first, builds in an isolated temporary
tree, and outputs `kernel.development.sweet`. See [LOCAL-BUILD.md](../LOCAL-BUILD.md)
for complete usage and requirements.

### Remote Builds (GitHub Actions)

The workflow packages `xnu-kernel` and a separate
build-tree artifact, with limited retention. Review the run and artifact
provenance before downloading or packaging; a build result is not a hardware
boot result. See [README.md](../README.md) and [NOTES.md](../NOTES.md) for
private packaging dependencies and recorded River tests.
