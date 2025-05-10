#include <stdio.h>
#include <string.h>

#include "common.h"
#include "scanner.h"


void initScanner(Parser* parser, const char* source)
{
    parser->source = source;
    parser->tokenStart = source;
    parser->currentChar = source;
    parser->currentLine = 1;

#ifdef DEBUG_PRINT_TOKENS
    int line = -1;
    for (;;) {
        Token token = scanToken();
        if (token.line != line) {
            printf("%4d ", token.line);
            line = token.line;
        } else {
            printf("   | ");
        }
        printf("%2d '%.*s'\n", token.type, token.length, token.start);

        if (token.type == TOKEN_EOF)
            break;
    }

    parser->source = source;
    parser->tokenStart = source;
    parser->currentChar = source;
    parser->currentLine = 1;
#endif
}

static bool isAlpha(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static bool isDigit(char c)
{
    return c >= '0' && c <= '9';
}

static bool isAtEnd(Parser* parser)
{
    return *parser->currentChar == '\0';
}

static char advance(Parser* parser)
{
    parser->currentChar++;
    return parser->currentChar[-1];
}

static char peek(Parser* parser)
{
    return *parser->currentChar;
}

static char peekNext(Parser* parser)
{
    if (isAtEnd(parser))
        return '\0';
    return parser->currentChar[1];
}

static bool match(Parser* parser, char expected)
{
    if (isAtEnd(parser))
        return false;
    if (*parser->currentChar != expected)
        return false;
    parser->currentChar++;
    return true;
}


static Token makeToken(Parser* parser, TokenType type)
{
    Token token;
    token.type = type;
    token.start = parser->tokenStart;
    token.length = (int)(parser->currentChar - parser->tokenStart);
    token.line = parser->currentLine;
    return token;
}

static Token errorToken(Parser* parser, const char* message)
{
    Token token;
    token.type = TOKEN_ERROR;
    token.start = message;
    token.length = (int)strlen(message);
    token.line = parser->currentLine;
    return token;
}

static void skipWhitespaces(Parser* parser)
{
    for (;;) {
        char c = peek(parser);
        switch (c) {
        case ' ':
        case '\r':
        case '\t':
            advance(parser);
            break;
        case '\n':
            parser->currentLine++;
            advance(parser);
            break;
        case '/':
            // TODO: add multiline comments
            if (peekNext(parser) == '/') {
                while (peek(parser) != '\n' && !isAtEnd(parser)) {
                    advance(parser);
                }
            } else {
                return;
            }
            break;
        default:
            return;
        }
    }
}

static TokenType checkKeyword(
    Parser* parser, int start, int length, const char* rest, TokenType type)
{
    if (parser->currentChar - parser->tokenStart == start + length
        && memcmp(parser->tokenStart + start, rest, length) == 0) {
        return type;
    }
    return TOKEN_IDENTIFIER;
}

static TokenType identifierType(Parser* parser)
{
    switch (parser->tokenStart[0]) {
    case 'a':
        return checkKeyword(parser, 1, 2, "nd", TOKEN_AND);
    case 'c':
        if (parser->currentChar - parser->tokenStart > 1) {
            switch (parser->tokenStart[1]) {
            case 'l':
                return checkKeyword(parser, 2, 3, "ass", TOKEN_CLASS);
            case 'o':
                return checkKeyword(parser, 2, 3, "nst", TOKEN_CONST);
            }
        }
        break;
    case 'e':
        return checkKeyword(parser, 1, 3, "lse", TOKEN_ELSE);
    case 'f':
        if (parser->currentChar - parser->tokenStart > 1) {
            switch (parser->tokenStart[1]) {
            case 'a':
                return checkKeyword(parser, 2, 3, "lse", TOKEN_FALSE);
            case 'o':
                return checkKeyword(parser, 2, 1, "r", TOKEN_FOR);
            case 'u':
                return checkKeyword(parser, 2, 1, "n", TOKEN_FUN);
            }
        }
        break;
    case 'i':
        return checkKeyword(parser, 1, 1, "f", TOKEN_IF);
    case 'n':
        return checkKeyword(parser, 1, 2, "il", TOKEN_NIL);
    case 'o':
        return checkKeyword(parser, 1, 1, "r", TOKEN_OR);
    case 'p':
        return checkKeyword(parser, 1, 4, "rint", TOKEN_PRINT);
    case 'r':
        return checkKeyword(parser, 1, 5, "eturn", TOKEN_RETURN);
    case 's':
        return checkKeyword(parser, 1, 4, "uper", TOKEN_SUPER);
    case 't':
        if (parser->currentChar - parser->tokenStart > 1) {
            switch (parser->tokenStart[1]) {
            case 'h':
                return checkKeyword(parser, 2, 2, "is", TOKEN_THIS);
            case 'r':
                return checkKeyword(parser, 2, 2, "ue", TOKEN_TRUE);
            }
        }
        break;
    case 'v':
        return checkKeyword(parser, 1, 2, "ar", TOKEN_VAR);
    case 'w':
        return checkKeyword(parser, 1, 4, "hile", TOKEN_WHILE);
    }
    return TOKEN_IDENTIFIER;
}

static Token identifier(Parser* parser)
{
    while (isAlpha(peek(parser)) || isDigit(peek(parser))) {
        advance(parser);
    }
    return makeToken(parser, identifierType(parser));
}

static Token number(Parser* parser)
{
    while (isDigit(peek(parser))) {
        advance(parser);
    }
    if (peek(parser) == '.' && isDigit(peekNext(parser))) {
        // Consume the '.'
        advance(parser);
        while (isDigit(peek(parser))) {
            advance(parser);
        }
    }
    return makeToken(parser, TOKEN_NUMBER);
}

static Token string(Parser* parser)
{
    while (peek(parser) != '"' && !isAtEnd(parser)) {
        if (peek(parser) == '\n') {
            parser->currentLine++;
        }
        advance(parser);
    }

    if (isAtEnd(parser)) {
        return errorToken(parser, "Unterminated string.");
    }
    // consume the closing quote
    advance(parser);
    return makeToken(parser, TOKEN_STRING);
}

Token scanToken(Parser* parser)
{
    skipWhitespaces(parser);
    parser->tokenStart = parser->currentChar;
    if (isAtEnd(parser))
        return makeToken(parser, TOKEN_EOF);

    char c = advance(parser);

    if (isAlpha(c))
        return identifier(parser);

    // allow other number formats 0xFF / .99 / o777 / etc.
    if (isDigit(c))
        return number(parser);
    switch (c) {
    case '(':
        return makeToken(parser, TOKEN_LEFT_PAREN);
    case ')':
        return makeToken(parser, TOKEN_RIGHT_PAREN);
    case '{':
        return makeToken(parser, TOKEN_LEFT_BRACE);
    case '}':
        return makeToken(parser, TOKEN_RIGHT_BRACE);
    case '[':
        return makeToken(parser, TOKEN_LEFT_BRACKET);
    case ']':
        return makeToken(parser, TOKEN_RIGHT_BRACKET);
    case ';':
        return makeToken(parser, TOKEN_SEMICOLON);
    case ',':
        return makeToken(parser, TOKEN_COMMA);
    case '.':
        return makeToken(parser, TOKEN_DOT);
    case '-':
        return makeToken(parser, TOKEN_MINUS);
    case '+':
        return makeToken(parser, TOKEN_PLUS);
    case '/':
        return makeToken(parser, TOKEN_SLASH);
    case '*':
        return makeToken(parser, TOKEN_STAR);
    case '!':
        return makeToken(parser, match(parser, '=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
    case '=':
        return makeToken(parser, match(parser, '=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
    case '<':
        return makeToken(parser, match(parser, '=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
    case '>':
        return makeToken(parser, match(parser, '=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
    case '"': // TODO: support single quote strings / template strings
        return string(parser);
    }

    return errorToken(parser, "Unexpected character.");
}
