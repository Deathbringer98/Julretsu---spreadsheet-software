# Steps 1–2 acceptance report

Date: September 18, 2026.

## Delivered
Original MIT-licensed C++20 source, modular CMake, verified future dependency pins,
headless example, sparse row storage and styles, compact selections, staged
transactions, undo/redo, native tokenizer/parser/AST/evaluator, forward/reverse
dependencies, iterative SCC diagnosis, Kahn recalculation, and future-only I/O
interfaces. A fake scalar evaluator tests the reserved formula extension boundary.

## Actually built and run
Platform: Microsoft Windows 11 Home, version 10.0.26200.
CPU: Intel Core i5-14400F, 10 cores / 16 logical processors.
OS-reported visible memory: 16,607,684 KiB.
Compiler: LLVM-MinGW Clang 23.1.1, UCRT x86_64.
Build tools: Ninja 1.13.2, CMake 4.4.3 and CMake 3.20.6.

- Release: configured and compiled successfully, no compiler warnings in the final build.
- Debug: configured/compiled/tested using CMake 3.20.6.
- CTest: 1 registered executable, 9 internal suites, 38,724 checks, zero failed suites.
- Release checks stay active under NDEBUG (no assert-based test framework).
- PowerShell one-command build, test, example, and benchmarks all ran successfully.
- Headless example printed B1 = 20, then B1 = 14, with only 2 populated cells.
- Headless builds fetched no Lua, sol2, ImGui, or GLFW dependencies.

## Acceptance scenarios
| Scenario | Local result |
| --- | --- |
| A1/AA1, final coordinate, invalid/overflow references | Passed |
| Absent reads do not grow storage | Passed |
| Precedence, nested calls, quoted strings, comparisons | Passed |
| SUM/AVERAGE mixed ranges and errors | Passed |
| Lazy IF with unused failing branch | Passed |
| Incremental A1/B1 edit | Passed |
| Replacement removes obsolete edges | Passed |
| Clear retains dependent subscriptions | Passed |
| Self-, multi-cell, and range cycles | Passed |
| Actual cycle vs downstream provenance | Passed |
| Cycle recovery | Passed |
| Blank cell edits inside referenced ranges | Passed |
| Atomic rejection and duplicate last-write-wins | Passed |
| 10,000-cell chain, cycle, and recovery | Passed |
| Oversized formula, range, depth, graph, and evaluation limits | Passed |
| Unrelated formulas retain revision and are not evaluated | Passed |
| Undo/redo, row selection clearing, sparse row styles | Passed |
| Fake extension and default unsupported LUA | Passed |
| Seeded DAG edits against independent value oracle | Passed |
| Seeded cyclic graphs against transitive-closure oracle | Passed |

Random fixture seed is 73891. The DAG test uses 30 graphs of 60 cells and 15 edits
per graph. The cyclic oracle checks 50 graphs of 20 cells. The long-chain test
covers 10,000 populated cells, not a million populated formulas.

## Observed benchmark results
Final documented PowerShell run, Release, seven samples per operation:

| Fixture | p50 (ms) | p95 (ms) |
| --- | ---: | ---: |
| Borrowed traversal of 10,000 sparse cells | 0.0981 | 0.1635 |
| 100,000 absent-cell reads | 0.9423 | 1.0708 |
| 1,000-cell batch in a 10,000-cell sheet | 3.6396 | 4.0615 |
| 10,000-cell dependency chain recalculation | 24.8674 | 27.6738 |

Logical dimensions: 1,000,000 rows × 16,384 columns.
Sparse fixture: 10,000 populated cells across every 100th row.
Chain fixture: 10,000 populated cells with 9,999 edges.
Undo is disabled in these performance fixtures; normal editing defaults to eight
snapshots and can be materially more expensive. The batch test includes copying
the whole populated candidate state. Seven-sample nearest-rank p95 is the maximum;
these are small diagnostic fixtures, not a statistically rigorous performance SLA.
Timing varies with background load, allocator state, and hardware.

Process memory and allocation counts are not instrumented. No UI exists, so no
frame-time, scrolling, DPI, or zero-allocation rendering claim has been verified.

## Reviewed/prepared, not locally verified
- Windows/Linux/macOS CI configuration is present; it has not run on a hosted runner.
- Linux and macOS builds and ASan/UBSan are unverified in this Windows session.
- Future dependency recipes/source lists have been reviewed, but GUI/Lua targets
  have not been compiled, linked, or executed.
- POSIX convenience script is supplied but was not executed on Linux/macOS.
- Cross-platform success is not inferred from these Windows results.

## Known limitations
This deliverable is an engine foundation, not a usable windowed Excel replacement.
It cannot yet open/save CSV or XLSX, persist workbooks, execute Lua, or connect
to AI providers. The current extension interface supports pure scalar evaluation;
dynamic scripting reads require Step 3's dependency-discovery protocol.

Full-state transaction snapshots, duplicated literal text caches, map-node
overhead, and bounded range expansion limit scalability. No large-population
million-row benchmark was run. Formula semantics are intentionally narrower
than Excel; no dates, formatting-aware evaluation, multi-sheet references,
structural reference rewriting, or localization.

AST/evaluation recursion is bounded; graph traversal is iterative. Input/resource
limits are size/work caps, not real-time guarantees. Arbitrary third-party C++
extensions are trusted cooperative code, not sandboxed by this engine.

The source is ready for local development. It has not been published to GitHub
or packaged as an installer. Per the supplied brief, Steps 3 and 4 require the
user's confirmation.
