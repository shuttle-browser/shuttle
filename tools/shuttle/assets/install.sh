#!/usr/bin/env bash
# Shuttle installer:  curl -fsSL https://get.shuttlebrowser.com | bash
#
#   macOS (Apple Silicon): the latest DMG from GitHub releases, checksum-verified, copied to /Applications
#                          (~/Applications if /Applications isn't writable).
#   Linux (x86_64):        apt or dnf: adds the signed repo at https://pkg.shuttlebrowser.com and installs the
#                          `shuttle` package, so updates come with `apt upgrade` / `dnf upgrade` (needs root or sudo).
#                          Other distros or no sudo: the tarball into ~/.local/share/shuttle + ~/.local/bin/shuttle.
#
# Options (env): SHUTTLE_CHANNEL=stable|testing (Linux repo suite, default stable)
#                SHUTTLE_METHOD=repo|tarball    (Linux, default: repo when apt/dnf + root/sudo exist)
#                SHUTTLE_APPS_DIR=<dir>         (macOS, default /Applications)
#                SHUTTLE_LAUNCH=ask|yes|no      (start Shuttle when done; default ask, no without a terminal)
# Source: https://github.com/shuttle-browser/shuttle/blob/main/tools/shuttle/assets/install.sh
#
# Everything runs inside main(), called on the last line, so a truncated download runs nothing.

set -euo pipefail

TMP=
MNT=
cleanup() {
  if [ -n "$MNT" ]; then hdiutil detach -quiet "$MNT" 2>/dev/null || true; fi
  if [ -n "$TMP" ]; then rm -rf "$TMP"; fi
}
trap cleanup EXIT

REPO_URL=https://pkg.shuttlebrowser.com
RELEASES=https://github.com/shuttle-browser/shuttle/releases/latest/download
# Release files are fetched through get.shuttlebrowser.com/dl/ (counted, then redirected to $RELEASES); the
# checksums still come straight from GitHub.
DL=https://get.shuttlebrowser.com/dl

say()  { printf '\033[1m==>\033[0m %s\n' "$*"; }
warn() { printf '\033[33mwarning:\033[0m %s\n' "$*" >&2; }
die()  { printf '\033[31merror:\033[0m %s\n' "$*" >&2; exit 1; }
have() { command -v "$1" >/dev/null 2>&1; }

fetch() { curl -fsSL --proto '=https' --tlsv1.2 --retry 3 "$@"; }

# Prints "<sha256>  <file>" for the release file matching the pattern (grep -E on the file name).
release_entry() {
  local line
  line=$(fetch "$RELEASES/SHA256SUMS" | grep -E "  $1\$" | head -1) || true
  [ -n "$line" ] || die "no file matching '$1' in the latest release"
  printf '%s\n' "$line"
}

sha256_of() {
  if have sha256sum; then sha256sum "$1" | cut -d' ' -f1; else shasum -a 256 "$1" | cut -d' ' -f1; fi
}

download_verified() {  # $1 = file name in the release, $2 = expected sha256, $3 = destination
  say "Downloading $1"
  fetch -o "$3" "$DL/$1?via=installer"
  [ "$(sha256_of "$3")" = "$2" ] || die "checksum mismatch for $1"
}

# Offer to start Shuttle. With `curl | bash` stdin is the script, so ask on the terminal; no terminal
# (CI, ssh -T) means don't start it unless SHUTTLE_LAUNCH=yes.
offer_launch() {  # $@ = command that starts Shuttle in the background
  local mode=${SHUTTLE_LAUNCH:-ask} answer=
  case $mode in
    no) return 0 ;;
    yes) ;;
    ask)
      { true </dev/tty; } 2>/dev/null || return 0
      printf '\033[1m==>\033[0m Start Shuttle now? [Y/n] ' >/dev/tty
      read -r answer </dev/tty || answer=n
      case $answer in ''|[Yy]*) ;; *) return 0 ;; esac ;;
    *) die "SHUTTLE_LAUNCH must be ask, yes or no" ;;
  esac
  say "Starting Shuttle"
  "$@"
}

