#!/bin/bash
#
# Performance comparison of rpn against GNU dc on compute-heavy expressions.
# Each expression runs BENCH_N times per binary (default 3); the best time is
# reported. Requires dc (skips with exit 0 when dc is not installed).
#
# Usage: make bench          # or: BENCH_N=1 bash tests/bench.sh

cd "$(dirname "$0")/.."

if ! command -v dc >/dev/null 2>&1; then
    echo "dc not installed; skipping benchmark"
    exit 0
fi

RPN=./rpn
N=${BENCH_N:-3}

bench() {
    local label=$1 expr=$2
    local best_r=999999999 best_d=999999999 t
    for _ in $(seq "$N"); do
        t=$(date +%s%N); printf '%s\n' "$expr" | "$RPN" >/dev/null 2>&1
        t=$(( ($(date +%s%N) - t) / 1000000 ))
        [ "$t" -lt "$best_r" ] && best_r=$t
        t=$(date +%s%N); printf '%s\n' "$expr" | dc >/dev/null 2>&1
        t=$(( ($(date +%s%N) - t) / 1000000 ))
        [ "$t" -lt "$best_d" ] && best_d=$t
    done
    printf '%-36s rpn %6d ms   dc %6d ms   %s\n' "$label" "$best_r" "$best_d" \
        "$(awk -v r="$best_r" -v d="$best_d" 'BEGIN{ if (r==0) print "inf"; else printf "%.1fx slower than rpn", d/r }')"
}

echo -e "\nbenchmarks: best of $N runs each, wall time in ms (rpn vs $(dc --version 2>&1 | head -1))"
bench 'pow 2^1000000'            '2 1000000 ^ p'
bench 'pow 2^3000000'            '2 3000000 ^ p'
bench 'parse 100k-digit literal' "$(printf '1%.0s' $(seq 1 100000)) p"
bench 'hex 10k digits in+out'    "16 i $(printf 'F%.0s' $(seq 1 10000)) A i 16 o p"
bench 'div at k=500'             '500 k 1 123456789123456789 / p'
bench 'sqrt at k=500'            '500 k 2 v p'
bench 'modpow, 26-digit exponent' '2 99999999999999999999999999 1000000007 | p'
bench 'modpow, 60-digit exponent' '7 111111111111111111111111111111111111111111111111111111111111 998244353 | p'
bench 'factorial 900 (loop)'     '900 sn 1 sa [la ln * sa ln 1 - sn 0 ln >m] sm lmx la p'
bench 'sum 1..100000 (loop)'     '0 sa 100000 sn [la ln + sa ln 1 - sn 0 ln >m] sm lmx la p'
bench 'countdown 100000 (loop)'  '100000 sc [lc 1 - sc 0 lc >m] sm lmx'

# R rotation: build a 900-deep stack, then rotate the whole stack 100 times
# per loop iteration over 800 iterations (macro nesting caps loops at ~1000)
rot_body=$(printf '900 R %.0s' $(seq 1 100))
bench 'R rotate 900-stack 80k times' "900 sn [ln d 1 - sn 0 ln >m] sm lmx 800 sc [${rot_body} lc 1 - sc 0 lc >m] sm lmx"

bench 'big multiplication'       '99999999999999999999 99999999999999999999 * p'

echo ""
