#!/usr/bin/env bash
set -uo pipefail

JQ_FILE="$(dirname "$0")/../codex_usage.jq"
FAILURES=0
# jq 1.7 keeps number literals as written (38.0); compare numbers by value.
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

FIVE='{"used_percent":38,"limit_window_seconds":18000,"reset_after_seconds":15733,"reset_at":1789574599}'
SEVEN='{"used_percent":54,"limit_window_seconds":604800,"reset_after_seconds":430270,"reset_at":1789989136}'
BOTH='{"id":"five_hour","resetsAt":1789574599,"usedPercentage":38},{"id":"seven_day","resetsAt":1789989136,"usedPercentage":54}'

expect "maps the real ChatGPT usage response" \
  "$(run "{\"plan_type\":\"plus\",\"rate_limit\":{\"allowed\":true,\"primary_window\":$FIVE,\"secondary_window\":$SEVEN},\"credits\":{\"balance\":\"0\"}}")" \
  "{\"observedAt\":1800000000,\"provider\":\"codex\",\"source\":\"testhost\",\"windows\":[$BOTH]}"

expect "windows are matched by length, not by primary/secondary" \
  "$(run "{\"rate_limit\":{\"primary_window\":$SEVEN,\"secondary_window\":$FIVE}}")" \
  "{\"observedAt\":1800000000,\"provider\":\"codex\",\"source\":\"testhost\",\"windows\":[$BOTH]}"

expect "unknown window length is dropped" \
  "$(run "{\"rate_limit\":{\"primary_window\":{\"used_percent\":10,\"limit_window_seconds\":3600,\"reset_at\":1},\"secondary_window\":$SEVEN}}")" \
  '{"observedAt":1800000000,"provider":"codex","source":"testhost","windows":[{"id":"seven_day","resetsAt":1789989136,"usedPercentage":54}]}'

expect "null percentage drops the window" \
  "$(run "{\"rate_limit\":{\"primary_window\":{\"used_percent\":null,\"limit_window_seconds\":18000,\"reset_at\":1},\"secondary_window\":null}}")" \
  '{"observedAt":1800000000,"provider":"codex","source":"testhost","windows":[]}'

expect "percentage is clamped to 0..100" \
  "$(run '{"rate_limit":{"primary_window":{"used_percent":130,"limit_window_seconds":18000,"reset_at":5}}}')" \
  '{"observedAt":1800000000,"provider":"codex","source":"testhost","windows":[{"id":"five_hour","resetsAt":5,"usedPercentage":100}]}'

expect "missing rate_limit yields an empty list" \
  "$(run '{"plan_type":"free","rate_limit":null}')" \
  '{"observedAt":1800000000,"provider":"codex","source":"testhost","windows":[]}'

[ "$FAILURES" -eq 0 ] || exit 1
echo "all codex usage mapping tests passed"
