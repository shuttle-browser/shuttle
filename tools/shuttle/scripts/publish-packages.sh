#!/usr/bin/env bash
# Publishes Shuttle .deb/.rpm packages to the signed repo at https://pkg.shuttlebrowser.com (R2 bucket shuttle-packages):
# apt at /apt (suites stable + testing, component main, amd64), dnf at /rpm/<suite>, key at /shuttle.{asc,gpg}.
# Usage (Mac): scripts/publish-packages.sh [--suite stable|testing] [--dry-run] [file.deb|file.rpm ...]
#   No files: regenerate + re-sign the metadata and upload whatever changed (key, index page, .repo files).
#   --dry-run: build and sign in a scratch copy, list the uploads, change nothing.
# Master copy: $SHUTTLE_RELEASE/pkg-repo = what the bucket holds (.state/: suite lists, upload manifest; not uploaded).
# Needs: gpg + the signing key ($SHUTTLE_PKG_KEY; pinentry asks for the passphrase), rpm from brew (rpmsign),
#   $SHUTTLE_BUILD_HOST with apt-utils + createrepo-c (metadata), wrangler ($SHUTTLE_ROOT/shuttle-website, or
#   SHUTTLE_WRANGLER="<cmd>"), $SHUTTLE_CF_ACCOUNT. Versions are immutable: a file name that is already in the repo with
#   other bytes is refused (bump the version instead).
set -euo pipefail
. "$(dirname "$0")/lib/paths.sh"
KEY=${SHUTTLE_PKG_KEY:-81D0827DDC5004E5CCCD674943FDBF62DB5D8304}
URL=${SHUTTLE_PKG_URL:-https://pkg.shuttlebrowser.com}
BUCKET=${SHUTTLE_PKG_BUCKET:-shuttle-packages}
HOST=${SHUTTLE_BUILD_HOST:?set SHUTTLE_BUILD_HOST in ~/.shuttle/env}
: "${SHUTTLE_CF_ACCOUNT:?set SHUTTLE_CF_ACCOUNT in ~/.shuttle/env}"
REPO=$SHUTTLE_RELEASE/pkg-repo
RDIR=.cache/shuttle-pkg-repo   # working copy on the build host, relative to ~
ASSETS=$SHUTTLE_TOOLS/assets/linux/repo
SUITES="stable testing"

suite=stable dry=0 files=()
while [ $# -gt 0 ]; do
  case $1 in
    --suite) suite=${2:-}; shift 2 ;;
    --dry-run) dry=1; shift ;;
    -h|--help) sed -n '2,12p' "$0"; exit 0 ;;
    -*) echo "unknown option $1" >&2; exit 2 ;;
    *) [ -f "$1" ] || { echo "FATAL: no such file $1" >&2; exit 2; }
       files+=("$(cd "$(dirname "$1")" && pwd -P)/$(basename "$1")"); shift ;;
  esac
done
case " $SUITES " in *" $suite "*) ;; *) echo "FATAL: --suite must be one of: $SUITES" >&2; exit 2 ;; esac
command -v rpmsign >/dev/null || { echo "FATAL: rpmsign missing (brew install rpm)" >&2; exit 2; }
GPG=$(command -v gpg)
fpr=$("$GPG" --show-keys --with-colons "$ASSETS/shuttle.asc" | awk -F: '/^fpr/{print $10; exit}')
[ "$fpr" = "$KEY" ] || { echo "FATAL: $ASSETS/shuttle.asc is $fpr, signing key is $KEY" >&2; exit 2; }
"$GPG" --list-secret-keys "$KEY" >/dev/null 2>&1 || { echo "FATAL: secret key $KEY not in this keyring" >&2; exit 2; }

wrangler() {
  if [ -n "${SHUTTLE_WRANGLER:-}" ]; then CLOUDFLARE_ACCOUNT_ID=$SHUTTLE_CF_ACCOUNT $SHUTTLE_WRANGLER "$@"
  else (cd "$SHUTTLE_ROOT/shuttle-website" && CLOUDFLARE_ACCOUNT_ID=$SHUTTLE_CF_ACCOUNT npx --no-install wrangler "$@"); fi
}
sha() { shasum -a 256 "$1" | cut -d' ' -f1; }
sign() { "$GPG" --quiet --yes --local-user "$KEY" --digest-algo SHA512 "$@"; }

WORK=$(mktemp -d /tmp/sh-pkg.XXXXXX)
trap 'rm -rf "$WORK"' EXIT
mkdir -p "$REPO/.state"
cp -cR "$REPO" "$WORK/repo" 2>/dev/null || cp -R "$REPO" "$WORK/repo"   # APFS clone: cheap scratch copy
R=$WORK/repo
touch "$R/.state/uploaded" "$R/.state/rpm-inputs" "$R/.state/apt-stable.list" "$R/.state/apt-testing.list"

