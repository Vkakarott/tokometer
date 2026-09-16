#!/usr/bin/env bash
# Pushes the Claude account's subscription usage to the tokometer backend.
#
# Reads the same account usage the Claude Code /usage screen shows, so the
# number follows every client: the VS Code extension, the CLI and claude.ai.
# Runs from a Claude Code Stop hook (after each reply) and from launchd (idle).
#
# The usage route is internal to Claude Code and undocumented: it can change
# without notice. The OAuth token is only ever sent to Anthropic.
set -uo pipefail

COLLECTOR_NAME="claude"
STATE_DIR="${TOKESP_STATE_DIR:-$HOME/.cache/tokesp}"
MIN_INTERVAL_SECONDS="${TOKESP_MIN_INTERVAL_SECONDS:-60}"
HOOK_DELAY_SECONDS="${TOKESP_HOOK_DELAY_SECONDS:-3}"
USAGE_URL="https://api.anthropic.com/api/oauth/usage"
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=lib.sh
. "$script_dir/lib.sh"

read_oauth_token() {
  security find-generic-password -s "Claude Code-credentials" -w 2>/dev/null \
    | jq -r '.claudeAiOauth.accessToken // empty' 2>/dev/null
}

fetch_claude_usage() {
  local token
  token=$(read_oauth_token)
  [ -n "$token" ] || { log "Claude Code OAuth token unavailable in Keychain"; return 1; }
  fetch_json "$USAGE_URL" \
    -H @<(printf 'Authorization: Bearer %s\n' "$token") \
    -H "anthropic-beta: oauth-2025-04-20"
}

if [ "${1:-}" = "--detach" ]; then
  detach_run "$0"
  exit 0
fi

collect_and_push fetch_claude_usage "$script_dir/usage.jq" "$STATE_DIR/last-fetch"
