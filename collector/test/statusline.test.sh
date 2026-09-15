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

LIMITS_29='{"model":{"display_name":"Opus"},"rate_limits":{"five_hour":{"used_percentage":29,"resets_at":1800003600}}}'
LIMITS_30='{"model":{"display_name":"Opus"},"rate_limits":{"five_hour":{"used_percentage":30,"resets_at":1800003600}}}'
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
expect_pushes "same values are not re-sent with a fresh timestamp" 1

render "$NO_DATA"
expect_pushes "a new empty session does not overwrite the last data" 1

render "$LIMITS_30"
expect_pushes "changed values are pushed" 2

FAKE_CURL_EXIT=22 render "$(printf '%s' "$LIMITS_29")"
expect_pushes "values are pushed even when the backend rejects them" 3

render "$LIMITS_29"
expect_pushes "a rejected push is retried with the same values" 4

output="$(printf '%s' "$LIMITS_29" | "$SCRIPT")"
if [ "$output" = "Opus  5h 29%" ]; then
  echo "ok - status line text is still printed"
else
  echo "FAIL - status line text: $output"
  FAILURES=$((FAILURES + 1))
fi

[ "$FAILURES" -eq 0 ] || exit 1
echo "all statusline tests passed"
