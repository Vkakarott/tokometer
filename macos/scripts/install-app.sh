#!/bin/zsh
set -euo pipefail

script_dir=${0:A:h}
project_dir=${script_dir:h}
install_dir="${TOKESP_INSTALL_DIR:-$HOME/.local/share/tokesp}"
app_source="$project_dir/.build/TokEsp.app"
app_target="$install_dir/TokEsp.app"
label="com.tokesp.app"
launch_dir="$HOME/Library/LaunchAgents"
plist="$launch_dir/$label.plist"
log="$HOME/Library/Logs/tokesp-app.log"
domain="gui/$(id -u)"

"$script_dir/build-app.sh" >/dev/null
mkdir -p "$install_dir" "$launch_dir" "$HOME/Library/Logs"
rm -rf "$app_target"
ditto "$app_source" "$app_target"

launchctl bootout "$domain/$label" 2>/dev/null || true
sed \
  -e "s#__PROGRAM__#$app_target/Contents/MacOS/TokEsp#g" \
  -e "s#__LOG__#$log#g" \
  "$project_dir/launchd/$label.plist" > "$plist"
launchctl bootstrap "$domain" "$plist"

echo "TokEsp instalado em $app_target"
echo "Início automático ativado para o login"
