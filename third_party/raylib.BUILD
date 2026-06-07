load("@rules_cc//cc:defs.bzl", "cc_library")

# Build raylib 5.5 from source for the desktop GLFW backend (OpenGL 3.3).
#
# raylib compiles as a small set of translation units. Almost everything else
# in the tree -- the bundled GLFW, the per-platform rcore backend, the stb_*
# single-file libraries, stb_vorbis.c, qoaplay.c -- is pulled in by `#include`
# of a .c file, so those must be visible to the preprocessor but must NOT be
# compiled as standalone translation units. They therefore go in `textual_hdrs`,
# never in `srcs`.

# The only files raylib actually compiles on their own.
_RAYLIB_SRCS = [
    "src/rcore.c",  # #includes platforms/rcore_desktop_glfw.c
    "src/rshapes.c",
    "src/rtextures.c",
    "src/rtext.c",
    "src/rmodels.c",
    "src/raudio.c",  # #includes external/stb_vorbis.c, external/qoaplay.c
    "src/utils.c",
    "src/rglfw.c",  # #includes external/glfw/src/*.c (the whole GLFW backend)
]

# Public headers consumed by downstream targets (e.g. //:game).
_RAYLIB_HDRS = [
    "src/raylib.h",
    "src/raymath.h",
    "src/rlgl.h",
]

cc_library(
    name = "raylib",
    srcs = _RAYLIB_SRCS,
    hdrs = _RAYLIB_HDRS,
    textual_hdrs = [
        # Internal raylib headers.
        "src/config.h",
        "src/utils.h",
        "src/rcamera.h",
        "src/rgestures.h",
    ] + glob([
        # Per-platform rcore backends, #included by rcore.c.
        "src/platforms/*.c",
        # stb_* libs, miniaudio, cgltf, qoi, stb_vorbis.c, qoaplay.c, glad...
        "src/external/*.h",
        "src/external/*.c",
        # Bundled GLFW: public headers + all backend sources #included by rglfw.c.
        "src/external/glfw/include/GLFW/*.h",
        "src/external/glfw/src/*.h",
        "src/external/glfw/src/*.c",
        "src/external/glfw/src/*.m",  # macOS Cocoa backend
    ]),
    # `includes` propagates to dependents so `#include "raylib.h"` just works.
    includes = [
        "src",
        "src/external/glfw/include",
    ],
    # local_defines stay private to raylib's own compilation.
    local_defines = [
        "PLATFORM_DESKTOP",  # selects PLATFORM_DESKTOP_GLFW in rcore.c
        "GRAPHICS_API_OPENGL_33",  # OpenGL 3.3 core profile
    ] + select({
        # GLFW needs its windowing backend chosen explicitly. raylib's own
        # Makefile passes -D_GLFW_X11 for Linux desktop builds.
        "@platforms//os:linux": ["_GLFW_X11"],
        "//conditions:default": [],
    }),
    # The bundled GLFW + stb code is noisy; quiet it without hiding our own code.
    copts = select({
        "@platforms//os:windows": [],
        "//conditions:default": ["-w"],
    }),
    linkopts = select({
        "@platforms//os:linux": [
            "-lGL",
            "-lm",
            "-lpthread",
            "-ldl",
            "-lrt",
            "-lX11",
        ],
        "@platforms//os:macos": [
            "-framework",
            "CoreVideo",
            "-framework",
            "IOKit",
            "-framework",
            "Cocoa",
            "-framework",
            "GLUT",
            "-framework",
            "OpenGL",
        ],
        "@platforms//os:windows": [
            "-lopengl32",
            "-lgdi32",
            "-lwinmm",
        ],
        "//conditions:default": [],
    }),
    visibility = ["//visibility:public"],
)
