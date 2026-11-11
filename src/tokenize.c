/**
MIT License

Copyright (c) 2016-2026 Pierre Faivre

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

/* The tokenizer is the pure lexing layer of the parser: it turns an input
 * line into tokens without touching the stack, registers or any calculator
 * state. Errors are returned as status codes; the caller reports them. */

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "number.h"
#include "tokenize.h"

static bool tokenize_continues(const char *token, size_t len, char c) {
    // A digit char (0-9 or hex letter A-F) continues a number (possibly
    // negative: _ prefix, or after a .)
    if (number_digit_value(c) >= 0)
        return len > 0 && (token[len-1] == '_' || token[len-1] == '.'
                           || number_digit_value(token[len-1]) >= 0);

    // A '.' continues a number token that does not contain one yet: 1.5, .5
    // and 1. are single tokens, and a second '.' starts a new token
    // (1..2 is read as 1. then .2)
    if (c == '.') {
        if (strchr(token, '.') != NULL)
            return false;
        for (size_t i = 0; i < len; i++)
            if (token[i] != '_' && number_digit_value(token[i]) < 0)
                return false;
        return true;
    }

    // The two-character conditionals !>, !< and !=
    if ((c == '>' || c == '<' || c == '=') && len == 1 && token[0] == '!')
        return true;

    return false;
}

TokenResult tokenize_first_token(char *token, char *str, char **next, bool *is_string) {
    size_t i = 0;

    *is_string = false;
    if (next)
        *next = str;

    if (!token || !str)
        return TOKEN_END;

    // Ignore leading spaces, tabs and new lines
    while (*str == ' ' || *str == '\t' || *str == '\n')
        str++;

    // End of input: clear the token so the caller's loop stops
    if (*str == '\0') {
        token[0] = '\0';
        return TOKEN_END;
    }

    // String literal: copy everything up to the matching ']', keeping inner
    // bracket pairs (nesting is allowed: [[a]b] is the string "[a]b")
    if (*str == '[') {
        int depth = 1;
        char *p = str + 1;

        token[0] = '\0';
        while (*p != '\0') {
            if (*p == ']') {
                if (--depth == 0) {
                    p++;
                    break;
                }
            } else if (*p == '[') {
                depth++;
            }
            token[i++] = *p++;
            token[i] = '\0';
        }
        if (depth != 0) {
            token[0] = '\0';
            return TOKEN_UNTERMINATED;
        }
        *is_string = true;
        if (next)
            *next = p;
        return TOKEN_OK;
    }

    token[i] = '\0';

    // Copy token chars until the next one or end of string
    while (*str != ' ' && *str != '\t' && *str != '\n' && *str != '\0') {
        // Stop when the next char does not belong to the current token
        if (i > 0 && !tokenize_continues(token, i, *str))
            break;
        token[i++] = *str++;
        token[i] = '\0';
    }

    if (next)
        *next = str;
    return TOKEN_OK;
}

int tokenize_register_name(char **ptr) {
    while (**ptr == ' ' || **ptr == '\t' || **ptr == '\n')
        (*ptr)++;
    char c = **ptr;
    if (c == '\0' || c == '#' || c == '[')
        return -1;  // no name: the caller reports the error
    (*ptr)++;
    return (unsigned char) c;
}

const char *tokenize_conditional_op(const char *token) {
    // >, < and = are always single-char tokens (the tokenizer splits after
    // any non-digit)
    if (strchr("><=", token[0]) != NULL)
        return token;
    // !>, !< and != are the only two-char tokens starting with !
    if (token[0] == '!' && token[1] != '\0' && token[2] == '\0'
        && strchr("><=", token[1]) != NULL)
        return token;
    return NULL;
}