start_linux() {  # $1 = shuttle binary; detached, so it outlives this script and the terminal
  # --shuttle-open-agents: open chrome://agents (quick start); without a tty Shuttle would stay windowless.
  if have setsid; then setsid -f "$1" --shuttle-open-agents >/dev/null 2>&1 </dev/null
  else nohup "$1" --shuttle-open-agents >/dev/null 2>&1 </dev/null & fi
}

can_start_linux() {  # a display to open windows on, and not root (Chromium's sandbox refuses root)
  [ -n "${DISPLAY:-}${WAYLAND_DISPLAY:-}" ] && [ "$(id -u)" != 0 ]
}

install_macos() {
  local arch
  arch=$(uname -m)
  if [ "$arch" != arm64 ] && [ "$(sysctl -n sysctl.proc_translated 2>/dev/null || echo 0)" != 1 ]; then
    die "Shuttle for macOS needs Apple Silicon (this Mac is $arch)"
  fi
  local entry sha file dest=${SHUTTLE_APPS_DIR:-/Applications}
  [ -n "${SHUTTLE_APPS_DIR:-}" ] || [ -w "$dest" ] || dest=$HOME/Applications
  mkdir -p "$dest"
  if pgrep -qf "$dest/Shuttle.app/Contents/MacOS/"; then
    die "Shuttle is running: quit it (Cmd-Q), then run the installer again"
  fi

  entry=$(release_entry 'Shuttle-[0-9.]+-arm64\.dmg')
  sha=${entry%%  *}; file=${entry#*  }
  TMP=$(mktemp -d)
  download_verified "$file" "$sha" "$TMP/$file"

  say "Installing Shuttle.app into $dest"
  mkdir -p "$TMP/mnt"
  hdiutil attach -quiet -nobrowse -readonly -mountpoint "$TMP/mnt" "$TMP/$file"
  MNT=$TMP/mnt
  [ -d "$MNT/Shuttle.app" ] || die "Shuttle.app not found in $file"
  rm -rf "$dest/Shuttle.app.new"
  ditto "$MNT/Shuttle.app" "$dest/Shuttle.app.new"
  rm -rf "$dest/Shuttle.app"
  mv "$dest/Shuttle.app.new" "$dest/Shuttle.app"

  say "Installed ${file%-arm64.dmg} to $dest/Shuttle.app"
  echo "    Start it: open -a Shuttle   (or from Launchpad / Applications)"
  echo "    Connect your agent: https://www.shuttlebrowser.com/download#connect"
  offer_launch open -a "$dest/Shuttle.app" --args --shuttle-open-agents
}

linux_repo_apt() {  # $1 = sudo prefix (may be empty), $2 = suite
  local s=$1
  say "Adding the Shuttle apt repository ($2)"
  $s install -d -m 0755 /etc/apt/keyrings
  fetch "$REPO_URL/shuttle.gpg" | $s tee /etc/apt/keyrings/shuttle.gpg >/dev/null
  $s chmod 0644 /etc/apt/keyrings/shuttle.gpg
  echo "deb [arch=amd64 signed-by=/etc/apt/keyrings/shuttle.gpg] $REPO_URL/apt $2 main" \
    | $s tee /etc/apt/sources.list.d/shuttle.list >/dev/null
  say "Installing shuttle"
  $s env DEBIAN_FRONTEND=noninteractive apt-get update -q \
    -o Dir::Etc::sourcelist=sources.list.d/shuttle.list -o Dir::Etc::sourceparts=- -o APT::Get::List-Cleanup=0
  $s env DEBIAN_FRONTEND=noninteractive apt-get install -y -q shuttle || {
    # dependencies may need fresh lists of the distro's own repos
    $s env DEBIAN_FRONTEND=noninteractive apt-get update -q
    $s env DEBIAN_FRONTEND=noninteractive apt-get install -y -q shuttle
  }
}

linux_repo_dnf() {  # $1 = sudo prefix (may be empty), $2 = suite
  local s=$1 name=shuttle.repo
  [ "$2" = stable ] || name=shuttle-$2.repo
  say "Adding the Shuttle dnf repository ($2)"
  fetch "$REPO_URL/rpm/$name" | $s tee /etc/yum.repos.d/shuttle.repo >/dev/null
  # Import the key now: with repo_gpgcheck=1, a later unattended `dnf upgrade` without -y would hang at the prompt.
  $s rpm --import "$REPO_URL/shuttle.asc"
  say "Installing shuttle"
  $s dnf install -y shuttle
}

linux_tarball() {
  local entry sha file root=$HOME/.local/share/shuttle bin=$HOME/.local/bin
  entry=$(release_entry 'shuttle-linux-x64\.tar\.xz')
  sha=${entry%%  *}; file=${entry#*  }
  have xz || die "the tarball needs xz (install xz-utils / xz)"
  TMP=$(mktemp -d)
  download_verified "$file" "$sha" "$TMP/$file"
  say "Installing into $root"
  tar -C "$TMP" -xJf "$TMP/$file"
  [ -x "$TMP/shuttle/shuttle" ] || die "unexpected tarball layout"
  rm -rf "$root.new"; mkdir -p "$(dirname "$root")" "$bin"
  mv "$TMP/shuttle" "$root.new"
  rm -rf "$root"; mv "$root.new" "$root"
  ln -sf "$root/shuttle" "$bin/shuttle"
  say "Installed: $bin/shuttle"
  case ":$PATH:" in *":$bin:"*) ;; *) echo "    Add $bin to your PATH." ;; esac
  echo "    No updates and no dependency handling with the tarball: see $root/README-LINUX.txt for the"
  echo "    packages Shuttle needs (libnss3, libasound2, ...). The apt/dnf repo is the better path."
  if can_start_linux; then offer_launch start_linux "$bin/shuttle"; fi
}

