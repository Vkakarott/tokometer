{
  provider: "claude-code",
  source: $src,
  observedAt: $now,
  windows: [
    (.rate_limits.five_hour // empty
      | { id: "five_hour", usedPercentage: .used_percentage, resetsAt: .resets_at }),
    (.rate_limits.seven_day // empty
      | { id: "seven_day", usedPercentage: .used_percentage, resetsAt: .resets_at })
  ]
}
