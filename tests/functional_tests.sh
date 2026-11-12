#!/bin/bash
#
# Functional test suite for rpn.
# Each helper records a failure instead of aborting, so a single run
# reports every failing test. Exits non-zero if any test failed.

cd "$(dirname "$0")/.."

failures=0
tests_run=0

# test EXPRESSION EXPECTED_STDOUT -- feeds EXPRESSION as one stdin line
function test ()
{
    local expression=$1
    local expected_result=$2
    local result

    tests_run=$((tests_run + 1))
    result=$(echo "$expression" | ./rpn)

    if [ "$result" != "$expected_result" ]; then
        failures=$((failures + 1))
        echo "FAIL '$expression': expected '$expected_result', got '$result'"
    fi
}

# test_error EXPRESSION EXPECTED_STDERR_SUBSTRING
function test_error ()
{
    local expression=$1
    local expected_error=$2
    local error_result

    tests_run=$((tests_run + 1))
    error_result=$(echo "$expression" | ./rpn 2>&1 >/dev/null)

    if [[ "$error_result" != *"$expected_error"* ]]; then
        failures=$((failures + 1))
        echo "FAIL '$expression': expected stderr to contain '$expected_error', got '$error_result'"
    fi
}

# test_error_stack EXPRESSION EXPECTED_STDOUT STDERR_SUBSTRING -- error case whose operands stay on the stack
function test_error_stack ()
{
    local expression=$1
    local expected_result=$2
    local expected_error=$3
    local result error_result

    tests_run=$((tests_run + 1))
    error_result=$(echo "$expression" | ./rpn 2>&1 >/dev/null)
    result=$(echo "$expression" | ./rpn 2>/dev/null)

    if [[ "$error_result" != *"$expected_error"* ]] || [ "$result" != "$expected_result" ]; then
        failures=$((failures + 1))
        echo "FAIL '$expression': expected stderr '$expected_error' and stdout '$expected_result', got stderr '$error_result', stdout '$result'"
    fi
}

# test_stack EXPRESSION EXPECTED_TOP [EXPECTED_NEXT ...] -- 'f' prints the stack top first
function test_stack ()
{
    local expression=$1
    shift
    local expected_values=("$@")
    local result expected_output

    tests_run=$((tests_run + 1))
    result=$(echo "$expression" | ./rpn)
    expected_output=$(IFS=$'\n'; echo "${expected_values[*]}")

    if [ "$result" != "$expected_output" ]; then
        failures=$((failures + 1))
        echo "FAIL '$expression': expected stack '$expected_output', got '$result'"
    fi
}

# test_lines EXPECTED_STDOUT INPUT -- INPUT is fed to stdin, printf '%b' escapes
function test_lines ()
{
    local expected_result=$1
    local input=$2
    local result

    tests_run=$((tests_run + 1))
    result=$(printf '%b' "$input" | ./rpn 2>/dev/null)

    if [ "$result" != "$expected_result" ]; then
        failures=$((failures + 1))
        echo "FAIL lines '$input': expected '$expected_result', got '$result'"
    fi
}

# test_e EXPECTED_STDOUT ARGS... -- runs ./rpn ARGS... and checks stdout
function test_e ()
{
    local expected_result=$1
    shift
    local result

    tests_run=$((tests_run + 1))
    result=$(./rpn "$@")

    if [ "$result" != "$expected_result" ]; then
        failures=$((failures + 1))
        echo "FAIL './rpn $*': expected '$expected_result', got '$result'"
    fi
}

# test_e_error EXPECTED_STDERR_SUBSTRING ARGS... -- ./rpn ARGS... writes the substring to stderr
function test_e_error ()
{
    local expected_error=$1
    shift
    local error_result

    tests_run=$((tests_run + 1))
    error_result=$(./rpn "$@" 2>&1 >/dev/null)

    if [[ "$error_result" != *"$expected_error"* ]]; then
        failures=$((failures + 1))
        echo "FAIL './rpn $*': expected stderr to contain '$expected_error', got '$error_result'"
    fi
}

# test_contains SUBSTRING ARGS... -- stdout of ./rpn ARGS... contains SUBSTRING
function test_contains ()
{
    local expected=$1
    shift
    local output

    tests_run=$((tests_run + 1))
    output=$(./rpn "$@" 2>/dev/null)

    if [[ "$output" != *"$expected"* ]]; then
        failures=$((failures + 1))
        echo "FAIL './rpn $*': expected output to contain '$expected', got '$output'"
    fi
}

# test_exit EXPECTED_CODE ARGS... -- ./rpn ARGS... exits with EXPECTED_CODE
function test_exit ()
{
    local expected_code=$1
    shift
    local code

    tests_run=$((tests_run + 1))
    ./rpn "$@" >/dev/null 2>&1
    code=$?

    if [ "$code" != "$expected_code" ]; then
        failures=$((failures + 1))
        echo "FAIL './rpn $*': expected exit $expected_code, got $code"
    fi
}

# test_exit_stdin EXPECTED_CODE EXPECTED_STDOUT INPUT -- stdin run checks both exit code and stdout
function test_exit_stdin ()
{
    local expected_code=$1
    local expected_result=$2
    local input=$3
    local result code

    tests_run=$((tests_run + 1))
    result=$(printf '%b' "$input" | ./rpn 2>/dev/null)
    code=$?

    if [ "$code" != "$expected_code" ] || [ "$result" != "$expected_result" ]; then
        failures=$((failures + 1))
        echo "FAIL lines '$input': expected exit $expected_code and stdout '$expected_result', got exit $code, stdout '$result'"
    fi
}

# test_bytes INPUT HEX -- feeds INPUT (printf '%b' escapes) and compares raw stdout bytes as hex, for output containing NUL bytes ($() would drop them)
function test_bytes ()
{
    local input=$1
    local expected_hex=$2
    local result

    tests_run=$((tests_run + 1))
    result=$(printf '%b' "$input" | ./rpn 2>/dev/null | od -An -tx1 | tr -d ' \n')

    if [ "$result" != "$expected_hex" ]; then
        failures=$((failures + 1))
        echo "FAIL bytes '$input': expected hex '$expected_hex', got '$result'"
    fi
}


# ===== ARITHMETIC =====

# 1+1=2
test "1 1 + f"  "2"

# 1+(-2)=-1
test "1 _2 + f"  "-1"

# 5-1=4
test "5 1 - f"  "4"

# 8*4=32
test "8 4 * f"  "32"

# 29/3=9 (integer division)
test "29 3 / f"  "9"

# 29%3=2
test "29 3 % f"  "2"

# Sum 1 to 10 = 55
test "1 2 3 4 5 6 7 8 9 10 + + + + + + + + + f"  "55"

# ===== DIVISION SEMANTICS =====
# GMP tdiv: quotient rounds toward zero, remainder takes the dividend's sign

test "0 5 / p"  "0"
test "1 1 / p"  "1"
test "_7 2 / p"  "-3"
test "7 _2 / p"  "-3"
test "_7 _2 / p"  "3"
test "_7 2 % p"  "-1"
test "7 _2 % p"  "1"

# ===== LARGE NUMBERS (arbitrary precision) =====

# 9 223 372 036 854 775 807 * 1000 = 9 223 372 036 854 775 807 000
test "9223372036854775807 1000 * f"  "9223372036854775807000"

# (2^63-1)^2
test "9223372036854775807 9223372036854775807 * p"  "85070591730234615847396907784232501249"

# ===== ~ OPERATOR (DIVISION WITH REMAINDER) =====
# dc-style: pushes quotient first, then remainder, so the remainder ends on top

