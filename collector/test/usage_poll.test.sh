#!/usr/bin/env bash
set -uo pipefail

SCRIPT="$(cd "$(dirname "$0")/.." && pwd)/usage_poll.sh"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
FAILURES=0

mkdir -p "$WORK/bin"
# Fake Keychain: returns Claude Code credentials unless FAKE_SECURITY_FAIL is set.
cat > "$WORK/bin/security" <<'EOF'
#!/usr/bin/env bash
[ -n "${FAKE_SECURITY_FAIL:-}" ] && exit 44
printf '{"claudeAiOauth":{"accessToken":"oauth-test-token"}}'
EOF
# Fake curl: serves the usage route with FAKE_USAGE_STATUS and records ingest bodies.
cat > "$WORK/bin/curl" <<'EOF'
#!/usr/bin/env bash
body=""; url=""; out=""; format=""; headers=""
while [ $# -gt 0 ]; do
  case "$1" in
    -d) body="$2"; shift ;;
    -o) out="$2"; shift ;;
    -w) format="$2"; shift ;;
    -D) headers="$2"; shift ;;
    http*) url="$1" ;;
  esac
  shift
done
case "$url" in
  */api/oauth/usage)
    echo fetch >> "$FAKE_LOG_DIR/fetches.log"
    status="${FAKE_USAGE_STATUS:-200}"
    [ "$status" = 200 ] && printf '%s' "$FAKE_USAGE_BODY" > "$out"
    if [ -n "$headers" ]; then
      printf 'HTTP/1.1 %s\r\n' "$status" > "$headers"
      [ -n "${FAKE_RETRY_AFTER:-}" ] && printf 'retry-after: %s\r\n' "$FAKE_RETRY_AFTER" >> "$headers"
    fi
    [ -n "$format" ] && printf '%s' "$status" ;;
  */ingest)
    printf '%s\n' "$body" >> "$FAKE_LOG_DIR/pushes.log" ;;
esac
EOF
chmod +x "$WORK/bin/security" "$WORK/bin/curl"
printf 'export TOKESP_URL="http://backend.test"\nexport TOKESP_TOKEN="secret"\n' > "$WORK/config.sh"

export PATH="$WORK/bin:$PATH"
export TOKESP_CONFIG="$WORK/config.sh"
export TOKESP_STATE_DIR="$WORK/state"
export FAKE_LOG_DIR="$WORK"
export TOKESP_MIN_INTERVAL_SECONDS=0
export TOKESP_HOOK_DELAY_SECONDS=0
export FAKE_USAGE_BODY='{"five_hour":{"utilization":39.0,"resets_at":"2026-09-15T19:10:00.110795+00:00"},"seven_day":{"utilization":45.0,"resets_at":"2026-09-16T03:00:00.110816+00:00"}}'

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
expect "fetches account usage and pushes one snapshot" "$(count pushes.log)" 1
expect "pushed windows carry the account values" \
  "$(tail -1 "$WORK/pushes.log" | jq -c '[.windows[] | [.id, (.usedPercentage + 0), .resetsAt]]')" \
  '[["five_hour",39,1789499400],["seven_day",45,1789527600]]'

TOKESP_MIN_INTERVAL_SECONDS=3600 "$SCRIPT" 2>/dev/null
expect "a burst within the minimum interval does not refetch" "$(count fetches.log)" 1

FAKE_USAGE_STATUS=500 "$SCRIPT" 2>"$WORK/errors.log"
expect "failed usage request pushes nothing" "$(count pushes.log)" 1
expect "failed usage request logs the HTTP status" "$(grep -c 'HTTP 500' "$WORK/errors.log")" 1

FAKE_SECURITY_FAIL=1 "$SCRIPT" 2>/dev/null
expect "missing Keychain token neither fetches nor pushes" "$(count fetches.log)" 2

FAKE_USAGE_BODY='{"extra_usage":{}}' "$SCRIPT" 2>/dev/null
expect "response without windows does not erase backend data" "$(count pushes.log)" 1

started=$(date +%s)
"$SCRIPT" --detach
expect "--detach returns immediately" "$(( $(date +%s) - started < 2 ))" 1
sleep 1
expect "--detach still pushes in the background" "$(count pushes.log)" 2

FAKE_USAGE_STATUS=429 FAKE_RETRY_AFTER=900 "$SCRIPT" 2>"$WORK/errors.log"
expect "rate limit honors a long Retry-After" "$(grep -c 'rate limited, retrying in 900s' "$WORK/errors.log")" 1
fetches_before="$(count fetches.log)"
"$SCRIPT" 2>/dev/null
expect "no request is made while rate limited" "$(count fetches.log)" "$fetches_before"
echo 0 > "$TOKESP_STATE_DIR/last-fetch.backoff"
"$SCRIPT" 2>/dev/null
expect "requests resume once the wait is over" "$(count fetches.log)" "$((fetches_before + 1))"

FAKE_USAGE_STATUS=429 "$SCRIPT" 2>"$WORK/errors.log"
expect "rate limit without Retry-After waits the minimum" "$(grep -c 'rate limited, retrying in 300s' "$WORK/errors.log")" 1

echo 0 > "$TOKESP_STATE_DIR/last-fetch.backoff"
FAKE_USAGE_STATUS=429 FAKE_RETRY_AFTER=0 "$SCRIPT" 2>"$WORK/errors.log"
expect "Retry-After: 0 still waits the minimum" "$(grep -c 'rate limited, retrying in 300s' "$WORK/errors.log")" 1

# pgrep never matches itself, unlike `ps | grep`, whose own argument holds the token.
if pgrep -f "oauth-test-token" >/dev/null; then
  echo "FAIL - token visible in process list"; FAILURES=$((FAILURES + 1))
else
  echo "ok - token is not passed on the command line"
fi

[ "$FAILURES" -eq 0 ] || exit 1
echo "all usage poll tests passed"
