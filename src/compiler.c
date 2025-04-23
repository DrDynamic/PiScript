#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "compiler.h"
#include "scanner.h"
#include "util/VarArray.h"
#include "util/memory.h"

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

typedef enum {
    TYPE_FUNCTION,
    TYPE_INITIALIZER,
    TYPE_METHOD,
    TYPE_SCRIPT,
} FunctionType;

typedef struct {
    uint8_t index;
    bool isLocal;
    Var* varProps;
} Upvalue;


typedef struct Compiler {
    struct Compiler* enclosing;
    ObjFunction* function;
    FunctionType type;

    Table localNames;
    VarArray localProps;

    Upvalue upvalues[UINT8_COUNT];

    int scopeDepth;
} Compiler;

typedef struct ClassCompiler {
    struct ClassCompiler* enclosing;
} ClassCompiler;


Parser parser;
Compiler* current = NULL;
ClassCompiler* currentClass = NULL;
static Chunk* currentChunk()
{
    return &current->function->chunk;
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


static void emitBytes(uint8_t byte1, uint8_t byte2)
{
    emitByte(byte1);
    emitByte(byte2);
}


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
    if (current->type == TYPE_INITIALIZER) {
        emitBytes(OP_GET_LOCAL, 0);
    } else {
        emitByte(OP_NIL);
    }
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

static void initCompiler(Compiler* compiler, FunctionType type)
{
    compiler->enclosing = current;
    compiler->function = NULL;
    compiler->type = type;

    initTable(&compiler->localNames);
    initVarArray(&compiler->localProps);

    compiler->scopeDepth = 0;
    compiler->function = newFunction();
    current = compiler;

    if (type != TYPE_SCRIPT) {
        current->function->name = copyString(parser.previous.start, parser.previous.length);
    }

    if (type != TYPE_FUNCTION) {
        // if (type == TYPE_METHOD) {
        ObjString* identifier = copyString("this", 4);
        push(OBJ_VAL(identifier));
        writeVarArray(&current->localProps,
            (Var) {
                .depth = 0,
                .identifier = identifier,
                .shadowAddr = -1,
                .readonly = true,
                .isCaptured = false,
            });
        tableSetUint32(&current->localNames, identifier, 0);
        pop();
    } else {
        writeVarArray(&current->localProps,
            (Var) {
                .depth = -1,
                .identifier = NULL,
                .shadowAddr = -1,
                .readonly = true,
                .isCaptured = false,
            });
    }
}

static void freeCompiler(Compiler* compiler)
{
    freeTable(&compiler->localNames);
    freeVarArray(&compiler->localProps);

    //    initCompiler(compiler, TYPE_SCRIPT);
}

static ObjFunction* endCompiler()
{
    emitReturn();
    ObjFunction* function = current->function;
#ifdef DEBUG_PRINT_CODE
    if (!parser.hadError) {
        disassembleChunk(
            currentChunk(), function->name != NULL ? function->name->chars : "<script>");
    }
#endif
    Compiler* enclosing = current->enclosing;
    freeCompiler(current);
    current = enclosing;
    return function;
}

static void beginScope()
{
    current->scopeDepth++;
}

static void endScope()
{
    current->scopeDepth--;

    while (current->localProps.count > 0
        && current->localProps.values[current->localProps.count - 1].depth > current->scopeDepth) {

        Var* local = &current->localProps.values[current->localProps.count - 1];
        if (local->shadowAddr != -1) {
            tableSetUint32(&current->localNames, local->identifier, local->shadowAddr);
        } else {
            tableDelete(&current->localNames, local->identifier);
        }

        if (current->localProps.values[current->localProps.count - 1].isCaptured) {
            emitByte(OP_CLOSE_UPVALUE);
        } else {
            emitByte(OP_POP);
        }
        current->localProps.count--;
    }
}

static int resolveUpvalue(Compiler* compiler, Token* name, Var** varProps);
static void expression();
static void statement();
static void declaration();
static ParseRule* getRule(TokenType type);
static void parsePrecedence(Precedence precedence);
static uint32_t identifierConstant(Token* name);
static uint32_t firstOrMakeGlobal(Token* name);
static int resolveLocal(Compiler* compiler, Token* name);
static void and_(bool canAssign);
static void or_(bool canAssign);
static uint8_t argumentList();

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

static void call(bool canAssign)
{
    (void)canAssign;
    uint8_t argCount = argumentList();
    emitByte(OP_CALL);
    emitByte(argCount);
}

static void dot(bool canAssign)
{
    consume(TOKEN_IDENTIFIER, "Expect property after '.'.");
    uint32_t addr = identifierConstant(&parser.previous);

    if (canAssign && match(TOKEN_EQUAL)) {
        expression();
        emitConstant(addr, parser.previous.line, OP_SET_PROPERTY, OP_SET_PROPERTY_LONG);
    } else if (match(TOKEN_LEFT_PAREN)) {
        uint8_t argCount = argumentList();
        emitConstant(addr, parser.previous.line, OP_INVOKE, OP_INVOKE_LONG);
        emitByte(argCount);
    } else {
        emitConstant(addr, parser.previous.line, OP_GET_PROPERTY, OP_GET_PROPERTY_LONG);
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
    Var* var = NULL;
    if (addr != -1) {
        var = &current->localProps.values[addr];

        getOp = OP_GET_LOCAL;
        getOpLong = OP_GET_LOCAL_LONG;
        setOp = OP_SET_LOCAL;
        setOpLong = OP_SET_LOCAL_LONG;
    } else if ((addr = resolveUpvalue(current, &name, &var)) != -1) {
        getOp = OP_GET_UPVALUE;
        getOpLong = 0xFF; // not supported
        setOp = OP_SET_UPVALUE;
        setOpLong = 0xFF; // not supported
    } else {
        addr = firstOrMakeGlobal(&name);
        var = &vm.globalProps.values[addr];

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

static void this_(bool canAssign)
{
    (void)canAssign;

    if (currentClass == NULL) {
        error("Can't use 'this' outside of a class.");
        return;
    }
    variable(false);
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
    [TOKEN_LEFT_PAREN] = { grouping, call, PREC_CALL },
    [TOKEN_RIGHT_PAREN] = { NULL, NULL, PREC_NONE },
    [TOKEN_LEFT_BRACE] = { NULL, NULL, PREC_NONE },
    [TOKEN_RIGHT_BRACE] = { NULL, NULL, PREC_NONE },
    [TOKEN_COMMA] = { NULL, NULL, PREC_NONE },
    [TOKEN_DOT] = { NULL, dot, PREC_CALL },
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
    [TOKEN_THIS] = { this_, NULL, PREC_NONE },
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
    return makeConstant(OBJ_VAL(identifier));
}

static uint32_t firstOrMakeGlobal(Token* name)
{
    ObjString* identifier = copyString(name->start, name->length);
    uint32_t addr;
    if (tableGetUint32(&vm.globalAddresses, identifier, &addr)) {
        return addr;
    }

    addr = vm.globalCount++;
    tableSetUint32(&vm.globalAddresses, identifier, addr);
    writeVarArray(&vm.globalProps, (Var) { .identifier = identifier, .readonly = false });

    return addr;
}

static int resolveLocal(Compiler* compiler, Token* name)
{
    ObjString* identifier = copyString(name->start, name->length);
    uint32_t addr;
    if (tableGetUint32(&compiler->localNames, identifier, &addr)) {
        if (compiler->localProps.values[addr].depth == -1) {
            error("Can't read local variable in its own initializer.");
        }
        return addr;
    }
    return -1;
}

static int addUpvalue(Compiler* compiler, uint32_t index, bool isLocal, Var* varProps)
{
    int upvalueCount = compiler->function->upvalueCount;

    for (int i = 0; i < upvalueCount; i++) {
        Upvalue* upvalue = &compiler->upvalues[i];
        if (upvalue->index == index && upvalue->isLocal == isLocal) {
            return i;
        }
    }

    if (upvalueCount == UINT8_COUNT) {
        error("Too many closure variables in function.");
        return 0;
    }

    compiler->upvalues[upvalueCount].isLocal = isLocal;
    compiler->upvalues[upvalueCount].index = index;
    compiler->upvalues[upvalueCount].varProps = varProps;
    return compiler->function->upvalueCount++;
}

static int resolveUpvalue(Compiler* compiler, Token* name, Var** varProps)
{
    if (compiler->enclosing == NULL) {
        return -1;
    }

    int local = resolveLocal(compiler->enclosing, name);
    if (local != -1) {
        compiler->enclosing->localProps.values[local].isCaptured = true;
        *varProps = &compiler->localProps.values[local];
        return addUpvalue(compiler, (uint32_t)local, true, *varProps);
    }

    int upvalue = resolveUpvalue(compiler->enclosing, name, varProps);
    if (upvalue != -1) {
        return addUpvalue(compiler, upvalue, false, *varProps);
    }

    return -1;
}

static uint32_t addLocal(Token name)
{
    ObjString* identifier = copyString(name.start, name.length);
    push(OBJ_VAL(identifier));

    uint32_t addr;
    if (tableGetUint32(&current->localNames, identifier, &addr)) {

        tableSetUint32(&current->localNames, identifier, current->localProps.count);
        writeVarArray(&current->localProps,
            (Var) {
                .identifier = identifier,
                .depth = -1,
                .readonly = false,
                .shadowAddr = addr,
                .isCaptured = false,
            });

    } else {
        tableSetUint32(&current->localNames, identifier, current->localProps.count);
        writeVarArray(&current->localProps,
            (Var) {
                .identifier = identifier,
                .depth = -1,
                .readonly = false,
                .shadowAddr = -1,
                .isCaptured = false,
            });
        addr = current->localProps.count - 1;
    }
    pop(); // identifier
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
        if (current->localProps.values[addr].depth == current->scopeDepth) {
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

    return firstOrMakeGlobal(&parser.previous);
}

static void markInitialized()
{
    if (current->scopeDepth == 0) {
        return;
    }
    current->localProps.values[current->localProps.count - 1].depth = current->scopeDepth;
}

static void defineVariable(uint32_t addr, bool readonly)
{
    if (current->scopeDepth > 0) {
        markInitialized();
        current->localProps.values[addr].readonly = readonly;
        return;
    }

    vm.globalProps.values[addr].readonly = readonly;
    emitConstant(addr, parser.previous.line, OP_DEFINE_GLOBAL, OP_DEFINE_GLOBAL_LONG);
}


static uint8_t argumentList()
{
    uint8_t argCount = 0;
    if (!check(TOKEN_RIGHT_PAREN)) {
        do {
            expression();
            if (argCount == 255) {
                error("Can't have more than 255 arguments.");
            }
            argCount++;
        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
    return argCount;
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

static void function(FunctionType type)
{
    Compiler compiler;
    initCompiler(&compiler, type);
    beginScope();

    consume(TOKEN_LEFT_PAREN, "Expect '(' after function name.");
    if (!check(TOKEN_RIGHT_PAREN)) {
        do {
            current->function->arity++;
            if (current->function->arity > 255) {
                errorAtCurrent("Can't have more than 255 parameters.");
            }
            uint8_t constant = parseVariable("Expect parameter name");
            defineVariable(constant, false);
        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
    consume(TOKEN_LEFT_BRACE, "Expect '{' before function body.");
    block();

    ObjFunction* function = endCompiler();
    emitConstant(
        makeConstant(OBJ_VAL(function)), parser.previous.line, OP_CLOSURE, OP_CLOSURE_LONG);

    for (int i = 0; i < function->upvalueCount; i++) {
        emitByte(compiler.upvalues[i].isLocal ? 1 : 0);
        emitByte(compiler.upvalues[i].index);
    }
}

static void method()
{
    consume(TOKEN_IDENTIFIER, "Expect method name.");
    uint32_t constantAddr = identifierConstant(&parser.previous);

    FunctionType type = TYPE_METHOD;
    if (parser.previous.length == 4 && memcmp(parser.previous.start, "init", 4) == 0) {
        type = TYPE_INITIALIZER;
    }
    function(type);

    emitConstant(constantAddr, parser.previous.line, OP_METHOD, OP_METHOD_LONG);
}

static void classDeclaration()
{
    uint32_t varAddr = parseVariable("Expect class name.");
    Token className = parser.previous;
    // TODO: remove name from class declaration?
    uint32_t nameAddr = identifierConstant(&parser.previous);
    emitConstant(nameAddr, parser.previous.line, OP_CLASS, OP_CLASS_LONG);
    defineVariable(varAddr, true);

    ClassCompiler classCompiler;
    classCompiler.enclosing = currentClass;
    currentClass = &classCompiler;

    namedVariable(className, false);

    consume(TOKEN_LEFT_BRACE, "Expect '{' before class body.");
    while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
        method();
    }
    consume(TOKEN_RIGHT_BRACE, "Expect '}' after class body.");
    emitByte(OP_POP);

    currentClass = currentClass->enclosing;
}

static void funDeclaration()
{
    uint8_t addr = parseVariable("Expect function name.");
    markInitialized();
    function(TYPE_FUNCTION);
    defineVariable(addr, true);
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

static void returnStatement()
{
    if (current->type == TYPE_SCRIPT) {
        error("Can't return from top-level code.");
    }

    if (match(TOKEN_SEMICOLON)) {
        emitReturn();
    } else {
        if (current->type == TYPE_INITIALIZER) {
            error("Can't return a value from an initializer.");
        }
        expression();
        consume(TOKEN_SEMICOLON, "Expect ';' after return value.");
        emitByte(OP_RETURN);
    }
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
    if (match(TOKEN_CLASS)) {
        classDeclaration();
    } else if (match(TOKEN_FUN)) {
        funDeclaration();
    } else if (match(TOKEN_VAR)) {
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
    } else if (match(TOKEN_RETURN)) {
        returnStatement();
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

void defineNative(const char* name, NativeFn function)
{
    ObjString* identifier = copyString(name, (int)strlen(name));
    push(OBJ_VAL(identifier));
    ObjNative* native = newNative(function);
    push(OBJ_VAL(native));

    uint32_t addr = vm.globalCount++;
    tableSetUint32(&vm.globalAddresses, identifier, addr);
    writeVarArray(&vm.globalProps, (Var) { .identifier = identifier, .readonly = true });
    writeValueArray(&vm.globals, OBJ_VAL(native));

    pop();
    pop();
}

ObjFunction* compile(const char* source)
{
    initScanner(source);
    Compiler compiler;
    initCompiler(&compiler, TYPE_SCRIPT);

    parser.hadError = false;
    parser.panicMode = false;

    advance();

    while (!match(TOKEN_EOF)) {
        declaration();
    }

    ObjFunction* scriptFunction = endCompiler();
    return parser.hadError ? NULL : scriptFunction;
}

void markCompilerRoots()
{
    Compiler* compiler = current;
    while (compiler != NULL) {
        markObject((Obj*)compiler->function);
        markTable(&compiler->localNames);
        compiler = compiler->enclosing;
    }
}