# 5/2 = 2 remainder 1
test "5 2 ~ p"  "1"
test "5 2 ~ r p"  "2"

# 17/3 = 5 remainder 2
test "17 3 ~ p"  "2"
test "17 3 ~ r p"  "5"

# -10/3 = -3 remainder -1
test "_10 3 ~ p"  "-1"
test "_10 3 ~ r p"  "-3"

# Sign combinations: quotient then remainder, top first
test_stack "5 2 ~ f"  "1" "2"
test_stack "0 5 ~ f"  "0" "0"
test_stack "_7 _2 ~ f"  "-1" "3"
test_stack "10 _3 ~ f"  "1" "-3"

test "1 1 ~ p"  "0"

# ~ inside larger expressions
test "10 3 ~ + p"  "4"       # q=3 r=1; 3+1=4
test "20 3 ~ 2 * + p"  "10"  # q=6 r=2; 2*2=4; 6+4=10
test "10 3 ~ 2 * + p"  "5"   # q=3 r=1; 1*2=2; 3+2=5
test "20 3 ~ 4 * 5 + p"  "13"  # q=6 r=2; 2*4=8; 8+5=13
test "1000000000000 7 ~ + p"  "142857142858"

# ===== EXPONENTIATION AND SQUARE ROOT =====

# 12^3 = 1728
test "12 3 ^ f"  "1728"
test "2 10 ^ p"  "1024"
test "2 100 ^ p"  "1267650600228229401496703205376"
test "2 0 ^ p"  "1"
test "0 5 ^ p"  "0"
test "0 0 ^ p"  "1"

# Negative exponents follow dc at scale 0: the integer part of base^exp,
# which is 0 unless the base is 1 or _1
test "2 _1 ^ p"  "0"
test "_2 _3 ^ p"  "0"
test "0 _1 ^ p"  "0"
test "99999999999999999999 _2 ^ p"  "0"
test "1 _5 ^ p"  "1"
test "_1 _2 ^ p"  "1"
test "_1 _3 ^ p"  "-1"
test "_1 _99999999999999999999999 ^ p"  "-1"

# Positive exponents whose result would exceed 32 MiB (MAX_POW_BITS) are
# rejected gracefully with the operands kept on the stack, instead of
# letting GMP abort on a huge allocation failure. The estimate is
# exp * bits(base): base 2 has 2 bits, so the cap allows exp up to 2^27.
test_error_stack "2 18446744073709551615 ^ f"  $'18446744073709551615\n2'  "exponent too large in raise"
test_error_stack "_2 18446744073709551615 ^ f"  $'18446744073709551615\n-2'  "exponent too large in raise"
test_error_stack "2 134217729 ^ f"  $'134217729\n2'  "exponent too large in raise"  # 2*exp > 2^28 bits
test_error_stack "10 67108865 ^ f"  $'67108865\n10'  "exponent too large in raise"  # 4*exp > 2^28 bits
test_exit 1 -e "2 18446744073709551615 ^"
# The cap itself is allowed: 2 * 2^27 = 2^28 bits = exactly 32 MiB
test "2 134217728 ^ c f"  ""
# Bases 0 and +-1 stay instant for any exponent
test "0 18446744073709551615 ^ p"  "0"
test "1 18446744073709551615 ^ p"  "1"
test "_1 18446744073709551615 ^ p"  "-1"

# Integer square root truncates: sqrt(456789) = 675
test "456789 v f"  "675"
test "2 v p"  "1"
test "0 v p"  "0"

# sqrt((12 + (-3)^4) / 11) - 22 = -20
test "12 _3 4 ^ + 11 / v 22 - f"  "-20"

# ===== STACK OPERATIONS =====

# 3+3=6
test "3 d + f"  "6"

# 7+7+7=21
test "7 d d + + p"  "21"

# d at a capacity boundary: 8 pushes fill the initial capacity (8), so the
# duplicate forces a realloc of the backing array while copying the top
# (regression test for a use-after-free of the mpz handle in stack_push_value)
test_stack "1 2 3 4 5 6 7 8 d f"  "8" "8" "7" "6" "5" "4" "3" "2" "1"
test_stack "1 2 3 4 5 6 7 8 [s] 9 d f"  "9" "9" "s" "8" "7" "6" "5" "4" "3" "2" "1"

# 5-2=3 with swapped operands: 2-5=-3
test "5 2 r - f"  "-3"

# Swap twice restores the original order
test_stack "1 2 r r f"  "2" "1"

# Swap with fewer than two values is a silent no-op (as in dc)
test "r f"  ""
test "1 r f"  "1"

# R rotates the top n values by one position: a positive n moves the bottom
# of the rotated segment to the top (the top value becomes second from top)
test_stack "1 2 3 4 2 R f"  "3" "4" "2" "1"
test_stack "1 2 3 4 3 R f"  "2" "4" "3" "1"

# A negative n rotates the other way: the top value moves to the bottom of
# the rotated segment
test_stack "1 2 3 4 _3 R f"  "3" "2" "4" "1"
test_stack "1 2 3 4 _2 R f"  "3" "4" "2" "1"

# The count is truncated to its integer part
test_stack "1 2 3 4 2.9 R f"  "3" "4" "2" "1"
test_stack "1 2 3 4 _2.9 R f"  "3" "4" "2" "1"

# n = 0 and n = 1 are no-ops; a count >= the stack size rotates the whole
# stack once (as in dc)
test_stack "1 2 3 4 0 R f"  "4" "3" "2" "1"
test_stack "1 2 3 4 1 R f"  "4" "3" "2" "1"
test_stack "1 2 3 4 6 R f"  "1" "4" "3" "2"
test_stack "1 2 3 8 R f"     "1" "3" "2"
test_stack "1 2 3 _8 R f"    "2" "1" "3"

# A huge count rotates the whole stack once (arbitrary precision; dc punts
# with "value overflows simple integer" and does not rotate)
test_stack "1 2 3 2 100 ^ R f"  "1" "3" "2"
test_stack "1 2 3 0 2 100 ^ - R f"  "2" "1" "3"   # huge negative count: the other way

# R rotates strings like any other value; fewer than two values left after
# popping the count is a no-op
test_stack "1 [ab] 2 2 R f"  "ab" "2" "1"
test "5 1 R f"  "5"
test "1 R f"  ""

# A string count is an error and the operands stay on the stack (as in dc)
test_error_stack "1 2 [s] R f"  $'s\n2\n1'  "non-numeric value"
test_error "R"  "stack underflow"

# Clear then push
test "5 c 3 p"  "3"

# Printing an empty stack prints nothing
test "f"  ""
test "1 2 c f"  ""

# ===== n AND P (print without newline) =====

# n prints the value without a trailing newline and pops it
test "65 n"  "65"
test "65 n 66 n"  "6566"
test_error "5 n n"  "stack underflow"
test_error "n"  "stack underflow"

# P prints the number as bytes, most significant byte first (dc semantics)
test "72 P"  "H"
test "26952 P"  "iH"          # 26952 = 105*256 + 72
test "72 105 256 * + P"  "iH"
test "16706 P"  "AB"          # 16706 = 0x4142
test "4276803 P"  "ABC"       # 0x414243
test "_72 P"  "H"             # absolute value
test "18496 P"  "H@"          # 0x4840
test_error "P"  "stack underflow"
# 0 prints as a single NUL byte, as in dc (byte-level check: $() drops NULs)
test_bytes '0 P\n'  "00"
test_bytes '26952 P\n'  "6948"

# ===== COMPACT NOTATION (no spaces between tokens) =====

