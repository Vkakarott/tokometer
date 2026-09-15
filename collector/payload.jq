.context_window as $context
| {
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
  + (
    if (
      $context.total_input_tokens != null and
      $context.total_output_tokens != null and
      $context.context_window_size != null and
      $context.used_percentage != null
    ) then {
      context: {
        inputTokens: $context.total_input_tokens,
        outputTokens: $context.total_output_tokens,
        windowSize: $context.context_window_size,
        usedPercentage: $context.used_percentage
      }
    } else {} end
  )
