#ifndef EXPR_H
#define EXPR_H

#include <cglm/cglm.h>

typedef enum {
    // Values
    TOKEN_LITERAL,
    TOKEN_PI,
    TOKEN_E,
    TOKEN_X,
    TOKEN_Y,
    TOKEN_Z,
    // Binary Operators
    TOKEN_ADD,
    TOKEN_SUBTRACT,
    TOKEN_MULTIPLY,
    TOKEN_DIVIDE,
    TOKEN_FLOOR_DIVIDE,
    TOKEN_MODULO,
    TOKEN_EXPONENTIATE,
    // Unary Operators
    TOKEN_NEGATE,
    // Functions
    TOKEN_ABS,
    TOKEN_MIN,
    TOKEN_MAX,
    TOKEN_FLOOR,
    TOKEN_SIN,
    TOKEN_COS,
    TOKEN_TAN,
    TOKEN_ASIN,
    TOKEN_ACOS,
    TOKEN_ATAN,
    TOKEN_ATAN2,
    TOKEN_LN,
    TOKEN_LOG,
    TOKEN_SQRT,
    TOKEN_NROOT,
    TOKEN_NOISE,
    // Brackets
    TOKEN_LBRACKET,
    TOKEN_RBRACKET,
    // Delimiter
    TOKEN_COMMA,
    // Start / End
    TOKEN_START,
    TOKEN_END
} TokenType;

typedef struct {
    TokenType type;
    float value;  // Undefined unless type == TOKEN_LITERAL
} Token;

/// Parses an expression, returns true if successful, false otherwise.
bool parseExpression(char *str, Token *out, size_t outSize, char *errMsg);
/// Performs a false run of a parsed expression to check for correct stack usage.
bool validateExpression(Token *expr, char *errMsg);
/// Evaluates an expression at a point in 3D space.
float evaluateExpression(Token *expr, vec3 point);

#endif
