# Welcome to Julretsu

Double-click Julretsu.exe (or Start Julretsu.cmd in the project folder).
The example budget is editable. New workbook starts a blank workbook.

## Save locally
- File > Save (Ctrl+S): choose a filename and folder the first time; update that file afterward.
- File > Save As (Ctrl+Shift+S): save a separate copy in another location or with another name.
- File > Open (Ctrl+O): reopen a .julretsu workbook.
- Windows asks before overwriting an existing file. Cancel leaves the current workbook open.
- Closing, starting New, or opening another workbook prompts to save modified work.
- The header shows the workbook name and whether it has unsaved changes.

The native .julretsu format keeps cells, formulas, row formatting, and the Lua script. File dialogs currently support Windows.

## Exchange files with Excel and other apps
- File > Import CSV / Export CSV (values): UTF-8 CSV with quoted commas, quotes and multi-line cells.
- File > Import Excel (cached values or supported formulas) / Export Excel (.xlsx): the first worksheet,
  SUM/AVERAGE/IF and arithmetic formulas, bold text, colours and 0/2 decimal row formats.
- Imports replace the current workbook (you are asked to save first). Excel imports show what will
  and will not carry over before anything loads. Keep a .julretsu copy for full fidelity.
  Details: docs/DATA-EXCHANGE.md.

## Ask AI (optional)
- Open Ask AI on the Home ribbon, or AI assistant in the left panel.
- Under Connection choose Anthropic (Claude) or an OpenAI-compatible server (OpenAI, Ollama,
  LM Studio), enter the model, and save your API key. The key is kept by Windows Credential
  Manager, never in workbooks. Usage is billed by your provider.
- Describe a change, choose Propose edits, then review each edit's before and after values.
  Untick anything you don't want and choose Apply. Ctrl+Z undoes the whole proposal.
- The panel says how many cells will be sent and to which server before you send.
  Details: docs/AI.md.

## Edit your sheet
- Click a cell; double-click or press F2 to edit it.
- Enter applies an edit; Escape cancels.
- The formula bar shows the original input. Use its Apply button to commit.
- A leading apostrophe forces literal text; = introduces a formula.
- Arrow keys, Tab, Page Up/Down move the active cell.
- The coordinate box jumps directly to a cell, including XFD1000000.
- Ctrl+Home jumps to A1; Ctrl+End jumps to XFD1000000.
- Mouse wheel scrolls rows; Shift+wheel scrolls columns.
- The bottom scrollbar controls the column offset; use the mouse wheel for rows.
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
