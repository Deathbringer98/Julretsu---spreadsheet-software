# Dependency configuration (0.2)

The headless core has no external library dependencies. Merely including
Dependencies.cmake does not populate a dependency. Enable JULRETSU_ENABLE_LUA or
JULRETSU_BUILD_GUI to compile the integrations; GUI also enables Lua.

## Verified pins
| Library | Release | Immutable commit / archive SHA-256 |
| --- | --- | --- |
| Dear ImGui | v1.91.9b | f5befd2d29e66809cd1110a152e375a7f1981f06 |
| GLFW | 3.4 | 7b6aead9fb88b3623e3b3725ebb42670cbe4c579 |
| sol2 | v3.5.0 | 9190880c593dfb018ccf5cc9729ab87739709862 |
| miniz | 3.1.2 | 77d0dce8627735138c51770d1799a1ef48f2117d |
| pugixml | 1.15 | ee86beb30e4973f5feffe3ce63bfa4fbadf72f38 |
| Lua | 5.4.9 | 2335b6c582a52654f94612bf10d2f4672805d05329aa6568b1d8cd9e5c6fb8e6 |

GLFW and sol2 annotated tags were peeled to commits using upstream release refs.
Lua's archive digest was verified against https://www.lua.org/ftp/.
The sol2 3.3.1 pin from Step 1 failed with Clang 23.1.1 because its optional<T&>
implementation referenced a nonexistent construct member; 3.5.0 fixes that
compiler incompatibility.

sol2 is header-only. Its 3.5.0 upstream CMake requires 3.26, so FetchContent uses a
nonexistent SOURCE_SUBDIR to populate without configuring that upstream project.
Julretsu defines its own julretsu_sol2 interface target over the pinned include
directory. This was built with CMake 3.20.6. No upstream sol2 source was patched.

## Targets and explicit source lists
- julretsu_core: strictly C++20, no UI/Lua headers.
- julretsu_lua_runtime: explicit portable Lua C source list, excluding lua.c/luac.c.
  luaconf.h detects Windows; macOS/Linux definitions and UNIX m/dl linking are
  supplied where required.
- julretsu_lua: LuaEngine plus core, sol2 interface, Lua runtime.
- julretsu_imgui: imgui.cpp, imgui_draw.cpp, imgui_tables.cpp, imgui_widgets.cpp,
  GLFW backend, and OpenGL3 backend.
- julretsu_ui: GridUI and core-facing application behavior.
- julretsu: native lifecycle, graphics setup, and allocation instrumentation.

GLFW examples/tests/docs/install are disabled. No warning-as-error settings are
applied to upstream source. Standard Lua library code is compiled, but only the
restricted allowlist described in LUA.md is exposed to user scripts.

## Offline builds
Pre-populated pinned source directories can be supplied with:
```text
-DFETCHCONTENT_SOURCE_DIR_IMGUI=/path/to/imgui
-DFETCHCONTENT_SOURCE_DIR_GLFW=/path/to/glfw
-DFETCHCONTENT_SOURCE_DIR_SOL2=/path/to/sol2
-DFETCHCONTENT_SOURCE_DIR_LUA=/path/to/lua-5.4.9
```
build.ps1 automatically uses the matching local .deps directories when present.
Fresh checkouts otherwise fetch the pinned upstream sources. Manually supplied
directories are trusted source overrides; FetchContent does not verify them.
All local native validation used populated sources downloaded from the pinned
official archive/commit URLs.

## Platform prerequisites and packaging
Windows: C++20 compiler, Windows SDK or LLVM-MinGW/UCRT, OpenGL driver.
Linux: C++20 compiler, OpenGL development headers, and GLFW X11/Wayland dependencies
(for example xorg-dev, libwayland-dev, libxkbcommon-dev, libgl1-mesa-dev).
macOS: Xcode command-line tools and CMake; forward-compatible OpenGL 3.2/GLSL 150
configuration is implemented. Other platforms request OpenGL 3.3/GLSL 330.
FetchContent does not install system SDKs or graphics drivers.

The local portable Windows package includes libc++.dll and libunwind.dll.
Their import tables were checked; remaining imports are Windows system libraries.
The package includes ImGui, GLFW, sol2, Lua, LLVM, and MinGW notices.
The Windows Segoe UI font is loaded from the user's OS installation and is not
redistributed. Other platforms use ImGui's bundled font; branding is Latin-only.

## Local toolchain
LLVM-MinGW 20260908 UCRT x86_64 (Clang 23.1.1), Ninja 1.13.2,
CMake 3.20.6 minimum-version validation and CMake 4.4.3 script validation.
Portable tools are confined to .tools with no permanent PATH or registry changes.
They are ignored by Git and retain upstream license files.
