#!/usr/bin/env bash
# Installs the tokometer backend as a launchd agent that starts with the Mac.
#
# Both run from a copy outside the repository on purpose: macOS privacy
# protection blocks launchd agents from reading files inside ~/Documents.
# Re-run after changing the backend to deploy the new version. The
# backend state (pairings and last usage) is never overwritten.
set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
INSTALL_DIR="${TOKESP_INSTALL_DIR:-$HOME/.local/share/tokesp}"
DOMAIN="gui/$(id -u)"
NODE_BIN="$(command -v node)"

deploy_backend() {
  local target="$INSTALL_DIR/backend"
  mkdir -p "$target"
  rsync -a --delete --exclude .env --exclude 'state.json*' \
    "$REPO_DIR/backend/package.json" "$REPO_DIR/backend/src" "$REPO_DIR/backend/node_modules" "$target/"
  install -m 600 "$REPO_DIR/backend/.env" "$target/.env"
  # First install only: carry over the pairings and the last usage.
  if [ ! -f "$target/state.json" ] && [ -f "$REPO_DIR/backend/state.json" ]; then
    install -m 600 "$REPO_DIR/backend/state.json" "$target/state.json"
  fi
}

# $1: label, $2: working directory, $3: log name, $4...: program arguments.
write_plist() {
  local label="$1" workdir="$2" log="$HOME/Library/Logs/$3.log" plist="$HOME/Library/LaunchAgents/$1.plist"
  shift 3
  {
    printf '<?xml version="1.0" encoding="UTF-8"?>\n'
    printf '<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">\n'
    printf '<plist version="1.0">\n<dict>\n'
    printf '  <key>Label</key>\n  <string>%s</string>\n' "$label"
    printf '  <key>ProgramArguments</key>\n  <array>\n'
    printf '    <string>%s</string>\n' "$@"
    printf '  </array>\n'
    printf '  <key>WorkingDirectory</key>\n  <string>%s</string>\n' "$workdir"
    printf '  <key>RunAtLoad</key>\n  <true/>\n  <key>KeepAlive</key>\n  <true/>\n'
    printf '  <key>StandardOutPath</key>\n  <string>%s</string>\n' "$log"
    printf '  <key>StandardErrorPath</key>\n  <string>%s</string>\n' "$log"
    printf '</dict>\n</plist>\n'
  } > "$plist"
  plutil -lint "$plist" >/dev/null
}

# $1: agent label.
unload_agent() {
  launchctl bootout "$DOMAIN/$1" 2>/dev/null || return 0
  for _ in $(seq 20); do
    launchctl print "$DOMAIN/$1" >/dev/null 2>&1 || return 0
    sleep 0.25
  done
}

# $1: agent label.
load_agent() {
  launchctl bootstrap "$DOMAIN" "$HOME/Library/LaunchAgents/$1.plist"
}

[ -n "$NODE_BIN" ] || { echo "node não encontrado no PATH" >&2; exit 1; }
mkdir -p "$HOME/Library/LaunchAgents" "$HOME/Library/Logs"

unload_agent com.tokesp.backend
unload_agent com.tokesp.web
deploy_backend

write_plist com.tokesp.backend "$INSTALL_DIR/backend" tokesp-backend \
  "$NODE_BIN" --env-file=.env src/main.ts
load_agent com.tokesp.backend

echo "Backend instalado em $INSTALL_DIR"
echo "Logs: ~/Library/Logs/tokesp-backend.log"
