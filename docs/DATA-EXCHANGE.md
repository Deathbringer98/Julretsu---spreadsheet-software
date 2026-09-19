# CSV and Excel import/export

Both formats are reached from the File menu. Imports build a complete replacement sheet
first; a malformed file is rejected with a message and the current workbook is untouched.
Before an import replaces modified work, Julretsu asks whether to save it.
Imported workbooks are unsaved until you save them as .julretsu.

## CSV (UTF-8)
| | Supported |
| --- | --- |
| Quoting | Commas, doubled quotes (`""`) and line breaks inside quoted fields |
| Line endings | CRLF, LF and CR |
| Encoding | UTF-8, with or without a byte-order mark. Invalid UTF-8 or NUL is rejected |
| Import types | Numbers and TRUE/FALSE are detected; everything else is text |
| Formulas | Never activated on import: `=SUM(A1:A2)` stays text |
| Export | Calculated values, every field quoted, CRLF rows |
| Export safety | Text starting with `= + - @`, tab or CR gets a leading apostrophe, so other spreadsheet apps do not run it as a formula |
| Limits | 32 MiB file, 1 MiB per field, 100,000 populated cells, 2,000,000 exported fields |

CSV has no formatting, formulas or scripts. Save as .julretsu to keep them.

## Excel (.xlsx)
Import reads the **first worksheet** only. The preview before loading lists exactly what
carries over and what does not.

| Feature | Import | Export |
| --- | --- | --- |
| Numbers, text, TRUE/FALSE | Yes (shared and inline strings) | Yes |
| Formulas | "Cached values" mode: Excel's last calculated results. "Supported formulas" mode: arithmetic, comparisons, SUM, AVERAGE and IF become live formulas; anything else keeps its cached value | Arithmetic, comparisons, SUM, AVERAGE, IF. Lua and other formulas export as values |
| Errors | Imported as text | Excel error codes; Julretsu-only errors become #VALUE! |
| Bold, text colour, fill colour | Yes, when a whole row shares one style | Yes (row styles) |
| Decimal places | Excel formats 0 and 0.00 | 0 or 2 places |
| Dates / custom number formats | Stay as Excel serial numbers | — |
| Merged cells, charts, column widths, other sheets, comments | No | No |
| Lua scripts | Never activated from .xlsx | Not included |

Limits: 64 MiB file and expanded content, 32 MiB per XML part, 2,048 ZIP parts,
100,000 cells, 4,096 styles. Encrypted workbooks, external worksheet links and XML DTDs
are rejected.

Verified: a Julretsu export opens in Microsoft Excel without a repair prompt, with
formulas, multi-line text, Booleans, bold and fill colours intact. The copy Excel saves
back imports into Julretsu with the live formula and formatting.
