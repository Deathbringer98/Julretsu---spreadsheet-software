# Julretsu 1.1.0
### A native spreadsheet workbench

**English** · [한국어](README.ko.md) · [日本語](README.ja.md)

Julretsu now includes a Windows-tested native ImGui application, a sparse C++20
spreadsheet engine, and bounded Lua formulas/macros. Julretsu is free to use but owned by Matthew Menchinton; it is not open source. See [LICENSE](LICENSE).
The architecture also targets Linux and macOS; those builds have not been run
locally in this Windows session.

- The app now uses your supplied icon and startup banner. See [branding notes](docs/BRANDING-0.2.1.md).

## Start the app
Double-click **Start Julretsu.cmd** in this folder, or open
**release/Julretsu-1.1.0/Julretsu.exe**. Keep the supplied DLLs beside the executable.

[Quick start and controls](docs/QUICKSTART.md) ·
[Steps 3–4 acceptance report](docs/ACCEPTANCE-0.2.md) ·
[Lua contracts](docs/LUA.md)

**Local saving is available through File > Save, Save As, and Open.** Native .julretsu files preserve inputs, formulas, row formatting, and Lua scripts. Unsaved changes can be saved before closing or replacing a workbook.
CSV and Excel (.xlsx) import/export exchange data with other spreadsheet apps; see
[what carries over](docs/DATA-EXCHANGE.md). The optional [AI assistant](docs/AI.md) uses
your own Claude or OpenAI-compatible provider and previews every edit before applying it.
This is an editable native MVP, not a complete Excel replacement.

## Implemented
- 1,000,000 logical rows × 16,384 columns (through XFD1000000), with sparse storage.
- Native arithmetic, comparisons, SUM, AVERAGE, lazy IF, and structured errors.
- Incremental dependency propagation, iterative cycle detection, atomic edits,
  sparse row styles, compact selections, and undo/redo.
- Lua formulas such as =LUA("return cell('A1') * 2"), including runtime dependency
  discovery, cycle diagnostics, computed references, and branch changes.
- Transactional Lua macros with instruction/memory/output/host-call/write limits.
- Native formula bar, coordinate box, cell editing, navigation, range and multi-row
  selection, row formatting, script editor, and status bar.
- Both-axis virtualized custom grid, DPI-aware font/style rebuilding, measured
  allocation counts and whole-frame timings.
- CSV import/export (quoted commas, quotes, multi-line cells, formula-injection-safe export).
- Excel .xlsx import/export: first worksheet, SUM/AVERAGE/IF and arithmetic formulas,
  bold, colours and decimal row formats, with an import preview of what carries over.
- Interface languages: English, Korean and Japanese, picked automatically from the system language
  and changeable under Settings; sample workbooks and reports follow the chosen language
- Optional AI assistant: Anthropic or OpenAI-compatible providers, keys in Windows
  Credential Manager, validated edit proposals reviewed before one-step, undoable apply.
- A portable Windows package with runtime DLLs and dependency license notices.

## Installers (1.1.0)
- **Windows:** `.\package.ps1` builds `release/Julretsu-1.1.0-Windows-Setup.exe`, a single-file per-user
  installer with license page, Start menu and desktop shortcuts, `.julretsu` file association and an
  uninstaller in Settings > Apps.
- **macOS and Linux:** the `Installers` GitHub Actions workflow builds `Julretsu-1.1.0-macOS.dmg`
  (universal, macOS 13.3+), `Julretsu-1.1.0-linux-amd64.deb` and `Julretsu-1.1.0-linux-x86_64.tar.gz`.
  Pushing a `v*` tag attaches them to a draft GitHub release.
- **Manuals:** `docs/Julretsu-User-Manual.pdf` (English), `-ko.pdf` (Korean) and `-ja.pdf` (Japanese),
  generated from the HTML in `docs/manual/` (screenshots come from
  `julretsu --manual-shots <folder> --language ko`). Every installer includes all three.

