#!/usr/bin/env bash
set -uo pipefail

SCRIPT="$(cd "$(dirname "$0")/.." && pwd)/codex_usage_poll.sh"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
FAILURES=0

mkdir -p "$WORK/bin" "$WORK/codex"
# Fake curl: serves the usage route with FAKE_USAGE_STATUS, records the headers
# it got and ingest bodies.
cat > "$WORK/bin/curl" <<'EOF'
#!/usr/bin/env bash
body=""; url=""; out=""; format=""
while [ $# -gt 0 ]; do
  case "$1" in
    -d) body="$2"; shift ;;
    -o) out="$2"; shift ;;
    -w) format="$2"; shift ;;
    -H) case "$2" in @*) cat "${2#@}" >> "$FAKE_LOG_DIR/headers.log" ;; esac; shift ;;
    http*) url="$1" ;;
  esac
  shift
done
case "$url" in
  */backend-api/wham/usage)
    echo fetch >> "$FAKE_LOG_DIR/fetches.log"
    status="${FAKE_USAGE_STATUS:-200}"
    [ "$status" = 200 ] && printf '%s' "$FAKE_USAGE_BODY" > "$out"
    [ -n "$format" ] && printf '%s' "$status" ;;
  */ingest)
    printf '%s\n' "$body" >> "$FAKE_LOG_DIR/pushes.log" ;;
esac
EOF
chmod +x "$WORK/bin/curl"
printf 'export TOKESP_URL="http://backend.test"\nexport TOKESP_TOKEN="secret"\n' > "$WORK/config.sh"
printf '{"auth_mode":"chatgpt","tokens":{"access_token":"codex-test-token","account_id":"acct-test"}}' > "$WORK/codex/auth.json"

export PATH="$WORK/bin:$PATH"
export TOKESP_CONFIG="$WORK/config.sh"
export TOKESP_STATE_DIR="$WORK/state"
export CODEX_HOME="$WORK/codex"
export FAKE_LOG_DIR="$WORK"
export TOKESP_MIN_INTERVAL_SECONDS=0
export TOKESP_HOOK_DELAY_SECONDS=0
export FAKE_USAGE_BODY='{"rate_limit":{"primary_window":{"used_percent":38,"limit_window_seconds":18000,"reset_at":1789574599},"secondary_window":{"used_percent":54,"limit_window_seconds":604800,"reset_at":1789989136}}}'

count() {
  if [ -f "$WORK/$1" ]; then wc -l < "$WORK/$1" | tr -d ' '; else echo 0; fi
}

expect() {
  local name="$1" actual="$2" expected="$3"
  if [ "$actual" = "$expected" ]; then
    echo "ok - $name"
  else
    echo "FAIL - $name (expected $expected, got $actual)"
    FAILURES=$((FAILURES + 1))
  fi
}

"$SCRIPT" 2>/dev/null
expect "fetches Codex usage and pushes one snapshot" "$(count pushes.log)" 1
expect "pushed snapshot is tagged as codex with both windows" \
  "$(tail -1 "$WORK/pushes.log" | jq -c '[.provider, [.windows[] | [.id, (.usedPercentage + 0), .resetsAt]]]')" \
  '["codex",[["five_hour",38,1789574599],["seven_day",54,1789989136]]]'
expect "sends the ChatGPT account id header" "$(grep -c '^ChatGPT-Account-Id: acct-test$' "$WORK/headers.log")" 1

TOKESP_MIN_INTERVAL_SECONDS=3600 "$SCRIPT" 2>/dev/null
expect "a burst within the minimum interval does not refetch" "$(count fetches.log)" 1

FAKE_USAGE_STATUS=401 "$SCRIPT" 2>"$WORK/errors.log"
expect "failed usage request pushes nothing" "$(count pushes.log)" 1
expect "failed usage request logs the HTTP status" "$(grep -c 'HTTP 401' "$WORK/errors.log")" 1

CODEX_HOME="$WORK/missing" "$SCRIPT" 2>/dev/null
expect "missing Codex login neither fetches nor pushes" "$(count fetches.log)" 2

FAKE_USAGE_BODY='{"rate_limit":null}' "$SCRIPT" 2>/dev/null
expect "response without windows does not erase backend data" "$(count pushes.log)" 1

started=$(date +%s)
"$SCRIPT" --detach '{"type":"agent-turn-complete"}'
expect "--detach returns immediately, ignoring Codex's JSON argument" "$(( $(date +%s) - started < 2 ))" 1
sleep 1
expect "--detach still pushes in the background" "$(count pushes.log)" 2

# pgrep never matches itself, unlike `ps | grep`, whose own argument holds the token.
if pgrep -f "codex-test-token" >/dev/null; then
  echo "FAIL - token visible in process list"; FAILURES=$((FAILURES + 1))
else
  echo "ok - token is not passed on the command line"
fi

[ "$FAILURES" -eq 0 ] || exit 1
echo "all codex usage poll tests passed"