test "2 3*4+p"  "10"
test "12_3-p"  "15"   # 12 - (-3)
test "10 2/p"  "5"
test "5 5 +p"  "10"
test "1 2+3*p"  "9"
test "65n"  "65"
test "72P"  "H"

# A lone underscore parses as 0, as in dc
test "_ p"  "0"

# Tabs separate tokens too
test_lines "3"  '1\t2 + p\n'

# ===== COMMENTS (# to end of line) =====

test "5 5 + p # comment"  "10"
test_lines "5"  '# just a comment\n5 p\n'
# A comment starts mid-token: 5#c leaves the 5 on the stack, the rest of the line is ignored
test_lines "5"  '5#c\np\n'

# ===== MULTILINE INPUT =====
# The stack persists across lines; an error on one line does not abort the session

test_lines $'5\n1'  '1 2\n3 + f\n'
test_lines "5"  '\n5 p\n'
test_lines "5"  '1 +\n5 p\n'

# The last line is evaluated even without a trailing newline
test_lines $'5\n6'  '5 p\n6 p'

# Empty stdin: no output, clean exit
test_lines ""  ''

# ===== COMMAND LINE =====

test_e "2" -e "1 1 + p"
test_e "2" -e "1 1" -e "+ p"   # multiple -e run in command-line order
test_e "2" -e "1 Q" -e "2 p"   # Q unwinds only its own -e frame (as in dc)
test_e "2" --expression="1 1 + p"
test_contains "rpn 0.2.2" -V
test_contains "rpn 0.2.2" --version
test_contains "Usage:" -h
test_contains "Usage:" --help
test_exit 0 -h
test_exit 0 -V
test_exit 1 -z   # unknown option

# ===== ERROR CONDITIONS =====
# All error messages go to stderr and make the process exit non-zero;
# the session itself keeps running, so later lines are still evaluated

# Unimplemented command
test_error "wy"  "'w' unimplemented"

# Division by zero (operands stay on the stack, for /, % and ~ alike)
test_error "5 0 /"  "divide by zero"
test_error "5 0 %"  "remainder by zero"
test_error "10 0 ~"  "divide by zero"
test_error_stack "10 0 / f"  $'0\n10'  "divide by zero"
test_error_stack "10 0 % f"  $'0\n10'  "remainder by zero"
test_error_stack "10 0 ~ f"  $'0\n10'  "divide by zero"

# Invalid operands: rejected with an error, operands kept on the stack
test_error "_4 v"  "square root of negative number"
test_error_stack "_4 v f"  "-4"  "square root of negative number"
test_error "2 99999999999999999999 ^"  "exponent too large in raise"
test_error_stack "2 99999999999999999999 ^ f"  $'99999999999999999999\n2'  "exponent too large in raise"

# Stack underflow
test_error "p"  "stack underflow"
test_error "v"  "stack underflow"
test_error "d"  "stack underflow"
test_error "1 +"  "stack underflow"
test_error "1 -"  "stack underflow"
test_error "1 *"  "stack underflow"
test_error "1 /"  "stack underflow"
test_error "1 %"  "stack underflow"
test_error "1 ~"  "stack underflow"
test_error "1 ^"  "stack underflow"

# ===== EXIT CODES =====

# Clean runs exit 0
test_exit 0 -e "1 1 + p"
test_exit_stdin 0 "10"  '5 5 + p\n'

# Any runtime error makes the process exit 1
test_exit 1 -e "1 0 /"
test_exit 1 -e "wy"
test_exit_stdin 1 ""  '5 0 /\n'
test_exit_stdin 1 ""  '5 0 %\n'
test_exit_stdin 1 ""  '10 0 ~\n'
test_exit_stdin 1 ""  '1 +\n'
test_exit_stdin 0 "0"  '2 _1 ^ p\n'
test_exit_stdin 1 ""  '_4 v\n'

# ...but the session continues after an error
test_exit_stdin 1 "5"  '1 +\n5 p\n'

# ===== INPUT BUFFER (io.c) =====

# Long lines force several flushes of the 16-byte input buffer
test "12345678901234567890123456789012345678901234567890 1 + p"  "12345678901234567890123456789012345678901234567891"
test "12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678 1 + p"  "12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345679"

# A NUL byte must not overflow the input buffer: strlen() stops at the NUL,
# which used to defeat the flush guard and write past the buffer
tests_run=$((tests_run + 1))
nul_result=$( { printf '5 5 + p\n1\x00'; printf 'A%.0s' $(seq 1 300); printf '\n'; } | ./rpn 2>/dev/null )
if [ "$nul_result" != "10" ]; then
    failures=$((failures + 1))
    echo "FAIL NUL byte test: expected '10', got '$nul_result'"
fi

# NUL-only input must terminate cleanly (used to read input[-1] and loop forever)
tests_run=$((tests_run + 1))
nul_only=$(printf '\x00' | timeout 5 ./rpn 2>/dev/null)
nul_only_code=$?
if [ "$nul_only_code" -ne 0 ] || [ -n "$nul_only" ]; then
    failures=$((failures + 1))
    echo "FAIL NUL-only input test: expected exit 0 and no output, got exit $nul_only_code, output '$nul_only'"
fi

# ===== STRINGS =====

# A [...] literal pushes a string (outer brackets stripped)
test "[hello] p"  "hello"
test "[hello world] p"  "hello world"

# P prints a string without a trailing newline and pops it
test "[abc] P"  "abc"
test "[a] P [b] P"  "ab"
test "[abc] P 7 f"  "abc7"      # P popped the string, only 7 remains

# n pops and prints a string without a trailing newline
test "[abc] n 5 p"  "abc5"

# An empty string is a valid value
test "[] P 5 p"  "5"
test "[] p"  ""

# Nested brackets: the outer pair is stripped, inner pairs are kept
test "[[a]b] P"  "[a]b"
test "[[a]] p"  "[a]"

# Strings may contain spaces, # and digits; a string that starts like a
# number (or a comment) stays a string
test "[a b] p"  "a b"
test "[#not a comment] p"  "#not a comment"
test "[5x] p"  "5x"
test "[_3] p"  "_3"

# Literals can be adjacent to other tokens
test "2[ab]f"  $'ab\n2'
test "2 3+[ab]f"  $'ab\n5'
test "[ab]5p"  "5"

# d, r, c work on strings
test_stack "2 3 [ab] r f"  "3" "ab" "2"
test "[a] d r f"  $'a\na'
test "[c] c f"  ""

# f prints strings and numbers, top first
test_stack "[x] 1 [y] f"  "y" "1" "x"

# Arithmetic on a string: error, operands stay on the stack (as in dc)
test_error_stack "[hi] 3 + f"  $'3\nhi'  "non-numeric value"
test_error_stack "3 [hi] + f"  $'hi\n3'  "non-numeric value"
test_error_stack "[a] [b] * f"  $'b\na'  "non-numeric value"
test_error_stack "[hi] v f"  "hi"  "non-numeric value"
test_error "[hi] 1 -"  "non-numeric value"
test_error "[hi] 0 /"  "non-numeric value"
test_error "[hi] 2 ^ f"  "non-numeric value"
test_error "1 [hi] ~"  "non-numeric value"

# An unterminated string literal is an error
test_error "[abc"  "unterminated string"
test_exit_stdin 1 ""  '[abc\n'

# Strings persist across lines like any other value
test_lines $'ab\ncd'  '[ab] p\n[cd] p\n'

# ===== MACROS (x) =====

# x executes a string as commands on the same stack
test "[1 2 + p] x"  "3"
test "1 [2 +] x p"  "3"
test "[5] x p"  "5"
test "[2 d * p] x"  "4"

