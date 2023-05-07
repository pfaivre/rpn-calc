#!/bin/bash

# 1+1=2
result=$(echo "1 1 + p" | ./rpn)

if [ $result -ne "2" ]; then
    echo "Test 1 1 + p failed"
    exit 1
fi

# 1+(-2)=-1
result=$(echo "1 _2 + p" | ./rpn)

if [ $result -ne "-1" ]; then
    echo "Test 1 _2 + p failed"
    exit 1
fi

#     ____________
#    /(12 + (-3)^4)
#   / ----------- = -20
# \/      11
result=$(echo "12 _3 4 ^ + 11 / v 22 - p" | ./rpn)

if [ $result -ne "-20" ]; then
    echo "Test 12 _3 4 ^ + 11 / v 22 - p failed"
    exit 1
fi

echo "All tests passed"
exit 0
