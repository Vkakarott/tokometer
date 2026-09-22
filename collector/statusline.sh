#!/usr/bin/env bash
# Claude Code statusline: prints the usage line and saves what it was given.
#
# Claude Code hands this script the account limits it got from the last API
# reply, so the numbers cost no request of our own and follow every reply. The
# macOS app reads the snapshot file; the backend push is for the ESP32 display.
set -uo pipefail

CONFIG_FILE="${TOKESP_CONFIG:-$HOME/.config/tokesp/config.sh}"
STATE_DIR="${TOKESP_STATE_DIR:-$HOME/.cache/tokesp}"
LAST_SENT_FILE="$STATE_DIR/last-sent"
SNAPSHOT_FILE="$STATE_DIR/claude-statusline.json"
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

# No API response in this session yet: keep the data we already have.
has_data() {
  printf '%s' "$1" | jq -e '(.windows | length > 0) or has("context")' >/dev/null
}

# What makes the data new: the values, or another reply in this session. The
# statusline also re-renders on its own, and those renders carry no new data.
fingerprint() {
  printf '%s %s' \
    "$(printf '%s' "$1" | jq -c 'del(.observedAt)')" \
    "$(printf '%s' "$input" | jq -c '[.session_id // "", .cost.total_api_duration_ms // 0]')"
}

# The app reads this file, so it is written even with no backend configured.
save_snapshot() {
  [ "$2" = "$(jq -r '.fingerprint // empty' "$SNAPSHOT_FILE" 2>/dev/null)" ] && return 0
  mkdir -p "$STATE_DIR"
  printf '%s' "$1" | jq -c --arg fp "$2" '. + {fingerprint: $fp}' > "$SNAPSHOT_FILE.tmp" \
    && mv "$SNAPSHOT_FILE.tmp" "$SNAPSHOT_FILE"
}

push_if_changed() {
  [ "$2" = "$(cat "$LAST_SENT_FILE" 2>/dev/null)" ] && return 0
  mkdir -p "$STATE_DIR"
  # Detached twice: the statusline render must never wait on the network, and
  # Claude Code cancels in-flight executions when a new update arrives. The
  # fingerprint is saved only once the backend accepts, so a failure retries.
  ( curl -fsS -m 3 -X POST "$TOKESP_URL/ingest" \
      -H "Authorization: Bearer $TOKESP_TOKEN" \
      -H 'Content-Type: application/json' \
      -d "$1" >/dev/null 2>&1 \
    && printf '%s' "$2" > "$LAST_SENT_FILE" & ) &
}

payload=$(build_payload)
if [ -n "$payload" ] && has_data "$payload"; then
  mark=$(fingerprint "$payload")
  save_snapshot "$payload" "$mark"
  if [ -n "${TOKESP_URL:-}" ] && [ -n "${TOKESP_TOKEN:-}" ]; then
    push_if_changed "$payload" "$mark"
  fi
fi

printf '%s' "$input" | jq -r '
  [ (.model.display_name // "?"),
    (if .rate_limits.five_hour then "5h \(.rate_limits.five_hour.used_percentage | round)%" else empty end),
    (if .rate_limits.seven_day then "7d \(.rate_limits.seven_day.used_percentage | round)%" else empty end)
  ] | join("  ")'