# Macros nest
test "[[2 3 + p] x] x"  "5"
test "[[[[1 p] x] x] x] x"  "1"

# A macro can push and print strings
test "[[ab] P] x"  "ab"
test "[[ab] p] x"  "ab"

# An empty macro is a no-op
test "[] x 5 p"  "5"

# x on a number does nothing: the stack is left untouched (as in dc)
test "5 x f"  "5"
test "5 6 x f"  $'6\n5'

# x needs one value on the stack
test_error "x"  "stack underflow"

# Errors inside a macro are reported and the session continues
test_error "[5 0 /] x"  "divide by zero"
test_exit_stdin 1 "7"  '[5 0 /] x 7 p\n'

# Compact notation
test "[1p]x"  "1"

# Macros work with -e too
test_e "3" -e "[1 2 + p] x"

# Nesting is depth-limited: a self-re-executing macro errors out cleanly
# instead of overflowing the C stack (regression test for a SIGSEGV)
test_error "[[dx]x]dx"  "macro nesting too deep"
test_exit 1 -e "[[dx]x]dx"

# Deep but bounded nesting still executes
deep="1p"
for _ in $(seq 1 50); do deep="[${deep}]x"; done
test "$deep"  "1"

# ===== REGISTERS (s l S L : ;) =====
# Each command letter is followed by the register name; the name may be
# written with or without a separating space (sa and s a both name register a)

# s pops and stores (replacing any previous content), l pushes a copy
test "5 sa la p"  "5"
test "5 sa 7 sa la p"  "7"
test "5 s a l a p"  "5"

# l on an empty register loads 0, as in dc
test "la p"  "0"

# Register names are case-sensitive and may be digits
test "5 sA lA p 6 s1 l1 p"  $'5\n6'
# The name is a single character even when a digit char follows it: sA2
# names register A and pushes 2, s12 names register 1 and pushes 2 (as in dc)
test "5 sA2 lA p"  "5"
test "5 s12 l1 p"  "5"
test "5 s 12 l1 p"  "5"          # whitespace before the name is skipped; name is '1'
test "42 3 :A7 3 ;A p"  "42"
test "[9 p] sA 1 2 >A5 p"  $'9\n5'

# Registers hold strings too, and macros can be stored and executed
test "[hi] sa la p"  "hi"
test "[1 p] sa la x"  "1"
test "5 sa [la 1 + p] x"  "6"

# S pushes onto the register's own stack, L pops it back; l peeks
test "1 2 3 Sa La f"  $'3\n2\n1'
test "1 Sa 2 Sa la p La La f"  $'2\n1\n2\n2'
test "[hi] Sa La p"  "hi"

# Registers persist across lines
test_lines "9"  '9 sa\nla p\n'

# s replaces a whole S-built register stack, not just the top
test "1 2 Sa 5 sa La p"  "5"
test_error "1 2 Sa 5 sa La La"  "stack register 'a' is empty"

# Register commands work under -e, and errors exit 1
test_e "5"  -e "5 sa la p"
test_exit 1  -e "La"
test_exit_stdin 1 "5"  '5 sa\nla p\nLa\nLa\n'

# Errors
test_error "sa"  "stack underflow"
test_error "Sa"  "stack underflow"
test_error "La"  "stack register 'a' is empty"
test_error "5 s"  "expected a single-character register name"
test_error "5 s [a]"  "expected a single-character register name"
# A comment is never a register name (dc names a register after the space)
test_error "5 s # note"  "expected a single-character register name"
test_error "5 s# note"  "expected a single-character register name"

# Arrays (: stores, ; loads): top of stack is the index, missing loads 0
test "42 3 :a 3 ;a p"  "42"
test "3 ;a p"  "0"
test "7 0 :a 0 ;a p"  "7"
test "1 0 :a 2 0 :a 0 ;a p"  "2"      # storing again overwrites
test "[hi] 2 :a 2 ;a p"  "hi"         # arrays hold strings
test "42 3 :a 5 sa 3 ;a p la p"  $'42\n5'  # a register's stack and array are independent
# Array indices are arbitrary-precision (dc 1.5.2 punts on these)
test "7 100000000000000000000 :a 100000000000000000000 ;a p"  "7"
test "100000000000000000000 ;a p"  "0"   # never-stored huge index loads 0

# The index must be a nonnegative integer; on error the operands stay
test_error_stack "1 [x] :a f"  $'x\n1'  "array index must be a nonnegative integer"
test_error_stack "[x] ;a f"  "x"  "array index must be a nonnegative integer"
test_error_stack "5 _2 :a f"  $'-2\n5'  "array index must be a nonnegative integer"
test_error "5 :a"  "stack underflow"
test_error ";a"  "stack underflow"

# Compact notation
test "5salap"  "5"

# ===== CONDITIONALS (> < = !> !< !=) =====
# Two values are popped and compared as numbers; the register fires when the
# original top of stack satisfies the comparison against the second value.
# The register is executed as a macro but not consumed.

test "[9 p] sa 1 2 >a"  "9"           # 2 > 1: fires
test "[9 p] sa 2 1 >a 7 p"  "7"       # 1 > 2: does not fire
test "[9 p] sa 1 2 >a 1 2 >a"  $'9\n9'  # the register is not consumed
test "[9 p] sa 2 1 >a f"  ""          # false: both values popped, nothing fired
test "[9 p] sa 1 2 >a f"  $'9\n9'     # true: the macro ran and left its 9

test "[8 p] sa 5 5 =a"  "8"
test "[8 p] sa 5 6 =a 7 p"  "7"
test "[8 p] sa 5 6 !=a"  "8"
test "[8 p] sa 5 5 !=a 7 p"  "7"
test "[8 p] sa 5 5 !>a"  "8"          # !> fires on top <= second
test "[8 p] sa 5 6 !>a 7 p"  "7"
test "[8 p] sa 6 5 !>a"  "8"
test "[8 p] sa 5 5 !<a"  "8"          # !< fires on top >= second
test "[8 p] sa 5 6 !<a"  "8"
test "[8 p] sa 4 6 !<a 7 p"  $'8\n7'  # top 6 >= second 4: fires
test "[8 p] sa 1 _1 >a 7 p"  "7"      # negatives compare numerically
test "[8 p] sa _1 1 >a"  "8"

# Comparisons are numeric only; a string operand errors and stays on the stack
test_error_stack "1 [a] >a f"  $'a\n1'  "non-numeric value"
test_error_stack "[a] [b] >a f"  $'b\na'  "non-numeric value"
test_error "1 >a"  "stack underflow"
test_error ">a"  "stack underflow"
test_error "1 2 >"  "expected a single-character register name"

# An empty register fires as a load of 0, which x then leaves on the stack
test "1 2 >z f"  "0"
# A number in a fired register is left on the stack (x on a number is a no-op)
test "5 sa 1 2 >a f"  "5"

# Conditionals work inside macros, and loops are possible: countdown from 5
test "[8 p] sb [1 2 >b] sa 1 2 >a"  "8"
test "5 sc [lc p lc 1 - sc 0 lc >m] sm lmx"  $'5\n4\n3\n2\n1'

# Compact notation
test "[9p]sa 1 2>a"  "9"

# ===== q AND Q =====

