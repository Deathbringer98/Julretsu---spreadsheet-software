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

## Title bar and ribbon
- The top two rows are Julretsu's title bar. Drag any blank part to move the window, double-click it
  to maximize, or drag it to a screen edge to snap. Minimize, maximize and close are at the top right;
  Save, Undo and Redo sit beside the menus. Start with `--system-titlebar` to use the standard Windows
  title bar instead.
- Beside the ribbon tabs: collapse or show the ribbon, open help, and control the workbook itself:
  minimize it to a small bar, restore it into a movable window inside Julretsu, maximize it again,
  or close it (you are asked to save first; a blank workbook opens).
- The Home ribbon follows Excel's layout:
  - **Clipboard:** Paste (or Paste values), Cut, Copy.
  - **Font:** font, size, bigger/smaller, bold, borders, fill colour and font colour.
  - **Alignment:** left, centre, right or automatic.
  - **Number:** number format, currency ($1,234.00), percent, comma style (1,234.00), fewer/more decimals.
  - **Styles:** cell styles (Good, Bad, Neutral, Title, Headings, Total, Input, Note, Accent) and table styles.
  - **Cells:** insert/delete rows, columns and worksheets, and cell formatting.
  - **Editing:** AutoSum, Fill, Clear (all, formats or contents), Sort & Filter, Find & Select.
  - **Assist:** Ask AI, functions, Lua scripts and appearance.
- Home formatting applies to the selected cells, like Excel. If you selected whole rows with the row
  headers, it formats those rows instead. Every change can be undone with Ctrl+Z.

## Safety net: catch mistakes before they cost you
- **Review before keeping risky changes.** After a big paste, fill, clear or Lua macro, a sort that
  leaves neighbouring columns behind, deleting a row or column that held data, overwriting formulas,
  or entering a number wildly out of scale, Julretsu shows **Review this change**. The result is
  already visible in the sheet. **Keep change** (Enter) accepts it; **Undo change** (Esc) puts
  everything back exactly. Set the size limit, or turn reviews off, on the **Review** tab.
- **Instant typo check.** Typing a number far larger or smaller than the rest of its column (extra
  zeros, a slipped decimal point) shows a warning in the status bar right away. Ctrl+Z undoes it.
- **Sheet check.** The badge at the bottom-left counts problems. Click it, or choose Review > Check
  sheet, to see them. It looks for:
  - a typed number, or a different formula, inside a column of matching formulas;
  - SUM or AVERAGE ranges that stop short of neighbouring numbers;
  - out-of-scale numbers;
  - numbers stored as text (which totals silently skip);
  - blank rows that split a table (which break sorting and filtering).

  Each problem has **Go to**, a one-click **fix** where one is safe, and **Ignore**. Fixes can be
  undone like any edit.
- **Cell history.** Right-click a cell, or choose Review > Cell history, to see every change to that
  cell: when, what action, and the value before and after. Review > Change log lists every change on
  the sheet. History, including undo and redo, is saved inside the .julretsu file (the latest 1,000
  changes; very large changes list their first 200 cells). Clear it from the Change log if you share
  the file and don't want the history included.
- Workbooks saved by this version use file format 4, which records history. Older versions of
  Julretsu cannot open them; this version still opens older files.

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

## Workbook tools (0.5)
- Edit menu or Ctrl+C/X/V: copy, cut, paste; Ctrl+Shift+V pastes values. Select a rectangular range, up to 10,000 cells. Internal copies preserve types and cell formatting; external text uses tab-separated rows with quoted multiline cells. External formulas stay literal until explicitly edited. Cut clears source cells only after successful paste within the same sheet; it does not rewrite formulas elsewhere that point at the moved cells.
- Format > Format selected cells: choose sans/serif/monospace, size, bold, text/fill colours, borders, alignment, numbers, dollar currency, percentages or Excel-serial dates. Native saving preserves cell formatting. Row-format commands remain separate.
- Drag the small selection-corner handle down or right to extend formulas or patterns. Two numeric seed cells extend a sequence. Relative A1 references adjust; $ anchors remain fixed. Fill is bounded to 10,000 cells.
- Data: insert/delete the active row or column, sort a rectangular selection using its active column, or filter by text in the active column. Exclude the header from sorting. Filters retain row 1; clear filtering before bulk edits to protect hidden rows.
- Structural edits update ordinary references. Deleting a referenced row/column is rejected; workbooks with dynamic SHEET/LUA references require those references to be removed first. These limits prevent silently incorrect formulas.
- View: freeze first row, first column, or both; show formulas; toggle gridlines.
- Workbook > Manage sheets: add, duplicate, rename, move, delete or switch sheets (up to 64). The sheet button also opens the sheet switcher. Use =SHEET("Budget","A1") to reference another worksheet. Cycles and excessive evaluation return errors. Lua through cross-sheet links is unsupported. Sheet deletion is confirmed and cannot be undone.
- Workbook: after a minute of unsaved changes, Julretsu keeps a crash-recovery copy (including unfinished cell input) and refreshes it every minute. If Julretsu closes unexpectedly, a recovery prompt appears at the next start: Restore, then Save As to keep the recovered work. The copy is deleted after a successful save, a normal exit, or choosing Keep or Restore. It is one rolling copy per Windows user, not a version history.
- Reports > Chart / print selected range: bar or line chart from the active numeric column, title, portrait/landscape and repeating first row. Print includes chart and table; choose Microsoft Print to PDF to save PDF. Reports are snapshots rather than saved chart objects. Maximum 12 columns / 10,000 cells; long printed text is ellipsized.

Use native .julretsu saving to preserve multiple sheets and individual-cell formatting. Older native files open; new files require this version. CSV and Excel export still target the active sheet and retain their previously documented format limits (new cell-level formatting is not exported to Excel yet). Theme, filter, freeze and chart-preview choices are session-only.
