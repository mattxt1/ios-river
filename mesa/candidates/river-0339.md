# River 0339 renderer candidate

`river-0339-full.patch` is a standalone source export against Mesa 26.2.5
commit `f9ce5e81f30cd694726bff6d8197e509a8374ea7`. It includes the recovered
River backend and current adapter. Do not stack it on the older backend export.
`river-0339-renderer-overhead.patch` shows only the four performance edits
against the recovered adapter; exact before/after hashes are in the provenance
JSON. No Apple SDK files, generated binaries or private captures are included.

The clear wrapper queries framebuffer/color/scissor state only when tracing
needs it. Generated GL dispatch wrappers time each call only with
`SWEETGL_PROFILE=1`, read once at process initialization. Coarse flush-rate,
GPU-finish and frame timing remain enabled. Drawing, context binding, error
checks, GPU completion barriers and surface ownership are unchanged.

Build with `mesa/ios-arm64.cross`, release optimization, freedreno driver and
River KMD; disable Vulkan, EGL, GLX, GBM, LLVM, shader cache, zstd, expat,
valgrind and libunwind. Enable GLES2 and zlib; leave platforms/tools empty.
Use local Meson/Mako/PyYAML and Bison 3, then build the
`src/gallium/drivers/freedreno/libsweetgl.dylib` Ninja target. Run
`python3 tools/test-river-renderer-overhead.py /absolute/path/to/mesa`.
The test executes the actual clear body and generated void/return wrappers.

Set the candidate install name to `/usr/local/lib/sweetgl-p0339.dylib`, ad-hoc
sign and verify it, then stage at that unused path and verify its full SHA256.
Preserve `/usr/local/lib/sweetgl.dylib` and verify its hash before/after staging.
Only the new temporary image selects the candidate through the repacker's
`--renderer-library-name sweetgl-p0339.dylib` option. The old image selects the
original library. These source/build checks do not establish a hardware FPS
improvement; compare fresh responsive sessions and retain the prior image.

Research: [ios-sailfish](https://github.com/joranv1/ios-sailfish) identifies CPU
overhead as a rendering limit. [Apollo's deadline scheduling patch](https://github.com/aTechGeekDev/ios-apollo/blob/1f75f381e31da96de537653fbac2252d2eb80418/xnu/patches/0148-apollo-fb-vsync-schedule-ddr-top.patch)
supports replacing work-relative frame timers. River 0339 uses its own smaller
deadline change, preserving existing presentation/fence ownership. Other
devices' GPU/DDR clock recipes and A6xx Metal work are not portable to A506.
