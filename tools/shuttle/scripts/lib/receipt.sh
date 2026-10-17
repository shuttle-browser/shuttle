# Sourced by build.sh / build-linux.sh (write) and release-mac.sh / package-linux.sh (check), after lib/paths.sh.
# A build receipt says which source/config/toolchain a binary was built from (maintenance review F7). Written only
# after a successful plain `chrome` build (no -n or other tool flags: SHUTTLE_BUILD_PLAIN) whose expected output
# exists, and only if the identity below read cleanly and didn't change while the build ran.
#   shuttle_receipt_begin <out_dir>                drop the old receipt, capture the identity
#   shuttle_receipt_end <out_dir> <rc> <output>    on rc 0: write <out_dir>/shuttle-build-receipt.txt
#   shuttle_receipt_fresh <out_dir> <file|dir>...  just the "no shipped file newer than the receipt" part
#   shuttle_receipt_check <out_dir> <file|dir>...  for packaging: receipt present, identity now == receipt (same
#                                                  clean HEAD, args, DEPS, toolchain: commit before you build), no
#                                                  shipped file newer than the receipt; prints the receipt
shuttle_sha256() {  # prints a 64-hex digest or fails
  local o
  if command -v sha256sum >/dev/null; then o=$(sha256sum "$1") || return 1; else o=$(shasum -a 256 "$1") || return 1; fi
  o=${o%% *}; [ ${#o} = 64 ] || return 1; echo "$o"
}
shuttle_receipt_identity() {  # <out_dir>: key=value lines; returns 1 if any value can't be read
  local head st base ver args deps gl gls clang depot llvm="$SHUTTLE_SRC/third_party/llvm-build/Release+Asserts/cr_build_revision"
  head=$(git -C "$SHUTTLE_SRC" rev-parse HEAD) || return 1
  st=$(git -C "$SHUTTLE_SRC" status --porcelain --untracked-files=no) || return 1
  base=$(git -C "$SHUTTLE_SRC" rev-parse -q --verify chromium) || return 1
  ver=$(. "$SHUTTLE_SRC/chrome/VERSION" && echo "$MAJOR.$MINOR.$BUILD.$PATCH") || return 1
  args=$(shuttle_sha256 "$1/args.gn") || return 1
  deps=$(shuttle_sha256 "$SHUTTLE_SRC/DEPS") || return 1
  gl=$(git -C "$SHUTTLE_SRC" ls-files -s) || return 1
  printf '%s\n' "$gl" | awk '$1 == "160000"' > "$1/.receipt-gitlinks.tmp" || return 1
  gls=$(shuttle_sha256 "$1/.receipt-gitlinks.tmp"); rm -f "$1/.receipt-gitlinks.tmp"; [ -n "$gls" ] || return 1
  clang=$(cat "$llvm") && [ -n "$clang" ] || return 1
  depot=$(git -C "$HOME/depot_tools" rev-parse HEAD) || return 1
  printf '%s\n' "source_sha=$head" "tree_clean=$([ -z "$st" ] && echo yes || echo no)" "chromium_base=$base" \
    "chromium_version=$ver" "args_gn_sha256=$args" "deps_sha256=$deps" "gitlinks_sha256=$gls" "clang=$clang" \
    "depot_tools=$depot"
}
shuttle_receipt_begin() { rm -f "$1/shuttle-build-receipt.txt"; SHUTTLE_RECEIPT_START=$(shuttle_receipt_identity "$1") || SHUTTLE_RECEIPT_START=""; }
shuttle_receipt_end() {
  [ "$2" = 0 ] || return 0
  [ "${SHUTTLE_BUILD_PLAIN:-0}" = 1 ] || { echo "receipt: not a plain build (dry run/tool flags): none written" >&2; return 0; }
  [ -e "$3" ] || { echo "receipt: expected output $3 missing: none written" >&2; return 0; }
  local now; now=$(shuttle_receipt_identity "$1") || now=""
  if [ -z "$now" ] || [ -z "$SHUTTLE_RECEIPT_START" ] || [ "$now" != "$SHUTTLE_RECEIPT_START" ]; then
    echo "receipt: source/config identity unreadable or changed during the build: none written" >&2; return 0
  fi
  { printf '%s\n' "$now"; echo "host=$(uname -sm | tr ' ' '-')"; echo "built_at=$(date -u +%Y-%m-%dT%H:%M:%SZ)"; } \
    > "$1/shuttle-build-receipt.txt"
  echo "receipt: $1/shuttle-build-receipt.txt"
}
shuttle_receipt_check() {
  local out=$1 r="$1/shuttle-build-receipt.txt" now newer; shift
  [ -f "$r" ] || { echo "FATAL: no build receipt in $out: build it with build.sh / build-linux.sh" >&2; return 2; }
  grep -qx 'tree_clean=yes' "$r" || { echo "FATAL: $out was built from a dirty tree ($r)" >&2; return 2; }
  now=$(shuttle_receipt_identity "$out") || { echo "FATAL: can't read the current source identity" >&2; return 2; }
  [ "$now" = "$(grep -v -e '^host=' -e '^built_at=' "$r")" ] ||
    { echo "FATAL: checkout/config differs from the build ($r): package from the commit you built" >&2; diff <(printf '%s\n' "$now") <(grep -v -e '^host=' -e '^built_at=' "$r") >&2; return 2; }
  shuttle_receipt_fresh "$out" "$@" || return 2
  cat "$r"
}
shuttle_receipt_fresh() {  # <out_dir> <file|dir>...: all exist, none newer than the receipt, traversal must succeed
  local r="$1/shuttle-build-receipt.txt" newer; shift
  for f; do [ -e "$f" ] || { echo "FATAL: $f missing" >&2; return 2; }; done
  newer=$(find "$@" -type f -newer "$r" ! -path '*/Contents/Helpers/agent-browser' ! -name 'agent-browser.*') ||
    { echo "FATAL: can't scan the shipped files" >&2; return 2; }
  [ -z "$newer" ] || { echo "FATAL: rebuilt after the receipt (outside build.sh?): $(printf '%s\n' "$newer" | head -3)" >&2; return 2; }
}
