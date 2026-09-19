# Local workbook saving — 0.4.0

File menu: New, Open, Save, Save As; shortcuts Ctrl+N/O/S and Ctrl+Shift+S. Native Windows dialogs use the application window as owner, support Unicode paths, and request overwrite confirmation. Save returns false on cancellation or error, and close/replacement stops. Pending cell edits are committed when saving. Lua editor changes also mark a workbook modified.

Version 1 .julretsu files store original typed cell inputs, formulas, row styles (including empty styled rows), and the Lua editor script. Cached formula results are recalculated on opening; scripts are not automatically executed. Workbook inputs are bounded by the existing core limits, file size by 40 MiB, and Lua editor text by 16 KiB. Loading validates the whole file and applies it to a replacement Sheet before switching the UI. Loading clears undo history. CSV/XLSX, multiple sheets, and recent-files lists are not part of this update.

Saving writes an exclusively created sibling temporary file, flushes it, and replaces the destination only after the full write succeeds. Existing file replacement uses Windows MoveFileExW with write-through. Filesystem failure is reported without marking the workbook saved. No crash recovery/autosave is claimed.

Verification: versioned file round trips (numbers, Booleans, literal strings, formulas, blank row styling, final coordinate, Unicode paths, Lua script), overwrite, blank files, every truncated prefix of the fixture, malformed headers/coordinates/types, oversized script rejection, failed destination replacement and cleanup. Native smoke checks save and reopen Lua formulas/formatting and use Ctrl+S to save an unfinished cell edit. Existing core, viewport, Lua and native UI checks also pass. Native dialog cancellation and overwrite prompts follow Win32 standard behavior; their manual interaction was not automated.
## Windows file icons (0.4.1)
.julretsu files are registered for the current user as Julretsu Workbook, with the original app-icon.ico. The quoted open command passes the workbook to Julretsu using --open. Unicode filenames use the Windows wide-character command line. Existing UserChoice overrides are respected.

On another PC, run Register-Julretsu.ps1 beside the portable Julretsu.exe to register that location. Keep the package in place. Later packaging updates an existing Julretsu registration to the latest package.

Verified: all four test suites and native smoke; packaged startup opening a filename with spaces and Japanese characters; Windows AssocQueryString resolving the correct .julretsu icon and open command. Original artwork remains unchanged.
