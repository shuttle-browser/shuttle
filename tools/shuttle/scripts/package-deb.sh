#!/usr/bin/env bash
# Builds shuttle_<ver>_amd64.deb and shuttle-<ver>-1.x86_64.rpm from a released Linux tarball (the exact bytes users get
# from the tarball, so both formats share one sha-checked input). Uses nfpm + config/nfpm-shuttle.yaml.
# Usage (Linux build host): scripts/package-deb.sh <shuttle-linux-x64.tar.xz> <pkg-version> [dest-dir]
#   pkg-version: Shuttle version, e.g. 155.1.0 (test builds 155.1.0~test1; "~" sorts before the release)
#   dest-dir:    default: the tarball's directory
# Env: SHUTTLE_PKG_REPO_SOURCE=1  postinst adds our package repo once (deb: apt source, rpm: /etc/yum.repos.d file) and
#        the packages ship the key from assets/linux/repo/shuttle.asc (off by default: only with the repo live)
#        SHUTTLE_PKG_REPO_URL=<url>  default https://pkg.shuttlebrowser.com
#      SHUTTLE_NO_RPM=1 skips the .rpm. NFPM=<path> (default: nfpm on PATH, else ~/.local/bin/nfpm).
# Layout: /opt/shuttle (tarball tree), /usr/bin/shuttle symlink, desktop file, hicolor icons, /usr/share/doc/shuttle.
set -euo pipefail

