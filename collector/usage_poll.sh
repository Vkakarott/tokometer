#!/usr/bin/env bash
# Pushes the Claude account's subscription usage to the tokEsp backend.
#
# Reads the same account usage the Claude Code /usage screen shows, so the
# number follows every client: the VS Code extension, the CLI and claude.ai.
# Runs from a Claude Code Stop hook (after each reply) and from launchd (idle).
#
# The usage route is internal to Claude Code and undocumented: it can change
# without notice. The OAuth token is only ever sent to Anthropic.
set -uo pipefail

CONFIG_FILE="${TOKESP_CONFIG:-$HOME/.config/tokesp/config.sh}"
STATE_DIR="${TOKESP_STATE_DIR:-$HOME/.cache/tokesp}"
LAST_FETCH_FILE="$STATE_DIR/last-fetch"
USAGE_URL="https://api.anthropic.com/api/oauth/usage"
MIN_INTERVAL_SECONDS="${TOKESP_MIN_INTERVAL_SECONDS:-15}"
HOOK_DELAY_SECONDS="${TOKESP_HOOK_DELAY_SECONDS:-3}"
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=/dev/null
[ -f "$CONFIG_FILE" ] && . "$CONFIG_FILE"

log() {
  printf '%s tokesp: %s\n' "$(date '+%Y-%m-%dT%H:%M:%S')" "$*" >&2
}

read_oauth_token() {
  security find-generic-password -s "Claude Code-credentials" -w 2>/dev/null \
    | jq -r '.claudeAiOauth.accessToken // empty' 2>/dev/null
}

# Stop hooks fire in bursts; the usage route is not ours to hammer.
fetched_recently() {
  local last
  last=$(cat "$LAST_FETCH_FILE" 2>/dev/null || echo 0)
  [ $(($(date +%s) - last)) -lt "$MIN_INTERVAL_SECONDS" ]
}

# The token goes through a file descriptor so it never shows up in `ps`.
fetch_usage() {
  curl -sf -m 10 "$USAGE_URL" \
    -H @<(printf 'Authorization: Bearer %s\n' "$1") \
    -H "anthropic-beta: oauth-2025-04-20"
}

build_payload() {
  jq -c --arg src "$(hostname -s)" --argjson now "$(date +%s)" -f "$script_dir/usage.jq"
}

push_snapshot() {
  curl -sf -m 5 -X POST "$TOKESP_URL/ingest" \
    -H @<(printf 'Authorization: Bearer %s\n' "$TOKESP_TOKEN") \
    -H 'Content-Type: application/json' \
    -d "$1" >/dev/null
}

main() {
  if [ -z "${TOKESP_URL:-}" ] || [ -z "${TOKESP_TOKEN:-}" ]; then
    log "missing TOKESP_URL/TOKESP_TOKEN in $CONFIG_FILE"; return 1
  fi
  fetched_recently && return 0

  local token usage payload
  token=$(read_oauth_token)
  [ -n "$token" ] || { log "Claude Code OAuth token unavailable in Keychain"; return 1; }
  mkdir -p "$STATE_DIR" && date +%s > "$LAST_FETCH_FILE"

  usage=$(fetch_usage "$token") || { log "usage request failed"; return 1; }
  payload=$(printf '%s' "$usage" | build_payload) || { log "unexpected usage response"; return 1; }
  # An empty snapshot would erase the backend's last known data.
  printf '%s' "$payload" | jq -e '.windows | length > 0' >/dev/null \
    || { log "usage response has no windows"; return 1; }
  push_snapshot "$payload" || { log "backend rejected the snapshot"; return 1; }
}

if [ "${1:-}" = "--detach" ]; then
  # From the Stop hook: return at once so Claude Code never waits, and give the
  # server a moment to count the reply that just finished.
  ( sleep "$HOOK_DELAY_SECONDS"; "$0" ) </dev/null >/dev/null 2>&1 &
  exit 0
fi

main
