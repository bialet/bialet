#!/usr/bin/env bash
#
# install-tools.sh - idempotent Debian/Ubuntu installer for the Bialet
# performance harness.
#
# Installs the compiler and the profiling/load tools used by bench/run.sh:
# perf, heaptrack, valgrind, strace, ltrace, hyperfine, oha (or wrk), sysstat,
# cpupower and the FlameGraph scripts. It never changes kernel settings: it
# prints the perf_event_paranoid and cpufreq commands for you to run.
#
# Usage: sudo bench/install-tools.sh [options]
#        bench/install-tools.sh --help
#
# Options:
#   --no-network     skip the optional oha and FlameGraph downloads
#   --no-update      do not run apt-get update
#   --no-flamegraph  skip the FlameGraph checkout
#   --strict         exit 1 if any tool is still missing at the end
#   -h, --help       show this help
#
# Exit status is 0 when the required toolchain is present, even if optional
# tools (oha, flamegraph, cpupower) are missing.

set -euo pipefail

# shellcheck source=lib/common.sh
source "$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)/lib/common.sh"

NO_NETWORK=0
NO_UPDATE=0
NO_FLAMEGRAPH=0
STRICT=0

usage() {
  print_usage_header "${BASH_SOURCE[0]}"
}

while (($#)); do
  case "$1" in
    -h|--help) usage; exit 0 ;;
    --no-network) NO_NETWORK=1; shift ;;
    --no-update) NO_UPDATE=1; shift ;;
    --no-flamegraph) NO_FLAMEGRAPH=1; shift ;;
    --strict) STRICT=1; shift ;;
    *) die "unknown option: $1 (try --help)" ;;
  esac
done

is_linux || die "this installer targets Debian/Ubuntu; the profiling stack is Linux-only"
have apt-get || die "apt-get not found: this installer supports Debian/Ubuntu only"
[[ "$(id -u)" -eq 0 ]] || have sudo || die "run as root, or install sudo"

run_root() {
  if [[ "$(id -u)" -eq 0 ]]; then
    "$@"
  else
    sudo "$@"
  fi
}

pkg_installed() {
  dpkg-query -W -f='${Status}' "$1" 2>/dev/null | grep -q 'install ok installed'
}

pkg_available() {
  apt-cache show "$1" >/dev/null 2>&1
}

