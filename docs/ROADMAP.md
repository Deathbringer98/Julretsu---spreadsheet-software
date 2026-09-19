# Remaining milestones

Steps 3 and 4 are implemented in version 0.2. See LUA.md, QUICKSTART.md, and ACCEPTANCE-0.2.md for the shipped behavior and tested limits. The following items remain future work.

## Streaming CSV and optional XLSX
IO.hpp is an interface contract only; no import/export implementation exists.

CSV adapter: process std::istream/std::ostream through bounded buffers, preserve
UTF-8 bytes, support quoted fields, doubled quotes, CRLF/LF, and embedded newlines.
Validate encoding at the adapter boundary if strict UTF-8 is required. Cap field,
row, total byte, and cell counts. Never split records at embedded newlines.
Default import interpretation is TextOnly. NativeFormulas requires explicit user
choice, and imported LUA calls must remain disabled/quarantined until separately
approved. Import text must never execute scripts automatically.

Report progress by consumed bytes and staged/committed cells; honor cancellation
between chunks. Commit bounded batches. Earlier completed batches remain after
cancellation/failure; the result must report partial import accurately. For
all-or-nothing import, stage in a separate bounded Sheet and publish only on
success. A future bulk transaction API may consolidate undo across chunks.

CSV export must specify whether it exports values or formula source, quote fields
containing delimiters/quotes/newlines, and offer formula-injection-safe text export
for opening in other spreadsheet applications. Numeric locale is invariant.

XLSX belongs behind a separate SheetStreamAdapter implementation with limits on
ZIP expansion and XML processing. Workbook sheets, formula dialect, cached values,
styles, number/date systems, and unsupported features require explicit compatibility
decisions and round-trip fixtures. Do not claim XLSX support until those tests pass.

## Optional Connect Your AI (future, not implemented)
Support local connections using each user's own provider access and usage limits;
no developer-funded inference. Separate provider transport, OS credential storage,
selected-context serialization, and proposed-edit validation.

Credentials must use Windows Credential Manager, macOS Keychain, or Linux Secret
Service. Never place keys in workbook files, project config, logs, or undo history.
If a secure credential store is unavailable, offer an explicit session-only mode.

OpenAI/Anthropic API keys and officially supported Codex/Claude Code account
integrations are desired connection types. Verify their current official interfaces
at implementation time; do not assume subscription sign-in grants API access.
Never copy private app tokens, scrape authentication stores, or invent an account
integration. Unsupported account connections must be clearly unavailable.

Send only the range/cells the user selects and approves for the request. Show a
context preview and proposed cell/style diff. Treat provider output as untrusted
structured edits, validate bounds/limits/types, then apply one undoable Sheet batch
only after approval. AI-proposed formulas/scripts require the same explicit
interpretation and script policy as imported data. Cancelled/failed requests must
not mutate cells. Keep transport work off the owning thread and reject stale
revision-based edit proposals.
