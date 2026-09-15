#!/usr/bin/env bash
# Installs the usage poller and its launchd agent.
#
# The scripts are copied out of the repository on purpose: macOS privacy
# protection blocks launchd agents from running files inside ~/Documents.
# Re-run after updating the collector to refresh the installed copy.
set -euo pipefail

SOURCE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
INSTALL_DIR="${TOKESP_INSTALL_DIR:-$HOME/.local/share/tokesp}"
LABEL="com.tokesp.usage-poll"
DOMAIN="gui/$(id -u)"
PLIST="$HOME/Library/LaunchAgents/$LABEL.plist"

install_files() {
  mkdir -p "$INSTALL_DIR" "$HOME/Library/LaunchAgents" "$HOME/Library/Logs"
  install -m 755 "$SOURCE_DIR/usage_poll.sh" "$INSTALL_DIR/usage_poll.sh"
  install -m 644 "$SOURCE_DIR/usage.jq" "$INSTALL_DIR/usage.jq"
}

unload_agent() {
  launchctl bootout "$DOMAIN/$LABEL" 2>/dev/null || return 0
  for _ in $(seq 20); do
    launchctl print "$DOMAIN/$LABEL" >/dev/null 2>&1 || return 0
    sleep 0.25
  done
}

load_agent() {
  sed -e "s#__COLLECTOR_DIR__#$INSTALL_DIR#g" -e "s#__HOME__#$HOME#g" \
    "$SOURCE_DIR/launchd/$LABEL.plist" > "$PLIST"
  plutil -lint "$PLIST" >/dev/null
  launchctl bootstrap "$DOMAIN" "$PLIST"
}

install_files
unload_agent
load_agent
echo "Collector instalado em $INSTALL_DIR"
echo "Hook Stop do ~/.claude/settings.json: $INSTALL_DIR/usage_poll.sh --detach"
