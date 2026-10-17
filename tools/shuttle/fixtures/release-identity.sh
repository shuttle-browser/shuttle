#!/usr/bin/env bash
# Build receipt (lib/receipt.sh), pinned sidecar (fetch-agent-browser.sh), release-mac.sh refusals before signing.
. "$(dirname "$0")/lib.sh"; fake_src
export SHUTTLE_ROOT="$FIXTURE_TMP"
cd "$SHUTTLE_SRC" || exit 1
mkdir -p chrome out/R && printf 'MAJOR=155\nMINOR=0\nBUILD=8059\nPATCH=80\n' > chrome/VERSION && echo deps > DEPS
echo 'is_debug = false' > out/R/args.gn && git add chrome DEPS && git -c user.name=f -c user.email=f@f commit -q -m v
mkdir -p "third_party/llvm-build/Release+Asserts" && echo llvmorg-test > "third_party/llvm-build/Release+Asserts/cr_build_revision"
git init -q "$HOME/depot_tools" && git -C "$HOME/depot_tools" -c user.name=f -c user.email=f@f commit -q --allow-empty -m d
. "$SCRIPTS/lib/paths.sh"; . "$SCRIPTS/lib/receipt.sh"
mkdir -p out/R/app; touch out/R/bin out/R/app/res; sleep 1
export SHUTTLE_BUILD_PLAIN=0                                          # dry run / tool flags: no receipt
shuttle_receipt_begin out/R; shuttle_receipt_end out/R 0 out/R/bin 2>/dev/null
[[ ! -f out/R/shuttle-build-receipt.txt ]] || { echo "receipt written for a dry run"; failures=$((failures + 1)); }
export SHUTTLE_BUILD_PLAIN=1
shuttle_receipt_begin out/R; shuttle_receipt_end out/R 0 out/R/missing 2>/dev/null
[[ ! -f out/R/shuttle-build-receipt.txt ]] || { echo "receipt written without the output"; failures=$((failures + 1)); }
shuttle_receipt_begin out/R; shuttle_receipt_end out/R 0 out/R/bin >/dev/null
expect 0 shuttle_receipt_check out/R out/R/bin out/R/app; expect_out "tree_clean=yes"
expect 2 shuttle_receipt_check out/R out/R/bin out/R/gone                # shipped file missing
git -c user.name=f -c user.email=f@f commit -q --allow-empty -m later
expect 2 shuttle_receipt_check out/R out/R/bin; expect_out "differs from the build"   # packaging from another commit
git reset -q --hard HEAD~1
sleep 1; touch out/R/app/res
expect 2 shuttle_receipt_check out/R out/R/bin out/R/app; expect_out "rebuilt after the receipt"  # resource-only rebuild
touch -t 202001010000 out/R/app/res
echo 'is_debug = true' > out/R/args.gn
expect 2 shuttle_receipt_check out/R out/R/bin                       # args changed since the build
echo 'is_debug = false' > out/R/args.gn; sleep 1; touch out/R/bin
expect 2 shuttle_receipt_check out/R out/R/bin                       # binary rebuilt outside build.sh
shuttle_receipt_begin out/R; shuttle_receipt_end out/R 1 out/R/bin
[[ ! -f out/R/shuttle-build-receipt.txt ]] || { echo "receipt written for a failed build"; failures=$((failures + 1)); }
shuttle_receipt_begin out/R; echo x >> DEPS; shuttle_receipt_end out/R 0 out/R/bin 2>/dev/null
[[ ! -f out/R/shuttle-build-receipt.txt ]] || { echo "receipt written though the tree changed mid-build"; failures=$((failures + 1)); }
shuttle_receipt_begin out/R; shuttle_receipt_end out/R 0 out/R/bin >/dev/null
expect 2 shuttle_receipt_check out/R out/R/bin; expect_out "dirty tree"
git checkout -q DEPS
# identity reads fail closed: a failing hasher, missing toolchain metadata, a failed freshness scan
shuttle_receipt_begin out/R; shuttle_receipt_end out/R 0 out/R/bin >/dev/null
( shasum() { return 1; }; sha256sum() { return 1; }; expect 2 shuttle_receipt_check out/R out/R/bin; exit $failures ) || failures=$((failures + 1))
mv third_party/llvm-build/Release+Asserts/cr_build_revision llvm.bak
expect 2 shuttle_receipt_check out/R out/R/bin 2>/dev/null
shuttle_receipt_begin out/R; shuttle_receipt_end out/R 0 out/R/bin 2>/dev/null
[[ ! -f out/R/shuttle-build-receipt.txt ]] || { echo "receipt written without toolchain metadata"; failures=$((failures + 1)); }
mv llvm.bak third_party/llvm-build/Release+Asserts/cr_build_revision
shuttle_receipt_begin out/R; shuttle_receipt_end out/R 0 out/R/bin >/dev/null
( find() { return 1; }; expect 2 shuttle_receipt_check out/R out/R/bin; exit $failures ) || failures=$((failures + 1))
# sidecar: a package whose binary doesn't match the pin is refused before copy/exec
p="$FIXTURE_TMP/pkg"; mkdir -p "$p/bin"; echo '{ "version": "0.38.2" }' > "$p/package.json"; echo lic > "$p/LICENSE"
printf '#!/bin/sh\necho agent-browser 0.38.2\n' > "$p/bin/agent-browser-linux-x64"; chmod +x "$p/bin/agent-browser-linux-x64"
AGENT_BROWSER_SRC=$p expect 1 bash "$SCRIPTS/fetch-agent-browser.sh" "$FIXTURE_TMP/h" linux-x64; expect_out "refusing"
[[ ! -e "$FIXTURE_TMP/h/agent-browser" ]] || { echo "unpinned binary installed"; failures=$((failures + 1)); }
SHUTTLE_RELEASE_MODE=1 SHUTTLE_SIDECAR_UNPINNED=1 AGENT_BROWSER_SRC=$p expect 2 bash "$SCRIPTS/fetch-agent-browser.sh" "$FIXTURE_TMP/h" linux-x64
# release-mac.sh: version scheme and destination checks happen before anything is built or signed
export SHUTTLE_SIGN_ID=0 SHUTTLE_NOTARY_PROFILE=x; mkdir -p out/Release/Shuttle.app chrome/installer/mac && touch chrome/installer/mac/sign_chrome.py
expect 2 zsh "$SCRIPTS/release-mac.sh"; expect_out "SHUTTLE_VERSION"
SHUTTLE_VERSION=154.1.0 expect 2 zsh "$SCRIPTS/release-mac.sh"; expect_out "Chromium major 155"
mkdir -p "$FIXTURE_TMP/release/v155.1.0/mac"
SHUTTLE_VERSION=155.1.0 expect 2 zsh "$SCRIPTS/release-mac.sh"; expect_out "never overwritten"
rmdir "$FIXTURE_TMP/release/v155.1.0/mac"
SHUTTLE_VERSION=155.1.0 expect 2 zsh "$SCRIPTS/release-mac.sh"; expect_out "no build receipt"
done_fixture
