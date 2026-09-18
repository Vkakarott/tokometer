#!/bin/zsh
set -euo pipefail

script_dir=${0:A:h}
project_dir=${script_dir:h}
cd "$project_dir"

swift build --product TokEsp
binary_dir=$(swift build --show-bin-path)
bundle_path="$project_dir/.build/TokEsp.app"

rm -rf "$bundle_path"
mkdir -p "$bundle_path/Contents/MacOS"
cp "$binary_dir/TokEsp" "$bundle_path/Contents/MacOS/TokEsp"
cp "$project_dir/Resources/Info.plist" "$bundle_path/Contents/Info.plist"

echo "$bundle_path"