# --- 1. Add packages ---
for f in ${files[@]+"${files[@]}"}; do
  b=$(basename "$f")
  if [ -f "$f.sha256" ]; then
    [ "$(sha "$f")" = "$(cut -d' ' -f1 "$f.sha256")" ] || { echo "FATAL: $b does not match $b.sha256" >&2; exit 2; }
  fi
  case $b in
    shuttle_*_amd64.deb)
      dst=$R/apt/pool/main/s/shuttle/$b
      if [ -f "$dst" ]; then
        cmp -s "$f" "$dst" || { echo "FATAL: $b is already in the repo with other bytes (bump the version)" >&2; exit 2; }
      else
        mkdir -p "$(dirname "$dst")"; cp "$f" "$dst"
      fi
      grep -qxF "$b" "$R/.state/apt-$suite.list" || echo "$b" >> "$R/.state/apt-$suite.list"
      echo "+ apt $suite: $b" ;;
    shuttle-*.x86_64.rpm)
      dst=$R/rpm/$suite/$b in=$(sha "$f")
      old=$(awk -v k="$suite/$b" '$2==k{print $1}' "$R/.state/rpm-inputs")
      if [ -n "$old" ]; then
        [ "$old" = "$in" ] || { echo "FATAL: $suite/$b is already in the repo from other bytes (bump the version)" >&2; exit 2; }
        echo "= rpm $suite: $b (already published)"; continue
      fi
      mkdir -p "$WORK/sign" "$(dirname "$dst")"; cp "$f" "$WORK/sign/$b"
      rpmsign --addsign --define "__gpg $GPG" --define "_gpg_name $KEY" "$WORK/sign/$b" 2>&1 | grep -v GPG_TTY || true
      rpm -qp --qf '%{RSAHEADER:pgpsig}\n' "$WORK/sign/$b" 2>/dev/null | grep -qi "Key ID $(echo "${KEY: -16}" | tr A-F a-f)" \
        || { echo "FATAL: $b: rpmsign did not add our signature" >&2; exit 2; }
      mv "$WORK/sign/$b" "$dst"
      echo "$in $suite/$b" >> "$R/.state/rpm-inputs"
      echo "+ rpm $suite: $b (signed)" ;;
    *) echo "FATAL: $b: expected shuttle_<ver>_amd64.deb or shuttle-<ver>-<rel>.x86_64.rpm" >&2; exit 2 ;;
  esac
done

# --- 2. Static files ---
cp "$ASSETS/shuttle.asc" "$R/shuttle.asc"
"$GPG" --yes --dearmor -o "$R/shuttle.gpg" "$ASSETS/shuttle.asc"
sed -e "s|@URL@|$URL|g" -e "s|@FPR@|$KEY|g" "$ASSETS/index.html" > "$R/index.html"
mkdir -p "$R/rpm"
for s in $SUITES; do
  sfx=-$s; [ "$s" = stable ] && sfx=
  sed -e "s|@URL@|$URL|g" -e "s|@SUITE@|$s|g" -e "s|@ID_SUFFIX@|$sfx|g" "$ASSETS/shuttle.repo" > "$R/rpm/shuttle$sfx.repo"
done

# --- 3. Metadata on the build host (apt-ftparchive, createrepo_c) ---
echo "== metadata on $HOST"
ssh "$HOST" "mkdir -p $RDIR"
rsync -a --delete "$R/" "$HOST:$RDIR/"
ssh "$HOST" bash -s -- "$RDIR" "$SUITES" <<'REMOTE'
set -euo pipefail
cd ~/"$1"; shift; SUITES="$*"   # ssh joins the args into one command line: the suites arrive as separate words
for t in apt-ftparchive createrepo_c; do
  command -v $t >/dev/null || { echo "FATAL: $t missing on $(hostname): sudo apt install apt-utils createrepo-c" >&2; exit 2; }
