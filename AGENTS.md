# Repository guidelines

TokEsp combines ESP32 firmware, a TypeScript backend, a macOS app, shell collectors, and hardware design files. Keep changes scoped to the component being worked on and preserve compatibility at their boundaries.

## Git workflow

- Inspect `git status` before committing; never stage unrelated changes.
- Commit each completed code or documentation change unless the user explicitly asks not to.
- Use English Conventional Commit messages (for example, `feat: add device retry policy`).
- Do not add `Co-authored-by` trailers.
- Never run `git push`, create a pull request, merge, rebase shared history, or change remotes without the user's explicit authorization in the current request.
- Do not use destructive Git commands (`reset --hard`, forced checkout, clean, or forced push) unless explicitly authorized.

## General engineering

- Keep code, identifiers, filenames, and technical documentation in English. Write user-facing app text in pt-BR unless requested otherwise.
- Prefer small, focused changes. Do not refactor unrelated code while addressing a task.
- Apply SOLID principles where they improve maintainability; favor clear ownership, dependency boundaries, and testable code over abstractions with no concrete need.
- Preserve existing public behavior unless the request calls for a behavior change. Update relevant documentation and tests when contracts change.
- Do not add unnecessary comments. Explain non-obvious constraints or hardware/protocol decisions when code alone cannot communicate them.

## Security and configuration

- Never commit credentials, tokens, Wi-Fi passwords, device identifiers, or generated local configuration. Use the existing `*.example.*` pattern for templates.
- Treat backend endpoints, payload schemas, pairing flow, and firmware API codecs as a cross-component contract. Coordinate compatible changes across all affected components.

## Component-specific practices

- Firmware: target the configured ESP32/PlatformIO environment; avoid blocking loops, preserve watchdog-friendly control flow, and keep display, network, storage, and application-state responsibilities separate. Test protocol and formatting logic under `firmware/test` when feasible.
- Backend: validate external input, preserve authentication and pairing boundaries, and add or update tests under `backend/test` for changed behavior.
- macOS app: keep UI presentation separate from data acquisition and persistence. Preserve offline/error states and update Swift tests for changed parsing or source-selection behavior.
- Collectors: write portable, defensive shell; quote variables, handle missing commands/data, and update the matching shell tests in `collector/test`.
- Hardware: do not overwrite generated CAD/mesh artifacts or alter physical dimensions without explicit intent. Keep source scripts and generated models consistent when a hardware design change is requested.

## Verification

- Run the smallest relevant checks before committing and report anything not run.
- For changes spanning components, verify the shared payload or API contract in addition to each component's local tests.
