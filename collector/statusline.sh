#!/usr/bin/env bash
set -uo pipefail

CONFIG_FILE="${TOKESP_CONFIG:-$HOME/.config/tokesp/config.sh}"
STATE_DIR="${TOKESP_STATE_DIR:-$HOME/.cache/tokesp}"
LAST_SENT_FILE="$STATE_DIR/last-sent"
# shellcheck source=/dev/null
[ -f "$CONFIG_FILE" ] && . "$CONFIG_FILE"

input=$(cat)
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

build_payload() {
  printf '%s' "$input" | jq -c \
    --arg src "$(hostname -s)" \
    --argjson now "$(date +%s)" \
    -f "$script_dir/payload.jq" 2>/dev/null
}

# Pushes only values that changed since the last accepted push. Re-sending the
# same numbers with a fresh observedAt would make old data look current.
push_if_changed() {
  local payload="$1" fingerprint
  # No API response in this session yet: keep the data the backend already has.
  printf '%s' "$payload" | jq -e '(.windows | length > 0) or has("context")' >/dev/null || return 0

  fingerprint=$(printf '%s' "$payload" | jq -c 'del(.observedAt)')
  [ "$fingerprint" = "$(cat "$LAST_SENT_FILE" 2>/dev/null)" ] && return 0

  mkdir -p "$STATE_DIR"
  # Detached twice: the statusline render must never wait on the network, and
  # Claude Code cancels in-flight executions when a new update arrives. The
  # fingerprint is saved only once the backend accepts, so a failure retries.
  ( curl -fsS -m 3 -X POST "$TOKESP_URL/ingest" \
      -H "Authorization: Bearer $TOKESP_TOKEN" \
      -H 'Content-Type: application/json' \
      -d "$payload" >/dev/null 2>&1 \
    && printf '%s' "$fingerprint" > "$LAST_SENT_FILE" & ) &
}

if [ -n "${TOKESP_URL:-}" ] && [ -n "${TOKESP_TOKEN:-}" ]; then
  payload=$(build_payload)
  [ -n "$payload" ] && push_if_changed "$payload"
fi

printf '%s' "$input" | jq -r '
  [ (.model.display_name // "?"),
    (if .rate_limits.five_hour then "5h \(.rate_limits.five_hour.used_percentage | round)%" else empty end),
    (if .rate_limits.seven_day then "7d \(.rate_limits.seven_day.used_percentage | round)%" else empty end)
  ] | join("  ")'
