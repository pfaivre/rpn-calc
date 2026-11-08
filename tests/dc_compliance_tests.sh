#!/bin/bash
#
# dc compliance tests for rpn.
#
# Two kinds of assertions:
#   same      EXPR [EXPECTED]  -- rpn and GNU dc must agree: identical stdout
#                                (dc's 70-column backslash-wrapping is normalized
#                                away) and both must error or both not error.
#                                When EXPECTED is given, rpn's stdout is also
#                                pinned to it.
#   diverges  EXPR RPN_OUT RPN_ERR_SUBSTR RPN_CODE
#                             -- pins a deliberate rpn-vs-dc divergence: rpn must
#                                produce exactly RPN_OUT, an stderr containing
#                                RPN_ERR_SUBSTR, and exit RPN_CODE, AND dc must
#                                actually differ (otherwise it is not a divergence).
#                                The dc behavior is documented in the comment above
#                                each call.
#
# Skips with exit 0 when dc is not installed, so `make test` stays green on
# systems without dc.

cd "$(dirname "$0")/.."

if ! command -v dc >/dev/null 2>&1; then
    echo "dc not installed; skipping dc compliance tests"
    exit 0
fi

RPN=./rpn
failures=0
tests_run=0
tmpdir=$(mktemp -d)
trap 'rm -rf "$tmpdir"' EXIT

echo "dc compliance against: $(dc --version 2>&1 | head -1)"

# Run rpn on one stdin line; sets R_OUT / R_ERR / R_CODE
function run_rpn ()
{
    printf '%s\n' "$1" | "$RPN" >"$tmpdir/r.out" 2>"$tmpdir/r.err"
    R_CODE=$?
    R_OUT=$(cat "$tmpdir/r.out")
    R_ERR=$(cat "$tmpdir/r.err")
}

# Run dc on one stdin line; sets D_OUT (line-wrap normalized) / D_ERR / D_CODE
function run_dc ()
{
    printf '%s\n' "$1" | dc >"$tmpdir/d.out" 2>"$tmpdir/d.err"
    D_CODE=$?
    D_OUT=$(sed -z 's/\\\n//g' "$tmpdir/d.out")  # undo dc's backslash-newline wrapping
    D_ERR=$(cat "$tmpdir/d.err")
}

# same EXPR [EXPECTED] -- rpn and dc agree on stdout and on errored-or-not
function same ()
{
    local input=$1 want=${2:-}
    tests_run=$((tests_run + 1))
    run_rpn "$input"
    run_dc "$input"
    local rerr=0 derr=0
    [ -n "$R_ERR" ] && rerr=1
    [ -n "$D_ERR" ] && derr=1
    if [ "$R_OUT" != "$D_OUT" ] || [ "$rerr" -ne "$derr" ]; then
        failures=$((failures + 1))
        echo "FAIL same '$input': rpn out='$R_OUT' err='$R_ERR' vs dc out='$D_OUT' err='$D_ERR'"
        return
    fi
    if [ -n "$want" ] && [ "$R_OUT" != "$want" ]; then
        failures=$((failures + 1))
        echo "FAIL same '$input': expected '$want', got '$R_OUT'"
    fi
}

# diverges EXPR RPN_OUT RPN_ERR_SUBSTR RPN_CODE -- pin a deliberate divergence
function diverges ()
{
    local input=$1 want_out=$2 want_err=$3 want_code=$4
    tests_run=$((tests_run + 1))
    run_rpn "$input"
    if [ "$R_OUT" != "$want_out" ] || [[ "$R_ERR" != *"$want_err"* ]] || [ "$R_CODE" != "$want_code" ]; then
        failures=$((failures + 1))
        echo "FAIL diverge '$input': rpn out='$R_OUT' err='$R_ERR' code=$R_CODE (want out='$want_out' err='*$want_err*' code=$want_code)"
        return
    fi
    run_dc "$input"
    local rerr=0 derr=0
    [ -n "$R_ERR" ] && rerr=1
    [ -n "$D_ERR" ] && derr=1
    # dc must differ in stdout, errored-state, or exit code; else not a divergence
    if [ "$D_OUT" = "$R_OUT" ] && [ "$rerr" -eq "$derr" ] && [ "$D_CODE" = "$R_CODE" ]; then
        failures=$((failures + 1))
        echo "FAIL diverge '$input': dc is identical (out='$D_OUT' err='$D_ERR' code=$D_CODE); not a divergence"
    fi
}

