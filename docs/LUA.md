# Lua execution and dependency contract (0.2)

LuaEngine implements the core FormulaExtension through a borrowed sol2 state_view
over a unique lua_State owner. The Lua C API is used for allocator hooks,
instruction hooks, protected calls, environment construction, and host callbacks.
This avoids C++ temporaries remaining live across Lua longjmp error paths.
No core header includes sol2 or Lua.

## Formula syntax
=LUA("return cell('A1') * 2")

Exactly one script string is required. Return a finite number, Boolean, string,
or nil (empty). Other returned types produce a typed value error. Empty read cells
become numeric zero; text/Boolean/error values retain their respective semantics.
Spreadsheet numbers enter Lua as floating-point numbers. For computed addresses,
use math.floor to produce an integer, e.g. 'B' .. math.floor(cell('A2') + 1).

Each invocation gets a new state and isolated _ENV. Values written to script globals
cannot affect other invocations. Arithmetic follows Lua 5.4 semantics, including
integer arithmetic for integer literals; it is not an Excel numeric compatibility
layer. math floating-point results can differ slightly across platform libm builds.

## Dynamic dependencies: suspend, order, retry
A core-provided read callback tracks exact runtime addresses. When a script reads
an unknown precedent, that read returns an internal suspension error. Execution
stops; the core adds the edge and reruns SCC/Kahn ordering before retrying the
formula. No stale cached value is supplied for that newly discovered read.
Known dirty precedents are ordered ahead of their dependents. Only a stable
candidate graph/value state can be committed.

At most 64 discovery passes are allowed per transaction. A straight-line script
can therefore discover up to 63 new addresses before its final successful pass.
Exceeding the retry cap rejects the entire batch with a resource error. Repeated
script execution is safe because formula scripts cannot mutate host state.

Dirty closure is computed using old runtime subscriptions first. Runtime edges
for the affected formulas are then cleared, retaining their native static edges.
Reads are rediscovered, so changed branches remove obsolete subscriptions and can
recover from an old dynamic cycle. Within one transaction discovered edges grow
monotonically. SCC detection classifies native/Lua, Lua/native, and Lua/Lua cycles;
cycle members and downstream errors remain distinct.

The graph has per-formula dependency and total-edge caps. Native expressions still
conservatively extract both IF branches. Lua runtime branches track the reads that
actually execute. There is no regular-expression dependency inference.

## Restricted environment
Allowlisted base functions: assert, error, type, tonumber, ipairs, select.
Allowlisted math: abs, ceil, floor, max, min, sqrt, fmod, sin, cos, tan, log, exp, pi.
Allowlisted string: len, sub, byte, char, lower, upper.

cell is read-only. set exists only in macro environments. No _G, filesystem,
process, network, package/require, load/loadfile/dofile, debug, coroutine, metatable
APIs, random/time, pcall/xpcall, pairs/next, or unrestricted tostring are exposed.
The global string metatable is removed so string-method syntax cannot bypass
the explicit string allowlist. Only text chunks are accepted.

Instruction errors cannot be swallowed with pcall/xpcall because those functions
are absent. Memory exhaustion is sticky even if Lua's allocator retries after GC.
Host callback errors retain a core typed error, preventing script error text from
changing the meaning of a dependency failure. Source errors remain editable.

## Default limits
| Resource | Default |
| --- | ---: |
| Lua state memory | 4 MiB |
| Instructions | 200,000 per invocation, further limited by formula work budget |
| Source bytes | 16,384 |
| Returned text / total macro text bytes | 16,384 |
| Host callbacks | 2,048 |
| Macro writes | 1,024 |
| Distinct macro rows | 256 |
| Dynamic discovery passes | 64 per transaction |

Native formula parsing separately caps formula bytes at 8 KiB. Limits include
hard constructor maxima. Lua compilation/state setup is memory-bounded and
source-length-bounded but is not counted by the VM instruction hook.

The Lua allocator cap counts Lua allocations, not C++ graph/storage allocations.
Host callbacks, inputs, output, and writes have separate bounds. Frame allocation
instrumentation is distinct from the Lua allocator.

This is in-process resource restriction, not operating-system isolation against
bugs in Lua, sol2, the C++ host, or the graphics stack. It is not a real-time
execution deadline. A discovery transaction can run a script repeatedly up to
the documented retry cap.

## Macros
run_macro evaluates against the committed pre-macro sheet and queues writes.
Earlier queued writes are not visible to cell() during the same macro. Duplicate
writes use the core's last-write-wins behavior. Strings are literal; there is no
implicit formula creation or recursive script execution from a string written by
a macro. set(address, nil) clears a cell.

On success, one Sheet batch commits and recalculates, and one undo restores all
edits. Script errors, resource exhaustion, invalid addresses/types, or a rejected
core transaction leave the sheet unchanged. Macros may return scalar status data
but do not print or emit unbounded logs.
