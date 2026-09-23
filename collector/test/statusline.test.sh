#!/usr/bin/env bash
set -uo pipefail

SCRIPT="$(cd "$(dirname "$0")/.." && pwd)/statusline.sh"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
FAILURES=0

# Fake curl: records each push and exits with FAKE_CURL_EXIT (0 = accepted).
mkdir -p "$WORK/bin"
cat > "$WORK/bin/curl" <<'EOF'
#!/usr/bin/env bash
echo push >> "$FAKE_CURL_LOG"
exit "${FAKE_CURL_EXIT:-0}"
EOF
chmod +x "$WORK/bin/curl"
printf 'export TOKESP_URL="http://backend.test"\nexport TOKESP_TOKEN="secret"\n' > "$WORK/config.sh"

export PATH="$WORK/bin:$PATH"
export TOKESP_CONFIG="$WORK/config.sh"
export TOKESP_STATE_DIR="$WORK/state"
export FAKE_CURL_LOG="$WORK/pushes.log"

LIMITS_29='{"model":{"display_name":"Opus"},"session_id":"s1","cost":{"total_api_duration_ms":1000},"rate_limits":{"five_hour":{"used_percentage":29,"resets_at":1800003600}}}'
# Same numbers, but a later reply: the API answered again, so the data is fresh.
LIMITS_29_LATER='{"model":{"display_name":"Opus"},"session_id":"s1","cost":{"total_api_duration_ms":2000},"rate_limits":{"five_hour":{"used_percentage":29,"resets_at":1800003600}}}'
LIMITS_30='{"model":{"display_name":"Opus"},"session_id":"s1","cost":{"total_api_duration_ms":3000},"rate_limits":{"five_hour":{"used_percentage":30,"resets_at":1800003600}}}'
LIMITS_31='{"model":{"display_name":"Opus"},"session_id":"s1","cost":{"total_api_duration_ms":4000},"rate_limits":{"five_hour":{"used_percentage":31,"resets_at":1800003600}}}'
NO_DATA='{"model":{"display_name":"Opus"}}'

push_count() {
  if [ -f "$FAKE_CURL_LOG" ]; then wc -l < "$FAKE_CURL_LOG" | tr -d ' '; else echo 0; fi
}

# The push runs detached, so give it time to finish (and save state) first.
render() {
  printf '%s' "$1" | "$SCRIPT" >/dev/null
  sleep 0.5
}

expect_pushes() {
  local name="$1" expected="$2" actual
  actual="$(push_count)"
  if [ "$actual" = "$expected" ]; then
    echo "ok - $name"
  else
    echo "FAIL - $name (expected $expected pushes, got $actual)"
    FAILURES=$((FAILURES + 1))
  fi
}

render "$NO_DATA"
expect_pushes "session without API response yet does not push" 0

render "$LIMITS_29"
expect_pushes "first values are pushed" 1

render "$LIMITS_29"
expect_pushes "a re-render without a new reply is not re-sent" 1

render "$LIMITS_29_LATER"
expect_pushes "the same values after a new reply are pushed" 2

render "$NO_DATA"
expect_pushes "a new empty session does not overwrite the last data" 2

render "$LIMITS_30"
expect_pushes "changed values are pushed" 3

FAKE_CURL_EXIT=22 render "$(printf '%s' "$LIMITS_31")"
expect_pushes "values are pushed even when the backend rejects them" 4

render "$LIMITS_31"
expect_pushes "a rejected push is retried with the same values" 5

# Another session renders with its own, older snapshot of the same window.
OLDER_SESSION='{"model":{"display_name":"Opus"},"session_id":"s2","cost":{"total_api_duration_ms":10},"rate_limits":{"five_hour":{"used_percentage":12,"resets_at":1800003600}}}'
# After the window rolls over, a smaller number is the new truth.
NEXT_WINDOW='{"model":{"display_name":"Opus"},"session_id":"s2","cost":{"total_api_duration_ms":20},"rate_limits":{"five_hour":{"used_percentage":3,"resets_at":1800021600}}}'

snapshot="$TOKESP_STATE_DIR/claude-statusline.json"

if [ "$(jq -r '.windows[0].usedPercentage, (.observedAt > 0)' "$snapshot" 2>/dev/null | tr '\n' ' ')" = "31 true " ]; then
  echo "ok - the app snapshot file is written"
else
  echo "FAIL - app snapshot file: $(cat "$snapshot" 2>/dev/null)"
  FAILURES=$((FAILURES + 1))
fi

render "$OLDER_SESSION"
expect_pushes "a lower value in the same window is not sent" 5
if [ "$(jq -r '.windows[0].usedPercentage' "$snapshot" 2>/dev/null)" = "31" ]; then
  echo "ok - the higher value of the window is kept"
else
  echo "FAIL - kept value: $(jq -c '.windows' "$snapshot" 2>/dev/null)"
  FAILURES=$((FAILURES + 1))
fi

render "$NEXT_WINDOW"
expect_pushes "a new window is sent even with a lower value" 6
if [ "$(jq -r '.windows[0].usedPercentage' "$snapshot" 2>/dev/null)" = "3" ]; then
  echo "ok - the new window replaces the old one"
else
  echo "FAIL - after reset: $(jq -c '.windows' "$snapshot" 2>/dev/null)"
  FAILURES=$((FAILURES + 1))
fi

output="$(printf '%s' "$LIMITS_29" | "$SCRIPT")"
if [ "$output" = "Opus  5h 29%" ]; then
  echo "ok - status line text is still printed"
else
  echo "FAIL - status line text: $output"
  FAILURES=$((FAILURES + 1))
fi

[ "$FAILURES" -eq 0 ] || exit 1
echo "all statusline tests passed"
