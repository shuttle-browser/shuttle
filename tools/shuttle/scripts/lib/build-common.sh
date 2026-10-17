# Sourced by build.sh (zsh) and build-linux.sh (bash) after lib/paths.sh: the build safety rules in code.
# shellcheck disable=SC2034  # SHUTTLE_BUILD_* globals are read by the callers
#   shuttle_build_lock <out_dir>     one build per out dir (mkdir lock + pid; any existing lock refuses); call before
#                                    touching args.gn / gn gen / logs, and `trap shuttle_build_unlock EXIT` yourself
#   shuttle_build_disk <out_dir>     refuse below SHUTTLE_MIN_FREE_GB (default 20) free on its filesystem
#   shuttle_build_args <default_jobs> <args...>  -> SHUTTLE_BUILD_ARGS: the args with exactly one valid
#                                    -local_jobs (given as `-local_jobs N` or `-local_jobs=N`; else
#                                    $SHUTTLE_LOCAL_JOBS; else the default); a second or non-numeric value is an error
#                                    SHUTTLE_BUILD_PLAIN=1 if the args are only -local_jobs / -k (receipt-eligible)
#   shuttle_build_prefix             -> SHUTTLE_BUILD_PREFIX: nice -n ${NICE:-19} (+ taskpolicy -c background on
#                                    macOS when SHUTTLE_BG=1; it pins work to the efficiency cores, ~14x slower)
# Errors print to stderr and return 2; callers `|| exit 2`.
shuttle_build_err() { echo "build: $*" >&2; return 2; }

shuttle_build_lock() {  # fails closed on any existing lock; a dead owner's lock is removed by hand (message says how)
  SHUTTLE_BUILD_LOCK="$(dirname "$1")/.$(basename "$1").build-lock"
  mkdir -p "$(dirname "$1")"
  if ! mkdir "$SHUTTLE_BUILD_LOCK" 2>/dev/null; then
    local l=$SHUTTLE_BUILD_LOCK p; SHUTTLE_BUILD_LOCK=""; p=$(cat "$l/pid" 2>/dev/null)
    if [ -n "$p" ] && kill -0 "$p" 2>/dev/null; then shuttle_build_err "another build (pid $p) runs in $1"; return; fi
    shuttle_build_err "stale lock $l (owner ${p:-unknown} not running): if no build runs there, rm -rf '$l'"; return
  fi
  echo $$ > "$SHUTTLE_BUILD_LOCK/pid"
}
shuttle_build_unlock() {  # only our own lock (pid file = this shell)
  if [ -n "${SHUTTLE_BUILD_LOCK:-}" ] && [ "$(cat "$SHUTTLE_BUILD_LOCK/pid" 2>/dev/null)" = "$$" ]; then rm -rf "$SHUTTLE_BUILD_LOCK"; fi
  SHUTTLE_BUILD_LOCK=""
}

shuttle_build_disk() {
  local min=${SHUTTLE_MIN_FREE_GB:-20} free
  free=$(df -Pk "$1" | awk 'NR==2 {print int($4 / 1048576)}')
  [ -n "$free" ] || { shuttle_build_err "can't read free space of $1"; return; }
  [ "$free" -ge "$min" ] || shuttle_build_err "only ${free} GB free on $1 (need ${min}; SHUTTLE_MIN_FREE_GB)"
}

shuttle_build_valid_jobs() { case "$1" in ''|*[!0-9]*|0) return 1 ;; esac; }

shuttle_build_args() {
  local def=$1 jobs="" next=0 knext=0 a
  shift
  SHUTTLE_BUILD_ARGS=(); SHUTTLE_BUILD_PLAIN=1  # 0 = dry run/tool/other flags: never writes a build receipt
  for a in "$@"; do
    if [ "$next" = 1 ]; then next=0; [ -z "$jobs" ] || { shuttle_build_err "-local_jobs given twice"; return; }
      jobs=$a; shuttle_build_valid_jobs "$a" || { shuttle_build_err "bad -local_jobs value '$a'"; return; }
    elif [ "$knext" = 1 ]; then knext=0
    elif [ "$a" = -k ]; then knext=1
    elif [ "$a" = -local_jobs ]; then next=1
    elif [ "${a#-local_jobs=}" != "$a" ]; then [ -z "$jobs" ] || { shuttle_build_err "-local_jobs given twice"; return; }
      jobs=${a#-local_jobs=}; shuttle_build_valid_jobs "$jobs" || { shuttle_build_err "bad -local_jobs value '$jobs'"; return; }
    else SHUTTLE_BUILD_PLAIN=0
    fi
    SHUTTLE_BUILD_ARGS=(${SHUTTLE_BUILD_ARGS[@]+"${SHUTTLE_BUILD_ARGS[@]}"} "$a")
  done
  [ "$next" = 0 ] || { shuttle_build_err "-local_jobs needs a value"; return; }
  if [ -z "$jobs" ]; then
    jobs=${SHUTTLE_LOCAL_JOBS:-$def}
    shuttle_build_valid_jobs "$jobs" || { shuttle_build_err "bad SHUTTLE_LOCAL_JOBS '$jobs'"; return; }
    SHUTTLE_BUILD_ARGS=(${SHUTTLE_BUILD_ARGS[@]+"${SHUTTLE_BUILD_ARGS[@]}"} -local_jobs "$jobs")
  fi
}

shuttle_build_prefix() {
  SHUTTLE_BUILD_PREFIX=(nice -n "${NICE:-19}")
  if [ "${SHUTTLE_BG:-0}" = 1 ] && [ "$(uname)" = Darwin ]; then
    SHUTTLE_BUILD_PREFIX=("${SHUTTLE_BUILD_PREFIX[@]}" taskpolicy -c background)
  fi
}
