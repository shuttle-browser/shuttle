#!/usr/bin/env bash
# package-linux.sh: any shipped output newer than the build receipt (incl. ones the old check list missed) is refused.
. "$(dirname "$0")/lib.sh"; fake_src
stub realpath 'for a; do [[ $a == -m ]] || python3 -c "import os,sys;print(os.path.realpath(sys.argv[1]))" "$a"; done'
cd "$SHUTTLE_SRC" || exit 1
O=out/Release; mkdir -p "$O/locales" "$O/gen/components/resources" chrome/app/theme/chromium/linux "third_party/llvm-build/Release+Asserts"
printf 'MAJOR=155\nMINOR=0\nBUILD=8059\nPATCH=80\n' > chrome/VERSION; echo deps > DEPS; echo lic > LICENSE
for s in 24 48 64 128 256; do touch "chrome/app/theme/chromium/linux/product_logo_$s.png"; done
echo llvmorg-test > "third_party/llvm-build/Release+Asserts/cr_build_revision"
git init -q "$HOME/depot_tools" && git -C "$HOME/depot_tools" -c user.name=f -c user.email=f@f commit -q --allow-empty -m d
git add -A && git -c user.name=f -c user.email=f@f commit -q -m tree && git branch -f chromium
echo 'is_official_build = true' > "$O/args.gn"
for f in shuttle chrome_crashpad_handler chrome_sandbox resources.pak icudtl.dat locales/en-US.pak snapshot_blob.bin \
         theme_resources_100_percent.pak gen/components/resources/about_credits.html; do echo x > "$O/$f"; done
chmod +x "$O/shuttle"; touch -t 202001010000 "$O"/* "$O"/*/* "$O"/gen/components/resources/*
. "$SCRIPTS/lib/paths.sh"; . "$SCRIPTS/lib/receipt.sh"
export SHUTTLE_BUILD_PLAIN=1; shuttle_receipt_begin "$O"; shuttle_receipt_end "$O" 0 "$O/shuttle" >/dev/null
[[ -f "$O/shuttle-build-receipt.txt" ]] || { echo "fixture setup: no receipt"; exit 1; }
sleep 1; touch "$O/theme_resources_100_percent.pak"          # a resource rebuilt outside build-linux.sh
SHUTTLE_VERSION=155.1.0 expect 2 bash "$SCRIPTS/package-linux.sh" "$O" "$FIXTURE_TMP/dest"; expect_out "rebuilt after the receipt"
[[ ! -e "$FIXTURE_TMP/dest/shuttle-linux-x64.tar.xz" ]] || { echo "tarball written"; failures=$((failures + 1)); }
done_fixture
