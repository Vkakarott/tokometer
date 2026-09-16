#!/usr/bin/env bash
# Pushes the ChatGPT account's Codex usage to the tokometer backend.
#
# Reads the same account usage Codex shows, so the number follows every client:
# the ChatGPT app, the Codex CLI and the IDE extension. Runs from Codex's
# `notify` program (after each turn) and from launchd (idle).
#
# The usage route is internal to Codex and undocumented: it can change without
# notice. The token is only ever sent to OpenAI.
set -uo pipefail

COLLECTOR_NAME="codex"
STATE_DIR="${TOKESP_STATE_DIR:-$HOME/.cache/tokesp}"
MIN_INTERVAL_SECONDS="${TOKESP_MIN_INTERVAL_SECONDS:-60}"
HOOK_DELAY_SECONDS="${TOKESP_HOOK_DELAY_SECONDS:-3}"
USAGE_URL="https://chatgpt.com/backend-api/wham/usage"
CODEX_AUTH_FILE="${CODEX_HOME:-$HOME/.codex}/auth.json"
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=lib.sh
. "$script_dir/lib.sh"

# $1: jq path inside Codex's auth.json.
read_auth_field() {
  jq -r "$1 // empty" "$CODEX_AUTH_FILE" 2>/dev/null
}

fetch_codex_usage() {
  local token account
  token=$(read_auth_field '.tokens.access_token')
  account=$(read_auth_field '.tokens.account_id')
  if [ -z "$token" ] || [ -z "$account" ]; then
    log "Codex ChatGPT login unavailable in $CODEX_AUTH_FILE"
    return 1
  fi
  fetch_json "$USAGE_URL" \
    -H @<(printf 'Authorization: Bearer %s\nChatGPT-Account-Id: %s\n' "$token" "$account") \
    -H "User-Agent: codex_cli_rs"
}

# Codex appends a JSON argument to `notify`, so the flag must come first.
if [ "${1:-}" = "--detach" ]; then
  detach_run "$0"
  exit 0
fi

collect_and_push fetch_codex_usage "$script_dir/codex_usage.jq" "$STATE_DIR/last-fetch-codex"