apt_install() {
  local pkgs=() pkg
  for pkg in "$@"; do
    [[ -n "$pkg" ]] || continue
    pkg_installed "$pkg" || pkgs+=("$pkg")
  done
  ((${#pkgs[@]} > 0)) || return 0
  info "apt-get install: ${pkgs[*]}"
  run_root env DEBIAN_FRONTEND=noninteractive \
    apt-get install -y --no-install-recommends "${pkgs[@]}"
}

# ---------------------------------------------------------------------------
# Package selection
# ---------------------------------------------------------------------------

CORE_PKGS=(
  build-essential
  pkg-config
  libsqlite3-dev
  libcurl4-openssl-dev
  libssl-dev
  python3
  sysstat
  procps
  curl
  ca-certificates
  git
  perl
)

TOOL_PKGS=(
  heaptrack
  valgrind
  strace
  ltrace
  hyperfine
  gdb
)

PERF_CANDIDATES=(
  "linux-tools-$(uname -r)"
  "linux-tools-generic"
  "linux-perf"
)

UPDATE_NEEDED=0
for pkg in "${CORE_PKGS[@]}"; do
  pkg_installed "$pkg" || UPDATE_NEEDED=1
done
if ((UPDATE_NEEDED == 1)) && ((NO_UPDATE == 0)); then
  info "apt-get update"
  run_root env DEBIAN_FRONTEND=noninteractive apt-get update
fi

apt_install "${CORE_PKGS[@]}"

# perf: the package name differs across Debian/Ubuntu releases and cloud
# kernels. Try the exact running-kernel tools first, then generic fallbacks.
PERF_PKG=""
if have perf; then
  info "perf already present: $(command -v perf)"
else
  for candidate in "${PERF_CANDIDATES[@]}"; do
    if pkg_available "$candidate"; then
      PERF_PKG="$candidate"
      break
    fi
  done
  if [[ -n "$PERF_PKG" ]]; then
    apt_install "$PERF_PKG" || true
  fi
  # Ubuntu's wrapper needs linux-tools-common for /usr/bin/perf. Install it
  # only when the candidate did not provide a working perf.
  if ! have perf && pkg_available linux-tools-common; then
    apt_install linux-tools-common || true
  fi
fi

for tool_pkg in "${TOOL_PKGS[@]}"; do
  apt_install "$tool_pkg" || warn "could not install $tool_pkg"
done

# cpupower lives in linux-cpupower on Debian and in linux-tools-common on
# Ubuntu. Both spellings are attempted; neither is required.
if have cpupower; then
  info "cpupower already present"
else
  for candidate in linux-cpupower linux-tools-common; do
    if pkg_available "$candidate"; then
      apt_install "$candidate" || true
      have cpupower && break
    fi
  done
fi

# wrk, then oha from GitHub releases. oha is the preferred client because its
# --json output is portable; wrk is kept when the distro ships it.
if have wrk; then
  info "wrk already present: $(command -v wrk)"
else
  if pkg_available wrk; then
    apt_install wrk || warn "wrk package failed to install"
  fi
fi

install_oha() {
  have oha && { info "oha already present: $(command -v oha)"; return 0; }
  ((NO_NETWORK == 0)) || { warn "oha missing (--no-network); install it manually"; return 0; }
  local arch url tmp api
  case "$(uname -m)" in
    x86_64|amd64) arch="amd64" ;;
    aarch64|arm64) arch="arm64" ;;
    *) warn "oha: unsupported architecture $(uname -m)"; return 0 ;;
  esac
  info "installing oha from GitHub releases"
  api="$(curl -fsSL https://api.github.com/repos/hatoo/oha/releases/latest 2>/dev/null || true)"
  url=""
  if [[ -n "$api" ]] && have python3; then
    url="$(printf '%s' "$api" | python3 -c '
import json, sys
arch = sys.argv[1]
data = json.load(sys.stdin)
for asset in data.get("assets", []):
    name = asset.get("name", "")
    if name == "oha-linux-" + arch or name.endswith("linux-" + arch):
        print(asset.get("browser_download_url", ""))
        break
' "$arch" 2>/dev/null || true)"
  fi
  if [[ -z "$url" ]]; then
    url="https://github.com/hatoo/oha/releases/latest/download/oha-linux-${arch}"
  fi
  tmp="$(mktemp "${TMPDIR:-/tmp}/oha.XXXXXX")"
  if curl -fL --retry 2 -o "$tmp" "$url"; then
    run_root install -m 0755 "$tmp" /usr/local/bin/oha
    info "oha installed: $(command -v oha)"
  else
    warn "could not download oha ($url); install it from https://github.com/hatoo/oha"
  fi
  rm -f -- "$tmp"
}

if ! have wrk; then
  install_oha
fi

