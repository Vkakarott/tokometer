#!/usr/bin/env bash
# Installs the Claude and Codex usage collectors and their launchd agents.
#
# The scripts are copied out of the repository on purpose: macOS privacy
# protection blocks launchd agents from running files inside ~/Documents.
# Re-run after updating the collector to refresh the installed copy.
set -euo pipefail

SOURCE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
INSTALL_DIR="${TOKESP_INSTALL_DIR:-$HOME/.local/share/tokesp}"
DOMAIN="gui/$(id -u)"

install_files() {
  mkdir -p "$INSTALL_DIR" "$HOME/Library/LaunchAgents" "$HOME/Library/Logs"
  install -m 644 "$SOURCE_DIR/lib.sh" "$INSTALL_DIR/lib.sh"
  install -m 755 "$SOURCE_DIR/usage_poll.sh" "$SOURCE_DIR/codex_usage_poll.sh" "$INSTALL_DIR/"
  install -m 644 "$SOURCE_DIR/usage.jq" "$SOURCE_DIR/codex_usage.jq" "$INSTALL_DIR/"
}

# $1: agent label.
unload_agent() {
  launchctl bootout "$DOMAIN/$1" 2>/dev/null || return 0
  for _ in $(seq 20); do
    launchctl print "$DOMAIN/$1" >/dev/null 2>&1 || return 0
    sleep 0.25
  done
}

# $1: agent label, $2: installed script, $3: log file name.
load_agent() {
  local plist="$HOME/Library/LaunchAgents/$1.plist"
  sed -e "s#__LABEL__#$1#g" \
      -e "s#__PROGRAM__#$INSTALL_DIR/$2#g" \
      -e "s#__LOG__#$HOME/Library/Logs/$3.log#g" \
    "$SOURCE_DIR/launchd/usage-poll.plist" > "$plist"
  plutil -lint "$plist" >/dev/null
  launchctl bootstrap "$DOMAIN" "$plist"
}

install_agent() {
  unload_agent "$1"
  load_agent "$@"
}

install_files
install_agent com.tokesp.usage-poll usage_poll.sh tokesp-usage-poll
install_agent com.tokesp.codex-usage-poll codex_usage_poll.sh tokesp-codex-usage-poll
echo "Collectors instalados em $INSTALL_DIR"
echo "Hook Stop do ~/.claude/settings.json: $INSTALL_DIR/usage_poll.sh --detach"
echo "notify do ~/.codex/config.toml: [\"$INSTALL_DIR/codex_usage_poll.sh\", \"--detach\"]"
