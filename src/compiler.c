#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "compiler.h"
#include "scanner.h"
#include "util/LocalVarArray.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

typedef struct {
    Token current;
    Token previous;
    bool hadError;
    bool panicMode;
} Parser;

typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT, // =
    PREC_OR, // or
    PREC_AND, // and
    PREC_EQUALITY, // == !=
    PREC_COMPARISON, // < > <= >=
    PREC_TERM, // + -
    PREC_FACTOR, // * /
    PREC_UNARY, // ! -
    PREC_CALL, // . ()
    PREC_PRIMARY
} Precedence;

typedef void (*ParseFn)(bool canAssign);

typedef struct {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;

typedef struct {
    Table identifiers;
    LocalVarArray identifierProps;
    uint32_t identifiersCount;

    Table localNames;
    LocalVarArray localDepths;

    int scopeDepth;
} Compiler;

Parser parser;
Compiler* current = NULL;
Chunk* compilingChunk;

static Chunk* currentChunk()
{
    return compilingChunk;
}

static void errorAt(Token* token, const char* message)
{
    if (parser.panicMode) {
        return;
    }
    parser.panicMode = true;
    fprintf(stderr, "[line %d] Error", token->line);

    if (token->type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    } else if (token->type == TOKEN_ERROR) {
    } else {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);
    parser.hadError = true;
}

static void error(const char* message)
{
    errorAt(&parser.previous, message);
}

static void errorAtCurrent(const char* message)
{
    errorAt(&parser.current, message);
}

static void advance()
{
    parser.previous = parser.current;

    for (;;) {
        parser.current = scanToken();
        if (parser.current.type != TOKEN_ERROR) {
            break;
        }

        errorAtCurrent(parser.current.start);
    }
}

static void consume(TokenType type, const char* message)
{
    if (parser.current.type == type) {
        advance();
        return;
    }

    errorAtCurrent(message);
}

static bool check(TokenType type)
{
    return parser.current.type == type;
}

static bool match(TokenType type)
{
    if (!check(type)) {
        return false;
    }
    advance();
    return true;
}

static void emitByte(uint8_t byte)
{
    writeChunk(currentChunk(), byte, parser.previous.line);
}

/*
static void emitBytes(uint8_t byte1, uint8_t byte2)
{
    emitByte(byte1);
    emitByte(byte2);
}
    */

static void emitLoop(int loopStart)
{
    emitByte(OP_LOOP);

    int offset = currentChunk()->count - loopStart + 2;
    if (offset > UINT16_MAX)
        error("Loop body too large.");

    emitByte((offset >> 8) & 0xff);
    emitByte(offset & 0xff);
}


static int emitJump(uint8_t instruction)
{
    emitByte(instruction);
    emitByte(0xFF);
    emitByte(0xFF);

    return currentChunk()->count - 2;
}

static void emitReturn()
{
    emitByte(OP_RETURN);
}

static uint32_t makeConstant(Value value)
{
    uint32_t addrerss = addConstant(currentChunk(), value);
    return addrerss;
}

static void emitConstant(uint32_t addrerss, int line, OpCode opCodeShort, OpCode opCodeLong)
{
    if (addrerss > 0xFF) {
        uint8_t idx1 = addrerss & 0xFF;
        uint8_t idx2 = (addrerss >> 8) & 0xFF;
        uint8_t idx3 = (addrerss >> 16) & 0xFF;

        writeChunk(currentChunk(), opCodeLong, line);
        writeChunk(currentChunk(), idx3, line);
        writeChunk(currentChunk(), idx2, line);
        writeChunk(currentChunk(), idx1, line);
    } else {
        writeChunk(currentChunk(), opCodeShort, line);
        writeChunk(currentChunk(), addrerss, line);
    }
}

static void patchJump(int offset)
{
    // -2 to adjust for the bytecode for the jump offset itself.
    int jump = currentChunk()->count - offset - 2;
    if (jump > UINT16_MAX) {
        error("Too much code to jump over.");
    }
    currentChunk()->code[offset] = (jump >> 8) & 0xFF;
    currentChunk()->code[offset + 1] = jump & 0xFF;
}

static void initCompiler(Compiler* compiler)
{
    initTable(&compiler->identifiers);
    initLocalVarArray(&compiler->identifierProps);
    compiler->identifiersCount = 0;

    initTable(&compiler->localNames);
    initLocalVarArray(&compiler->localDepths);

    compiler->scopeDepth = 0;
    current = compiler;
}

static void freeCompiler(Compiler* compiler)
{
    freeTable(&compiler->identifiers);
    freeLocalVarArray(&compiler->identifierProps);

    freeTable(&compiler->localNames);
    freeLocalVarArray(&compiler->localDepths);

    initCompiler(compiler);
}

static void endCompiler()
{
    emitReturn();
#ifdef DEBUG_PRINT_CODE
    if (!parser.hadError) {
        disassembleChunk(currentChunk(), "code");
    }
#endif

    freeCompiler(current);
}

static void beginScope()
{
    current->scopeDepth++;
}

static void endScope()
{
    current->scopeDepth--;

    while (current->localDepths.count > 0
        && current->localDepths.values[current->localDepths.count - 1].depth
            > current->scopeDepth) {

        LocalVar* local = &current->localDepths.values[current->localDepths.count - 1];
        if (local->shadowAddr != -1) {
            tableSetUint32(&current->localNames, local->identifier, local->shadowAddr);
        } else {
            tableDelete(&current->localNames, local->identifier);
        }
        emitByte(OP_POP);
        current->localDepths.count--;
    }
}

static void expression();
static void statement();
static void declaration();
static ParseRule* getRule(TokenType type);
static void parsePrecedence(Precedence precedence);
static uint32_t identifierConstant(Token* name);
static int resolveLocal(Compiler* compiler, Token* name);
static void and_(bool canAssign);
static void or_(bool canAssign);

static void binary(bool canAssign)
{
    (void)canAssign;
    TokenType operatorType = parser.previous.type;
    ParseRule* rule = getRule(operatorType);
    parsePrecedence((Precedence)(rule->precedence + 1));

    switch (operatorType) {
    case TOKEN_BANG_EQUAL:
        emitByte(OP_NOT_EQUAL);
        break;
    case TOKEN_EQUAL_EQUAL:
        emitByte(OP_EQUAL);
        break;
    case TOKEN_GREATER:
        emitByte(OP_GREATER);
        break;
    case TOKEN_GREATER_EQUAL:
        emitByte(OP_GREATER_EQUAL);
        break;
    case TOKEN_LESS:
        emitByte(OP_LESS);
        break;
    case TOKEN_LESS_EQUAL:
        emitByte(OP_LESS_EQUAL);
        break;
    case TOKEN_PLUS:
        emitByte(OP_ADD);
        break;
    case TOKEN_MINUS:
        emitByte(OP_SUBTRACT);
        break;
    case TOKEN_STAR:
        emitByte(OP_MULTIPLY);
        break;
    case TOKEN_SLASH:
        emitByte(OP_DIVIDE);
        break;
    default:
        return;
    }
}

static void literal(bool canAssign)
{
    (void)canAssign;
    switch (parser.previous.type) {
    case TOKEN_NIL:
        emitByte(OP_NIL);
        break;
    case TOKEN_TRUE:
        emitByte(OP_TRUE);
        break;
    case TOKEN_FALSE:
        emitByte(OP_FALSE);
        break;
    default:
        return;
    }
}

static void grouping(bool canAssign)
{
    (void)canAssign;
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void number(bool canAssign)
{
    (void)canAssign;
    double value = strtod(parser.previous.start, NULL);

    uint32_t addr = makeConstant(NUMBER_VAL(value));
    emitConstant(addr, parser.previous.line, OP_CONSTANT, OP_CONSTANT_LONG);
}

static void or_(bool canAssign)
{
    (void)canAssign;
    int elseJump = emitJump(OP_JUMP_IF_FALSE);
    int endJump = emitJump(OP_JUMP);

    patchJump(elseJump);
    emitByte(OP_POP);

    parsePrecedence(PREC_OR);
    patchJump(endJump);
}

static void string(bool canAssign)
{
    (void)canAssign;
    uint32_t addr
        = makeConstant(OBJ_VAL(copyString(parser.previous.start + 1, parser.previous.length - 2)));
    emitConstant(addr, parser.previous.line, OP_CONSTANT, OP_CONSTANT_LONG);
}

static void namedVariable(Token name, bool canAssign)
{
    OpCode getOp, getOpLong, setOp, setOpLong;

    int addr = resolveLocal(current, &name);
    LocalVar* var = NULL;
    if (addr != -1) {
        var = &current->localDepths.values[addr];

        getOp = OP_GET_LOCAL;
        getOpLong = OP_GET_LOCAL_LONG;
        setOp = OP_SET_LOCAL;
        setOpLong = OP_SET_LOCAL_LONG;
    } else {
        addr = identifierConstant(&name);
        var = &current->identifierProps.values[addr];

        getOp = OP_GET_GLOBAL;
        getOpLong = OP_GET_GLOBAL_LONG;
        setOp = OP_SET_GLOBAL;
        setOpLong = OP_SET_GLOBAL_LONG;
    }

    int line = parser.previous.line;

    if (canAssign && match(TOKEN_EQUAL)) {
        if (var->readonly) {
            error("Can not assign to constant.");
        }
        expression();
        emitConstant(addr, line, setOp, setOpLong);
    } else {
        emitConstant(addr, line, getOp, getOpLong);
    }
}

static void variable(bool canAssign)
{
    namedVariable(parser.previous, canAssign);
}

static void unary(bool canAssign)
{
    (void)canAssign;
    TokenType operatorType = parser.previous.type;

    parsePrecedence(PREC_UNARY);

    switch (operatorType) {
    case TOKEN_BANG:
        emitByte(OP_NOT);
        break;
    case TOKEN_MINUS:
        emitByte(OP_NEGATE);
        break;

    default:
        break;
    }
}

ParseRule rules[] = {
    [TOKEN_LEFT_PAREN] = { grouping, NULL, PREC_NONE },
    [TOKEN_RIGHT_PAREN] = { NULL, NULL, PREC_NONE },
    [TOKEN_LEFT_BRACE] = { NULL, NULL, PREC_NONE },
    [TOKEN_RIGHT_BRACE] = { NULL, NULL, PREC_NONE },
    [TOKEN_COMMA] = { NULL, NULL, PREC_NONE },
    [TOKEN_DOT] = { NULL, NULL, PREC_NONE },
    [TOKEN_MINUS] = { unary, binary, PREC_TERM },
    [TOKEN_PLUS] = { NULL, binary, PREC_TERM },
    [TOKEN_SEMICOLON] = { NULL, NULL, PREC_NONE },
    [TOKEN_SLASH] = { NULL, binary, PREC_FACTOR },
    [TOKEN_STAR] = { NULL, binary, PREC_FACTOR },
    [TOKEN_BANG] = { unary, NULL, PREC_NONE },
    [TOKEN_BANG_EQUAL] = { NULL, binary, PREC_EQUALITY },
    [TOKEN_EQUAL] = { NULL, NULL, PREC_NONE },
    [TOKEN_EQUAL_EQUAL] = { NULL, binary, PREC_EQUALITY },
    [TOKEN_GREATER] = { NULL, binary, PREC_COMPARISON },
    [TOKEN_GREATER_EQUAL] = { NULL, binary, PREC_COMPARISON },
    [TOKEN_LESS] = { NULL, binary, PREC_COMPARISON },
    [TOKEN_LESS_EQUAL] = { NULL, binary, PREC_COMPARISON },
    [TOKEN_IDENTIFIER] = { variable, NULL, PREC_NONE },
    [TOKEN_STRING] = { string, NULL, PREC_NONE },
    [TOKEN_NUMBER] = { number, NULL, PREC_NONE },
    [TOKEN_AND] = { NULL, and_, PREC_AND },
    [TOKEN_CLASS] = { NULL, NULL, PREC_NONE },
    [TOKEN_ELSE] = { NULL, NULL, PREC_NONE },
    [TOKEN_FALSE] = { literal, NULL, PREC_NONE },
    [TOKEN_FOR] = { NULL, NULL, PREC_NONE },
    [TOKEN_FUN] = { NULL, NULL, PREC_NONE },
    [TOKEN_IF] = { NULL, NULL, PREC_NONE },
    [TOKEN_NIL] = { literal, NULL, PREC_NONE },
    [TOKEN_OR] = { NULL, or_, PREC_OR },
    [TOKEN_PRINT] = { NULL, NULL, PREC_NONE },
    [TOKEN_RETURN] = { NULL, NULL, PREC_NONE },
    [TOKEN_SUPER] = { NULL, NULL, PREC_NONE },
    [TOKEN_THIS] = { NULL, NULL, PREC_NONE },
    [TOKEN_TRUE] = { literal, NULL, PREC_NONE },
    [TOKEN_VAR] = { NULL, NULL, PREC_NONE },
    [TOKEN_CONST] = { NULL, NULL, PREC_NONE },
    [TOKEN_WHILE] = { NULL, NULL, PREC_NONE },
    [TOKEN_ERROR] = { NULL, NULL, PREC_NONE },
    [TOKEN_EOF] = { NULL, NULL, PREC_NONE },
};

static void parsePrecedence(Precedence precedence)
{
    advance();
    ParseFn prefixRule = getRule(parser.previous.type)->prefix;
    if (prefixRule == NULL) {
        error("Expect expression.");
        return;
    }

    bool canAssign = precedence <= PREC_ASSIGNMENT;
    prefixRule(canAssign);

    while (precedence <= getRule(parser.current.type)->precedence) {
        advance();
        ParseFn infixRule = getRule(parser.previous.type)->infix;
        infixRule(canAssign);
    }

    if (canAssign && match(TOKEN_EQUAL)) {
        error("Invalid assignment target.");
    }
}

static uint32_t identifierConstant(Token* name)
{
    ObjString* identifier = copyString(name->start, name->length);
    uint32_t addr;
    if (tableGetUint32(&current->identifiers, identifier, &addr)) {
        return addr;
    }

    addr = current->identifiersCount++;
    tableSetUint32(&current->identifiers, identifier, addr);
    writeLocalVarArray(
        &current->identifierProps, (LocalVar) { .identifier = identifier, .readonly = false });

    return addr;
}

static int resolveLocal(Compiler* compiler, Token* name)
{
    ObjString* identifier = copyString(name->start, name->length);
    uint32_t addr;
    if (tableGetUint32(&compiler->localNames, identifier, &addr)) {
        if (compiler->localDepths.values[addr].depth == -1) {
            error("Can't read local variable in its own initializer.");
        }
        return addr;
    }
    return -1;
}

static uint32_t addLocal(Token name)
{
    ObjString* identifier = copyString(name.start, name.length);

    uint32_t addr;
    if (tableGetUint32(&current->localNames, identifier, &addr)) {

        tableSetUint32(&current->localNames, identifier, current->localDepths.count);
        writeLocalVarArray(&current->localDepths,
            (LocalVar) {
                .identifier = identifier, .depth = -1, .readonly = false, .shadowAddr = addr });

    } else {
        tableSetUint32(&current->localNames, identifier, current->localDepths.count);
        writeLocalVarArray(&current->localDepths,
            (LocalVar) {
                .identifier = identifier, .depth = -1, .readonly = false, .shadowAddr = -1 });
        addr = current->localDepths.count - 1;
    }

    return addr;
}

static uint32_t declareVariable()
{
    if (current->scopeDepth == 0)
        return 0;

    Token* name = &parser.previous;
    ObjString* identifier = copyString(name->start, name->length);
    uint32_t addr;
    if (tableGetUint32(&current->localNames, identifier, &addr)) {
        if (current->localDepths.values[addr].depth == current->scopeDepth) {
            error("Already a variable with this name in this scope.");
        }
    }

    addr = addLocal(*name);
    return addr;
}

static uint32_t parseVariable(const char* errorMessage)
{
    consume(TOKEN_IDENTIFIER, errorMessage);

    uint32_t localAddr = declareVariable();
    if (current->scopeDepth > 0) {

        return localAddr;
    }

    return identifierConstant(&parser.previous);
}

static void markInitialized()
{
    current->localDepths.values[current->localDepths.count - 1].depth = current->scopeDepth;
}

static void defineVariable(uint32_t addr, bool readonly)
{
    if (current->scopeDepth > 0) {
        markInitialized();
        current->localDepths.values[addr].readonly = readonly;
        return;
    }
    current->identifierProps.values[addr].readonly = readonly;
    emitConstant(addr, parser.previous.line, OP_DEFINE_GLOBAL, OP_DEFINE_GLOBAL_LONG);
}

static void and_(bool canAssign)
{
    (void)canAssign;
    int endJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);

    parsePrecedence(PREC_AND);

    patchJump(endJump);
}

static ParseRule* getRule(TokenType type)
{
    return &rules[type];
}

static void expression()
{
    parsePrecedence(PREC_ASSIGNMENT);
}

static void block()
{
    while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
        declaration();
    }

    consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

static void varDeclaration(bool readonly)
{
    uint32_t addr = parseVariable("Expect variable name.");

    if (match(TOKEN_EQUAL)) {
        expression();
    } else {
        emitByte(OP_NIL);
    }
    match(TOKEN_SEMICOLON);
    //    consume(TOKEN_SEMICOLON, "Expect ';' after expression.");

    defineVariable(addr, readonly);
}

static void expressionStatement()
{
    expression();
    match(TOKEN_SEMICOLON);
    //    consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
    emitByte(OP_POP);
}

static void forStatement()
{
    beginScope();
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");
    if (match(TOKEN_SEMICOLON)) {
        // no initializer.
    } else if (match(TOKEN_VAR)) {
        varDeclaration(false);

    } else {
        expressionStatement();
    }

    int loopStart = currentChunk()->count;
    int exitJump = -1;
    if (!match(TOKEN_SEMICOLON)) {
        expression();
        consume(TOKEN_SEMICOLON, "Expect ';' after loop condition.");

        // Jump out of the loop, when condition is false.
        exitJump = emitJump(OP_JUMP_IF_FALSE);
        emitByte(OP_POP); // Condition
    }

    if (!match(TOKEN_RIGHT_PAREN)) {
        int bodyJump = emitJump(OP_JUMP);
        int incrementStart = currentChunk()->count;
        expression();
        emitByte(OP_POP);
        consume(TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");

        emitLoop(loopStart);
        loopStart = incrementStart;
        patchJump(bodyJump);
    }


    statement();
    emitLoop(loopStart);

    if (exitJump != -1) {
        patchJump(exitJump);
        emitByte(OP_POP); // Condition
    }
    endScope();
}

static void ifStatement()
{
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    int thenJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);
    statement();

    int elseJump = emitJump(OP_JUMP);
    patchJump(thenJump);
    emitByte(OP_POP);

    if (match(TOKEN_ELSE)) {
        statement();
    }
    patchJump(elseJump);
}

static void printStatement()
{
    expression();
    match(TOKEN_SEMICOLON);
    //    consume(TOKEN_SEMICOLON, "Expected ; after value.");
    emitByte(OP_PRINT);
}

static void whileStatement()
{
    int loopStart = currentChunk()->count;

    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    int exitJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);
    statement();

    emitLoop(loopStart);

    patchJump(exitJump);
    emitByte(OP_POP);
}