TOOLS=$(cd "$(dirname "$0")/.." && pwd)
[ $# -ge 2 ] || { sed -n '2,13p' "$0" >&2; exit 2; }
TARBALL=$(realpath "$1"); PKG_VERSION=$2
DEST=$(realpath -m "${3:-$(dirname "$TARBALL")}")
NFPM=${NFPM:-$(command -v nfpm || echo "$HOME/.local/bin/nfpm")}
[ -x "$NFPM" ] || { echo "FATAL: nfpm not found (install: docs/TOOLING.md)" >&2; exit 2; }
[[ $PKG_VERSION =~ ^[0-9]+\.[0-9]+\.[0-9]+([~+][A-Za-z0-9.]+)?$ ]] || { echo "FATAL: bad version '$PKG_VERSION'" >&2; exit 2; }

REPO_SOURCE=${SHUTTLE_PKG_REPO_SOURCE:-0}
REPO_URL=${SHUTTLE_PKG_REPO_URL:-https://pkg.shuttlebrowser.com}
REPO_KEY=$TOOLS/assets/linux/repo/shuttle.asc
if [ "$REPO_SOURCE" = 1 ]; then
  command -v gpg >/dev/null || { echo "FATAL: SHUTTLE_PKG_REPO_SOURCE=1 needs gpg (dearmors the repo key)" >&2; exit 2; }
fi

# Input check: tarball sha256 next to it must match.
if [ -f "$TARBALL.sha256" ]; then
  (cd "$(dirname "$TARBALL")" && sha256sum -c --quiet "$(basename "$TARBALL").sha256") || { echo "FATAL: sha256 mismatch" >&2; exit 2; }
else
  echo "warning: no $TARBALL.sha256, input not verified" >&2
fi

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
tar -C "$WORK" -xf "$TARBALL"
IN=$WORK/shuttle
[ -x "$IN/shuttle" ] && [ -f "$IN/chrome-sandbox" ] || { echo "FATAL: $TARBALL has no shuttle/shuttle + chrome-sandbox" >&2; exit 2; }
CHROMIUM_VERSION=$(sed -n '1s/^Shuttle \([0-9.]*\) .*/\1/p' "$IN/README-LINUX.txt")
[ -n "$CHROMIUM_VERSION" ] || { echo "FATAL: no Chromium version in README-LINUX.txt" >&2; exit 2; }

# --- Dependency cross-check (fails the build if the binaries need a library the curated lists don't cover) ---
CONF=$TOOLS/config/nfpm-shuttle.yaml
SHIPPED=$(cd "$IN" && ls lib*.so* | tr '\n' ' ')
ELFS=()
for f in "$IN"/*; do
  [ -f "$f" ] || continue
  case $(basename "$f") in libqt5_shim.so|libqt6_shim.so) continue ;; esac   # Qt deps stay optional
  head -c4 "$f" | grep -q 'ELF' && ELFS+=("$f")
done
fail=0
glibc_max=$(for f in "${ELFS[@]}"; do objdump -T "$f" | grep -o 'GLIBC_[0-9.]*'; done | sort -uV | tail -1)
[ "$(printf '%s\nGLIBC_2.28\n' "$glibc_max" | sort -V | tail -1)" = GLIBC_2.28 ] \
  || { echo "DEPS: binaries need $glibc_max, config says libc6 >= 2.28: raise both deb + rpm" >&2; fail=1; }
rpm_deps=$(sed -n '/^  rpm:/,/^  [a-z]/p' "$CONF")
for so in $(for f in "${ELFS[@]}"; do readelf -d "$f" | sed -n 's/.*(NEEDED).*\[\(.*\)\]/\1/p'; done | sort -u); do
  case " $SHIPPED " in *" $so "*) continue ;; esac
  case $so in libc.so.6|libm.so.6|libdl.so.2|libpthread.so.0|librt.so.1|ld-linux-x86-64.so.2) continue ;; esac
  grep -qF -- "- $so(" <<<"$rpm_deps" || { echo "DEPS: $so (NEEDED) missing from rpm depends in $CONF" >&2; fail=1; }
done
if command -v dpkg-shlibdeps >/dev/null; then
  mkdir -p "$WORK/shl/debian"; printf 'Source: shuttle\n\nPackage: shuttle\nArchitecture: amd64\n' > "$WORK/shl/debian/control"
  args=(); for f in "${ELFS[@]}"; do args+=(-e "$f"); done
  shl=$(cd "$WORK/shl" && dpkg-shlibdeps -O -l"$IN" "${args[@]}" 2>/dev/null | sed 's/^shlibs:Depends=//')
  deb_deps=$(sed -n '/^  deb:/,/^  rpm:/p' "$CONF")
  for p in $(tr ',' '\n' <<<"$shl" | sed 's/^ *//; s/ .*//; s/t64$//' | sort -u); do
    grep -qE -- "(- | \| )${p//./\\.}(t64)? " <<<"$deb_deps" || grep -qE -- "- ${p//./\\.}$" <<<"$deb_deps" \
      || { echo "DEPS: $p (dpkg-shlibdeps) missing from deb depends in $CONF" >&2; fail=1; }
  done
else
  echo "warning: dpkg-shlibdeps not installed (dpkg-dev), deb Depends not cross-checked" >&2
fi
[ $fail = 0 ] || { echo "FATAL: dependency lists out of date (docs/TOOLING.md: Linux packages)" >&2; exit 2; }

# --- Stage ---
STAGE=$WORK/stage
mkdir -p "$STAGE/opt" "$STAGE/usr/share/applications" "$STAGE/usr/share/doc/shuttle"
mv "$IN" "$STAGE/opt/shuttle"
O=$STAGE/opt/shuttle
mv "$O/chrome-sandbox" "$STAGE/chrome-sandbox"
rm -f "$O/README-LINUX.txt"   # tarball instructions (deps, ./shuttle) don't apply to the package
sed 's|@EXEC@|/usr/bin/shuttle|g' "$TOOLS/assets/linux/shuttle.desktop" > "$STAGE/usr/share/applications/shuttle.desktop"
for s in 24 48 64 128 256; do
  install -Dm644 "$O/product_logo_$s.png" "$STAGE/usr/share/icons/hicolor/${s}x$s/apps/shuttle.png"
done
cat > "$STAGE/usr/share/doc/shuttle/copyright" <<EOF
Format: https://www.debian.org/doc/packaging-manuals/copyright-format/1.0/
Upstream-Name: Shuttle
Upstream-Contact: Silkweave <tobias.strebitzer@gmail.com>
Source: https://github.com/shuttle-browser/shuttle

Files: *
Copyright: The Chromium Authors and Silkweave
License: BSD-3-Clause
 Shuttle is based on Chromium $CHROMIUM_VERSION. The full license text is in /opt/shuttle/LICENSE;
 third-party licenses are listed in /opt/shuttle/about_credits.html. The agent-browser sidecar
 (/opt/shuttle/agent-browser) is Apache-2.0: /opt/shuttle/agent-browser.LICENSE.
EOF
chmod -R u=rwX,go=rX "$STAGE/usr" "$O"
chmod 644 "$O"/lib*.so*   # shared libraries are not executables (lintian)
for s in postinst postrm; do
  sed -e "s|@REPO_SOURCE@|$REPO_SOURCE|" -e "s|@REPO_URL@|$REPO_URL|" "$TOOLS/assets/linux/deb/$s" > "$STAGE/$s"
done

export SHUTTLE_PKG_VERSION=$PKG_VERSION SHUTTLE_CHROMIUM_VERSION=$CHROMIUM_VERSION SHUTTLE_STAGE=$STAGE
render() {  # $1 = packager; repo key: deb = binary keyring for apt's signed-by, rpm = armored key for dnf's gpgkey
  rm -rf "${STAGE:?}/usr/share/keyrings" "${STAGE:?}/etc"
  if [ "$REPO_SOURCE" = 1 ]; then
    case $1 in
      deb) mkdir -p "$STAGE/usr/share/keyrings"
           gpg --dearmor < "$REPO_KEY" > "$STAGE/usr/share/keyrings/shuttle-archive-keyring.gpg" ;;
      rpm) install -Dm644 "$REPO_KEY" "$STAGE/etc/pki/rpm-gpg/RPM-GPG-KEY-shuttle" ;;
    esac
  fi
  local roots=(usr/share); [ -d "$STAGE/etc" ] && roots+=(etc)   # etc/ only holds the rpm repo key
  SHUTTLE_SHARE_CONTENTS=$(cd "$STAGE" && find "${roots[@]}" -type f | sort | while read -r f; do
    printf '  - src: %s\n    dst: /%s\n    file_info:\n      mode: 0644\n' "$STAGE/$f" "$f"; done)
  export SHUTTLE_SHARE_CONTENTS
  envsubst '${SHUTTLE_PKG_VERSION} ${SHUTTLE_CHROMIUM_VERSION} ${SHUTTLE_STAGE} ${SHUTTLE_SHARE_CONTENTS}' < "$CONF" > "$WORK/nfpm-$1.yaml"
}

mkdir -p "$DEST"
built=()
for p in deb rpm; do
  [ $p = rpm ] && [ "${SHUTTLE_NO_RPM:-}" = 1 ] && continue
  render $p
  case $p in deb) f=shuttle_${PKG_VERSION}_amd64.deb ;; rpm) f=shuttle-${PKG_VERSION}-1.x86_64.rpm ;; esac
  rm -f "$DEST/$f"
  "$NFPM" pkg --config "$WORK/nfpm-$p.yaml" --packager $p --target "$DEST/$f" >/dev/null
  (cd "$DEST" && sha256sum "$f" > "$f.sha256")
  built+=("$DEST/$f")
done

for f in "${built[@]}"; do echo "$f: $(du -h "$f" | cut -f1)  $(cut -c1-16 "$f.sha256")…"; done
if command -v lintian >/dev/null && [ -f "$DEST/shuttle_${PKG_VERSION}_amd64.deb" ]; then
  echo "lintian (advisory):"; lintian --no-tag-display-limit "$DEST/shuttle_${PKG_VERSION}_amd64.deb" 2>&1 | sed 's/^/  /' | sort | uniq -c | sort -rn | head -40 || true
fi
