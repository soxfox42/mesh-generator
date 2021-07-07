#include "expr.h"

#include <string.h>
#include <cglm/cglm.h>
#include <noise1234.h>

#define TOKEN(TYPE) \
    (Token) { TOKEN_##TYPE, 0.0 }
#define LITERAL(VALUE) \
    (Token) { TOKEN_LITERAL, VALUE }

#define EVAL_STACK_SIZE 64

typedef enum {
    CLASS_VALUE,
    CLASS_BINARY_OP,
    CLASS_UNARY_OP,
    CLASS_FUNCTION,
    CLASS_LBRACKET,
    CLASS_RBRACKET,
    CLASS_DELIMITER,
    CLASS_START,
    CLASS_END
} TokenClass;

typedef enum { ASSOC_LEFT, ASSOC_RIGHT } Assoc;

char *getClassName(TokenClass class) {
    switch (class) {
        case CLASS_VALUE:
            return "a value";
        case CLASS_BINARY_OP:
            return "an operator";
        case CLASS_UNARY_OP:
            return "an unary operator";
        case CLASS_FUNCTION:
            return "a function name";
        case CLASS_LBRACKET:
            return "a left bracket";
        case CLASS_RBRACKET:
            return "a right bracket";
        case CLASS_DELIMITER:
            return "a comma";
        default:
            return "an invalid token";
    }
}

TokenClass getTokenClass(Token token) {
    // C enums are ordered, so this compares token class by ranges.
    if (token.type >= TOKEN_LITERAL && token.type <= TOKEN_Z) {
        return CLASS_VALUE;
    } else if (token.type >= TOKEN_ADD && token.type <= TOKEN_EXPONENTIATE) {
        return CLASS_BINARY_OP;
    } else if (token.type == TOKEN_NEGATE) {
        return CLASS_UNARY_OP;
    } else if (token.type >= TOKEN_ABS && token.type <= TOKEN_NOISE) {
        return CLASS_FUNCTION;
    } else if (token.type == TOKEN_COMMA) {
        return CLASS_DELIMITER;
    } else if (token.type == TOKEN_LBRACKET) {
        return CLASS_LBRACKET;
    } else if (token.type == TOKEN_RBRACKET) {
        return CLASS_RBRACKET;
    } else if (token.type == TOKEN_START) {
        return CLASS_START;
    } else {
        return CLASS_END;
    }
}

int getOperatorPrecedence(Token token) {
    switch (token.type) {
        case TOKEN_ADD:
        case TOKEN_SUBTRACT:
            return 1;
        case TOKEN_MULTIPLY:
        case TOKEN_DIVIDE:
        case TOKEN_FLOOR_DIVIDE:
        case TOKEN_MODULO:
            return 2;
        // These two are at the same level (and right-associative) so that
        // expressions such as -x^-y are parsed correctly as -(x^(-y))
        case TOKEN_EXPONENTIATE:
        case TOKEN_NEGATE:
            return 3;
        default:
            return -1;
    }
}

Assoc getOperatorAssoc(Token token) {
    switch (token.type) {
        case TOKEN_EXPONENTIATE:
        case TOKEN_NEGATE:
            return ASSOC_RIGHT;
        default:
            return ASSOC_LEFT;
    }
}

int getTokenStackEffect(Token token) {
    TokenClass class = getTokenClass(token);
    if (class == CLASS_BINARY_OP) return -1;
    if (class == CLASS_UNARY_OP) return 0;
    if (class == CLASS_VALUE) return 1;
    switch (token.type) {
        case TOKEN_MIN:
        case TOKEN_MAX:
        case TOKEN_ATAN2:
        case TOKEN_LOG:
        case TOKEN_NROOT:
            return -1;
        case TOKEN_NOISE:
            return -2;
        default:
            return 0;
    }
}

bool isStartToken(Token token) {
    TokenClass class = getTokenClass(token);
    return class == CLASS_BINARY_OP || class == CLASS_UNARY_OP ||
           class == CLASS_START || class == CLASS_LBRACKET ||
           class == CLASS_DELIMITER;
}

/// Returns the next character from the cursor, and updates the cursor.
char getNextChar(char **cursor) {
    char nextChar = **cursor;
    *cursor += 1;
    return nextChar;
}

bool checkToken(char **cursor, char *token) {
    size_t len = strlen(token);
    if (strncmp(*cursor, token, len) == 0) {
        *cursor += len;
        return true;
    }
    return false;
}

/// Finds the first token at the start of the string pointed to by cursor,
/// and updates cursor to point to the next character.
Token getTokenRaw(char **cursor, Token previousToken) {
    // Skip whitespace and commas at the start of the string.
    size_t skip = strspn(*cursor, " \n\t");
    *cursor += skip;

    // If it is a valid next token, check for a floating point literal, and
    // return early if found.
    if (isStartToken(previousToken)) {
        char *float_str_end;
        float value = strtof(*cursor, &float_str_end);
        if (*cursor != float_str_end) {
            *cursor = float_str_end;
            return LITERAL(value);
        }
    }

    if (checkToken(cursor, "pi")) return TOKEN(PI);
    if (checkToken(cursor, "e")) return TOKEN(E);
    if (checkToken(cursor, "abs")) return TOKEN(ABS);
    if (checkToken(cursor, "min")) return TOKEN(MIN);
    if (checkToken(cursor, "max")) return TOKEN(MAX);
    if (checkToken(cursor, "floor")) return TOKEN(FLOOR);
    if (checkToken(cursor, "sin")) return TOKEN(SIN);
    if (checkToken(cursor, "cos")) return TOKEN(COS);
    if (checkToken(cursor, "tan")) return TOKEN(TAN);
    if (checkToken(cursor, "asin")) return TOKEN(ASIN);
    if (checkToken(cursor, "acos")) return TOKEN(ACOS);
    if (checkToken(cursor, "atan2")) return TOKEN(ATAN2);
    if (checkToken(cursor, "atan")) return TOKEN(ATAN);
    if (checkToken(cursor, "ln")) return TOKEN(LN);
    if (checkToken(cursor, "log")) return TOKEN(LOG);
    if (checkToken(cursor, "sqrt")) return TOKEN(SQRT);
    if (checkToken(cursor, "nroot")) return TOKEN(NROOT);
    if (checkToken(cursor, "noise")) return TOKEN(NOISE);

    switch (getNextChar(cursor)) {
        case 'x':
            return TOKEN(X);
        case 'y':
            return TOKEN(Y);
        case 'z':
            return TOKEN(Z);
        case '+':
            return TOKEN(ADD);
        case '-':
            if (isStartToken(previousToken))
                return TOKEN(NEGATE);
            else
                return TOKEN(SUBTRACT);
        case '*':
            return TOKEN(MULTIPLY);
        case '/':
            if (**cursor == '/') {
                (*cursor)++;
                return TOKEN(FLOOR_DIVIDE);
            } else {
                return TOKEN(DIVIDE);
            }
        case '^':
            return TOKEN(EXPONENTIATE);
        case '%':
            return TOKEN(MODULO);
        case '(':
            return TOKEN(LBRACKET);
        case ')':
            return TOKEN(RBRACKET);
        case ',':
            return TOKEN(COMMA);
    }
    return TOKEN(END);
}

void formatError(char *errMsg, TokenClass previousClass,
                 TokenClass currentClass) {
    if (previousClass == CLASS_START) {
        sprintf(errMsg, "Error: expression must not start with %s.",
                getClassName(currentClass));
    } else if (currentClass == CLASS_END) {
        sprintf(errMsg, "Error: expression must not end with %s.",
                getClassName(previousClass));
    } else if (previousClass == CLASS_FUNCTION) {
        strcpy(errMsg,
               "Error: a function name must be followed by a left bracket.");
    } else {
        sprintf(errMsg, "Error: %s must not be followed by %s.",
                getClassName(previousClass), getClassName(currentClass));
    }
}

/// Gets the next token, moving cursor forwards, and performs error checking.
/// Returns false if an error occurred in the token stream, and sets errMsg to
/// a descriptive error message.
bool getToken(char **cursor, Token previousToken, Token *outToken,
              char *errMsg) {
    *outToken = getTokenRaw(cursor, previousToken);
    TokenClass previousClass = getTokenClass(previousToken);
    TokenClass currentClass = getTokenClass(*outToken);

    // Only some pairings of token classes are valid, so check that then create
    // a suitable error message.
    switch (previousClass) {
        case CLASS_VALUE:
        case CLASS_RBRACKET:
            if (currentClass == CLASS_VALUE || currentClass == CLASS_UNARY_OP ||
                currentClass == CLASS_FUNCTION ||
                currentClass == CLASS_LBRACKET) {
                formatError(errMsg, previousClass, currentClass);
                return false;
            }
            break;
        case CLASS_BINARY_OP:
        case CLASS_UNARY_OP:
        case CLASS_LBRACKET:
        case CLASS_DELIMITER:
        case CLASS_START:
            if (currentClass == CLASS_BINARY_OP ||
                currentClass == CLASS_RBRACKET ||
                currentClass == CLASS_DELIMITER || currentClass == CLASS_END) {
                if (!(previousClass == CLASS_START && currentClass == CLASS_END))
                    formatError(errMsg, previousClass, currentClass);
                else
                    strcpy(errMsg, "Error: Must enter an expression.");
                return false;
            }
            break;
        case CLASS_FUNCTION:
            if (currentClass != CLASS_LBRACKET) {
                formatError(errMsg, previousClass, currentClass);
                return false;
            }
            break;
        default:
            // If we got here, something caused us to parse over the end.
            // This indicates a major problem, so treat this as a fatal error.
            fprintf(stderr, "Fatal error in expression parser. Exiting...\n");
            exit(EXIT_FAILURE);
    }
    return true;
}

bool parseExpression(char *str, Token *out, size_t outSize, char *errMsg) {
    char *cursor = str;
    Token currentToken, previousToken = TOKEN(START);
    Token operators[outSize];
    size_t outIndex = 0, operatorIndex = 0;
    do {
        if (!getToken(&cursor, previousToken, &currentToken, errMsg)) {
            return false;
        }
        TokenClass class = getTokenClass(currentToken);
        if (class == CLASS_VALUE) {
            out[outIndex++] = currentToken;
        } else if (class == CLASS_LBRACKET || class == CLASS_FUNCTION) {
            operators[operatorIndex++] = currentToken;
        } else if (class == CLASS_BINARY_OP || class == CLASS_UNARY_OP) {
            int precedence = getOperatorPrecedence(currentToken);
            // As long as there are tokens on top of the stack, try moving them
            // to output.
            while (operatorIndex > 0) {
                Token top = operators[operatorIndex - 1];
                // Brackets should be considered as the current bottom of the
                // stack.
                if (top.type == TOKEN_LBRACKET) break;
                int topPrecedence = getOperatorPrecedence(top);
                Assoc topAssoc = getOperatorAssoc(top);
                // Higher precedence operators, and left-associative equal
                // precedence operators can be moved.
                if (topPrecedence > precedence ||
                    (topPrecedence == precedence && topAssoc == ASSOC_LEFT)) {
                    out[outIndex++] = top;
                    operatorIndex--;
                } else {
                    // Otherwise, terminate the loop.
                    break;
                }
            }
            operators[operatorIndex++] = currentToken;
        } else if (class == CLASS_RBRACKET) {
            // Move operators to output until the matching bracket is found.
            bool foundLeftBracket = false;
            while (operatorIndex > 0) {
                Token top = operators[--operatorIndex];
                TokenClass topClass = getTokenClass(top);
                // If the top operator is a left bracket, check for a possible
                // function, then stop moving tokens.
                if (topClass == CLASS_LBRACKET) {
                    foundLeftBracket = true;
                    top = operators[operatorIndex - 1];
                    topClass = getTokenClass(top);
                    if (topClass == CLASS_FUNCTION) {
                        out[outIndex++] = top;
                        operatorIndex--;
                    }
                    break;
                }
                out[outIndex++] = top;
            }
            if (!foundLeftBracket) {
                strcpy(errMsg, "Error: mismatched brackets");
                return false;
            }
        } else if (class == CLASS_DELIMITER) {
            while (operatorIndex > 0) {
                Token top = operators[operatorIndex - 1];
                // Brackets should be considered as the current bottom of the
                // stack.
                if (top.type == TOKEN_LBRACKET) break;
                out[outIndex++] = top;
                operatorIndex--;
            }
        }
        previousToken = currentToken;
    } while (currentToken.type != TOKEN_END);
    while (operatorIndex > 0) {
        if (operators[operatorIndex - 1].type == TOKEN_LBRACKET) {
            strcpy(errMsg, "Error: mismatched brackets");
            return false;
        }
        out[outIndex++] = operators[--operatorIndex];
    }
    out[outIndex] = TOKEN(END);
    return true;
}

bool validateExpression(Token *expr, char *errMsg) {
    int rpnIndex = 0;

    Token *currentToken = expr;
    while (currentToken->type != TOKEN_END) {
        rpnIndex += getTokenStackEffect(*currentToken);
        if (rpnIndex < 0 || rpnIndex > EVAL_STACK_SIZE) {
            strcpy(errMsg, "Error: invalid expression");
            return false;
        }
        currentToken++;
    }
    if (rpnIndex != 1) {
        strcpy(errMsg, "Error: invalid expression");
        return false;
    }
    return true;
}

void pushStack(float *rpnStack, size_t *rpnIndex, float value) {
    // Set the top of the stack to value, then increment the index.
    rpnStack[(*rpnIndex)++] = value;
}

float popStack(float *rpnStack, size_t *rpnIndex) {
    // Decrement the index, then return the top value of the stack
    return rpnStack[--(*rpnIndex)];
}

float evaluateExpression(Token *expr, vec3 point) {
    // Hard limit on equation complexity, but this should be above pretty much
    // any normal usage.
    float rpnStack[EVAL_STACK_SIZE];
    size_t rpnIndex = 0;

    Token *currentToken = expr;
    while (currentToken->type != TOKEN_END) {
        switch (currentToken->type) {
            case TOKEN_LITERAL:
                pushStack(rpnStack, &rpnIndex, currentToken->value);
                break;
            case TOKEN_PI:
                pushStack(rpnStack, &rpnIndex, M_PI);
                break;
            case TOKEN_E:
                pushStack(rpnStack, &rpnIndex, M_E);
                break;
            case TOKEN_X:
                pushStack(rpnStack, &rpnIndex, point[0]);
                break;
            case TOKEN_Y:
                pushStack(rpnStack, &rpnIndex, point[1]);
                break;
            case TOKEN_Z:
                pushStack(rpnStack, &rpnIndex, point[2]);
                break;
            case TOKEN_ADD: {
                float b = popStack(rpnStack, &rpnIndex);
                float a = popStack(rpnStack, &rpnIndex);
                pushStack(rpnStack, &rpnIndex, a + b);
                break;
            }
            case TOKEN_SUBTRACT: {
                float b = popStack(rpnStack, &rpnIndex);
                float a = popStack(rpnStack, &rpnIndex);
                pushStack(rpnStack, &rpnIndex, a - b);
                break;
            }
            case TOKEN_MULTIPLY: {
                float b = popStack(rpnStack, &rpnIndex);
                float a = popStack(rpnStack, &rpnIndex);
                pushStack(rpnStack, &rpnIndex, a * b);
                break;
            }
            case TOKEN_DIVIDE: {
                float b = popStack(rpnStack, &rpnIndex);
                float a = popStack(rpnStack, &rpnIndex);
                pushStack(rpnStack, &rpnIndex, a / b);
                break;
            }
            case TOKEN_FLOOR_DIVIDE: {
                float b = popStack(rpnStack, &rpnIndex);
                float a = popStack(rpnStack, &rpnIndex);
                pushStack(rpnStack, &rpnIndex, floor(a / b));
                break;
            }
            case TOKEN_MODULO: {
                float b = popStack(rpnStack, &rpnIndex);
                float a = popStack(rpnStack, &rpnIndex);
                pushStack(rpnStack, &rpnIndex, remainder(a, b));
                break;
            }
            case TOKEN_EXPONENTIATE: {
                float b = popStack(rpnStack, &rpnIndex);
                float a = popStack(rpnStack, &rpnIndex);
                pushStack(rpnStack, &rpnIndex, pow(a, b));
                break;
            }
            case TOKEN_NEGATE:
                rpnStack[rpnIndex - 1] *= -1;
                break;
            case TOKEN_ABS:
                rpnStack[rpnIndex - 1] = fabs(rpnStack[rpnIndex - 1]);
                break;
            case TOKEN_MIN: {
                float b = popStack(rpnStack, &rpnIndex);
                float a = popStack(rpnStack, &rpnIndex);
                pushStack(rpnStack, &rpnIndex, fmin(a, b));
                break;
            }
            case TOKEN_MAX: {
                float b = popStack(rpnStack, &rpnIndex);
                float a = popStack(rpnStack, &rpnIndex);
                pushStack(rpnStack, &rpnIndex, fmax(a, b));
                break;
            }
            case TOKEN_FLOOR:
                rpnStack[rpnIndex - 1] = floor(rpnStack[rpnIndex - 1]);
                break;
            case TOKEN_SIN:
                rpnStack[rpnIndex - 1] = sin(rpnStack[rpnIndex - 1]);
                break;
            case TOKEN_COS:
                rpnStack[rpnIndex - 1] = cos(rpnStack[rpnIndex - 1]);
                break;
            case TOKEN_TAN:
                rpnStack[rpnIndex - 1] = tan(rpnStack[rpnIndex - 1]);
                break;
            case TOKEN_ASIN:
                rpnStack[rpnIndex - 1] = asin(rpnStack[rpnIndex - 1]);
                break;
            case TOKEN_ACOS:
                rpnStack[rpnIndex - 1] = acos(rpnStack[rpnIndex - 1]);
                break;
            case TOKEN_ATAN:
                rpnStack[rpnIndex - 1] = atan(rpnStack[rpnIndex - 1]);
                break;
            case TOKEN_ATAN2: {
                float x = popStack(rpnStack, &rpnIndex);
                float y = popStack(rpnStack, &rpnIndex);
                pushStack(rpnStack, &rpnIndex, atan2(y, x));
                break;
            }
            case TOKEN_LN:
                rpnStack[rpnIndex - 1] = log(rpnStack[rpnIndex - 1]);
                break;
            case TOKEN_LOG: {
                float x = popStack(rpnStack, &rpnIndex);
                float base = popStack(rpnStack, &rpnIndex);
                pushStack(rpnStack, &rpnIndex, log(x) / log(base));
                break;
            }
            case TOKEN_SQRT:
                rpnStack[rpnIndex - 1] = sqrt(rpnStack[rpnIndex - 1]);
                break;
            case TOKEN_NROOT: {
                float x = popStack(rpnStack, &rpnIndex);
                float n = popStack(rpnStack, &rpnIndex);
                pushStack(rpnStack, &rpnIndex, pow(x, 1 / n));
                break;
            }
            case TOKEN_NOISE: {
                float x = popStack(rpnStack, &rpnIndex);
                float y = popStack(rpnStack, &rpnIndex);
                float z = popStack(rpnStack, &rpnIndex);
                pushStack(rpnStack, &rpnIndex, noise3(x, y, z));
                break;
            }
            default:
                // These tokens should never appear in a parsed expression,
                // don't need to handle them.
                break;
        }
        currentToken++;
    }
    return popStack(rpnStack, &rpnIndex);
}