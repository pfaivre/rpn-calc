#!/bin/bash

function test ()
{
    local expression=$1
    local expected_result=$2

    result=$(echo "$expression" | ./rpn)

    if [ "$result" != "$expected_result" ]; then
        echo "Test $expression failed: expected '$expected_result', got '$result'"
        exit 1
    fi
}

function test_error ()
{
    local expression=$1
    local expected_error=$2

    error_result=$(echo "$expression" | ./rpn 2>&1 >/dev/null)
    
    if [[ "$error_result" != *"$expected_error"* ]]; then
        echo "Error test $expression failed: expected '$expected_error', got '$error_result'"
        exit 1
    fi
}

function test_stack ()
{
    local expression=$1
    local expected_values=("${@:2}")  # Array of expected stack values

    result=$(echo "$expression" | ./rpn)
    expected_output=$(IFS=$'\n'; echo "${expected_values[*]}")
    
    if [ "$result" != "$expected_output" ]; then
        echo "Stack test $expression failed"
        echo "Expected:"
        echo "$expected_output"
        echo "Got:"
        echo "$result"
        exit 1
    fi
}


# 1+1=2
test "1 1 + f"  "2"

# 1+(-2)=-1
test "1 _2 + f"  "-1"

# 5-1=4
test "5 1 - f"  "4"

# 8*4=32
test "8 4 * f"  "32"

# 29/3=9
test "29 3 / f"  "9"

# 29%3=2
test "29 3 % f"  "2"

# 12^3=2
test "12 3 ^ f"  "1728"

#   ______
# \/456789 = 675
test "456789 v f"  "675"

#     _____________
#    /(12 + (-3)^4)
#   / ------------- -22  = -20
# \/       11
test "12 _3 4 ^ + 11 / v 22 - f"  "-20"

# 9 223 372 036 854 775 807 * 1000 = 9 223 372 036 854 775 807 000
test "9223372036854775807 1000 * f"  "9223372036854775807000"

# 3+3=6
test "3 d + f"  "6"

# 2-5=6
test "5 2 r - f"  "-3"

# ===== NEW TESTS FOR ~ OPERATOR =====

# Basic ~ operator: 5/2 = 2 remainder 1
test "5 2 ~ p"  "1"  # Top of stack should be remainder (1)
test "5 2 ~ r p"  "2"  # After swap, should be quotient (2)

# ~ operator with different numbers: 17/3 = 5 remainder 2
test "17 3 ~ p"  "2"  # Remainder should be on top
test "17 3 ~ r p"  "5"  # Quotient after swap

# ~ operator with negative numbers: -10/3 = -3 remainder -1
test "_10 3 ~ p"  "-1"  # Remainder
test "_10 3 ~ r p"  "-3"  # Quotient

# ~ operator in complex expression
test "10 3 ~ + p"  "4"  # 10/3 gives 3,1; 3+1=4
test "20 3 ~ 2 * + p"  "10"  # 20/3=6,2; stack=[6,2]; 2*2=4; stack=[6,4]; 6+4=10

# Test both quotient and remainder are pushed
test_stack "5 2 ~ f" "1" "2"

# ===== ERROR CONDITION TESTS =====

# Division by zero
test_error "5 0 /"  "divide by zero"
test_error "10 0 ~"  "divide by zero"

# Stack underflow
test_error "1 +"  "stack empty"
test_error "1 ~"  "stack empty"
test_error "1 %"  "stack empty"

# ===== EDGE CASE TESTS =====

# Zero division
test "0 5 / p"  "0"
test_stack "0 5 ~ f"  "0" "0"  # 0/5 = 0 remainder 0

# One divided by one
test "1 1 / p"  "1"
test "1 1 ~ p"  "0"  # Remainder should be 0

# Large numbers
test "1000000000000 7 ~ + p"  "142857142858"  # Large division

# ===== STACK OPERATION TESTS =====

# Clear stack
test "5 c 3 p"  "3"

# ===== COMPLEX EXPRESSION TESTS =====

# Multi-step calculation with ~
test "10 3 ~ 2 * + p"  "5"  # 10/3=3,1; stack=[1,3] (remainder on top); 1*2=2; stack=[2,3]; 2+3=5

# Mixed operations
test "20 3 ~ 4 * 5 + p"  "13"  # 20/3=6,2; stack=[2,6]; 2*4=8; stack=[8,6]; 8+5=13

# Sum 1 to 10
test "1 2 3 4 5 6 7 8 9 10 + + + + + + + + + f"  "55"

echo "All tests passed"
exit 0
