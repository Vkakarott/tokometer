# Maps the Claude account usage response (the data behind Claude Code's /usage
# screen) to the backend ingest payload.

# "2026-09-15T19:10:00.110795+00:00" -> epoch seconds; null when unparseable.
def epoch:
  try (sub("\\.[0-9]+"; "") | sub("\\+00:00$"; "Z") | fromdateiso8601) catch null;

def percentage:
  [[., 0] | max, 100] | min;

# A window missing either value is dropped: the backend never gets a null.
def window($id):
  select(. != null and .utilization != null and .resets_at != null)
  | (.resets_at | epoch) as $resets
  | select($resets != null)
  | { id: $id, usedPercentage: (.utilization | percentage), resetsAt: $resets };

{
  provider: "claude-code",
  source: $src,
  observedAt: $now,
  windows: [
    (.five_hour | window("five_hour")),
    (.seven_day | window("seven_day"))
  ]
}