install_linux() {
  [ "$(uname -m)" = x86_64 ] || die "Shuttle for Linux is x86_64 only for now (this machine is $(uname -m))"
  local suite=${SHUTTLE_CHANNEL:-stable} method=${SHUTTLE_METHOD:-} s=
  case $suite in stable|testing) ;; *) die "SHUTTLE_CHANNEL must be stable or testing" ;; esac

  if [ "$(id -u)" != 0 ]; then
    if have sudo; then s=sudo; else [ -n "$method" ] || method=tarball; fi
  fi
  if [ -z "$method" ]; then
    if have apt-get || have dnf; then method=repo; else method=tarball; fi
  fi

  if [ "$method" = repo ]; then
    [ "$(id -u)" = 0 ] || have sudo || die "the repo install needs root or sudo (or SHUTTLE_METHOD=tarball)"
    if [ -n "$s" ]; then say "sudo is needed to add the repository and install the package"; fi
    if have apt-get; then linux_repo_apt "$s" "$suite"
    elif have dnf; then linux_repo_dnf "$s" "$suite"
    else die "no apt-get or dnf found (use SHUTTLE_METHOD=tarball)"; fi
    say "Installed $(shuttle --version 2>/dev/null || echo Shuttle)"
    echo "    Start it: shuttle   (no window until an agent opens a tab; the app menu entry opens chrome://agents)"
    echo "    Updates: through apt/dnf. Connect your agent: https://www.shuttlebrowser.com/download#connect"
    if can_start_linux; then offer_launch start_linux "$(command -v shuttle)"; fi
  else
    linux_tarball
  fi
}

main() {
  have curl || die "curl is required"
  case $(uname -s) in
    Darwin) install_macos ;;
    Linux)  install_linux ;;
    *)      die "unsupported system: $(uname -s) (macOS and Linux only)" ;;
  esac
}

main "$@"
