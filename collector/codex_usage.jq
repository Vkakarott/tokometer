# Maps the ChatGPT account usage response (what Codex shows) to the backend
# ingest payload. Windows are matched by their length, not by primary/secondary,
# so a reordered response can never swap 5H and 7D.

def window_id:
  if .limit_window_seconds == 18000 then "five_hour"
  elif .limit_window_seconds == 604800 then "seven_day"
  else null end;

def percentage:
  [[., 0] | max, 100] | min;

# A window missing either value is dropped: the backend never gets a null.
def window:
  select(. != null and .used_percent != null and .reset_at != null)
  | window_id as $id
  | select($id != null)
  | { id: $id, usedPercentage: (.used_percent | percentage), resetsAt: .reset_at };

{
  provider: "codex",
  source: $src,
  observedAt: $now,
  windows: ([(.rate_limit.primary_window, .rate_limit.secondary_window) | window] | unique_by(.id))
}
