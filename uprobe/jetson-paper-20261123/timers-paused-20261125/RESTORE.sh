#!/usr/bin/env bash
# Restore the timers paused on 2026-11-25 for the ATC measurement campaign (all were enabled before).
set -x
sudo systemctl enable --now anacron.timer apt-daily.timer apt-daily-upgrade.timer dpkg-db-backup.timer e2scrub_all.timer fstrim.timer fwupd-refresh.timer man-db.timer motd-news.timer update-notifier-download.timer update-notifier-motd.timer
sudo systemctl start systemd-tmpfiles-clean.timer
systemctl list-timers --all --no-pager
