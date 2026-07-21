#!/usr/bin/env bash
set -uo pipefail

CONFIG_FILE="${TOKESP_CONFIG:-$HOME/.config/tokesp/config.sh}"
# shellcheck source=/dev/null
[ -f "$CONFIG_FILE" ] && . "$CONFIG_FILE"

input=$(cat)
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [ -n "${TOKESP_URL:-}" ] && [ -n "${TOKESP_TOKEN:-}" ]; then
  payload=$(printf '%s' "$input" | jq -c \
    --arg src "$(hostname -s)" \
    --argjson now "$(date +%s)" \
    -f "$script_dir/payload.jq" 2>/dev/null)

  if [ -n "$payload" ]; then
    # Detached twice: the statusline render must never wait on the network, and
    # Claude Code cancels in-flight executions when a new update arrives.
    ( curl -sS -m 3 -X POST "$TOKESP_URL/ingest" \
        -H "Authorization: Bearer $TOKESP_TOKEN" \
        -H 'Content-Type: application/json' \
        -d "$payload" >/dev/null 2>&1 & ) &
  fi
fi

printf '%s' "$input" | jq -r '
  [ (.model.display_name // "?"),
    (if .rate_limits.five_hour then "5h \(.rate_limits.five_hour.used_percentage | round)%" else empty end),
    (if .rate_limits.seven_day then "7d \(.rate_limits.seven_day.used_percentage | round)%" else empty end)
  ] | join("  ")'