# q quits two levels of macro execution (the current macro and its invoker);
# at the top level, or in a macro called directly from the top level, it
# exits the program. A numeric top of stack is not popped.
test "1 p q 3 p"  "1"
test_lines "1"  '1 p q\n3 p\n'                 # stops reading further lines
test "[1 p q 2 p] x 3 p"  "1"                  # top-level-called macro: exits
test "[1 p q 2 p] sa lax 3 p"  "1"
test "[[1 p q 2 p] x 3 p] x 4 p"  $'1\n4'      # at depth 2, unwinds two levels
test "[[7 q] x] x f"  "7"                      # q does not pop a numeric top
test "[[1 p 2 q 2 p] x 3 p] x 4 p"  $'1\n4'    # the number before q is not a count
test_exit_stdin 0 "1"  '1 p q\n'

# Q pops a count N and quits N levels of macro execution; it never exits
test "[1 p 1 Q 2 p] x 3 p"  $'1\n3'
test "[[1 p 2 Q 2 p] x 3 p] x 4 p"  $'1\n4'
test "[1 p 0 Q 2 p] x 3 p"  $'1\n2\n3'         # 0 Q is a no-op
test "0 Q 5 p"  "5"
test_error_stack "2 Q f"  ""  "exceeded string execution depth"  # count popped on error
test_error_stack "[a] Q f"  "a"  "requires a number >= 0"        # operand kept
test_error_stack "_1 Q f"  "-1"  "requires a number >= 0"
test_error_stack "999999999999999999999 Q f"  "999999999999999999999"  "requires a number >= 0"
test_error "Q"  "stack underflow"
test_error_stack "1 p 1 Q 2 p"  $'1\n2'  "exceeded string execution depth"  # Q never exits

# A loop terminated by Q fired from a conditional (verified against dc)
test "3 sc [1 Q] sq [lc p lc 1 - sc lc 0 =q 0 lc >m] sm lmx"  $'3\n2\n1'

# Over-depth Q (N > depth) is an error (count popped) but still unwinds the
# available levels, min(N, depth), as in dc
test_error_stack "[1 p 2 Q 2 p] x 3 p"  $'1\n3'  "exceeded string execution depth"
test_error_stack "[1 p 3 Q 9 p] x 3 p"  $'1\n3'  "exceeded string execution depth"
test_error_stack "[[1 p 3 Q 9 p] x 3 p] x 4 p"  $'1\n4'  "exceeded string execution depth"
test_error_stack "[[[1 p 5 Q 2 p] x 3 p] x 4 p] x 5 p"  $'1\n5'  "exceeded string execution depth"
test_error_stack "[2 Q] x 5 p"  "5"  "exceeded string execution depth"

# ===== -e RUNS AT MACRO DEPTH 1 (as in dc) =====

# In -e mode the expression is one macro level above the top level, so q/Q
# counting matches dc: 1 Q at depth 1 is valid and unwinds the -e frame
test_e "" -e "1 Q 5 p"
test_e "" -e "[2 Q] x 5 p"
test_e "5" -e "0 Q 5 p"
test_e "1" -e "1 p q 3 p"
test_e "5" -e "5 p"
test_e "1" -e "[1 p 2 Q 2 p] x 3 p"       # 2 Q at depth 2: valid, unwinds both frames
test_e_error "exceeded string execution depth" -e "5 Q 3 p"  # over-depth at depth 1: error, unwinds the -e frame

# ===== DECIMAL NUMBERS AND SCALE (k, K) =====
# A number is num / 10^scale. Parsing: 1.5, .5, 1., _.5; a second '.' starts
# a new token (1..2 is 1. then .2). Printing keeps the scale (1.50) but drops
# a leading zero (.5), and zero always prints as "0".

test "1.5 p"  "1.5"
test ".5 p"  ".5"
test "1. p"  "1"
test "_0.5 p"  "-.5"
test "007.50 p"  "7.50"
test "0.50 p"  ".50"
test "0.0 p"  "0"
test "0.00 p"  "0"
test "_0.0 p"  "0"
test "0.10 p"  ".10"
test "1..2 f"  $'.2\n1'
test "1.2.3 f"  $'.3\n1.2'
test ". p"  "0"
test ".. f"  $'0\n0'
test "_. p"  "0"

# k sets the scale factor (integer part of the top value), K pushes it
test "K p"  "0"
test "2 k K p"  "2"
test "2.5 k K p"  "2"          # k truncates its operand
test_error "_1 k"  "scale must be a nonnegative number"
test_error_stack "_1 k f"  "-1"  "scale must be a nonnegative number"
test_error "[a] k"  "scale must be a nonnegative number"
test_error "1000001 k"  "scale too large"   # MAX_SCALE cap
test "1000000 k K p"  "1000000"

# / produces exactly k fractional digits, truncating toward zero
test "0 k 1 3 / p"  "0"
test "2 k 1 3 / p"  ".33"
test "5 k 1 2 / p"  ".50000"
test "2 k _1 3 / p"  "-.33"
test "2 k 1 _3 / p"  "-.33"
test "2 k 10 3 / p"  "3.33"
test "2 k 1.50 3 / p"  ".50"
test_error "2 k 1 0 /"  "divide by zero"

# % and ~: quotient at scale k, remainder at scale max(sa, sb+k)
test "0 k 5 3 % p"  "2"
test "2 k 5 3 % p"  ".02"
test "0 k 5.0 3 % p"  "2.0"
test "1 k 5 3.00 % p"  ".200"
test "3 k 5.0 3 % p"  ".002"
test "2 k 5 3.0 ~ f"  $'.020\n1.66'
test "2 k 5 3 ~ f"  $'.02\n1.66'
test_error "2 k 5 0 %"  "remainder by zero"
test_error_stack "2 k 10 0 ~ f"  $'0\n10'  "divide by zero"
# % and ~ error with the operands kept when the remainder scale max(sa, sb+k)
# exceeds MAX_SCALE
test_error_stack "1000000 k 3.0 1.0 % f"  $'1.0\n3.0'  "scale too large"
test_error_stack "1000000 k 3.0 1.0 ~ f"  $'1.0\n3.0'  "scale too large"

# + and -: result scale max(sa, sb); *: min(sa+sb, max(sa, sb, k))
test "1.5 2.25 + p"  "3.75"
test "1.5 2 + p"  "3.5"
test "1.5 2 - p"  "-.5"
test "5 k 1.5 2 + p"  "3.5"
test "1.5 2.5 * p"  "3.7"      # capped at k=0
test "2 k 0.1 0.1 * p"  ".01"
test "5 k 1.5 2.5 * p"  "3.75"
test "1.5 2 * p"  "3.0"

# ^: integer exponent; result scale min(sa*n, max(sa, k)), or k for a
# negative exponent. The exponent is truncated to its integer part.
test "2 k 2 2 ^ p"  "4"
test "3 k 1.5 2 ^ p"  "2.25"
test "0 k 1.5 3 ^ p"  "3.3"
test "2 k 2.5 2 ^ p"  "6.25"
test "2 k 0.5 2 ^ p"  ".25"
test "0 k 2.5 0 ^ p"  "1"
test "3 k 1.0 5 ^ p"  "1.000"
test "3 k _1.0 3 ^ p"  "-1.000"
test "5 k 2 _2 ^ p"  ".25000"
test "3 k 2 _1 ^ p"  ".500"
test "2 k 1.5 _1 ^ p"  ".66"
test "1 k _1.5 _1 ^ p"  "-.6"
test "0 k 0.0 5 ^ p"  "0"
test_error_stack "2 99999999999999999999 ^ f"  $'99999999999999999999\n2'  "exponent too large in raise"

