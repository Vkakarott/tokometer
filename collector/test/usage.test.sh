#!/usr/bin/env bash
set -uo pipefail

JQ_FILE="$(dirname "$0")/../usage.jq"
FAILURES=0

# jq 1.7 keeps number literals as written (39.0); compare numbers by value.
NORMALIZE='walk(if type == "number" then . + 0 else . end)'

run() {
  printf '%s' "$1" | jq -c --arg src "testhost" --argjson now 1800000000 -f "$JQ_FILE" | jq -S -c "$NORMALIZE"
}

expect() {
  local name="$1" actual="$2" expected="$3"
  if [ "$actual" = "$(printf '%s' "$expected" | jq -S -c "$NORMALIZE")" ]; then
    echo "ok - $name"
  else
    echo "FAIL - $name"
    echo "  expected: $(printf '%s' "$expected" | jq -S -c "$NORMALIZE")"
    echo "  actual:   $actual"
    FAILURES=$((FAILURES + 1))
  fi
}

expect "both windows with fractional ISO reset times" \
  "$(run '{"five_hour":{"utilization":39.0,"resets_at":"2026-09-15T19:10:00.110795+00:00","limit_dollars":null},"seven_day":{"utilization":45.0,"resets_at":"2026-09-16T03:00:00.110816+00:00"}}')" \
  '{"observedAt":1800000000,"provider":"claude-code","source":"testhost","windows":[{"id":"five_hour","resetsAt":1789499400,"usedPercentage":39},{"id":"seven_day","resetsAt":1789527600,"usedPercentage":45}]}'

expect "reset time without fraction or with Z" \
  "$(run '{"five_hour":{"utilization":10,"resets_at":"2026-09-15T19:10:00Z"}}')" \
  '{"observedAt":1800000000,"provider":"claude-code","source":"testhost","windows":[{"id":"five_hour","resetsAt":1789499400,"usedPercentage":10}]}'

expect "null utilization drops the window, keeps the other" \
  "$(run '{"five_hour":{"utilization":null,"resets_at":"2026-09-15T19:10:00+00:00"},"seven_day":{"utilization":45,"resets_at":"2026-09-16T03:00:00+00:00"}}')" \
  '{"observedAt":1800000000,"provider":"claude-code","source":"testhost","windows":[{"id":"seven_day","resetsAt":1789527600,"usedPercentage":45}]}'

expect "unparseable reset drops the window" \
  "$(run '{"five_hour":{"utilization":10,"resets_at":"soon"}}')" \
  '{"observedAt":1800000000,"provider":"claude-code","source":"testhost","windows":[]}'

expect "utilization is clamped to 0..100" \
  "$(run '{"five_hour":{"utilization":130,"resets_at":"2026-09-15T19:10:00Z"},"seven_day":{"utilization":-5,"resets_at":"2026-09-16T03:00:00Z"}}')" \
  '{"observedAt":1800000000,"provider":"claude-code","source":"testhost","windows":[{"id":"five_hour","resetsAt":1789499400,"usedPercentage":100},{"id":"seven_day","resetsAt":1789527600,"usedPercentage":0}]}'

expect "zero percent is preserved" \
  "$(run '{"five_hour":{"utilization":0,"resets_at":"2026-09-15T19:10:00Z"}}')" \
  '{"observedAt":1800000000,"provider":"claude-code","source":"testhost","windows":[{"id":"five_hour","resetsAt":1789499400,"usedPercentage":0}]}'

expect "missing windows yield an empty list" \
  "$(run '{"extra_usage":{"user_disabled":true}}')" \
  '{"observedAt":1800000000,"provider":"claude-code","source":"testhost","windows":[]}'

[ "$FAILURES" -eq 0 ] || exit 1
echo "all usage mapping tests passed"
