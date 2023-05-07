#!/bin/bash

function test ()
{
    local expression=$1
    local expected_result=$2

    result=$(echo "$expression" | ./rpn)

    if [ $result != $expected_result ]; then
        echo "Test $expression failed"
        exit 1
    fi
}


# 1+1=2
test "1 1 + p"  "2"

# 1+(-2)=-1
test "1 _2 + p"  "-1"

# 5-1=4
test "5 1 - p"  "4"

# 8*4=32
test "8 4 * p"  "32"

# 29/3=9
test "29 3 / p"  "9"

# 29%3=2
test "29 3 % p"  "2"

# 12^3=2
test "12 3 ^ p"  "1728"

#   ______
# \/456789 = 675
test "456789 v p"  "675"

#     _____________
#    /(12 + (-3)^4)
#   / ------------- -22  = -20
# \/       11
test "12 _3 4 ^ + 11 / v 22 - p"  "-20"

# 9 223 372 036 854 775 807 * 1000 = 9 223 372 036 854 775 807 000
test "9223372036854775807 1000 * p"  "9223372036854775807000"

# 3+3=6
test "3 d + p"  "6"

# 2-5=6
test "5 2 r - p"  "-3"

echo "All tests passed"
exit 0
