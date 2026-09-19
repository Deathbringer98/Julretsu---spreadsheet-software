# Steps 3–4 acceptance report

Date: September 18, 2026. Supersedes the future-only Lua/UI statements in the
historical Steps 1–2 acceptance report.

## Delivered
LuaEngine.hpp/.cpp, dynamic dependency discovery in the core, integration tests,
a native ImGui/GLFW/OpenGL3 application, GridUI.hpp/.cpp, GridViewport.hpp,
allocation instrumentation, build/package scripts, a Windows portable executable,
runtime DLLs, and upstream license notices.

The GUI supports formula editing, coordinate jumps, keyboard navigation,
rectangular and multiple-row selection, row formatting, clearing, undo/redo,
and a transactional macro editor. Mutation/recalculation runs before each ImGui
frame, not inside grid submission.

## Local validation
Windows 11 Home 10.0.26200, Intel Core i5-14400F, NVIDIA GeForce RTX 5060,
Clang 23.1.1 (LLVM-MinGW/UCRT), Ninja 1.13.2.
The complete native application was built using CMake 3.20.6.
Release and Debug configurations passed CTest and native smoke checks; the
Debug run kept ImGui assertions enabled. The updated core-only configuration
also passed with Lua/GUI disabled on CMake 3.20.6.
The portable Windows package passed the native smoke and benchmark runs with
PATH reduced to Windows system directories, confirming that compiler installation
paths are not needed to launch it. Raw logs and PNG screenshots are in artifacts.

CTest passed:
- Core: 38,724 checks across nine suites.
- Lua: 86 integration checks.
- Viewport: both-axis clipping/reveal and coordinate checks through XFD1000000.

Lua tests cover computed addresses, changed branches, obsolete edges, cleared
precedents, dirty dependency ordering, self/mixed/Lua cycles, downstream provenance,
cycle recovery, missing restricted APIs, instruction and actual memory exhaustion,
non-finite output, isolated state, macro rollback/undo/redo, write caps, and the
64-pass dependency discovery rejection.

Native smoke tests inject actual ImGui key/mouse events:
- F2 edits C4 from 3 to 12; E4 recalculates to 5400.
- Ctrl+Z restores E4 to 1350; Ctrl+Y restores 5400.
- Run macro button applies the sample script; E4 becomes 5850.
- Undo restores the whole macro; E4 returns to 5400.
- Escape cancels a replacement edit.
- Entering a Lua formula in the formula bar returns 16124 as expected.
- The Bold rows button updates the active row's style.
- A jump to XFD1000000 and viewport hit-testing agree.
- Screenshots of the workspace and the final cell were visually reviewed.
  A partial-final-row clipping issue found during review was corrected.

## Measured render performance
Diagnostic run: Release, 1440 × 900 framebuffer/window, scale 1.0, hidden GLFW
window, OpenGL renderer NVIDIA GeForce RTX 5060/PCIe/SSE2, VSync off.
Timing includes event polling, pre-frame preparation, ImGui backend/NewFrame,
all application drawing, ImGui::Render, OpenGL submission, and buffer swap.

Sparse fixture: 10,000 populated cells across every 100th row, 1,000,000 logical
rows, 16,384 logical columns. After 60 warmup frames, 340 samples:
| Metric | Observed |
| --- | ---: |
| Full-frame p50 | 0.4562 ms |
| Full-frame p95 | 0.7144 ms |
| Full-frame p99 | 0.8247 ms |
| Warm C++ allocation calls, all measured frames | 0 |
| Warm ImGui allocation calls, all measured frames | 0 |
| Warm drawing C++ allocation calls | 0 |
| End-of-run process working set | 73,240,576 bytes |

The final Release scripted smoke fixture has 52 populated cells. After 80 warmup
frames, 70 measured frames: p50 0.5431 ms, p95 0.8841 ms, p99 1.0053 ms, and zero
tracked warm C++/ImGui allocation calls. Its process working set was 73,744,384 bytes.
The Debug smoke check also passed, with p50 0.9355 ms and zero tracked warm
allocations; Debug timing is not the performance target.

These measurements support the unchanged-draw allocation goal for these fixtures,
not every input, machine, window size, or graphics driver. The run is hidden and
VSync-free; it is not a user-visible 60 FPS certification. Edits, parsing, script
execution, resize/DPI changes, buffers/font growth, and startup allocate.
OS/driver and arbitrary C-library internal allocations are not intercepted.
Memory is the whole process working set, not isolated sheet storage.

## Remaining verification
Cross-platform CI is supplied but has not run remotely. Linux/macOS builds,
multi-monitor DPI changes, alternative GPUs, and sanitizer results are unverified
in this Windows session. Initialization failures have explicit error paths, but
not every driver/platform failure has been fault-injected.

## Known product limitations
No workbook persistence, CSV/XLSX implementation, AI connections, multiple sheets,
charts, pivots, collaboration, or Excel-compatibility guarantee.
Rows have basic formatting; there is no merged-cell or per-cell style editor.
The formula/cell editor holds at most 16 KiB; larger existing cell text remains
preserved and cannot be silently truncated by editing.
Cells render bounded previews, so long text is clipped within the cell.
Selections exceeding 10,000 rows must be formatted in smaller batches.

Runtime dependency discovery is deliberately bounded to 64 passes; scripts reading
many newly discovered cells can reject a batch. Snapshot-based undo/transactions
still copy populated data and graph metadata. See ARCHITECTURE.md and LUA.md.

All source remains local and MIT licensed. No GitHub publishing or installer
registration was performed.
