#!/usr/bin/env bash
set -uo pipefail

JQ_FILE="$(dirname "$0")/../payload.jq"
FAILURES=0

run() {
  printf '%s' "$1" | jq -S -c --arg src "testhost" --argjson now 1800000000 -f "$JQ_FILE"
}

expect() {
  local name="$1" actual="$2" expected="$3"
  if [ "$actual" = "$(printf '%s' "$expected" | jq -S -c .)" ]; then
    echo "ok - $name"
  else
    echo "FAIL - $name"
    echo "  expected: $(printf '%s' "$expected" | jq -S -c .)"
    echo "  actual:   $actual"
    FAILURES=$((FAILURES + 1))
  fi
}

expect "both windows present" \
  "$(run '{"rate_limits":{"five_hour":{"used_percentage":23.5,"resets_at":1800003600},"seven_day":{"used_percentage":41.2,"resets_at":1800086400}}}')" \
  '{"observedAt":1800000000,"provider":"claude-code","source":"testhost","windows":[{"id":"five_hour","resetsAt":1800003600,"usedPercentage":23.5},{"id":"seven_day","resetsAt":1800086400,"usedPercentage":41.2}]}'

expect "context tokens are included when available" \
  "$(run '{"context_window":{"total_input_tokens":12500,"total_output_tokens":2400,"context_window_size":200000,"used_percentage":7.45}}')" \
  '{"context":{"inputTokens":12500,"outputTokens":2400,"usedPercentage":7.45,"windowSize":200000},"observedAt":1800000000,"provider":"claude-code","source":"testhost","windows":[]}'

expect "rate_limits absent yields empty windows, not an error" \
  "$(run '{"model":{"display_name":"Opus"}}')" \
  '{"observedAt":1800000000,"provider":"claude-code","source":"testhost","windows":[]}'

expect "only five_hour present" \
  "$(run '{"rate_limits":{"five_hour":{"used_percentage":10,"resets_at":1800003600}}}')" \
  '{"observedAt":1800000000,"provider":"claude-code","source":"testhost","windows":[{"id":"five_hour","resetsAt":1800003600,"usedPercentage":10}]}'

expect "zero percent is preserved, not dropped as falsy" \
  "$(run '{"rate_limits":{"five_hour":{"used_percentage":0,"resets_at":1800003600}}}')" \
  '{"observedAt":1800000000,"provider":"claude-code","source":"testhost","windows":[{"id":"five_hour","resetsAt":1800003600,"usedPercentage":0}]}'

expect "window with null percentage is dropped, the other is kept" \
  "$(run '{"rate_limits":{"five_hour":{"used_percentage":null,"resets_at":1800003600},"seven_day":{"used_percentage":41.2,"resets_at":1800086400}}}')" \
  '{"observedAt":1800000000,"provider":"claude-code","source":"testhost","windows":[{"id":"seven_day","resetsAt":1800086400,"usedPercentage":41.2}]}'

expect "window without resets_at is dropped" \
  "$(run '{"rate_limits":{"five_hour":{"used_percentage":10}}}')" \
  '{"observedAt":1800000000,"provider":"claude-code","source":"testhost","windows":[]}'

[ "$FAILURES" -eq 0 ] || exit 1
echo "all payload tests passed"