# Negative exponent at scale k is the exact truncation of 10^(k+sa*n)/|num|^n
# (regression: a too-small bit-length heuristic used to print 0 where the exact
# result is >= 1, e.g. 2 33 ^ _1 at k=10)
test "10 k 2 33 ^ _1 ^ p"  ".0000000001"
test "20 k 2 66 ^ _1 ^ p"  ".00000000000000000001"
# A fractional exponent is truncated to its integer part
test "2 k 2 2.9 ^ p"  "4"
# A negative exponent whose 10^(k+sa*n) numerator would exceed the result-size
# cap is rejected with the operands kept (regression: used to allocate
# unboundedly and abort GMP from a short input)
test_error "1000000 k 1 10 1000000 ^ / _268435456 ^"  "exponent too large in raise"
test_error "0.5 _81000000 ^"  "exponent too large in raise"

# v: square root at scale max(sa, k)
test "0 k 2 v p"  "1"
test "3 k 2 v p"  "1.414"
test "0 k 2.000 v p"  "1.414"
test "5 k 2.000 v p"  "1.41421"
test "3 k 2.25 v p"  "1.500"
test "1 k 10. v p"  "3.1"
test_error "_4 v"  "square root of negative number"

# Comparisons ignore scale; P prints the integer part's bytes; registers,
# arrays and Q use the integer part
test "[8 p] sa 1.5 1.50 =a"  "8"
test "[8 p] sa 1.5 1.6 <a"  ""
test "1.5 sa la p"  "1.5"
test "5 1.5 :a 1 ;a p"  "5"
test "5 1.5 :a 1.9 ;a p"  "5"
test_bytes '1.5 P\n'  "01"
test_bytes '0.5 P\n'  "00"
test_error "1.5 0.5 :a"  ""  # 0.5 truncates to index 0: valid
test "1.5 0.5 :a 0 ;a p"  "1.5"
test_error_stack "5 _0.5 :a f"  $'-.5\n5'  "array index must be a nonnegative integer"

# Scale persists across lines
test_lines $'2\n.33'  '2 k K p\n1 3 / p\n'

# ===== RADICES (i I o O) =====
# Numbers are stored base 10 internally; i converts input digits, o converts
# output. Both default to 10 and persist across lines.

test "16 i FF p"  "255"
test "16 i 10 p"  "16"
test "16 o 255 p"  "FF"
test "16 o _255 p"  "-FF"
test "2 o 255 p"  "11111111"
test "16 i F.F p"  "15.9"
test "16 i F.FF p"  "15.99"
test "16 i 0.8 p"  ".5"
test "2 i 1.1 p"  "1.5"
test "2 i 10 p"  "2"
test "16 i _FF p"  "-255"
test "8 i 10 10 + p"  "16"
test "16 i A B + p"  "21"
test "16 i FF 2 ^ p"  "65025"
test "16 o 100 2 ^ p"  "2710"

# I and O push the current bases (printed in the output base)
test "I p O p"  $'10\n10'
test "16 i I p"  "16"
test "16 o O p"  "10"           # 16 in base 16
test "16 i FF 10 o p"  "FF"

# Fractional output in another base: ceil(log_base(10^scale)) digits
test "16 o 2 k 1 3 / p"  ".54"
test "16 o 3 k 1 3 / p"  ".553"
test "16 o 0.5 p"  ".8"
test "16 o _0.5 p"  "-.8"
test "2 o 3 k 1 2 / p"  ".1000000000"
test "8 o 2 k 1 3 / p"  ".250"
test "16 o 15.9375 p"  "F.F000"
test "16 o 2 k 255 v p"  "F.F5"
test "16 o 2 k 255.5 p"  "FF.8"
test "16 o 2 k 0.1 0.1 * p"  ".02"
test "16 i _F.F p"  "-15.9"

# Fraction digit count is exactly ceil(log_base(10^scale)), also in
# non-power-of-2 bases (mpz_sizeinbase may report one too many)
test "15 o 1 k 1 2 / p"  ".7"
test "3 o 7 k 1 2 / p"   ".111111111111111"
test "11 o 2 k 1 2 / p"  ".55"
test "5 o 4 k 1 2 / p"   ".222222"
test "7 o 5 k 1 3 / p"   ".222221"
test "6 o 9 k 1 7 / p"   ".050505050503"

# Bases switch back and persist across lines
test "10 o 16 o 255 p 10 o 255 p"  $'FF\n255'
test_lines "255"  '16 i FF p\n'
test_lines "FF"  '16 o\n255 p\n'

# In macros and registers
test "16 i FF sa la p"  "255"
test "16 i [FF p] x"  "255"
test "2 i 1010 2 o p"  "1010"
test "[8 p] sa 16 i B A <a"  "8"

# P prints bytes of the integer part, independent of the output base
test_bytes '16 o 255 P\n'  "ff"

# n and f print in the output base too; zero prints as 0 whatever its scale
test "16 o 255 n"  "FF"
test "16 o 255 5 f"  $'5\nFF'
test "16 o 0.00 p"  "0"
test "2 o _0.000 p"  "0"

# Errors (operands stay on the stack)
test_error "17 i 5 p"  "input base must be a number between 2 and 16 (inclusive)"
test_error "1 i 5 p"  "input base must be a number between 2 and 16 (inclusive)"
test_error "[a] i 5 p"  "input base must be a number between 2 and 16 (inclusive)"
test_error "1 o 5 p"  "output base must be a number greater than 1"
test_error "0 o 5 p"  "output base must be a number greater than 1"
test_error "[a] o 5 p"  "output base must be a number greater than 1"
test_error "17 o 5 p"  "output base must be a number between 2 and 16 (inclusive)"  # our cap; dc accepts > 16
test "2.5 o 5 p"  "101"        # o truncates its operand to the integer part
test "2.5 i 5 p"  "5"

# Lowercase letters are not digits
test_error "16 i G p"  "'G' unimplemented"

# i/o with no operand, a negative or a huge (non-slong) operand
test_error "i"  "stack underflow"
test_error "o"  "stack underflow"
test_error "_5 i 5 p"  "input base must be a number between 2 and 16 (inclusive)"
test_error "_5 o 5 p"  "output base must be a number greater than 1"
test_error "99999999999999999999 i 5 p"  "input base must be a number between 2 and 16 (inclusive)"
test_error "99999999999999999999 o 5 p"  "output base must be a number greater than 1"

# Long literals parse correctly (chunked accumulation, not per-digit) and
# stay fast: 32 F's in base 16, 64 ones in base 2, and 27 F's in base 10
# keeping their face value across chunk boundaries
test "16 i FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF p"  "340282366920938463463374607431768211455"
test "2 i 1111111111111111111111111111111111111111111111111111111111111111 p"  "18446744073709551615"
test "FFFFFFFFFFFFFFFFFFFFFFFFFFF p"  "1666666666666666666666666665"

# ===== STACK INTROSPECTION (z Z X) =====

# z pushes the stack depth without popping
test "z p"  "0"
test "1 2 3 z f"  $'3\n3\n2\n1'
test "1 z f"  $'1\n1'

# Z pops and pushes the decimal digit count of a number (sign and point not
# counted), or the character count of a string. The values 8, 9, 64, 512, 99
# and 99999999999999999999 pin the exact count: mpz_sizeinbase(_, 10) is
# one too big for them, so they catch an uncorrected upper bound.
test "123 Z p"  "3"
test "0 Z p"  "1"
test "_12 Z p"  "2"
test "100 Z p"  "3"
test "1.50 Z p"  "3"
test ".5 Z p"  "1"
test "[hello] Z p"  "5"
test "[] Z p"  "0"
test "2 10 ^ Z p"  "4"
test "8 Z p"  "1"
test "9 Z p"  "1"
test "64 Z p"  "2"
test "512 Z p"  "3"
test "99 Z p"  "2"
test "10 Z p"  "2"
test "_1.50 Z p"  "3"
test "99999999999999999999 Z p"  "20"
test "16 o 8 Z p"  "1"
test_error "Z"  "stack underflow"