# ===== IDENTICAL TO dc: arithmetic, printing, stack, registers, macros =====
same "2 3 + p"            "5"
same "2 3 - p"            "-1"
same "_5 3 + p"           "-2"
same "2 3 * p"            "6"
same "10 3 / p"           "3"
same "10 3 % p"           "1"
same "10 3 ~ f"           $'1\n3'
same "2 10 ^ p"           "1024"
same "0 0 ^ p"            "1"
same "2 0 ^ p"            "1"
same "2 v p"              "1"
same "2 _3 ^ p"           "0"        # negative exponent at scale 0 -> 0
same "1 _1 ^ p"           "1"        # 1^-1 -> 1
same "5 0 / f"            $'0\n5'    # divide by zero: operands kept, both error
same "5 0 % f"            $'0\n5'
same "5 0 ~ f"            $'0\n5'
same "1 2 3 f"            $'3\n2\n1'
same "1 2 r f"            $'1\n2'
same "1 d f"              $'1\n1'
same "[hello] p"          "hello"
same "[[a]b] p"           "[a]b"
same "5 sa la p"          "5"
same "5 sA2 lA p"         "5"        # the name is one character: A, then 2 is pushed
same "5 s12 l1 p"         "5"        # digit name followed by a digit: name 1, push 2
same "[9 p] sA 1 2 >A5 p" $'9\n5'    # conditional name followed by a digit
same "42 3 :A7 3 ;A p"    "42"       # array command name followed by a digit
same "5 Sa 6 Sa la la f"  $'6\n6'    # per-register value stack (S pushes, l peeks)
same "7 La"                              # L on empty register: both error, empty stdout
same "5 1 :a 1 ;a p"      "5"
same "9 2 :a 2 ;a p"      "9"
same "5 1 :a 3 ;a p"      "0"        # missing array cell loads 0
same "[1 p q 2 p] x 3 p"  "1"        # q unwinds one macro level
same "q"                              # q at top level exits
same "1 Q"                              # over-depth Q: both error, empty stdout
same "2 3 + p # comment"  "5"
same "65 P"               "A"        # P prints integer part as bytes, MSB first

# ===== IDENTICAL TO dc: decimal numbers and scale =====
same "1.5 p"              "1.5"
same ".5 p"               ".5"
same "1. p"               "1"
same "_0.5 p"             "-.5"
same "007.50 p"           "7.50"
same "0.0 p"              "0"
same "1..2 f"             $'.2\n1'  # second '.' starts a new token
same "2_3 f"              $'-3\n2'
same "2 k 1 3 / p"        ".33"
same "5 k 10 3 / p"       "3.33333"
same "2 k 5 3 % p"        ".02"
same "2 k 5 3 ~ f"        $'.02\n1.66'
same "3 k 2 v p"          "1.414"
same "3 k 2.25 v p"       "1.500"
same "2 k 1.5 2 ^ p"      "2.25"
same "2 k 2.5 2 ^ p"      "6.25"
same "2 k 2 _3 ^ p"       ".12"
same "5 k 2 _2 ^ p"       ".25000"
same "3 k K p"            "3"
same "1000000 k K p"      "1000000"
same "2 300 ^ p"                        # long output: exercises dc line-wrap normalization
same "1.5 2.25 + p"       "3.75"
same "1.5 2.5 * p"        "3.7"      # * capped at k=0

# ===== IDENTICAL TO dc: comparison conditionals (scale ignored) =====
same "[8 p] sa 1 2 >a"    "8"        # 2 > 1 fires
same "[8 p] sa 2 1 >a"    ""         # 1 > 2 does not fire
same "[9 p] sa 1.5 1.50 =a" "9"      # comparisons ignore scale
same "[9 p] sa 1.6 1.5 <a"  "9"      # 1.5 < 1.6 fires

