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
`kernel.development.sweet`. The workflow packages `xnu-kernel` and a separate
build-tree artifact, with limited retention. Review the run and artifact
provenance before downloading or packaging; a build result is not a hardware
boot result. See [README.md](../README.md) and [NOTES.md](../NOTES.md) for
private packaging dependencies and recorded River tests.