static void synchronize()
{
    parser.panicMode = false;

    while (parser.current.type != TOKEN_EOF) {
        if (parser.previous.type == TOKEN_SEMICOLON) {
            return;
        }
        switch (parser.current.type) {
        case TOKEN_CLASS:
        case TOKEN_FUN:
        case TOKEN_VAR:
        case TOKEN_CONST:
        case TOKEN_FOR:
        case TOKEN_IF:
        case TOKEN_WHILE:
        case TOKEN_PRINT:
        case TOKEN_RETURN:
            return;
        default:;
        }
        advance();
    }
}

static void declaration()
{
    if (match(TOKEN_VAR)) {
        varDeclaration(false);
    } else if (match(TOKEN_CONST)) {
        varDeclaration(true);
    } else {
        statement();
    }

    if (parser.panicMode)
        synchronize();
}

static void statement()
{
    if (match(TOKEN_PRINT)) {
        printStatement();
    } else if (match(TOKEN_FOR)) {
        forStatement();
    } else if (match(TOKEN_IF)) {
        ifStatement();
    } else if (match(TOKEN_WHILE)) {
        whileStatement();
    } else if (match(TOKEN_LEFT_BRACE)) {
        beginScope();
        block();
        endScope();
    } else {
        expressionStatement();
    }
}

bool compile(const char* source, Chunk* chunk)
{
    initScanner(source);
    Compiler compiler;
    initCompiler(&compiler);
    compilingChunk = chunk;


    parser.hadError = false;
    parser.panicMode = false;

    advance();

    while (!match(TOKEN_EOF)) {
        declaration();
    }

    endCompiler();
    return !parser.hadError;
}