# X pops and pushes the scale of the value (0 for a string)
test "5 X p"  "0"
test "1.50 X p"  "2"
test "1.50 X f"  "2"          # X popped the 1.50
test "[hi] X p"  "0"
test "16 o 0.5 X p"  "1"
test "2 k 1 3 / X p"  "2"
test_error "X"  "stack underflow"

# ===== ? (read a line from stdin at runtime) =====
# ? reads one line from stdin, echoes it, and executes it as a new macro
# level. At EOF it exits the program silently.

test_lines $'1\n2 + p\n3'  '1 p ?\n2 + p\n'
test_lines $'42\n42'  '? p\n42\n'
test_lines $'2 p\n2\n1'  '[? 1 Q 9 p] x 1 p\n2 p\n'   # 1 Q unwinds the macro only
test_lines $'q\n1'  '[?] x 1 p\nq\n'                  # q in the ?-line unwinds two levels
test_lines "1"  '1 p ? 2 p\n'                       # EOF on ?: exits silently
test_lines ""  '?\n'                                # bare ? at EOF
test_lines ""  '[[?]x 8 p]x 9 p\n'                # EOF inside nested ?: unwinds all levels, 9 p never runs
test_lines "1"  '1 p [[?]x]x 9 p\n'                # EOF inside nested ?: only output before EOF is printed

# ? works in -e mode too (reads the remaining stdin)
tests_run=$((tests_run + 1))
r=$(printf '7 p\n' | ./rpn -e '? p' 2>/dev/null)
if [ "$r" != $'7 p\n7\n7' ]; then
    failures=$((failures + 1))
    echo "FAIL -e '? p': expected '7 p\n7\n7', got '$r'"
fi

# ===== SCRIPT FILES (positional arguments) =====
# Files run in order after -e; stdin is read after files (or when neither
# -e nor files are given). stdin is redirected to /dev/null in these tests.

tmpd=$(mktemp -d)
printf '1 p\n2 2 + p\n' >"$tmpd/a.rpn"
printf '5 p\n' >"$tmpd/b.rpn"
printf '1 0 /\n7 p\n' >"$tmpd/bad.rpn"
printf '8 p' >"$tmpd/nonl.rpn"                          # no trailing newline
printf '1 p q\n2 p\n' >"$tmpd/q.rpn"
printf '5 sc [lc p lc 1 - sc 0 lc >m] sm lmx\n' >"$tmpd/loop.rpn"

# files run in order
tests_run=$((tests_run + 1))
r=$(./rpn "$tmpd/a.rpn" "$tmpd/b.rpn" </dev/null 2>/dev/null)
[ "$r" = $'1\n4\n5' ] || { failures=$((failures+1)); echo "FAIL files in order: got '$r'"; }

# -e runs before files
tests_run=$((tests_run + 1))
r=$(./rpn -e '9 p' "$tmpd/a.rpn" </dev/null 2>/dev/null)
[ "$r" = $'9\n1\n4' ] || { failures=$((failures+1)); echo "FAIL -e before files: got '$r'"; }

# stdin is read after files
tests_run=$((tests_run + 1))
r=$(printf '7 p\n' | ./rpn "$tmpd/a.rpn" 2>/dev/null)
[ "$r" = $'1\n4\n7' ] || { failures=$((failures+1)); echo "FAIL stdin after files: got '$r'"; }

# -e alone does not read stdin
tests_run=$((tests_run + 1))
r=$(printf '7 p\n' | ./rpn -e '1 p' 2>/dev/null)
[ "$r" = "1" ] || { failures=$((failures+1)); echo "FAIL -e no stdin: got '$r'"; }

# a missing file is an error (exit 1) but later files still run
tests_run=$((tests_run + 1))
r=$(./rpn "$tmpd/nope" "$tmpd/b.rpn" </dev/null 2>"$tmpd/err"); code=$?
if [ "$r" != "5" ] || [ "$code" != "1" ] || ! grep -q "could not open file" "$tmpd/err"; then
    failures=$((failures+1)); echo "FAIL missing file: out='$r' code=$code err='$(cat "$tmpd/err")'"
fi

# an error inside a file does not stop the session
tests_run=$((tests_run + 1))
r=$(./rpn "$tmpd/bad.rpn" "$tmpd/b.rpn" </dev/null 2>/dev/null); code=$?
[ "$r" = $'7\n5' ] && [ "$code" = "1" ] || { failures=$((failures+1)); echo "FAIL error in file: out='$r' code=$code"; }

# no trailing newline, loops in files, q in a file exits
tests_run=$((tests_run + 1))
r=$(./rpn "$tmpd/nonl.rpn" </dev/null 2>/dev/null)
[ "$r" = "8" ] || { failures=$((failures+1)); echo "FAIL no trailing newline: got '$r'"; }
tests_run=$((tests_run + 1))
r=$(./rpn "$tmpd/loop.rpn" </dev/null 2>/dev/null)
[ "$r" = $'5\n4\n3\n2\n1' ] || { failures=$((failures+1)); echo "FAIL loop in file: got '$r'"; }
tests_run=$((tests_run + 1))
r=$(./rpn "$tmpd/q.rpn" </dev/null 2>/dev/null)
[ "$r" = "1" ] || { failures=$((failures+1)); echo "FAIL q in file: got '$r'"; }

# a directory passed as a file argument is an error (exit 1), not silent EOF.
# The message is platform-pinned: "could not read" on POSIX (fopen succeeds,
# the read fails with EISDIR), "could not open file" on Windows (fopen itself
# fails with EACCES)
case "$(uname -s)" in MINGW*|MSYS*|CYGWIN*) dir_msg="could not open file";; *) dir_msg="could not read";; esac
tests_run=$((tests_run + 1))
r=$(./rpn "$tmpd" </dev/null 2>"$tmpd/err"); code=$?
if [ -n "$r" ] || [ "$code" != "1" ] || ! grep -qF "$dir_msg" "$tmpd/err"; then
    failures=$((failures+1)); echo "FAIL directory arg: out='$r' code=$code err='$(cat "$tmpd/err")'"
fi

# ===== -f / --file (script files given as options) =====
# -f files run in command-line order, interleaved with -e as given, and
# before positional files (as in dc)

tests_run=$((tests_run + 1))
r=$(./rpn -f "$tmpd/a.rpn" -f "$tmpd/b.rpn" </dev/null 2>/dev/null)
[ "$r" = $'1\n4\n5' ] || { failures=$((failures+1)); echo "FAIL -f in order: got '$r'"; }

tests_run=$((tests_run + 1))
r=$(./rpn -e '9 p' -f "$tmpd/a.rpn" "$tmpd/b.rpn" </dev/null 2>/dev/null)
[ "$r" = $'9\n1\n4\n5' ] || { failures=$((failures+1)); echo "FAIL -e -f positional order: got '$r'"; }

# options run in the order given: here -f runs before -e
tests_run=$((tests_run + 1))
r=$(./rpn -f "$tmpd/a.rpn" -e '9 p' "$tmpd/b.rpn" </dev/null 2>/dev/null)
[ "$r" = $'1\n4\n9\n5' ] || { failures=$((failures+1)); echo "FAIL -f -e interleaved: got '$r'"; }

# --file= long option
tests_run=$((tests_run + 1))
r=$(./rpn --file="$tmpd/b.rpn" </dev/null 2>/dev/null)
[ "$r" = "5" ] || { failures=$((failures+1)); echo "FAIL --file: got '$r'"; }

