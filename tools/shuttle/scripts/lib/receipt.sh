# Sourced by build.sh / build-linux.sh (write) and release-mac.sh / package-linux.sh (check), after lib/paths.sh.
# A build receipt says which source/config/toolchain a binary was built from (maintenance review F7). It is written
# only after a successful `chrome` build and only if HEAD and the tracked tree didn't change while it ran:
#   shuttle_receipt_begin <out_dir>        drop the old receipt, capture HEAD + clean-tree state
#   shuttle_receipt_end <out_dir> <rc>     on rc 0 write <out_dir>/shuttle-build-receipt.txt (key=value lines)
#   shuttle_receipt_check <out_dir> <bin>  for packaging: receipt present, clean tree, args.gn unchanged, binary not
#                                          newer than the receipt (rebuilt outside build.sh); prints the receipt
shuttle_receipt_state() {  # HEAD + dirty flag of tracked files
  echo "$(git -C "$SHUTTLE_SRC" rev-parse HEAD) $([ -z "$(git -C "$SHUTTLE_SRC" status --porcelain --untracked-files=no)" ] && echo yes || echo no)"
}
shuttle_sha256() { if command -v sha256sum >/dev/null; then sha256sum "$1" | cut -d' ' -f1; else shasum -a 256 "$1" | cut -d' ' -f1; fi; }
shuttle_receipt_begin() { rm -f "$1/shuttle-build-receipt.txt"; SHUTTLE_RECEIPT_START=$(shuttle_receipt_state); }
shuttle_receipt_end() {
  [ "$2" = 0 ] || return 0
  local now; now=$(shuttle_receipt_state)
  if [ "$now" != "$SHUTTLE_RECEIPT_START" ]; then
    echo "receipt: HEAD or tracked files changed during the build; no receipt written" >&2; return 0
  fi
  local llvm="$SHUTTLE_SRC/third_party/llvm-build/Release+Asserts/cr_build_revision"
  {
    echo "source_sha=${now% *}"
    echo "tree_clean=${now#* }"
    echo "chromium_base=$(git -C "$SHUTTLE_SRC" rev-parse -q --verify chromium 2>/dev/null || echo none)"
    echo "chromium_version=$(. "$SHUTTLE_SRC/chrome/VERSION"; echo "$MAJOR.$MINOR.$BUILD.$PATCH")"
    echo "args_gn_sha256=$(shuttle_sha256 "$1/args.gn")"
    echo "deps_sha256=$(shuttle_sha256 "$SHUTTLE_SRC/DEPS")"
    echo "gitlinks_sha256=$(git -C "$SHUTTLE_SRC" ls-files -s | awk '$1 == "160000"' > "$1/.gitlinks.tmp"; shuttle_sha256 "$1/.gitlinks.tmp"; rm -f "$1/.gitlinks.tmp")"
    echo "clang=$(cat "$llvm" 2>/dev/null || echo unknown)"
    echo "depot_tools=$(git -C "$HOME/depot_tools" rev-parse HEAD 2>/dev/null || echo unknown)"
    echo "host=$(uname -sm | tr ' ' '-')"
    echo "built_at=$(date -u +%Y-%m-%dT%H:%M:%SZ)"
  } > "$1/shuttle-build-receipt.txt"
  echo "receipt: $1/shuttle-build-receipt.txt (${now% *}, clean=${now#* })"
}
shuttle_receipt_check() {
  local r="$1/shuttle-build-receipt.txt"
  [ -f "$r" ] || { echo "FATAL: no build receipt in $1: build it with build.sh / build-linux.sh" >&2; return 2; }
  grep -qx 'tree_clean=yes' "$r" || { echo "FATAL: $1 was built from a dirty tree ($r)" >&2; return 2; }
  grep -qx "args_gn_sha256=$(shuttle_sha256 "$1/args.gn")" "$r" || { echo "FATAL: $1/args.gn changed since the build" >&2; return 2; }
  [ ! "$2" -nt "$r" ] || { echo "FATAL: $2 is newer than its receipt (rebuilt outside build.sh?)" >&2; return 2; }
  cat "$r"
}
