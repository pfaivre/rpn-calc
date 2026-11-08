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

#ifndef TOKENIZE_H
#define TOKENIZE_H

#include <stdbool.h>
#include <stddef.h>

/**
 * Whether c continues the token being built
 * @param token Token built so far
 * @param len Length of token
 * @param c The next character
 * @return true when c belongs to the token being built
 */
bool tokenize_continues(const char *token, size_t len, char c);

/**
 * Extract the first token from str into the token buffer
 * @param token An allocated string to store the token (a string literal is
 * stored without its outer brackets)
 * @param str String to search the element on (operator, operand or command)
 * @param is_string Set to true when the token is a [...] string literal
 * @return Pointer to str after the extracted token, or NULL at end of input
 * or on an unterminated string literal
 */
char *tokenize_first_token(char *token, char *str, bool *is_string);

/**
 * Fetch the register name following a register command letter or a
 * conditional operator: the single character after any whitespace, as in dc
 * (`sA2` names register A and leaves the 2 to be parsed as a number). A
 * comment or a string literal is never a name: a '#' char means the rest of
 * the line is a comment, so the command is left without a name.
 * @param ptr Input position, advanced past the name character
 * @return The register name, or -1 when there is none (error reported)
 */
int tokenize_register_name(char **ptr);

/**
 * Whether a token is a comparison conditional operator (>, <, =, !>, !<, !=)
 * @param token Token to inspect
 * @return Pointer to the operator within token, or NULL when not a conditional
 */
const char *tokenize_conditional_op(const char *token);

#endif
