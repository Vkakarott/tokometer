# Shared pieces of the usage collectors. Source it, do not run it.
#
# The caller sets COLLECTOR_NAME, STATE_DIR, MIN_INTERVAL_SECONDS and
# HOOK_DELAY_SECONDS before sourcing.

CONFIG_FILE="${TOKESP_CONFIG:-$HOME/.config/tokesp/config.sh}"
# shellcheck source=/dev/null
[ -f "$CONFIG_FILE" ] && . "$CONFIG_FILE"

# Anthropic answers 429 with "Retry-After: 0" and keeps refusing right after, so
# a short wait is never honored as is.
MIN_BACKOFF_SECONDS=300
MAX_BACKOFF_SECONDS=3600

log() {
  printf '%s tokesp[%s]: %s\n' "$(date '+%Y-%m-%dT%H:%M:%S')" "$COLLECTOR_NAME" "$*" >&2
}

require_backend_config() {
  [ -n "${TOKESP_URL:-}" ] && [ -n "${TOKESP_TOKEN:-}" ] && return 0
  log "missing TOKESP_URL/TOKESP_TOKEN in $CONFIG_FILE"
  return 1
}

# Hooks fire in bursts; the usage routes are not ours to hammer.
# $1: file holding the last fetch time.
fetched_recently() {
  local last
  last=$(cat "$1" 2>/dev/null || echo 0)
  [ $(($(date +%s) - last)) -lt "$MIN_INTERVAL_SECONDS" ]
}

mark_fetched() {
  mkdir -p "$(dirname "$1")" && date +%s > "$1"
}

# $1: backoff file. True while a rate limit still asks us to wait.
backing_off() {
  local until
  until=$(cat "$1" 2>/dev/null || echo 0)
  [ "$(date +%s)" -lt "$until" ]
}

# $1: response headers file. Honors Retry-After within MIN..MAX backoff.
start_backoff() {
  local wait
  wait=$(awk 'tolower($1) == "retry-after:" { gsub(/\r/, "", $2); print $2 }' "$1" 2>/dev/null | tail -1)
  case "$wait" in
    '' | *[!0-9]*) wait=$MIN_BACKOFF_SECONDS ;;
  esac
  [ "$wait" -lt "$MIN_BACKOFF_SECONDS" ] && wait=$MIN_BACKOFF_SECONDS
  [ "$wait" -gt "$MAX_BACKOFF_SECONDS" ] && wait=$MAX_BACKOFF_SECONDS
  echo $(($(date +%s) + wait)) > "$BACKOFF_FILE"
  log "rate limited, retrying in ${wait}s"
}

# $@: curl arguments (URL and headers). Prints the body on a 2xx response;
# otherwise logs the HTTP status, and backs off on a rate limit.
fetch_json() {
  local body headers status
  body=$(mktemp) && headers=$(mktemp) || return 1
  status=$(curl -s -m 10 -o "$body" -D "$headers" -w '%{http_code}' "$@")
  if [ "${status:0:1}" = "2" ]; then
    cat "$body"
  elif [ "$status" = "429" ]; then
    start_backoff "$headers"
  else
    log "usage request failed (HTTP ${status:-000})"
  fi
  rm -f "$body" "$headers"
  [ "${status:0:1}" = "2" ]
}

# $1: jq mapping file. Reads the provider's usage response on stdin.
build_payload() {
  jq -c --arg src "$(hostname -s)" --argjson now "$(date +%s)" -f "$1"
}

# An empty snapshot would erase the backend's last known data.
has_windows() {
  printf '%s' "$1" | jq -e '.windows | length > 0' >/dev/null
}

# The token goes through a file descriptor so it never shows up in `ps`.
push_snapshot() {
  curl -sf -m 5 -X POST "$TOKESP_URL/ingest" \
    -H @<(printf 'Authorization: Bearer %s\n' "$TOKESP_TOKEN") \
    -H 'Content-Type: application/json' \
    -d "$1" >/dev/null
}

# From an agent hook: return at once so the agent never waits, and give the
# server a moment to count the reply that just finished. $1: script path.
detach_run() {
  ( sleep "$HOOK_DELAY_SECONDS"; "$1" ) </dev/null >/dev/null 2>&1 &
}

# $1: function printing the provider's usage JSON, $2: jq mapping file,
# $3: last-fetch file (its ".backoff" sibling tracks rate limits).
collect_and_push() {
  local fetch="$1" mapping="$2" last_fetch="$3" usage payload
  BACKOFF_FILE="$last_fetch.backoff"
  require_backend_config || return 1
  backing_off "$BACKOFF_FILE" && return 0
  fetched_recently "$last_fetch" && return 0
  mark_fetched "$last_fetch"

  usage=$("$fetch") || return 1
  payload=$(printf '%s' "$usage" | build_payload "$mapping") || { log "unexpected usage response"; return 1; }
  has_windows "$payload" || { log "usage response has no windows"; return 1; }
  push_snapshot "$payload" || { log "backend rejected the snapshot"; return 1; }
}
