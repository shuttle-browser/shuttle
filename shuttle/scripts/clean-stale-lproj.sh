#!/bin/bash
# clean-stale-lproj.sh <out-dir>: deletes .lproj dirs other than en/base from Shuttle.app (and its framework,
# helpers). After the en-US-only change (B3) no rule produces them, but old ones stay in the out dir and would ship.
set -e
out=${1:?out dir}
app="$out/Shuttle.app"
[[ -d $app ]] || { echo "no $app"; exit 1; }
n=0
while IFS= read -r d; do
  case "$(basename "$d")" in en.lproj|base.lproj|Base.lproj) ;; *) rm -rf "$d"; n=$((n+1));; esac
done < <(find "$app" -type d -name '*.lproj' -prune)
echo "removed $n stale .lproj dirs from $app"
