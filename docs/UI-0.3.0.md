# Julretsu 0.3.0 interface update

The native interface now follows the supplied reference: mint and slate colors, an icon ribbon, left workbook navigation, a bordered formula bar, selected row/column headers, status badges, a sheet tab, and zoom. Original app-icon.png, app-icon.ico, and loading-banner.png are preserved. The header displays the original app icon.

Dark mode is available at the top right and under Settings. Appearance is stored in %LOCALAPPDATA%/Julretsu/appearance.txt on Windows, or the XDG configuration directory on Linux. JULRETSU_SETTINGS_PATH overrides the location for tests. Errors saving a preference appear in the status bar. Switching appearance does not alter workbook data.

The ribbon exposes available functions: new workbook, undo, redo, bold/fill/reset row formatting, decimal places, clear contents, formula help, and Lua scripts. Templates offers the existing project budget after confirmation. Ctrl+K jumps to a cell. The reference's sharing, charts, and file-saving features are not implemented; the app continues to label workbooks as in-memory.

Validation: Release and Debug builds; all three core/viewport/Lua test suites; native keyboard edits, cancellation, undo/redo, macro execution, Lua formula entry, row formatting, final-cell hit testing, dark-mode button; both appearance settings written and read by fresh UI instances using an isolated settings path. Light and dark framebuffer captures reviewed. No steady-state C++ or ImGui allocations in the smoke samples. Packaged PNG hashes exactly match the user-supplied originals.