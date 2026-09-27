#!/usr/bin/env bash
#
# Lodestone benchmark. Use it after `make release`:
#     sudo bash bench/bench.sh
#
# Creates a veth pair (lsb0 -> lsb1) and captures on lsb1 with lodestone while
# tcpreplay sends on lsb0, and prints one result line per test.
# Everything it creates is removed at the end, even on Ctrl+C.

set -u
HERE=$(cd "$(dirname "$0")" && pwd)
BIN=./build/release/lodestone-capture
SENDERS=${SENDERS:-3} # parallel tcpreplay instances for the stress test

[[ $EUID -eq 0 ]] || { echo "run it with sudo: sudo bash bench/bench.sh"; exit 1; }
[[ -x $BIN ]] || { echo "$BIN not found: run 'make release' first"; exit 1; }
command -v tcpreplay >/dev/null || { echo "tcpreplay not installed"; exit 1; }

T=$(mktemp -d)
cleanup() {
    pid=$(pidof lodestone-capture) && kill -INT $pid 2>/dev/null
    ip link del lsb0 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT

# ---------------------------------------------------------------- setup
ip link del lsb0 2>/dev/null
ip link add lsb0 type veth peer name lsb1
sysctl -qw net.ipv6.conf.lsb0.disable_ipv6=1 net.ipv6.conf.lsb1.disable_ipv6=1 2>/dev/null
ip link set lsb0 up
ip link set lsb1 up
python3 "$HERE/gen_pcap.py" --size 64   --count 10000 -o "$T/u64.pcap"
python3 "$HERE/gen_pcap.py" --size 1518 --count 10000 -o "$T/u1518.pcap"
sleep 1

BASE=0     # CPU seconds lodestone spends on startup/shutdown alone

# run NAME SENDERS [tcpreplay args...]   (SENDERS=0: idle run, no traffic)
run() {
    local name=$1 n=$2; shift 2

    # `time` must be the direct parent of lodestone to see its CPU usage
    bash -c "TIMEFORMAT='CPU %U %S'; time $BIN -i lsb1 -o /dev/null" > "$T/sniff.log" 2>&1 &
    local spid=$!
    sleep 1

    local t0 t1 pids=()
    t0=$(date +%s.%N)
    if (( n == 0 )); then
        sleep 3
    else
        for ((i = 0; i < n; i++)); do
            tcpreplay -i lsb0 -K "$@" > "$T/gen$i.log" 2>&1 &
            pids+=($!)
        done
        wait "${pids[@]}"
    fi
    t1=$(date +%s.%N)
    sleep 0.5 # let the last block reach userspace
    kill -INT "$(pidof lodestone-capture)"
    wait $spid

    local sent seen cap drop user sys
    sent=$(cat "$T"/gen*.log 2>/dev/null | awk '/Successful packets:/ { s += $3 } END { print s + 0 }')
    rm -f "$T"/gen*.log
    seen=$(awk '/seen by kernel/ { print $1 }' "$T/sniff.log")
    cap=$(awk '/packets captured/ { print $1 }' "$T/sniff.log")
    drop=$(awk '/dropped by kernel/ { print $1 }' "$T/sniff.log")
    read -r user sys < <(awk '/^CPU/ { print $2, $3 }' "$T/sniff.log")

    if (( n == 0 )); then
        BASE=$(awk "BEGIN { print $user + $sys }")
        printf '%-26s startup CPU %.3f s (subtracted from the runs below)\n' "$name" "$BASE"
        return
    fi
    awk -v name="$name" -v sent="$sent" -v seen="$seen" -v cap="$cap" -v drop="$drop" \
        -v cpu="$(awk "BEGIN { print $user + $sys - $BASE }")" -v secs="$(awk "BEGIN { print $t1 - $t0 }")" \
        'BEGIN {
            loss = sent ? 100 * (sent - cap) / sent : 0; if (loss < 0) loss = 0
            printf "%-26s sent %10d  rate %9.0f pps  seen %10d  captured %10d  dropped %8d  loss %.4f%%  CPU %5.1f%%  %6.1f ns/pkt  %s\n",
                   name, sent, sent / secs, seen, cap, drop, loss, 100 * cpu / secs,
                   cap ? cpu * 1e9 / cap : 0, (seen == cap + drop) ? "" : "(seen != captured+dropped!)"
        }'
}

# tests
echo "CPU:      $(grep -m1 'model name' /proc/cpuinfo | cut -d: -f2- | sed 's/^ //'), $(nproc) threads"
echo "Kernel:   $(uname -r)"
echo "Governor: $(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null || echo n/a)"
echo

run "idle (baseline)"            0
run "A  1518B @ 1 Gbps"          1 --pps=81274 --loop=244 "$T/u1518.pcap"
run "B  64B max, 1 sender"       1 --topspeed --loop=500 "$T/u64.pcap"
run "B  64B max, $SENDERS senders"  "$SENDERS" --topspeed --loop=500 "$T/u64.pcap"

echo
echo "Done."