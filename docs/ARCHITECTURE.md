# Engine contracts

## Ownership and data layout
A Sheet is owned and mutated by one thread. It owns an ordered map of populated
rows, each an ordered map of populated columns. This favors sparse row traversal
and predictable ordering without allocating empty rows. CellCoord fields are
zero-based; every Sheet mutation checks bounds. Reads outside bounds return #REF!.
Raw coordinates are value types; construction itself does not validate them.

Each Cell owns its original typed input, optional flat AST, cached typed value,
revision, dirty flag, and runtime precedent set. AST nodes use indices into a vector: there is no pointer
tree and no recursive graph ownership/destruction. Input and evaluated value are
separate variants; formula errors never become ordinary text. Literal text is
currently copied into the value cache, and transactions copy payloads. This is an
explicit memory cost, not a claim that all text is stored once.

No core header includes Lua, sol2, ImGui, GLFW, or a platform SDK.

## Views and selection
row_view and populated_rows borrow const map storage. Cell pointers and all row
views are valid only until the next successful nonempty commit, undo, redo, or
Sheet destruction. Treat even unrelated commits as invalidating all views.
Rejected or empty batches leave views valid. Never hold a view across mutation.
read returns a value copy (text can allocate); cell returns a const borrowed pointer.

RowSelection validates, sorts, and merges inclusive intervals. Selection creation
costs O(K log K); contains uses binary search. It never copies cells.
Zero-copy means borrowed row traversal and selection do not copy cell payloads.
It does not mean parsing, mutations, returned Value copies, history, or edits are
allocation-free. std::span is used only for contiguous extension arguments.

Row styles live in a separate sparse map; default styles are erased. Formatting
a blank row allocates one style entry, not a row of empty cells. Clearing row
contents leaves row formatting intact. Basic style data includes bold, colors,
and decimal places; rendering these properties is a later milestone.

## Transactions and publication
apply validates bounds, input budgets, numeric literals, and styles. All duplicate
edits must be valid. Last occurrence per cell or styled row wins. Valid formula
input containing a parse, reference, or formula-resource error is retained and
committed as an editable typed error. Oversized stored text, coordinates, batch
size, total cell/edge budgets, and nonfinite literals reject the whole batch.

A candidate State is copied, edited, recalculated, and validated before publication.
Rejected batches leave values, graph, history, and revision unchanged. No externally
visible mutation occurs until the candidate is complete. Allocation failure during
apply returns a resource error where allocation of that small diagnostic succeeds;
catastrophic exhaustion is not a guaranteed recoverable operating-system event.
Custom extensions must have no external side effects.

Every successful nonempty batch advances the Sheet revision once, even a repeated
identical input. changed is sorted and contains explicitly edited coordinates and
transitively dirty coordinates, including cleared cells; it is an invalidation
list, not a guarantee that every value differs. styled_rows reports row invalidation.
Unrelated cell revisions remain unchanged. Empty batches do not advance revision.

Undo/redo swap complete committed snapshots, including graph, styles, and caches;
they advance the public revision and invalidate all views. They do not rerun formulas.
History depth defaults to 8; 0 disables it. New commits discard redo. Undo/redo
allocation failures can throw before mutation. Snapshots are a correctness-first
choice; copy-on-write row blocks or a validated edit journal are future alternatives.

## Dependency graph
Forward edges point FROM a formula TO each referenced coordinate. Reverse edges
point FROM a referenced coordinate TO its dependent formulas. Referenced blank
cells may exist in graph metadata without materialized Cell storage.

Replacing a formula removes its old forward edges and matching reverse entries.
Its incoming dependent subscriptions remain intact. Clearing a precedent therefore
does not lose future notifications. Static extraction includes both IF branches.

Dirty propagation traverses reverse edges from edited coordinates. Iterative
Kosaraju identifies strongly connected components on the dirty formula subgraph.
Components with more than one cell or a self-edge are actual cycles. Their cache
gets #CYCLE! with an origin. Kahn's algorithm orders remaining dirty formulas,
using existing clean caches and diagnosed cycle caches. Reading a cycle value
changes its error code to #CYCLE-DEPENDENCY! and retains origin provenance.

Independent evaluation order is coordinate order. First-error propagation follows
left-to-right AST arguments and row-major ranges. Graph traversal does not recurse;
10,000-cell chains and cycles are tested. Cyclic references are a general graph,
not a DAG. Breaking any edge in a component dirties every affected component member.

## Complexity and resource limits
Let R = populated rows, C_r = populated columns in a row, N = populated cells,
E = reference edges, A = AST/input payload, D/E_D = affected formula cells/edges.

- Lookup: O(log R + log C_r). Borrowed traversal: O(R + N).
- Memory: O(R + N + E + A), plus sparse styles and up to 8 snapshots by default.
  Tree nodes add allocator/pointer overhead; no exact portable bytes-per-cell claim.
- Candidate construction and history: O(N + E + A) per transaction; this remains
  whole-sheet work even for a one-cell edit. Global capacity validation also scans
  populated candidate storage and graph edges.
- Dirty propagation, SCCs, and ordering: O((D + E_D) log(N + E)) with ordered
  containers, plus scanning precedent edges incident on affected formulas.
- Evaluation: affected formulas only, O(AST work + expanded range reads), bounded
  per formula. No unrelated formula reevaluation or whole-graph SCC pass.
- clear_rows traverses existing populated rows in selected intervals, then commits
  one batch and recalculates once.

Limits defaults are in Types.hpp: 8 KiB formulas; AST height/parse depth 64; 2,048
AST nodes; 10,000 cells per range; 20,000 distinct precedents/formula; 100,000
evaluation steps/formula; 100,000 edits/batch and populated cells; 1,000,000 edges;
1 MiB per stored text; 32 MiB batch/sheet input; eight history entries. Constructors
also enforce hard caps. Caller-owned input already occupies memory before apply.
Budgets bound sizes and work, not wall-clock latency or total allocator overhead.
Snapshots and caches can multiply the input size substantially.

## Extension and background work boundaries
FormulaExtension is borrowed and must outlive Sheet. It receives typed contiguous
arguments, a remaining cooperative work budget, and an ExtensionContext reader.
LuaEngine uses that reader for exact runtime discovery. Unknown reads suspend;
the core inserts their edges and recalculates in dependency order before retry.
Dirty runtime edges are cleared and rediscovered each transaction, after computing
dirty closure from old subscriptions. This lets changed branches recover from old
dynamic cycles. Up to 64 passes are allowed; exhaustion rejects the whole batch.
The SCC/Kahn process described above runs per discovery pass. See LUA.md.
Arbitrary C++ extension implementations remain trusted, cooperative host code.

GridUI queues Batch objects and actions, commits on the owning thread in prepare(),
then draws from a published revision. Rendering only reads cached cells and styles.
GridViewport uses integer logical offsets and clips both axes, with at most one
additional partially visible row/column. The final row/column remains fully
visible when revealed. Drawing uses bounded stack text buffers and no table
column array; it never scans the sheet or executes Lua. Population counts update
only when the sheet revision changes. ImGui allocation hooks and C++ allocation
interception measure unchanged drawing and full frame work separately.
Background parsing/import planning can carry a revision token and std::stop_token;
the owner must reject stale plans or revalidate before committing. No background
worker or concurrency framework is implemented here.
