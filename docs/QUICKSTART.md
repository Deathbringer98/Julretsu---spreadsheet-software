# Welcome to Julretsu 0.2

Double-click Julretsu.exe (or Start Julretsu.cmd in the project folder).
The example budget is editable. New sheet starts a blank workbook.

**This milestone keeps workbooks in memory. Saving, CSV/XLSX import/export, and
AI connections are not implemented yet. Closing the window or starting a new
sheet discards changes after confirmation. Do not use it for your only copy of
important work.**

## Edit your sheet
- Click a cell; double-click or press F2 to edit it.
- Enter applies an edit; Escape cancels.
- The formula bar shows the original input. Use its Apply button to commit.
- A leading apostrophe forces literal text; = introduces a formula.
- Arrow keys, Tab, Page Up/Down move the active cell.
- The coordinate box jumps directly to a cell, including XFD1000000.
- Ctrl+Home jumps to A1; Ctrl+End jumps to XFD1000000.
- Mouse wheel scrolls rows; Shift+wheel scrolls columns.
- Bottom sliders control the logical row and column offset (zero-based).
- Shift-click extends a rectangular selection.
- Click row headers to select rows; Ctrl-click adds/removes rows and Shift-click
  extends a row range.
- Bold rows, Tint rows, and Reset style apply to selected rows.
- Delete/Clear contents clears selected content. Undo/Redo restores changes.
- Ctrl+Z / Ctrl+Y undo and redo while the grid has focus.

## Native formulas
Examples:
```text
=SUM(E4:E9)
=AVERAGE(A1:A10)
=IF(B1>0,B1*2,0)
=LUA("return cell('A1') * 2")
```
SUM and AVERAGE ignore text, empty cells, and Boolean values. Lua reads a blank
cell as 0. Lua receives numeric cells as floating-point numbers; use math.floor
when computing an integer address, for example:
```text
=LUA("return cell('B' .. math.floor(cell('A2') + 1))")
```

## Lua macros
Open Lua scripts. A macro can read cells and queue changes:
```lua
for row = 4, 9 do
  set('C' .. row, cell('C' .. row) + 1)
end
```
Run macro applies all writes in one undoable transaction. Macro reads see the
sheet as it existed before the macro, not previous queued writes. Use local Lua
variables to accumulate intermediate results. Strings passed to set stay literal
text; they never implicitly execute as formulas. set(address, nil) clears a cell.

Only a small allowlist of Lua functions is available. There is no filesystem,
process, network, module loading, debug, pcall/xpcall, or shared global state.
Instruction, memory, output, host-call, row, and write limits bound execution.
A failed macro leaves the sheet unchanged.

The optional Connect Your AI feature and Excel file compatibility are future
milestones. There are no API keys, remote calls, or paid inference in this build.
