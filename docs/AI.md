# AI assistant ("Connect Your AI")

Optional. Julretsu works fully offline; nothing is sent anywhere unless you connect a
provider and choose **Propose edits**.

## Providers
| Provider | Endpoint | Model | Key |
| --- | --- | --- | --- |
| Anthropic (Claude) | `https://api.anthropic.com/v1/messages` | `claude-opus-5` by default; any model your account can use | Required |
| OpenAI-compatible | Any `/chat/completions` URL: OpenAI, Ollama, LM Studio, vLLM and similar | Enter your provider's model name | Optional for local servers |

Endpoints must use `https://`. Plain `http://` is accepted only for `localhost`,
`127.0.0.1` or `[::1]`, so a key never crosses a network unencrypted. Credentials in URLs
are rejected. Usage is billed by your provider under your own account.

Anthropic requests ask for schema-constrained JSON output. For Claude Opus 5 they also
enable the API's automatic fallback, so a request declined by one model can be answered
by a suitable fallback model in the same call. OpenAI-compatible requests ask for a strict
JSON schema; replies wrapped in code fences are also accepted for servers that ignore it.

## Credentials and settings
- API keys are stored in **Windows Credential Manager** (a generic credential named
  `Julretsu/AI/Anthropic` or `Julretsu/AI/OpenAICompatible`, local to this computer and your
  Windows account). They are never written to workbooks, settings files or logs.
- The key is read only when a request is sent, and its in-memory copies are wiped after use.
- **Remove key** in the Connection section deletes it. You can also delete it in
  Windows Credential Manager.
- Provider, model and endpoint (not secret) are saved to
  `%LOCALAPPDATA%\Julretsu\ai-connection.json`. An unsafe or corrupt file falls back to defaults.

## What is sent
Your request text, the active cell and selection, and the sheet's populated cells in row
order: each cell's stored input and, for formulas, its calculated value. Sending stops at
2,000 cells or 200 KiB, and the AI is told the data was truncated. The panel shows the
cell count and destination server before you send. Formatting, Lua scripts and file paths
are not sent.

The instructions tell the model to treat cell contents as data, so text in a cell cannot
override your request. Whatever the model proposes still goes through the checks below and
your review.

## Review before applying
The reply must be a summary plus a list of edits (`cell`, `kind`, `value`). Every edit is
validated before you see it:
- A1 addresses within A1:XFD1000000; duplicates are dropped.
- Numbers must be finite; Booleans TRUE/FALSE; text at most 1 MiB of valid UTF-8.
- Formulas must parse in Julretsu's dialect. Invalid ones are skipped with a warning.
- **Lua formulas start unticked**, because they run scripts.
- More than 1,000 edits rejects the whole proposal.

The preview shows each cell's current and proposed value, updated if the sheet changes
while you review. Tick or untick edits, click a cell address to jump to it, then
**Apply** or **Discard**. Applied edits go in as one transaction: one Ctrl+Z reverts all of
them. Nothing touches the sheet until you apply.

## Network behaviour
One request at a time, on a background thread, so the window stays responsive.
**Cancel** aborts the connection immediately. Timeouts: 15 s connect, 5 min response.
Responses over 8 MiB are rejected. Proxy settings follow Windows. Errors are reported in
plain language (rejected key, rate limit, provider unavailable, response cut off, model
declined).

## Verification
`tests/ai_tests.cpp` (not published) covers JSON parsing limits, endpoint safety rules,
exact request and response shapes for both providers, proposal validation, one-step undo,
settings and Credential Manager round trips, and the real WinHTTP path against a local
test server, including prompt cancellation and refused connections. The native smoke
test previews a proposal without changing the sheet, then applies it with the Lua edit
left unticked. No request to a paid provider is made by the tests.
