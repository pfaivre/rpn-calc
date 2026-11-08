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
test_e "2" -e "1 1" -e "+ p"   # multiple -e are concatenated
test_e "2" --expression="1 1 + p"
test_contains "rpn 0.1" -V
test_contains "rpn 0.1" --version
test_contains "Usage:" -h
test_contains "Usage:" --help
test_exit 0 -h
test_exit 0 -V
test_exit 1 -z   # unknown option

# ===== ERROR CONDITIONS =====
# All error messages go to stderr and make the process exit non-zero;
# the session itself keeps running, so later lines are still evaluated

# Unimplemented command
test_error "z"  "unimplemented"
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
test_exit 1 -e "z"
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

# Errors
test_error "sa"  "stack underflow"
test_error "Sa"  "stack underflow"
test_error "La"  "stack register 'a' is empty"
test_error "5 s"  "expected a single-character register name"
test_error "5 s [a]"  "expected a single-character register name"

# Arrays (: stores, ; loads): top of stack is the index, missing loads 0
test "42 3 :a 3 ;a p"  "42"
test "3 ;a p"  "0"
test "7 0 :a 0 ;a p"  "7"
test "1 0 :a 2 0 :a 0 ;a p"  "2"      # storing again overwrites
test "[hi] 2 :a 2 ;a p"  "hi"         # arrays hold strings
test "42 3 :a 5 sa 3 ;a p la p"  $'42\n5'  # a register's stack and array are independent
# Array indices are arbitrary-precision (dc 1.5.2 punts on these)
test "7 100000000000000000000 :a 100000000000000000000 ;a p"  "7"

# The index must be a nonnegative integer; on error the operands stay
test_error_stack "1 [x] :a f"  $'x\n1'  "array index must be a nonnegative integer"
test_error_stack "[x] ;a f"  "x"  "array index must be a nonnegative integer"
test_error_stack "5 _2 :a f"  $'-2\n5'  "array index must be a nonnegative integer"
test_error "5 :a"  "stack underflow"
test_error ";a"  "stack underflow"

# Compact notation
test "5salap"  "5"

# ===== SUMMARY =====

if [ "$failures" -eq 0 ]; then
    echo "All $tests_run tests passed"
    exit 0
else
    echo "$failures of $tests_run tests failed"
    exit 1
fi
