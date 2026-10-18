#!/usr/bin/env bash
# Pins the T480 into a reproducible measurement state and restores it afterwards.
#   sudo bench/setup_env.sh apply    governor=performance, turbo off, perf counters open
#   sudo bench/setup_env.sh revert   restore the state saved by the last apply
#        bench/setup_env.sh status   print the current state (no root needed)
#        bench/setup_env.sh check    exit 1 unless the state is ready to measure
set -euo pipefail

CPU_SYS=/sys/devices/system/cpu
PSTATE=$CPU_SYS/intel_pstate
PARANOID=/proc/sys/kernel/perf_event_paranoid
SAVED=${SAVED:-/var/tmp/stencil-bench-env.saved}

ac_online() {
    local supply
    for supply in /sys/class/power_supply/*; do
        [[ $(cat "$supply/type" 2>/dev/null) == Mains ]] && [[ $(cat "$supply/online") == 1 ]] && return 0
    done
    return 1
}

pkg_temp_c() {
    local zone
    for zone in /sys/class/thermal/thermal_zone*; do
        if [[ $(cat "$zone/type") == x86_pkg_temp ]]; then
            echo $(( $(cat "$zone/temp") / 1000 ))
            return
        fi
    done
    echo NA
}

governors() { cat $CPU_SYS/cpu*/cpufreq/scaling_governor | sort | uniq -c | awk '{printf "%s(x%s) ", $2, $1}'; }

status() {
    echo "AC power        : $(ac_online && echo online || echo OFFLINE)"
    echo "governor        : $(governors)"
    echo "no_turbo        : $(cat $PSTATE/no_turbo 2>/dev/null || echo NA)"
    echo "max freq (kHz)  : $(cat $CPU_SYS/cpu0/cpufreq/scaling_max_freq)"
    echo "SMT active      : $(cat $CPU_SYS/smt/active)"
    echo "perf paranoid   : $(cat $PARANOID)"
    echo "package temp    : $(pkg_temp_c) C"
    echo "throttle events : $(cat $CPU_SYS/cpu0/thermal_throttle/package_throttle_count)"
}

check() {
    local ok=0
    ac_online || { echo "not ready: running on battery" >&2; ok=1; }
    [[ $(sort -u $CPU_SYS/cpu*/cpufreq/scaling_governor) == performance ]] ||
        { echo "not ready: governor is not performance on every CPU" >&2; ok=1; }
    [[ $(cat $PSTATE/no_turbo 2>/dev/null) == 1 ]] || { echo "not ready: turbo is on" >&2; ok=1; }
    return $ok
}

require_root() { [[ $EUID == 0 ]] || { echo "run with sudo" >&2; exit 1; }; }

apply() {
    require_root
    ac_online || { echo "plug the charger in first: on battery the EC caps the package power" >&2; exit 1; }
    [[ -f $SAVED ]] || {
        echo "governor=$(cat $CPU_SYS/cpu0/cpufreq/scaling_governor)"
        echo "no_turbo=$(cat $PSTATE/no_turbo)"
        echo "paranoid=$(cat $PARANOID)"
    } > "$SAVED"
    for gov in $CPU_SYS/cpu*/cpufreq/scaling_governor; do echo performance > "$gov"; done
    echo 1 > $PSTATE/no_turbo
    echo 0 > $PARANOID
    status
}

revert() {
    require_root
    [[ -f $SAVED ]] || { echo "nothing saved at $SAVED" >&2; exit 1; }
    # shellcheck disable=SC1090
    source "$SAVED"
    for gov in $CPU_SYS/cpu*/cpufreq/scaling_governor; do echo "$governor" > "$gov"; done
    echo "$no_turbo" > $PSTATE/no_turbo
    echo "$paranoid" > $PARANOID
    rm -f "$SAVED"
    status
}

case ${1:-status} in
    apply) apply ;;
    revert) revert ;;
    status) status ;;
    check) check ;;
    *) echo "usage: $0 apply|revert|status|check" >&2; exit 2 ;;
esac
