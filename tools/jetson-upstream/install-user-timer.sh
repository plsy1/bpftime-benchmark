#!/bin/sh
set -eu

worktree=$(git rev-parse --show-toplevel)
unit_dir=${XDG_CONFIG_HOME:-"$HOME/.config"}/systemd/user
source_dir="$worktree/tools/jetson-upstream/systemd"

mkdir -p "$unit_dir"
sed "s|@WORKTREE@|$worktree|g" \
  "$source_dir/bpftime-jetson-daily.service" \
  > "$unit_dir/bpftime-jetson-daily.service"
cp "$source_dir/bpftime-jetson-daily.timer" \
  "$unit_dir/bpftime-jetson-daily.timer"
systemctl --user daemon-reload
systemctl --user enable --now bpftime-jetson-daily.timer
systemctl --user list-timers bpftime-jetson-daily.timer