## Build and test on this Windows machine
```powershell
.\build.ps1 -Gui -Run
.\build.ps1 -Gui -Smoke
.\build.ps1 -Gui -Configuration Debug
.\package.ps1
```
Portable C++ tools in .tools are used automatically, without a permanent PATH
change. Already-populated pinned sources in .deps are reused. Both directories
are ignored by Git. package.ps1 targets the local LLVM-MinGW Release build;
other compiler redistributable requirements must be handled by their packager.

Headless core still builds without Lua or graphics libraries:
```powershell
.\build.ps1 -Example -Benchmarks
```

## Clean checkout / other platforms
Use CMake 3.20+ and a C++20 compiler (MSVC 2022, GCC 12+, Clang 16+, or newer).
The core needs no external SDK beyond the compiler. Native builds also need
OpenGL and platform window-system development libraries.

```sh
cmake -S . -B build-native -DCMAKE_BUILD_TYPE=Release -DJULRETSU_BUILD_GUI=ON
cmake --build build-native --config Release --parallel 2
ctest --test-dir build-native -C Release --output-on-failure
```

- On Linux/macOS run ./build-native/julretsu. For the headless convenience script,
run sh ./build.sh. On Linux, GLFW may require xorg-dev, libwayland-dev,
libxkbcommon-dev, and libgl1-mesa-dev (package names vary by distribution).
macOS requires Xcode command-line tools. Windows requires an OpenGL-capable
graphics driver; this build was tested on an NVIDIA RTX 5060.

| Option | Default | Behavior |
| --- | --- | --- |
| JULRETSU_BUILD_GUI | OFF | Build the native app; also enables Lua |
| JULRETSU_ENABLE_LUA | OFF | Build Lua adapter and integration tests |
| JULRETSU_BUILD_TESTS | ON | Register headless test executables |
| JULRETSU_BUILD_BENCHMARKS | OFF | Build engine benchmarks |
| JULRETSU_ENABLE_SANITIZERS | OFF | ASan/UBSan on non-Windows GCC/Clang |

Current targets: julretsu_core, julretsu_ai, julretsu_headless, optional
julretsu_benchmarks, julretsu_lua, julretsu_xlsx, julretsu_ui, and julretsu
(plus test executables when a local tests/ folder is present).

## Performance checks
The app supports --smoke (keyboard/mouse checks and screenshots) and --benchmark
(10,000-cell sparse fixture). Both run hidden by default; --visible shows the
window. They measure full frame work and swaps, after warmup, with VSync disabled.
Normal interactive use enables VSync. See the acceptance report for actual results
and measurement boundaries; there is no universal frame-rate guarantee.

## Source map
- include/julretsu, src: core, formulas, dependency graph, LuaEngine, GridUI,
  GridViewport, native main, and allocation instrumentation.
- tests: core contracts, Lua sandbox/dependency/macro tests, viewport bounds.
- examples, benchmarks: headless example and engine benchmarks.
- cmake/Dependencies.cmake: verified pinned upstream dependencies.
- docs: architecture, formulas, Lua, controls, file exchange, AI assistant, acceptance reports.
- .github/workflows/ci.yml: cross-platform headless/Lua builds and sanitizer job.

Read [architecture](docs/ARCHITECTURE.md), [formula semantics](docs/FORMULAS.md),
[dependencies](docs/DEPENDENCIES.md), and [remaining work](docs/ROADMAP.md).
The earlier docs/ACCEPTANCE.md is the historical Steps 1–2 report.


## Workbook editing update (0.5)
Clipboard editing, cell-level formatting, formula-aware drag fill, row/column operations,
sorting, text filters, frozen headers, multiple native worksheets, recovery copies,
and chart/print-to-PDF reports are now available. The window uses an Office-style title bar with
workbook minimize/restore/close controls and an Excel-style grouped Home ribbon.
A built-in safety net catches common spreadsheet mistakes: risky changes (large pastes, partial
sorts, overwritten formulas, out-of-scale numbers) are reviewed before you keep them, a live sheet
check flags broken formula patterns, totals that skip rows, numbers stored as text and blank rows
in tables, and every change is recorded in a per-cell history saved with the workbook. See [Quick start](docs/QUICKSTART.md)
for controls and current compatibility limits. Native files preserve the new formatting
and worksheet features; Excel export currently remains limited to the active sheet.
