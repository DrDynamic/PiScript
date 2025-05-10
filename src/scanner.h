
#ifndef CLOX_SCANNER_H
#define CLOX_SCANNER_H

typedef enum {
    // Single-character tokens.
    TOKEN_LEFT_PAREN,
    TOKEN_RIGHT_PAREN,
    TOKEN_LEFT_BRACE,
    TOKEN_RIGHT_BRACE,
    TOKEN_LEFT_BRACKET,
    TOKEN_RIGHT_BRACKET,
    TOKEN_COMMA,
    TOKEN_DOT,
    TOKEN_MINUS,
    TOKEN_PLUS,
    TOKEN_SEMICOLON,
    TOKEN_SLASH,
    TOKEN_STAR,
    // One or two character tokens.
    TOKEN_BANG,
    TOKEN_BANG_EQUAL,
    TOKEN_EQUAL,
    TOKEN_EQUAL_EQUAL,
    TOKEN_GREATER,
    TOKEN_GREATER_EQUAL,
    TOKEN_LESS,
    TOKEN_LESS_EQUAL,
    // Literals.
    TOKEN_IDENTIFIER,
    TOKEN_STRING,
    TOKEN_NUMBER,
    // Keywords.
    TOKEN_AND,
    TOKEN_CLASS,
    TOKEN_ELSE,
    TOKEN_FALSE,
    TOKEN_FOR,
    TOKEN_FUN,
    TOKEN_IF,
    TOKEN_NIL, // TODO: rename to TOKEN_NULL
    TOKEN_OR,
    TOKEN_PRINT,
    TOKEN_RETURN,
    TOKEN_REQUIRE,
    TOKEN_SUPER,
    TOKEN_THIS,
    TOKEN_TRUE,
    TOKEN_VAR,
    TOKEN_CONST,
    TOKEN_WHILE,

    TOKEN_SYNTHETIC,
    TOKEN_ERROR,
    TOKEN_EOF
} TokenType;

typedef struct {
    TokenType type;
    const char* start;
    int length;
    int line;
} Token;

typedef struct sObjModule ObjModule;
typedef struct sVM VM;


typedef struct {
    VM* vm;

    // the current module
    ObjModule* module;

    // sourcecode of current module
    const char* source;
    // start of the current token in source
    const char* tokenStart;
    // the current character in source
    const char* currentChar;
    // the current line
    int currentLine;

    // the current token
    Token current;
    // the last token
    Token previous;
    // whether the lexer or compiler found an error in source
    bool hadError;
    // stop priniting errors to prevent error cascades
    bool panicMode;
} Parser;

void initScanner(Parser* parser, const char* source);
Token scanToken(Parser* parser);

#endif