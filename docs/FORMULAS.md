# Native formula dialect

Set formulas explicitly with FormulaInput{"=SUM(A1:A3)"}. Ordinary std::string input
is always literal text, even if it begins with "=" or contains Lua source.
The core never guesses formula intent.

Supported syntax:
- Decimal numbers including scientific notation; quoted strings; TRUE and FALSE.
- Case-insensitive A1 references from A1 through XFD1000000.
- Inclusive rectangles, including reversed endpoints normalized to a rectangle.
- Parentheses, unary +/-, binary +, -, *, /.
- Comparisons =, <>, !=, <, <=, >, >=.
- Case-insensitive SUM, AVERAGE, IF; reserved LUA.
- Double a quote inside a quoted string: ="say ""hi""" yields say "hi".
  Backslashes are literal, not escapes. Text bytes are preserved; Unicode
  normalization, locale parsing, and UTF-8 validation are not implemented.

Precedence (highest first): parentheses/calls; unary signs; multiplication/division;
addition/subtraction; comparisons. Binary operators are left-associative.
An AST is built once on edit; rendering must never parse or evaluate.

Arithmetic treats empty as zero and Boolean as 1/0; text gives #VALUE!.
Comparisons use numeric coercion for non-text operands; two text values compare
case-sensitively by byte sequence. Mixing text with non-text gives #VALUE!.

SUM and AVERAGE require at least one argument. Numeric values count; empty, text,
and Boolean values are ignored in both ranges and scalar arguments. This is an
intentional MVP dialect choice. Any consumed error propagates. AVERAGE with no
numbers returns #DIV/0!. SUM with no numeric values returns 0.
A range is not a scalar and produces #VALUE! outside aggregate arguments.

IF requires exactly three arguments and a numeric or Boolean condition.
Zero/FALSE selects the third argument; other numbers/TRUE select the second.
Only that branch evaluates, but dependency extraction includes both branches.
Thus =IF(FALSE,1/0,42) is 42, whereas =IF(FALSE,A1,42) in A1 is a static cycle.

Numeric literals and results must be finite; overflow gives #NUM!, division by
zero gives #DIV/0!. Explicitly supplied nonfinite literal inputs reject a batch.

| Code | Display | Meaning |
| --- | --- | --- |
| Ref | #REF! | Invalid or out-of-bounds reference |
| Cycle | #CYCLE! | Actual member of a circular component |
| CycleDependency | #CYCLE-DEPENDENCY! | Propagated circular dependency, origin retained |
| Name | #NAME? | Unknown function |
| Value | #VALUE! | Invalid type or argument count |
| DivZero | #DIV/0! | Division by zero or empty numeric average |
| Num | #NUM! | Invalid numeric literal or non-finite result |
| Parse | #PARSE! | Malformed syntax |
| Limit | #LIMIT! | Resource limit |
| Unsupported | #UNSUPPORTED! | Reserved integration not implemented |

With Lua enabled and a LuaEngine attached to Sheet,
=LUA("return cell('A1') * 2") executes in a fresh, bounded environment and tracks
runtime cell dependencies. See LUA.md for the allowlist, resource limits, and
discovery/retry contract. A core-only Sheet without an extension still returns
#UNSUPPORTED! for LUA. The native app attaches LuaEngine.