# FlameGraph scripts (perf script | stackcollapse-perf.pl | flamegraph.pl).
if ((NO_FLAMEGRAPH == 0)); then
  FLAMEGRAPH_DIR="${FLAMEGRAPH_DIR:-/opt/FlameGraph}"
  if [[ -x "$FLAMEGRAPH_DIR/flamegraph.pl" && -x "$FLAMEGRAPH_DIR/stackcollapse-perf.pl" ]]; then
    info "FlameGraph already present: $FLAMEGRAPH_DIR"
  elif ((NO_NETWORK == 0)); then
    if have git; then
      info "cloning FlameGraph into $FLAMEGRAPH_DIR"
      run_root git clone --depth 1 https://github.com/brendangregg/FlameGraph.git \
        "$FLAMEGRAPH_DIR" || warn "could not clone FlameGraph"
    else
      warn "git is missing; cannot fetch FlameGraph"
    fi
  fi
  if [[ -x "$FLAMEGRAPH_DIR/flamegraph.pl" && -x "$FLAMEGRAPH_DIR/stackcollapse-perf.pl" ]]; then
    run_root ln -sf "$FLAMEGRAPH_DIR/flamegraph.pl" /usr/local/bin/flamegraph.pl
    run_root ln -sf "$FLAMEGRAPH_DIR/stackcollapse-perf.pl" \
      /usr/local/bin/stackcollapse-perf.pl
  fi
fi

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------

tool_version() {
  case "$1" in
    perf) perf --version 2>/dev/null | head -n1 ;;
    heaptrack) heaptrack --version 2>/dev/null | head -n1 ;;
    valgrind) valgrind --version 2>/dev/null ;;
    strace) strace -V 2>/dev/null | head -n1 ;;
    ltrace) ltrace -V 2>/dev/null | head -n1 ;;
    hyperfine) hyperfine --version 2>/dev/null ;;
    wrk) wrk --version 2>&1 | head -n1 ;;
    oha) oha --version 2>/dev/null | head -n1 ;;
    cpupower) cpupower --version 2>/dev/null | head -n1 ;;
    python3) python3 --version 2>&1 ;;
    cc) cc --version 2>/dev/null | head -n1 ;;
  esac
}

REQUIRED=(cc make pkg-config python3 perf)
OPTIONAL=(heaptrack valgrind strace ltrace hyperfine gdb wrk oha cpupower \
          flamegraph.pl stackcollapse-perf.pl)
MISSING=()

printf '\n== Tool summary ==\n'
printf '%-24s %-8s %s\n' "tool" "status" "version / path"
for tool in "${REQUIRED[@]}" "${OPTIONAL[@]}"; do
  if have "$tool"; then
    printf '%-24s %-8s %s\n' "$tool" "ok" "$(tool_version "$tool")  [$(command -v "$tool")]"
  else
    printf '%-24s %-8s\n' "$tool" "MISSING"
    MISSING+=("$tool")
  fi
done

printf '\n== perf permissions (not changed by this script) ==\n'
if [[ -r /proc/sys/kernel/perf_event_paranoid ]]; then
  printf 'kernel.perf_event_paranoid = %s\n' "$(cat /proc/sys/kernel/perf_event_paranoid)"
else
  printf 'kernel.perf_event_paranoid: unavailable\n'
fi
cat <<'EOF'
To allow perf stat/record as your user (recommended for this harness):
  sudo sysctl -w kernel.perf_event_paranoid=1
  echo 'kernel.perf_event_paranoid=1' | sudo tee /etc/sysctl.d/99-bialet-bench.conf
If DWARF callchains come back empty, try 0 instead of 1. On a shared box,
leaving it at 2/3 is safer; run the harness with sudo in that case.
EOF

printf '\n== CPU frequency governor (not changed by this script) ==\n'
if [[ -r /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor ]]; then
  printf 'current governor: %s\n' "$(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor)"
  printf 'pin it for the duration of a run with:\n  sudo cpupower frequency-set -g performance\n'
  printf 'restore powersave/ondemand afterwards.\n'
else
  printf 'cpufreq governor not exposed (common on VMs); note it in the results metadata.\n'
fi

if ((${#MISSING[@]} > 0)); then
  printf '\nMissing tools: %s\n' "${MISSING[*]}"
  printf 'Baseline load runs still work with oha, wrk or python3; profiling modes need perf/heaptrack/strace.\n'
else
  printf '\nAll tools present.\n'
fi

if ((STRICT == 1)) && ((${#MISSING[@]} > 0)); then
  exit 1
fi