# ===== IDENTICAL TO dc: input/output bases (i I o O) =====
# Numbers are stored base 10 internally; i converts digits at parse, o at print.
same "16 i FF p"          "255"
same "16 i 10 p"          "16"
same "16 o 255 p"         "FF"
same "16 o _255 p"        "-FF"
same "2 o 255 p"          "11111111"
same "16 i F.F p"         "15.9"     # hex fraction truncated to 1 decimal digit
same "16 i F.FF p"        "15.99"
same "16 i 0.8 p"         ".5"
same "2 i 1.1 p"          "1.5"
same "2 i 10 p"           "2"
same "16 i _FF p"         "-255"
same "8 i 10 10 + p"      "16"
same "16 i A B + p"       "21"
same "I p O p"            $'10\n10'
same "16 i I p"           "16"
same "16 o O p"           "10"       # 16 printed in base 16
same "16 o 2 k 1 3 / p"   ".54"      # 0.33 decimal -> hex fraction
same "16 o 3 k 1 3 / p"   ".553"
same "16 o 0.5 p"         ".8"
same "2 o 3 k 1 2 / p"    ".1000000000"  # ceil(log2(1000)) = 10 binary fraction digits
same "8 o 2 k 1 3 / p"    ".250"
same "16 o 15.9375 p"     "F.F000"
same "16 o 2 k 255 v p"   "F.F5"
same "16 o 2 k 255.5 p"   "FF.8"
same "16 o 2 k 0.1 0.1 * p"  ".02"
same "16 i _F.F p"        "-15.9"
same "15 o 1 k 1 2 / p"   ".7"       # fraction digits: exact ceil(log_base(10^scale))
same "3 o 7 k 1 2 / p"    ".111111111111111"
same "11 o 2 k 1 2 / p"   ".55"
same "7 o 5 k 1 3 / p"    ".222221"
same "10 o 16 o 255 p 10 o 255 p"  $'FF\n255'
same "16 i FF 10 o p"     "FF"
same "16 i FF sa la p"    "255"
same "16 i [FF p] x"      "255"
same "2 i 1010 2 o p"     "1010"
same "[8 p] sa 16 i B A <a"  "8"     # comparisons work in another input base
same "16 i FF 2 ^ p"      "65025"
same "16 o 100 2 ^ p"     "2710"
same "16 i FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF p"  "340282366920938463463374607431768211455"  # long literal
same "2 i 1111111111111111111111111111111111111111111111111111111111111111 p"  "18446744073709551615"
same "16 o 65 P"          "A"        # P prints bytes of the integer part, any base
same "16 o 255 n"         "FF"       # n prints in the output base, no newline
same "16 o 255 5 f"       $'5\nFF'   # f prints the stack in the output base
same "16 o 0.00 p"        "0"        # zero prints as 0 whatever its scale
same "15 o 1 k 1 2 / p"   ".7"       # exact ceil fraction digits, non-power-of-2 base
same "3 o 7 k 1 2 / p"    ".111111111111111"
same "2.5 o 5 p"          "101"      # o truncates its operand
same "2.5 i 5 p"          "5"
same "1 i 5 p"                       # input base out of range: both error, stdout "5"
same "17 i 5 p"
same "1 o 5 p"                       # output base < 2: both error
same "0 o 5 p"
same "[a] i 5 p"
same "[a] o 5 p"
same "16 i G p"                      # 'G' is not a digit char: both error

# ===== IDENTICAL TO dc: stack introspection (z Z X) =====
same "1 2 3 z f"          $'3\n3\n2\n1'
same "z p"                "0"
same "123 Z p"            "3"
same "0 Z p"              "1"
same "_12 Z p"            "2"
same "1.50 Z p"           "3"
same ".5 Z p"             "1"
same "[hello] Z p"        "5"
same "[] Z p"             "0"
same "2 10 ^ Z p"         "4"
same "8 Z p"              "1"       # mpz_sizeinbase(8,10) is one too big
same "64 Z p"             "2"
same "512 Z p"            "3"
same "99999999999999999999 Z p"  "20"
same "_1.50 Z p"          "3"
same "16 o 8 Z p"         "1"
same "5 X p"              "0"
same "1.50 X p"           "2"
same "1.50 X f"           "2"        # X pops
same "[hi] X p"           "0"
same "2 k 1 3 / X p"      "2"
same "Z"                              # both error on empty stack
same "X"

# ===== IDENTICAL TO dc: ? at EOF (single-line stdin; dc's piped ? then sees
# EOF because it slurps stdin upfront, and exits silently — same as rpn) =====
same "?"                 ""    # bare ? at EOF: both exit 0, empty stdout
same "1 p ? 2 p"         "1"   # ? at EOF exits silently, rest of the line dropped

