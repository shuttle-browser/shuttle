#!/bin/bash
# Build the sccache-dist toolchain archive for Mac -> Linux build host remote compiles.
#
# Usage: scripts/sccache-toolchain.sh [chromium_src] [out.tar.gz]
#   defaults: $SHUTTLE_ROOT/src, ~/sccache/ac-clang-toolchain.tar.gz
# Env: SHUTTLE_SDK_VERSION (default 26.5) = Mac SDK version stamped into LC_BUILD_VERSION.
#
# The archive is the full sandbox rootfs the build server runs clang in (bwrap overlay):
#   /ac/llvm/bin/clang{,++}          Chromium's clang (Linux x86_64 host, any target)
#   /ac/llvm/lib/clang/<v>/{include,share}  resource dir (runtime libs not needed for -c)
#   /ac/bin/clang{,++}-wrap          archive_compiler_executable for the Mac client
#   /ac/src/<rel>                    compile-time files Chromium passes by ../../ path
#   /bin/sh (dash), glibc + libz/libgcc_s, /tmp (1777), /dev, /proc
# Re-run (and re-send the sha to the Mac) when: clang rolls, the Mac SDK changes, or any
# file in TOOLCHAIN_FILES changes in the fork. Output is deterministic (sorted, fixed mtime/owner).
set -euo pipefail
. "$(dirname "$0")/lib/paths.sh"
umask 002   # file modes are part of the archive sha
S=${1:-$SHUTTLE_SRC}
OUT=${2:-$HOME/sccache/ac-clang-toolchain.tar.gz}
SDK=${SHUTTLE_SDK_VERSION:-26.5}
L=$S/third_party/llvm-build/Release+Asserts
V=$(ls "$L/lib/clang")
R=$(mktemp -d)/root
trap 'rm -rf "$(dirname "$R")"' EXIT

# Non-preprocessor file args found in Chromium compile lines (out/Dev ninja, 2026-10-06).
TOOLCHAIN_FILES=(
  build/config/warning_suppression.txt                      # --warning-suppression-mappings=
  build/config/unsafe_buffers_paths.txt                     # unsafe-buffers plugin arg
  build/config/raw_ptr_plugin_config.yaml                   # raw-ptr plugin config-path=
  third_party/dawn/src/utils/warning_suppression_mappings.txt  # dawn targets
  tools/clang/alloc_token/ignores_apple.txt                 # -fsanitize-ignorelist= (if used)
)

mkdir -p "$R"/ac/llvm/bin "$R"/ac/llvm/lib/clang/"$V" "$R"/ac/bin "$R"/bin "$R"/lib64 \
  "$R"/usr/lib/x86_64-linux-gnu "$R"/tmp "$R"/dev "$R"/proc
chmod 1777 "$R"/tmp
cp "$L"/bin/clang "$R"/ac/llvm/bin/clang; ln -s clang "$R"/ac/llvm/bin/clang++
cp -r "$L"/lib/clang/"$V"/include "$L"/lib/clang/"$V"/share "$R"/ac/llvm/lib/clang/"$V"/
cp /bin/dash "$R"/bin/dash; ln -s dash "$R"/bin/sh
for lib in $( (ldd "$L"/bin/clang; ldd /bin/dash) | grep -oE '/[^ ]+\.so[^ ]*' | sort -u); do
  case $lib in /lib64/*) cp -L "$lib" "$R"/lib64/ ;; *) cp -L "$lib" "$R"/usr/lib/x86_64-linux-gnu/ ;; esac
done
ln -s usr/lib "$R"/lib
for f in "${TOOLCHAIN_FILES[@]}"; do
  mkdir -p "$R/ac/src/$(dirname "$f")"; cp "$S/$f" "$R/ac/src/$f"
done

for d in clang clang++; do cat > "$R/ac/bin/$d-wrap" <<EOF
#!/bin/sh
# Shuttle sccache-dist wrapper: rewrite compile-time (non-preprocessor) file
# args from ../../<path> to the copies under /ac/src, then exec the real $d with
# -Wno-gnu-line-marker (for -frewrite-includes output) and, for macOS targets, the
# Mac SDK version (no SDKSettings.json remotely; keeps LC_BUILD_VERSION identical).
AC_SDK_VERSION=$SDK
# The client forwards the Mac env (TMPDIR=/var/folders/...): force the sandbox /tmp.
TMPDIR=/tmp; export TMPDIR
mac=
for a; do
  shift
  case "\$a" in
    --warning-suppression-mappings=../../*) a="--warning-suppression-mappings=/ac/src/\${a#--warning-suppression-mappings=../../}" ;;
    config-path=../../*) a="config-path=/ac/src/\${a#config-path=../../}" ;;
    -fsanitize-ignorelist=../../*) a="-fsanitize-ignorelist=/ac/src/\${a#-fsanitize-ignorelist=../../}" ;;
    ../../build/config/unsafe_buffers_paths.txt) a=/ac/src/build/config/unsafe_buffers_paths.txt ;;
  esac
  case "\$a" in *-apple-macos*) mac=1 ;; esac
  set -- "\$@" "\$a"
done
if [ -n "\$mac" ]; then set -- "\$@" -Xclang -target-sdk-version=\$AC_SDK_VERSION; fi
exec /ac/llvm/bin/$d "\$@" -Wno-gnu-line-marker -Wno-unused-macros
EOF
chmod 755 "$R/ac/bin/$d-wrap"; done

# Smoke: both wrappers compile a GNU-line-marker TU and an .S for arm64-apple-macos in bwrap.
T=$(dirname "$R")/w; mkdir -p "$T"/out/X
printf '# 1 "../../base/t.cc"\n#define ONLY_IN_IF 1\n#if ONLY_IN_IF\n#endif\nint f(int x) { return x; }\n' > "$T"/out/X/t.cc
printf '.text\n.globl _f\n_f:\n  ret\n' > "$T"/out/X/x.S
for w in clang-wrap clang++-wrap; do
  for src in "-x c++ t.cc" "x.S"; do
    env -i TMPDIR=/var/folders/none PATH=/usr/bin:/bin bwrap --ro-bind "$R" / --dev /dev --proc /proc \
      --tmpfs /tmp --bind "$T" /tmp/w --chdir /tmp/w/out/X --unshare-all \
      /ac/bin/$w --target=arm64-apple-macos12 -Werror -Wunused-macros -Wgnu-line-marker \
      --warning-suppression-mappings=../../build/config/warning_suppression.txt $src -c -o o.o
  done
done
echo "smoke OK (clang $(cat "$L"/cr_build_revision 2>/dev/null || echo "$V"), SDK $SDK)"

mkdir -p "$(dirname "$OUT")"
tar -C "$R" --sort=name --owner=0 --group=0 --numeric-owner --mtime='2026-10-06 00:00Z' -czf "$OUT" .
sha256sum "$OUT" | tee "$OUT.sha256"