# -f - reads standard input
tests_run=$((tests_run + 1))
r=$(printf '7 p\n' | ./rpn -f - 2>/dev/null)
[ "$r" = "7" ] || { failures=$((failures+1)); echo "FAIL -f -: got '$r'"; }

# positional - reads standard input, after the options
tests_run=$((tests_run + 1))
r=$(printf '8 p\n' | ./rpn -e '3 p' - 2>/dev/null)
[ "$r" = $'3\n8' ] || { failures=$((failures+1)); echo "FAIL positional -: got '$r'"; }

# stdin is NOT read when only -e/-f options are given (as in dc) ...
tests_run=$((tests_run + 1))
r=$(printf '7 p\n' | ./rpn -f "$tmpd/a.rpn" 2>/dev/null)
[ "$r" = $'1\n4' ] || { failures=$((failures+1)); echo "FAIL -f only, no stdin: got '$r'"; }

# ... but it is read after positional files
tests_run=$((tests_run + 1))
r=$(printf '7 p\n' | ./rpn -f "$tmpd/a.rpn" "$tmpd/b.rpn" 2>/dev/null)
[ "$r" = $'1\n4\n5\n7' ] || { failures=$((failures+1)); echo "FAIL -f + positional, stdin: got '$r'"; }

# an unreadable -f file is an error (exit 1) but later arguments still run
tests_run=$((tests_run + 1))
r=$(./rpn -f "$tmpd/nope" "$tmpd/b.rpn" </dev/null 2>"$tmpd/err"); code=$?
if [ "$r" != "5" ] || [ "$code" != "1" ] || ! grep -q "could not open file" "$tmpd/err"; then
    failures=$((failures+1)); echo "FAIL missing -f file: out='$r' code=$code err='$(cat "$tmpd/err")'"
fi

# -f on a directory is an error, exit 1, with the same platform-pinned
# message as the directory-arg test above
tests_run=$((tests_run + 1))
r=$(./rpn -f "$tmpd" </dev/null 2>"$tmpd/err"); code=$?
if [ -n "$r" ] || [ "$code" != "1" ] || ! grep -qF "$dir_msg" "$tmpd/err"; then
    failures=$((failures+1)); echo "FAIL -f directory: out='$r' code=$code err='$(cat "$tmpd/err")'"
fi

# byte 0x1A (Ctrl-Z) in piped input is data, not EOF, on every platform:
# here it is an unknown command (error, exit 1) and the session continues
tests_run=$((tests_run + 1))
r=$(printf '5 p\n\x1a\n6 p\n' | ./rpn 2>/dev/null); code=$?
[ "$r" = $'5\n6' ] && [ "$code" = "1" ] || { failures=$((failures+1)); echo "FAIL 0x1A byte: out='$r' code=$code"; }

# an error inside a -f file sets exit 1 but does not stop the session
tests_run=$((tests_run + 1))
r=$(./rpn -f "$tmpd/bad.rpn" -f "$tmpd/b.rpn" </dev/null 2>/dev/null); code=$?
[ "$r" = $'7\n5' ] && [ "$code" = "1" ] || { failures=$((failures+1)); echo "FAIL error in -f file: out='$r' code=$code"; }

# over-depth Q reports the full message (count popped, still unwinds min(N, depth))
test_e_error "Q command argument exceeded string execution depth" -e "9 Q 3 p"

# -f without an argument is a usage error
test_exit 1 -f

rm -rf "$tmpd"

# ===== | (MODULAR EXPONENTIATION) =====
# Pops modulus (top), exponent, base: computes b^e mod m. Base and exponent
# are truncated to their integer parts; the modulus keeps its scale. Result
# scale is max(scale(base), scale(modulus)). Works with huge exponents.

test "2 3 5 | p"  "3"
test "2 0 5 | p"  "1"
test "0 0 5 | p"  "1"
test "2 3 1 | p"  "0"
test "3 0 1 | p"  "0"          # e=0, m=1: correct 0 (dc 1.5.2 gives 1)
test "0 5 3 | p"  "0"
test "7 0 3 | p"  "1"
test "10 100 7 | p"  "4"
test "123456789 987654321 1000000007 | p"  "652541198"
test "2 3 _5 | p"  "3"          # negative modulus
test "_2 3 5 | p"  "-3"         # negative base, odd exponent
test "_2 2 5 | p"  "4"          # negative base, even exponent
test "2.5 3 5 | p"  "3.0"       # base truncated, keeps its scale
test "2 3.9 5 | p"  "3"         # exponent truncated
test "2 3 5.0 | p"  "3.0"
test "2 3 5.5 | p"  "2.5"       # fractional modulus participates
test "2.5 3 5.00 | p"  "3.00"
test "2 99999999999999999999999999 5 | p"  "3"    # huge exponent
test "2 1000000000000000000000 97 | p"  "61"
test "2 3 5 | f"  "3"
test "1 2 3 4 | p"  "0"         # pops 4,3,2: 2^3 mod 4 = 0
test "[2 3 5 | p] x"  "3"
test "5 sa 2 3 la | p"  "3"
test "16 o 255 8 257 | p"  "100"        # 255^8 mod 257 = 256 = 0x100
test_error "2 3 0 |"  "remainder by zero"
test_error "2 _3 5 |"  "exponent must be non-negative"
test_error "|"  "stack underflow"
test_error "1 2 |"  "stack underflow"
test_error_stack "2 3 0 | f"  $'0\n3\n2'  "remainder by zero"
test_error "[a] 2 3 |"  "non-numeric value"

# ===== SHIPPED EXAMPLES =====
# (the dc compliance suite also runs these under GNU dc and requires identical
# output; here they are pinned under rpn alone so they are covered without dc)

# Miller-Rabin: 2^61-1 prime, 561 composite, 2^64+1 composite, 10^9+7 prime,
# 10^9+8 even, 2^89-1 prime, 2047 composite
tests_run=$((tests_run + 1))
r=$(./rpn examples/miller_rabin.rpn </dev/null 2>/dev/null)
[ "$r" = $'1\n0\n0\n1\n0\n1\n0' ] || { failures=$((failures+1)); echo "FAIL miller_rabin.rpn: got '$r'"; }

# Game of Life: 8 generations of a 6-row grid plus a blank separator = 56 lines
tests_run=$((tests_run + 1))
lines=$(./rpn examples/game_of_life.rpn </dev/null 2>/dev/null | wc -l)
row3=$(./rpn examples/game_of_life.rpn </dev/null 2>/dev/null | sed -n 3p)
if [ "$lines" != "56" ] || [ "$row3" != ".#.#......" ]; then
    failures=$((failures+1)); echo "FAIL game_of_life.rpn: lines=$lines row3='$row3'"
fi

# Mandelbrot 40x20 + Julia 50x18 (window cropped to the set at the same zoom),
# plus separators = 40 lines, with the solid interior (runs of X) in both
tests_run=$((tests_run + 1))
lines=$(./rpn examples/mandelbrot.rpn </dev/null 2>/dev/null | wc -l)
solid=$(./rpn examples/mandelbrot.rpn </dev/null 2>/dev/null | grep -c 'XXXXX')
if [ "$lines" != "40" ] || [ "$solid" -lt "2" ]; then
    failures=$((failures+1)); echo "FAIL mandelbrot.rpn: lines=$lines solid_rows=$solid"
fi

# ===== SUMMARY =====

if [ "$failures" -eq 0 ]; then
    echo -e "All $tests_run tests passed\n"
    exit 0
else
    echo -e "$failures of $tests_run tests failed\n"
    exit 1
fi