# ===== IDENTICAL TO dc: script files (positional arguments) =====
# stdin is redirected to /dev/null: dc reads stdin after files, so without
# the redirect both would block on the suite's stdin.
printf '1 p\n2 2 + p\n' >"$tmpdir/s1"
printf '5 p\n' >"$tmpdir/s2"
printf '1 0 /\n7 p\n' >"$tmpdir/sbad"
printf '8 p' >"$tmpdir/snonl"   # no trailing newline

# same_file ARGS... -- rpn and dc run on the same script files, stdin closed
function same_file ()
{
    tests_run=$((tests_run + 1))
    local r_out d_out r_err=0 d_err=0
    r_out=$(./rpn "$@" </dev/null 2>"$tmpdir/r.err"); [ -s "$tmpdir/r.err" ] && r_err=1
    d_out=$(dc "$@" </dev/null 2>"$tmpdir/d.err"); [ -s "$tmpdir/d.err" ] && d_err=1
    if [ "$r_out" != "$d_out" ] || [ "$r_err" -ne "$d_err" ]; then
        failures=$((failures + 1))
        echo "FAIL same_file '$*': rpn out='$r_out' err='$r_err' vs dc out='$d_out' err='$d_err'"
    fi
}

same_file "$tmpdir/s1" "$tmpdir/s2"          # files in order
same_file -e '9 p' "$tmpdir/s1"              # -e runs before files
same_file "$tmpdir/sbad" "$tmpdir/s2"        # error in a file: both error, session continues
same_file "$tmpdir/snonl"                    # no trailing newline
same_file "$tmpdir/nope" "$tmpdir/s1"        # missing file: both error, later files still run

# ===== DELIBERATE DIVERGENCES from dc (each pinned, with dc's behavior noted) =====

# rpn exits 1 when any command failed; dc always exits 0. Session continues in both.
# dc: "1 0 /" -> stderr "dc: divide by zero", exit 0, empty stdout.
diverges "1 0 /"  ""  "divide by zero"  1

# Error wording: rpn says "stack underflow", dc says "stack empty".
# dc: "p" -> stderr "dc: stack empty", exit 0.
diverges "p"  ""  "stack underflow"  1

# Q uses the integer part: rpn accepts _0.5 as count 0 (no-op); dc 1.5.2 rejects it
# ("Q command requires a number >= 0"). Both print 3, but only rpn is error-free.
# dc: "1 k _0.5 Q 3 p" -> stdout "3", stderr nonempty, exit 0.
diverges "1 k _0.5 Q 3 p"  "3"  ""  0

# Array index uses the integer part of a nonnegative value: rpn truncates 0.5 to
# cell 0 and loads 5; dc rejects any fractional index ("array index must be a
# nonnegative integer") and loads 0.
# dc: "1 k 5 0.5 :a 0 ;a p" -> stdout "0", stderr nonempty, exit 0.
diverges "1 k 5 0.5 :a 0 ;a p"  "5"  ""  0

# Scale factor is capped at MAX_SCALE (1,000,000): rpn errors "scale too large";
# dc accepts any scale.
# dc: "1000001 k" -> no error, exit 0, empty stdout.
diverges "1000001 k"  ""  "scale too large"  1

# A conditional without a following register name is an error in rpn; dc silently
# ignores it.
# dc: "1 2 =" -> no error, exit 0, empty stdout.
diverges "1 2 ="  ""  "expected a single-character register name"  1

# The output base is capped at 16 in rpn; dc 1.5.2 accepts larger bases and prints
# their digits space-separated as two-digit decimals.
# dc: "17 o 5 p" -> stdout " 05", no error, exit 0.
diverges "17 o 5 p"  "5"  "output base must be a number between 2 and 16 (inclusive)"  1

# In a base of 10 or less, a multi-character number run containing hex letters
# keeps the letters' face values in rpn (FF = 165); dc 1.5.2 clamps them to 9 (FF = 99).
# dc: "FF p" -> stdout "99", no error, exit 0.
diverges "FF p"  "165"  ""  0

# ===== SUMMARY =====

if [ "$failures" -eq 0 ]; then
    echo "All $tests_run dc compliance tests passed"
    exit 0
else
    echo "$failures of $tests_run dc compliance tests failed"
    exit 1
fi
