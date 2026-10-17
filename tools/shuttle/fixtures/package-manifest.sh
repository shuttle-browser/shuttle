#!/usr/bin/env bash
# package-deb.sh (release mode): refuses a tarball whose manifest is missing, belongs to another tarball, or names
# another version, before packaging anything. (Late paths need the Linux toolchain: covered on Linux runs.)
. "$(dirname "$0")/lib.sh"
stub realpath 'for a; do [[ $a == -m ]] || python3 -c "import os,sys;print(os.path.realpath(sys.argv[1]))" "$a"; done'
command -v sha256sum >/dev/null || stub sha256sum 'if [[ $1 == -c ]]; then shift; [[ $1 == --quiet ]] && shift; while read -r h f; do [[ $(shasum -a 256 "$f" | cut -d" " -f1) == "$h" ]] || exit 1; done < "$1"; else shasum -a 256 "$@"; fi'
stub nfpm 'echo "nfpm $*" >> '"$FIXTURE_TMP/calls"
export NFPM="$FIXTURE_TMP/bin/nfpm"
r="$FIXTURE_TMP/rel"; mkdir -p "$r" pkg/shuttle
printf '#!/bin/sh\n' > pkg/shuttle/shuttle; chmod +x pkg/shuttle/shuttle; touch pkg/shuttle/chrome-sandbox
echo "tree_clean=yes" > pkg/shuttle/BUILD-RECEIPT.txt; echo "Shuttle 155.0.8059.80 for Linux" > pkg/shuttle/README-LINUX.txt
tar -C pkg -cf "$r/shuttle-linux-x64.tar.xz" shuttle
(cd "$r" && sha256sum shuttle-linux-x64.tar.xz > shuttle-linux-x64.tar.xz.sha256)
D="$SCRIPTS/package-deb.sh"; T="$r/shuttle-linux-x64.tar.xz"
expect 2 bash "$D" "$T" 155.1.0; expect_out "is not the manifest"                       # no manifest
printf 'shuttle_version=155.1.0\nartifact_sha256=%064d\n' 0 > "$r/MANIFEST.txt"
expect 2 bash "$D" "$T" 155.1.0; expect_out "is not the manifest"                       # another tarball's
printf 'shuttle_version=155.2.0\nartifact_sha256=%s\n' "$(sha256sum "$T" | cut -d' ' -f1)" > "$r/MANIFEST.txt"
expect 2 bash "$D" "$T" 155.1.0; expect_out "shuttle_version"                           # another version
[[ ! -s "$FIXTURE_TMP/calls" ]] || { echo "packaged despite a refusal"; failures=$((failures + 1)); }
done_fixture