done
for s in $SUITES; do
  rm -rf "apt/dists/$s"
  if [ -s ".state/apt-$s.list" ]; then
    t=$(mktemp -d "$HOME/.cache/shuttle-pkg-tmp.XXXXXX"); mkdir -p "$t/pool/main/s/shuttle"
    while read -r b; do ln "apt/pool/main/s/shuttle/$b" "$t/pool/main/s/shuttle/$b"; done < ".state/apt-$s.list"
    d=apt/dists/$s/main/binary-amd64; mkdir -p "$d"
    (cd "$t" && apt-ftparchive packages pool) > "$d/Packages"
    gzip -9nkf "$d/Packages"
    rm -rf "$t"
    o=APT::FTPArchive::Release
    apt-ftparchive -o $o::Origin=Shuttle -o $o::Label=Shuttle -o $o::Suite=$s -o $o::Codename=$s \
      -o $o::Architectures=amd64 -o $o::Components=main -o "$o::Description=Shuttle Browser ($s)" \
      release "apt/dists/$s" > "$HOME/.cache/shuttle-pkg-Release"
    mv "$HOME/.cache/shuttle-pkg-Release" "apt/dists/$s/Release"
    echo "apt $s: $(grep -c '^Package:' "$d/Packages") package(s)"
  fi
  if ls rpm/"$s"/*.rpm >/dev/null 2>&1; then
    createrepo_c --quiet --update --general-compress-type=gz "rpm/$s"
    echo "rpm $s: $(ls rpm/"$s"/*.rpm | wc -l) package(s)"
  fi
done
REMOTE
rsync -a --delete --exclude='*.deb' --exclude='*.rpm' --exclude=/.state "$HOST:$RDIR/" "$R/"

# --- 4. Sign (Mac keyring; never leaves this machine) ---
echo "== sign"
verify() { gpgv --keyring "$R/shuttle.gpg" "$@" 2>/dev/null || { echo "FATAL: signature check failed: $*" >&2; exit 2; }; }
for s in $SUITES; do
  d=$R/apt/dists/$s
  if [ -f "$d/Release" ]; then
    sign --clearsign -o "$d/InRelease" "$d/Release"
    sign --armor --detach-sign -o "$d/Release.gpg" "$d/Release"
    verify "$d/InRelease"; verify "$d/Release.gpg" "$d/Release"
  fi
  m=$R/rpm/$s/repodata/repomd.xml
  if [ -f "$m" ]; then
    sign --armor --detach-sign -o "$m.asc" "$m"
    verify "$m.asc" "$m"
  fi
done

# --- 5. Upload what changed. Order: packages + hashed metadata, then signatures, then the entry points
#        (InRelease, repomd.xml) last, so clients never see an index that points at missing files. ---
(cd "$R" && find . -type f ! -path './.state/*' | sed 's|^\./||' | sort) > "$WORK/all"
: > "$WORK/todo"
while read -r p; do
  h=$(sha "$R/$p")
  grep -qxF "$h $p" "$R/.state/uploaded" || echo "$p" >> "$WORK/todo"
done < "$WORK/all"
rank() { case $1 in */InRelease|*/repomd.xml) echo 3 ;; */Release|*/Release.gpg|*/repomd.xml.asc) echo 2 ;; *) echo 1 ;; esac; }
while read -r p; do echo "$(rank "$p") $p"; done < "$WORK/todo" | sort -k1,1n -k2 | cut -d' ' -f2- > "$WORK/order"
n=$(wc -l < "$WORK/order" | tr -d ' ')
echo "== upload $n file(s) to r2://$BUCKET"
sed 's/^/  /' "$WORK/order"
if [ "$dry" = 1 ]; then echo "dry run: nothing uploaded, $REPO unchanged"; exit 0; fi

ctype() {
  case $1 in
    *.deb) echo application/vnd.debian.binary-package ;; *.rpm) echo application/x-rpm ;;
    *.gz) echo application/gzip ;; *.xml) echo application/xml ;; *.html) echo 'text/html; charset=utf-8' ;;
    *.asc|*/Release.gpg) echo application/pgp-signature ;; *.gpg) echo application/pgp-keys ;;
    *) echo 'text/plain; charset=utf-8' ;;
  esac
}
cache() {   # package files and content-addressed repodata never change; everything else must not be cached
  case $1 in
    *.deb|*.rpm) echo 'public, max-age=31536000, immutable' ;;
    */repodata/repomd.xml|*/repodata/repomd.xml.asc) echo no-cache ;;
    */repodata/*) echo 'public, max-age=31536000, immutable' ;;
    *) echo no-cache ;;
  esac
}
while read -r p; do
  printf '  put %s ... ' "$p"
  wrangler r2 object put "$BUCKET/$p" --file "$R/$p" --content-type "$(ctype "$p")" --cache-control "$(cache "$p")" \
    --remote >"$WORK/put.log" 2>&1 || { echo FAILED; cat "$WORK/put.log" >&2; echo "FATAL: upload failed; $REPO unchanged, re-run to retry" >&2; exit 1; }
  echo ok
done < "$WORK/order"
while read -r p; do echo "$(sha "$R/$p") $p"; done < "$WORK/all" > "$R/.state/uploaded"
rsync -a --delete "$R/" "$REPO/"
echo "== published; master copy $REPO"

# --- 6. Check the live repo (custom domain) ---
if [ -f "$R/apt/dists/$suite/InRelease" ]; then
  if curl -fsS "$URL/apt/dists/$suite/InRelease" -o "$WORK/live" && cmp -s "$WORK/live" "$R/apt/dists/$suite/InRelease"; then
    verify "$WORK/live"; echo "live: $URL/apt/dists/$suite/InRelease ok"
  else
    echo "warning: $URL/apt/dists/$suite/InRelease not live yet (or cached); check the custom domain" >&2
  fi
fi
