#!/bin/bash
#
# Adversarial tests for rpn: inputs aimed at known flaw classes rather than
# documented behavior -- memory corruption at stack-capacity boundaries,
# unbounded macro recursion, oversized allocations, parser edge cases and a
# deterministic fuzz smoke test. A crash (exit >= 128, e.g. SIGSEGV/SIGABRT)
# always fails a test. Meant to run on the normal build and under ASan/UBSan
# in CI, where any sanitizer report makes ./rpn exit non-zero or abort.
#
# The binary under test can be overridden with RPN_BIN (default ./rpn).

cd "$(dirname "$0")/.."

RPN_BIN=${RPN_BIN:-./rpn}

failures=0
tests_run=0
errfile=$(mktemp)
trap 'rm -f "$errfile"' EXIT

# expect_ok INPUT -- INPUT is fed as one stdin line; expects exit exactly 0
function expect_ok ()
{
    local input=$1
    tests_run=$((tests_run + 1))
    printf '%s\n' "$input" | "$RPN_BIN" >/dev/null 2>"$errfile"
    local code=$?
    if [ "$code" -ne 0 ]; then
        failures=$((failures + 1))
        echo "FAIL ok '${input:0:80}': expected exit 0, got $code"
        head -3 "$errfile" | sed 's/^/    /'
    fi
}

# expect_err INPUT SUBSTRING -- expects exit exactly 1 and stderr containing SUBSTRING
function expect_err ()
{
    local input=$1 substring=$2
    tests_run=$((tests_run + 1))
    local err code
    err=$(printf '%s\n' "$input" | "$RPN_BIN" 2>&1 >/dev/null)
    code=$?
    if [ "$code" -ne 1 ] || [[ "$err" != *"$substring"* ]]; then
        failures=$((failures + 1))
        echo "FAIL err '${input:0:80}': expected exit 1 and stderr '*$substring*', got exit $code, stderr '${err:0:120}'"
    fi
}

# ===== Capacity-boundary aliasing (regression: use-after-free in d) =====
# Pushing up to the backing-array capacity (8, 16, 32, ...) and then pushing
# again forces a realloc while the new value is being built. Duplicating the
# top at that moment used to read the mpz handle through a freed pointer;
# under ASan this fires at every boundary, and the largest one segfaulted.
for n in 7 8 15 16 31 32 63 64 127 128 255 256 8191 8192; do
    nums=$(seq 1 "$n" | tr '\n' ' ')
    expect_ok "${nums}d f"       # duplicate a number at the boundary
    expect_ok "${nums}[s]d f"    # duplicate a string at the boundary
done

# ===== Macro recursion / C-stack overflow (regression: SIGSEGV in x) =====
# A self-re-executing macro must hit the depth cap and error, never crash.
expect_err "[[dx]x]dx" "macro nesting too deep"

# Deep bracket nesting: tokenizing is iterative, executing is depth-capped,
# so this must exit 1 with the depth error rather than overflow the stack.
deep_open=$(yes '[' | head -n 20000 | tr -d '\n')
deep_close=$(yes ']x' | head -n 20000 | tr -d '\n')
expect_err "${deep_open}1p${deep_close}" "macro nesting too deep"

# ===== Oversized exponent (allocation-bomb guard) =====
expect_err "2 18446744073709551615 ^" "exponent too large in raise"
expect_err "_2 18446744073709551615 ^" "exponent too large in raise"
expect_err "2 134217729 ^" "exponent too large in raise"
expect_err "10 67108865 ^" "exponent too large in raise"
# Exactly at the 32 MiB cap is allowed, then cleared
expect_ok "2 134217728 ^ c"

# ===== Negative-exponent allocation bomb (regression) =====
# The 10^(k+sa*n) numerator must be size-capped like the positive branch;
# these used to make GMP abort on a huge allocation (SIGABRT) from a short
# input, and now error gracefully with exit 1.
expect_err "1000000 k 1 10 1000000 ^ / _268435456 ^" "exponent too large in raise"
expect_err "0.5 _81000000 ^" "exponent too large in raise"

# ===== Non-numeric operands on every numeric command =====
expect_err "[s] 0 /" "non-numeric value"
expect_err "0 [s] /" "non-numeric value"
expect_err "[s] 0 %" "non-numeric value"
expect_err "[s] [t] ~" "non-numeric value"
expect_err "[s] 2 ^" "non-numeric value"
expect_err "[s] v" "non-numeric value"
expect_err "[s] 1 +" "non-numeric value"
expect_err "1 [s] -" "non-numeric value"

# ===== Parser edge cases =====
expect_err "[abc" "unterminated string"
expect_ok "[] f"
expect_err "$(printf 'a%.0s' $(seq 1 100000))" "unimplemented"  # 100 KB token
expect_ok "$(printf '9%.0s' $(seq 1 100000)) 1 + p"               # 100k-digit number
# A literal with more than MAX_SCALE (1,000,000) fractional digits is rejected
expect_err "0.$(printf '%1000001s' '' | tr ' ' '0') p" "scale too large"
expect_ok "0.$(printf '%1000000s' '' | tr ' ' '0') c"             # exactly MAX_SCALE is ok

# ===== P byte-export path =====
expect_ok "0 P"
expect_ok "_72 P"
expect_ok "99999999999999999999999999 P"

# ===== Stack growth stress (realloc loop) =====
expect_ok "$(seq 1 100000 | tr '\n' ' ')c"

# ===== Underflow / no-op edges =====
expect_ok "r"
expect_ok "1 r"
expect_err "d" "stack underflow"
expect_err "x" "stack underflow"
expect_err "P" "stack underflow"

# ===== Deterministic fuzz smoke =====
# Fixed-seed token soup: any crash (exit >= 128) fails; runtime errors (exit
# 1) are fine. The seed is fixed, so this input is identical on every run.
tests_run=$((tests_run + 1))
fuzz=$(awk 'BEGIN {
    srand(20260101)
    n = split("0 1 2 9 _ + - * / % ~ ^ v c d r p n P f x [ ] # z", t, " ")
    for (l = 0; l < 300; l++) {
        line = ""
        nt = 1 + int(rand() * 8)
        for (i = 0; i < nt; i++)
            line = line t[1 + int(rand() * n)] (i < nt - 1 ? " " : "")
        print line
    }
}')
printf '%s\n' "$fuzz" | "$RPN_BIN" >/dev/null 2>"$errfile"
code=$?
if [ "$code" -ge 128 ]; then
    failures=$((failures + 1))
    echo "FAIL fuzz: crashed with exit $code"
    head -5 "$errfile" | sed 's/^/    /'
fi

# ===== SUMMARY =====

if [ "$failures" -eq 0 ]; then
    echo "All $tests_run adversarial tests passed"
    exit 0
else
    echo "$failures of $tests_run adversarial tests failed"
    exit 1
fi
