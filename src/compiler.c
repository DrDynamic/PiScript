#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "common.h"
#include "compiler.h"
#include "scanner.h"
#include "util/addresstable.h"
#include "util/VarArray.h"
#include "util/memory.h"
#include "util/util.h"

#ifdef DEBUG_PRINT_CODE
#include "util/debug.h"
#endif


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

typedef void (*ParseFn)(Compiler* compiler, bool canAssign);

typedef struct {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;

typedef enum {
    TYPE_FUNCTION,
    TYPE_INITIALIZER,
    TYPE_METHOD,
    TYPE_MODULE,
} FunctionType;

typedef struct {
    uint8_t index;
    bool isLocal;
    Var* varProps;
} Upvalue;

typedef struct sClassCompiler {
    struct sClassCompiler* enclosing;
    bool hasSuperclass;
} ClassCompiler;

typedef struct sCompiler {
    Parser* parser;

    // The compiler for the enclosing function,
    // or NULL when compiling the root function.
    struct sCompiler* enclosing;

    ClassCompiler* currentClass;

    ObjModule* currentModule;

    // The function beeing compiled
    ObjFunction* function;

    // The type of function beeing compiled
    FunctionType type;

    // relations between identifiers and addresses of locals
    AddressTable locals;

    // UpValues captured by the function
    Upvalue upvalues[MAX_UPVALUES];

    int scopeDepth;
} Compiler;

// all modules (added when starting to compile, to detect circular dependencies)
Table modules;
// this is the root of compilation, when compilation didn't start with a file (like in a repl)
ObjModule unnamedModule;

// temporary objects that should not be collected by the garbage collector
Value temps[MAX_COMPILER_TEMPS];
int tempCount = 0;

static Chunk* currentChunk(Compiler* compiler)
{
    return &compiler->function->chunk;
}

static void printError(
    Parser* parser, int line, const char* label, const char* format, va_list args)
{
    (void)parser;
    fprintf(stderr, "[line %d] %s: ", line, label);
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
}

static void errorAt(Parser* parser, Token* token, const char* format, ...)
{
    if (parser->panicMode) {
        return;
    }
    parser->panicMode = true;

    va_list args;
    va_start(args, format);

    if (token->type == TOKEN_ERROR) {
        printError(parser, token->line, "Error", format, args);
    } else if (token->type == TOKEN_EOF) {
        printError(parser, token->line, "Error at end", format, args);
    } else {
        char label[255];
        sprintf(label, "Error at '%.*s'", token->length, token->start);
        printError(parser, token->line, label, format, args);
    }

    parser->hadError = true;
}

static void error(Parser* parser, const char* message)
{
    errorAt(parser, &parser->previous, message);
}

static void errorAtCurrent(Parser* parser, const char* message)
{
    errorAt(parser, &parser->current, message);
}

static void advance(Parser* parser)
{
    parser->previous = parser->current;

    for (;;) {
        parser->current = scanToken(parser);
        if (parser->current.type != TOKEN_ERROR) {
            break;
        }

        errorAtCurrent(parser, parser->current.start);
    }
}

static void consume(Parser* parser, TokenType type, const char* message)
{
    if (parser->current.type == type) {
        advance(parser);
        return;
    }

    errorAtCurrent(parser, message);
}

static bool check(Parser* parser, TokenType type)
{
    return parser->current.type == type;
}

static bool match(Parser* parser, TokenType type)
{
    if (!check(parser, type)) {
        return false;
    }
    advance(parser);
    return true;
}


static void emitByte(Compiler* compiler, uint8_t byte)
{
    writeChunk(currentChunk(compiler), byte, compiler->parser->previous.line);
}


static void emitBytes(Compiler* compiler, uint8_t byte1, uint8_t byte2)
{
    emitByte(compiler, byte1);
    emitByte(compiler, byte2);
}


static void emitLoop(Compiler* compiler, int loopStart)
{
    emitByte(compiler, OP_LOOP);

    int offset = currentChunk(compiler)->count - loopStart + 2;
    if (offset > UINT16_MAX)
        error(compiler->parser, "Loop body too large.");

    emitByte(compiler, (offset >> 8) & 0xff);
    emitByte(compiler, offset & 0xff);
}


static int emitJump(Compiler* compiler, uint8_t instruction)
{
    emitByte(compiler, instruction);
    emitByte(compiler, 0xFF);
    emitByte(compiler, 0xFF);

    return currentChunk(compiler)->count - 2;
}

static void emitReturn(Compiler* compiler)
{
    if (compiler->type == TYPE_INITIALIZER) {
        emitBytes(compiler, OP_GET_LOCAL, 0);
    } else {
        emitByte(compiler, OP_NIL);
    }
    emitByte(compiler, OP_RETURN);
}

static uint32_t makeConstant(Compiler* compiler, Value value)
{
    uint32_t addrerss = addConstant(currentChunk(compiler), value);
    return addrerss;
}

// TODO: is line realy needed or is it always compiler->parser->previous->line
static void emitConstant(
    Compiler* compiler, uint32_t addrerss, int line, OpCode opCodeShort, OpCode opCodeLong)
{
    if (addrerss > 0xFF) {
        uint8_t idx1 = addrerss & 0xFF;
        uint8_t idx2 = (addrerss >> 8) & 0xFF;
        uint8_t idx3 = (addrerss >> 16) & 0xFF;

        writeChunk(currentChunk(compiler), opCodeLong, line);
        writeChunk(currentChunk(compiler), idx3, line);
        writeChunk(currentChunk(compiler), idx2, line);
        writeChunk(currentChunk(compiler), idx1, line);
    } else {
        writeChunk(currentChunk(compiler), opCodeShort, line);
        writeChunk(currentChunk(compiler), addrerss, line);
    }
}

static void patchJump(Compiler* compiler, int offset)
{
    // -2 to adjust for the bytecode for the jump offset itself.
    int jump = currentChunk(compiler)->count - offset - 2;
    if (jump > UINT16_MAX) {
        error(compiler->parser, "Too much code to jump over.");
    }
    currentChunk(compiler)->code[offset] = (jump >> 8) & 0xFF;
    currentChunk(compiler)->code[offset + 1] = jump & 0xFF;
}

static void initCompiler(
    ObjModule* module, Compiler* compiler, Parser* parser, Compiler* parent, FunctionType type)
{
    compiler->parser = parser;
    compiler->enclosing = parent;
    compiler->currentClass = NULL;
    compiler->function = NULL;
    compiler->currentModule = module;
    compiler->type = type;

    if (parent != NULL) {
        compiler->currentClass = parent->currentClass;
    }

    module->currentCompiler = compiler;

    initAddressTable(&compiler->locals);

    compiler->scopeDepth = 0;
    compiler->function = newFunction();
    temps[tempCount++] = OBJ_VAL(compiler->function);

    if (type != TYPE_MODULE) {
        compiler->function->name
            = copyString(compiler->parser->previous.start, compiler->parser->previous.length);
    }

    if (type != TYPE_FUNCTION) {
        // if (type == TYPE_METHOD) {
        ObjString* identifier = copyString("this", 4);
        push(OBJ_VAL(identifier));

        addresstableAdd(&compiler->locals, identifier,
            (Var) {
                .depth = 0,
                .identifier = identifier,
                .shadowAddr = -1,
                .readonly = true,
                .isCaptured = false,
            });

        pop();
    } else {
        ObjString* identifier = copyString("", 0);
        push(OBJ_VAL(identifier));

        addresstableAdd(&compiler->locals, identifier,
            (Var) {
                .depth = -1,
                .identifier = NULL,
                .shadowAddr = -1,
                .readonly = true,
                .isCaptured = false,
            });

        pop();
    }
    tempCount--; // compiler->function
}

static void freeCompiler(Compiler* compiler)
{
    freeAddressTable(&compiler->locals);
}

static ObjFunction* endCompiler(Compiler* compiler)
{
    emitReturn(compiler);
    ObjFunction* function = compiler->function;
#ifdef DEBUG_PRINT_CODE
    if (!compiler->parser->hadError) {
        disassembleChunk(
            currentChunk(compiler), function->name != NULL ? function->name->chars : "<script>");
    }
#endif
    //    Compiler* enclosing = compiler->enclosing;
    //    freeCompiler(compiler);
    compiler = compiler->enclosing;
    return function;
}

static void beginScope(Compiler* compiler)
{
    compiler->scopeDepth++;
}

static void endScope(Compiler* compiler)
{
    compiler->scopeDepth--;

    AddressTable* locals = &compiler->locals;
    while (!addresstableIsEmpty(locals)
        && addresstableGetLastProps(locals)->depth > compiler->scopeDepth) {
        Var* local = addresstableGetLastProps(locals);
        addresstablePop(locals);

        if (local->isCaptured) {
            emitByte(compiler, OP_CLOSE_UPVALUE);
        } else {
            emitByte(compiler, OP_POP);
        }
    }
}

static int resolveUpvalue(Compiler* compiler, Token* name, Var** varProps);
static void expression(Compiler* compiler);
static void statement(Compiler* compiler);
static void declaration(Compiler* compiler);
static ParseRule* getRule(TokenType type);
static void parsePrecedence(Compiler* compiler, Precedence precedence);
static uint32_t identifierConstant(Compiler* compiler, Token* name);
static uint32_t firstOrMakeGlobal(Token* name);
static int resolveLocal(Compiler* compiler, Token* name);
static void and_(Compiler* compiler, bool canAssign);
static void or_(Compiler* compiler, bool canAssign);
static uint8_t argumentList(Compiler* compiler, TokenType endToken);

static void binary(Compiler* compiler, bool canAssign)
{
    (void)canAssign;
    TokenType operatorType = compiler->parser->previous.type;
    ParseRule* rule = getRule(operatorType);
    parsePrecedence(compiler, (Precedence)(rule->precedence + 1));

    switch (operatorType) {
    case TOKEN_BANG_EQUAL:
        emitByte(compiler, OP_NOT_EQUAL);
        break;
    case TOKEN_EQUAL_EQUAL:
        emitByte(compiler, OP_EQUAL);
        break;
    case TOKEN_GREATER:
        emitByte(compiler, OP_GREATER);
        break;
    case TOKEN_GREATER_EQUAL:
        emitByte(compiler, OP_GREATER_EQUAL);
        break;
    case TOKEN_LESS:
        emitByte(compiler, OP_LESS);
        break;
    case TOKEN_LESS_EQUAL:
        emitByte(compiler, OP_LESS_EQUAL);
        break;
    case TOKEN_PLUS:
        emitByte(compiler, OP_ADD);
        break;
    case TOKEN_MINUS:
        emitByte(compiler, OP_SUBTRACT);
        break;
    case TOKEN_STAR:
        emitByte(compiler, OP_MULTIPLY);
        break;
    case TOKEN_SLASH:
        emitByte(compiler, OP_DIVIDE);
        break;
    default:
        return;
    }
}

static void call(Compiler* compiler, bool canAssign)
{
    (void)canAssign;
    uint8_t argCount = argumentList(compiler, TOKEN_RIGHT_PAREN);
    emitByte(compiler, OP_CALL);
    emitByte(compiler, argCount);
}

static void dot(Compiler* compiler, bool canAssign)
{
    consume(compiler->parser, TOKEN_IDENTIFIER, "Expect property name after '.'.");
    uint32_t addr = identifierConstant(compiler, &compiler->parser->previous);

    if (canAssign && match(compiler->parser, TOKEN_EQUAL)) {
        expression(compiler);
        emitConstant(
            compiler, addr, compiler->parser->previous.line, OP_SET_PROPERTY, OP_SET_PROPERTY_LONG);
    } else if (match(compiler->parser, TOKEN_LEFT_PAREN)) {
        uint8_t argCount = argumentList(compiler, TOKEN_RIGHT_PAREN);
        emitConstant(compiler, addr, compiler->parser->previous.line, OP_INVOKE, OP_INVOKE_LONG);
        emitByte(compiler, argCount);
    } else {
        emitConstant(
            compiler, addr, compiler->parser->previous.line, OP_GET_PROPERTY, OP_GET_PROPERTY_LONG);
    }
}

static void bracket(Compiler* compiler, bool canAssign)
{
    if (match(compiler->parser, TOKEN_RIGHT_BRACKET)) {
        // array add
        if (!canAssign) {
            return;
        }

        consume(compiler->parser, TOKEN_EQUAL, "Expect '=' after array add syntax ('[]').");
        expression(compiler);
        emitByte(compiler, OP_ARRAY_ADD);
    } else {
        // property access
        expression(compiler);
        consume(compiler->parser, TOKEN_RIGHT_BRACKET, "Expect ']' after array index.");

        if (match(compiler->parser, TOKEN_EQUAL)) {
            // property set
            expression(compiler);
            emitByte(compiler, OP_SET_PROPERTY_STACK);
        } else {
            // property get
            emitByte(compiler, OP_GET_PROPERTY_STACK);
        }
    }
}

static void literal(Compiler* compiler, bool canAssign)
{
    (void)canAssign;
    switch (compiler->parser->previous.type) {
    case TOKEN_NIL:
        emitByte(compiler, OP_NIL);
        break;
    case TOKEN_TRUE:
        emitByte(compiler, OP_TRUE);
        break;
    case TOKEN_FALSE:
        emitByte(compiler, OP_FALSE);
        break;
    default:
        return;
    }
}

static void grouping(Compiler* compiler, bool canAssign)
{
    (void)canAssign;
    expression(compiler);
    consume(compiler->parser, TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void number(Compiler* compiler, bool canAssign)
{
    (void)canAssign;
    double value = strtod(compiler->parser->previous.start, NULL);

    uint32_t addr = makeConstant(compiler, NUMBER_VAL(value));
    emitConstant(compiler, addr, compiler->parser->previous.line, OP_CONSTANT, OP_CONSTANT_LONG);
}

static void or_(Compiler* compiler, bool canAssign)
{
    (void)canAssign;
    int elseJump = emitJump(compiler, OP_JUMP_IF_FALSE);
    int endJump = emitJump(compiler, OP_JUMP);

    patchJump(compiler, elseJump);
    emitByte(compiler, OP_POP);

    parsePrecedence(compiler, PREC_OR);
    patchJump(compiler, endJump);
}

static void string(Compiler* compiler, bool canAssign)
{
    (void)canAssign;
    uint32_t addr = makeConstant(compiler,
        OBJ_VAL(copyString(
            compiler->parser->previous.start + 1, compiler->parser->previous.length - 2)));
    emitConstant(compiler, addr, compiler->parser->previous.line, OP_CONSTANT, OP_CONSTANT_LONG);
}

static void array(Compiler* compiler, bool canAssign)
{
    (void)canAssign;

    uint8_t argCount = argumentList(compiler, TOKEN_RIGHT_BRACKET);
    emitBytes(compiler, OP_ARRAY_INIT, argCount);
}

static void namedVariable(Compiler* compiler, Token name, bool canAssign)
{
    OpCode getOp, getOpLong, setOp, setOpLong;

    int addr = resolveLocal(compiler, &name);
    Var* var = NULL;
    if (addr != -1) {
        var = addresstableGetProps(&compiler->locals, addr);

        getOp = OP_GET_LOCAL;
        getOpLong = OP_GET_LOCAL_LONG;
        setOp = OP_SET_LOCAL;
        setOpLong = OP_SET_LOCAL_LONG;
    } else if ((addr = resolveUpvalue(compiler, &name, &var)) != -1) {
        getOp = OP_GET_UPVALUE;
        getOpLong = 0xFF; // not supported
        setOp = OP_SET_UPVALUE;
        setOpLong = 0xFF; // not supported
    } else {
        addr = firstOrMakeGlobal(&name);
        var = addresstableGetProps(&vm.gloablsTable, addr);

        getOp = OP_GET_GLOBAL;
        getOpLong = OP_GET_GLOBAL_LONG;
        setOp = OP_SET_GLOBAL;
        setOpLong = OP_SET_GLOBAL_LONG;
    }
    int line = compiler->parser->previous.line;

    if (canAssign && match(compiler->parser, TOKEN_EQUAL)) {
        if (var->readonly) {
            error(compiler->parser, "Can not assign to constant.");
        }
        expression(compiler);
        emitConstant(compiler, addr, line, setOp, setOpLong);
    } else {
        emitConstant(compiler, addr, line, getOp, getOpLong);
    }
}

static void variable(Compiler* compiler, bool canAssign)
{
    namedVariable(compiler, compiler->parser->previous, canAssign);
}

static void require(Compiler* compiler, bool canAssign)
{
    (void)canAssign;
    (void)compiler;

    //    const char* path = current->parser->current.start + 1 uint32_t addr =
    //    makeConstant(OBJ_VAL(
    //       copyString(current->parser->previous.start + 1, current->parser->previous.length -
    //       2)));
    //    char* source = NULL;
    //    char* error = readFile(path, &source);

    //    Parser parser;
    //    Compiler compiler;

    // TODO:
}

static Token syntheticToken(const char* text)
{
    Token token;
    token.start = text;
    token.length = (int)strlen(text);
    token.type = TOKEN_SYNTHETIC;
    token.line = -1;

    return token;
}

static void super_(Compiler* compiler, bool canAssign)
{
    (void)canAssign;

    if (compiler->currentClass == NULL) {
        error(compiler->parser, "Can't use 'super' outside of a class.");
    } else if (!compiler->currentClass->hasSuperclass) {
        error(compiler->parser, "Can't use 'super' in a class with no superclass.");
    }

    consume(compiler->parser, TOKEN_DOT, "Expect '.' after 'super'.");
    consume(compiler->parser, TOKEN_IDENTIFIER, "Expect superclass method name.");
    uint32_t name = identifierConstant(compiler, &compiler->parser->previous);

    namedVariable(compiler, syntheticToken("this"), false);

    if (match(compiler->parser, TOKEN_LEFT_PAREN)) {
        uint8_t argCount = argumentList(compiler, TOKEN_RIGHT_PAREN);
        namedVariable(compiler, syntheticToken("super"), false);
        emitConstant(
            compiler, name, compiler->parser->previous.line, OP_SUPER_INVOKE, OP_SUPER_INVOKE_LONG);
        emitByte(compiler, argCount);
    } else {
        namedVariable(compiler, syntheticToken("super"), false);
        emitConstant(
            compiler, name, compiler->parser->previous.line, OP_GET_SUPER, OP_GET_SUPER_LONG);
    }
}

static void this_(Compiler* compiler, bool canAssign)
{
    (void)canAssign;

    if (compiler->currentClass == NULL) {
        error(compiler->parser, "Can't use 'this' outside of a class.");
        return;
    }
    variable(compiler, false);
}

static void unary(Compiler* compiler, bool canAssign)
{
    (void)canAssign;
    TokenType operatorType = compiler->parser->previous.type;

    parsePrecedence(compiler, PREC_UNARY);

    switch (operatorType) {
    case TOKEN_BANG:
        emitByte(compiler, OP_NOT);
        break;
    case TOKEN_MINUS:
        emitByte(compiler, OP_NEGATE);
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
    [TOKEN_LEFT_BRACKET] = { array, bracket, PREC_CALL },
    [TOKEN_RIGHT_BRACKET] = { NULL, NULL, PREC_NONE },
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
    [TOKEN_REQUIRE] = { require, NULL, PREC_NONE },
    [TOKEN_SUPER] = { super_, NULL, PREC_NONE },
    [TOKEN_THIS] = { this_, NULL, PREC_NONE },
    [TOKEN_TRUE] = { literal, NULL, PREC_NONE },
    [TOKEN_VAR] = { NULL, NULL, PREC_NONE },
    [TOKEN_CONST] = { NULL, NULL, PREC_NONE },
    [TOKEN_WHILE] = { NULL, NULL, PREC_NONE },
    [TOKEN_ERROR] = { NULL, NULL, PREC_NONE },
    [TOKEN_EOF] = { NULL, NULL, PREC_NONE },
};

static void parsePrecedence(Compiler* compiler, Precedence precedence)
{
    advance(compiler->parser);
    ParseFn prefixRule = getRule(compiler->parser->previous.type)->prefix;
    if (prefixRule == NULL) {
        error(compiler->parser, "Expect expression.");
        return;
    }

    bool canAssign = precedence <= PREC_ASSIGNMENT;
    prefixRule(compiler, canAssign);

    while (precedence <= getRule(compiler->parser->current.type)->precedence) {
        advance(compiler->parser);
        ParseFn infixRule = getRule(compiler->parser->previous.type)->infix;
        infixRule(compiler, canAssign);
    }

    if (canAssign && match(compiler->parser, TOKEN_EQUAL)) {
        error(compiler->parser, "Invalid assignment target.");
    }
}

static uint32_t identifierConstant(Compiler* compiler, Token* name)
{
    ObjString* identifier = copyString(name->start, name->length);
    return makeConstant(compiler, OBJ_VAL(identifier));
}

static bool identifiersEqual(Token* a, Token* b)
{
    if (a->length != b->length)
        return false;
    return memcmp(a->start, b->start, a->length) == 0;
}

static uint32_t firstOrMakeGlobal(Token* name)
{
    ObjString* identifier = copyString(name->start, name->length);
    uint32_t addr;
    if (addresstableGetAddress(&vm.gloablsTable, identifier, &addr)) {
        return addr;
    }

    addr = addresstableAdd(&vm.gloablsTable, identifier,
        (Var) {
            .identifier = identifier,
            .readonly = false,
        });

    return addr;
}

static int resolveLocal(Compiler* compiler, Token* name)
{
    ObjString* identifier = copyString(name->start, name->length);
    uint32_t addr;

    if (addresstableGetAddress(&compiler->locals, identifier, &addr)) {
        if (addresstableGetProps(&compiler->locals, addr)->depth == -1) {
            error(compiler->parser, "Can't read local variable in its own initializer.");
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
        error(compiler->parser, "Too many closure variables in function.");
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
        addresstableGetProps(&compiler->enclosing->locals, local)->isCaptured = true;
        *varProps = addresstableGetProps(
            &compiler->enclosing->locals, local); // TODO: check - added enclosing
        return addUpvalue(compiler, (uint32_t)local, true, *varProps);
    }

    int upvalue = resolveUpvalue(compiler->enclosing, name, varProps);
    if (upvalue != -1) {
        return addUpvalue(compiler, upvalue, false, *varProps);
    }

    return -1;
}

static uint32_t addLocal(Compiler* compiler, Token name)
{
    ObjString* identifier = copyString(name.start, name.length);
    push(OBJ_VAL(identifier));

    uint32_t addr = addresstableAdd(&compiler->locals, identifier,
        (Var) {
            .identifier = identifier,
            .depth = -1,
            .readonly = false,
            .shadowAddr = -1,
            .isCaptured = false,
        });
    pop(); // identifier
    return addr;
}

static uint32_t declareVariable(Compiler* compiler)
{
    if (compiler->scopeDepth == 0)
        return 0;

    Token* name = &compiler->parser->previous;
    ObjString* identifier = copyString(name->start, name->length);
    uint32_t addr;
    if (addresstableGetAddress(&compiler->locals, identifier, &addr)) {
        if (addresstableGetProps(&compiler->locals, addr)->depth == compiler->scopeDepth) {
            error(compiler->parser, "Already a variable with this name in this scope.");
        }
    }

    addr = addLocal(compiler, *name);
    return addr;
}

static uint32_t parseVariable(Compiler* compiler, const char* errorMessage)
{
    consume(compiler->parser, TOKEN_IDENTIFIER, errorMessage);

    uint32_t localAddr = declareVariable(compiler);
    if (compiler->scopeDepth > 0) {
        return localAddr;
    }

    return firstOrMakeGlobal(&compiler->parser->previous);
}

static void markInitialized(Compiler* compiler)
{
    if (compiler->scopeDepth == 0) {
        return;
    }
    addresstableGetLastProps(&compiler->locals)->depth = compiler->scopeDepth;
}

static void defineVariable(Compiler* compiler, uint32_t addr, bool readonly)
{
    if (compiler->scopeDepth > 0) {
        markInitialized(compiler);
        addresstableGetProps(&compiler->locals, addr)->readonly = readonly;
        return;
    }

    addresstableGetProps(&vm.gloablsTable, addr)->readonly = readonly;
    emitConstant(
        compiler, addr, compiler->parser->previous.line, OP_DEFINE_GLOBAL, OP_DEFINE_GLOBAL_LONG);
}


static uint8_t argumentList(Compiler* compiler, TokenType endToken)
{
    uint8_t argCount = 0;
    if (!check(compiler->parser, endToken)) {
        do {
            expression(compiler);
            if (argCount == 255) {
                error(compiler->parser, "Can't have more than 255 arguments.");
            }
            argCount++;
        } while (match(compiler->parser, TOKEN_COMMA));
    }
    consume(compiler->parser, endToken, "Expect ')' after arguments.");
    return argCount;
}

static void and_(Compiler* compiler, bool canAssign)
{
    (void)canAssign;
    int endJump = emitJump(compiler, OP_JUMP_IF_FALSE);
    emitByte(compiler, OP_POP);
    parsePrecedence(compiler, PREC_AND);
    patchJump(compiler, endJump);
}

static ParseRule* getRule(TokenType type)
{
    return &rules[type];
}

static void expression(Compiler* compiler)
{
    parsePrecedence(compiler, PREC_ASSIGNMENT);
}

static void block(Compiler* compiler)
{
    while (!check(compiler->parser, TOKEN_RIGHT_BRACE) && !check(compiler->parser, TOKEN_EOF)) {
        declaration(compiler);
    }

    consume(compiler->parser, TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

static void function(Compiler* compiler, FunctionType type)
{
    Compiler funCompiler;
    initCompiler(compiler->currentModule, &funCompiler, compiler->parser, compiler, type);
    beginScope(&funCompiler);

    consume(funCompiler.parser, TOKEN_LEFT_PAREN, "Expect '(' after function name.");
    if (!check(funCompiler.parser, TOKEN_RIGHT_PAREN)) {
        do {
            funCompiler.function->arity++;
            if (funCompiler.function->arity > 255) {
                errorAtCurrent(funCompiler.parser, "Can't have more than 255 parameters.");
            }
            uint8_t constant = parseVariable(&funCompiler, "Expect parameter name");
            defineVariable(&funCompiler, constant, false);
        } while (match(funCompiler.parser, TOKEN_COMMA));
    }
    consume(funCompiler.parser, TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
    consume(funCompiler.parser, TOKEN_LEFT_BRACE, "Expect '{' before function body.");
    block(&funCompiler);

    ObjFunction* function = endCompiler(&funCompiler);
    emitConstant(compiler, makeConstant(compiler, OBJ_VAL(function)),
        compiler->parser->previous.line, OP_CLOSURE, OP_CLOSURE_LONG);

    for (int i = 0; i < function->upvalueCount; i++) {
        emitByte(compiler, funCompiler.upvalues[i].isLocal ? 1 : 0);
        emitByte(compiler, funCompiler.upvalues[i].index);
    }
    freeCompiler(&funCompiler);
}

static void method(Compiler* compiler)
{
    consume(compiler->parser, TOKEN_IDENTIFIER, "Expect method name.");
    // TODO: use addresses instead of strings to address methods
    uint32_t constantAddr = identifierConstant(compiler, &compiler->parser->previous);

    FunctionType type = TYPE_METHOD;
    if (compiler->parser->previous.length == 4
        && memcmp(compiler->parser->previous.start, "init", 4) == 0) {
        type = TYPE_INITIALIZER;
    }
    function(compiler, type);

    emitConstant(
        compiler, constantAddr, compiler->parser->previous.line, OP_METHOD, OP_METHOD_LONG);
}

static void classDeclaration(Compiler* compiler)
{
    uint32_t varAddr = parseVariable(compiler, "Expect class name.");
    Token className = compiler->parser->previous;
    // TODO: remove name from class declaration?
    uint32_t nameAddr = identifierConstant(compiler, &compiler->parser->previous);
    emitConstant(compiler, nameAddr, compiler->parser->previous.line, OP_CLASS, OP_CLASS_LONG);
    defineVariable(compiler, varAddr, true);

    ClassCompiler classCompiler;
    classCompiler.hasSuperclass = false;
    classCompiler.enclosing = compiler->currentClass;
    compiler->currentClass = &classCompiler;

    if (match(compiler->parser, TOKEN_LESS)) {
        consume(compiler->parser, TOKEN_IDENTIFIER, "Expect superclass name.");
        variable(compiler, false);

        if (identifiersEqual(&className, &compiler->parser->previous)) {
            error(compiler->parser, "A class can't inherit from itself.");
        }

        beginScope(compiler);
        addLocal(compiler, syntheticToken("super"));
        defineVariable(compiler, 0, true);

        namedVariable(compiler, className, false);
        emitByte(compiler, OP_INHERIT);
        classCompiler.hasSuperclass = true;
    }

    namedVariable(compiler, className, false);

    consume(compiler->parser, TOKEN_LEFT_BRACE, "Expect '{' before class body.");
    while (!check(compiler->parser, TOKEN_RIGHT_BRACE) && !check(compiler->parser, TOKEN_EOF)) {
        method(compiler);
    }
    consume(compiler->parser, TOKEN_RIGHT_BRACE, "Expect '}' after class body.");
    emitByte(compiler, OP_POP);

    if (classCompiler.hasSuperclass) {
        endScope(compiler);
    }

    compiler->currentClass = compiler->currentClass->enclosing;
}

static void funDeclaration(Compiler* compiler)
{
    uint8_t addr = parseVariable(compiler, "Expect function name.");
    markInitialized(compiler);
    function(compiler, TYPE_FUNCTION);
    defineVariable(compiler, addr, true);
}

static void varDeclaration(Compiler* compiler, bool readonly)
{
    uint32_t addr = parseVariable(compiler, "Expect variable name.");

    if (match(compiler->parser, TOKEN_EQUAL)) {
        expression(compiler);
    } else {
        emitByte(compiler, OP_NIL);
    }
    // match(TOKEN_SEMICOLON);
    consume(compiler->parser, TOKEN_SEMICOLON, "Expect ';' after expression.");

    defineVariable(compiler, addr, readonly);
}

static void expressionStatement(Compiler* compiler)
{
    expression(compiler);
    // match(TOKEN_SEMICOLON);
    consume(compiler->parser, TOKEN_SEMICOLON, "Expect ';' after expression.");
    emitByte(compiler, OP_POP);
}

static void forStatement(Compiler* compiler)
{
    beginScope(compiler);
    consume(compiler->parser, TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");
    if (match(compiler->parser, TOKEN_SEMICOLON)) {
        // no initializer.
    } else if (match(compiler->parser, TOKEN_VAR)) {
        varDeclaration(compiler, false);

    } else {
        expressionStatement(compiler);
    }

    int loopStart = currentChunk(compiler)->count;
    int exitJump = -1;
    if (!match(compiler->parser, TOKEN_SEMICOLON)) {
        expression(compiler);
        consume(compiler->parser, TOKEN_SEMICOLON, "Expect ';' after loop condition.");

        // Jump out of the loop, when condition is false.
        exitJump = emitJump(compiler, OP_JUMP_IF_FALSE);
        emitByte(compiler, OP_POP); // Condition
    }

    if (!match(compiler->parser, TOKEN_RIGHT_PAREN)) {
        int bodyJump = emitJump(compiler, OP_JUMP);
        int incrementStart = currentChunk(compiler)->count;
        expression(compiler);
        emitByte(compiler, OP_POP);
        consume(compiler->parser, TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");

        emitLoop(compiler, loopStart);
        loopStart = incrementStart;
        patchJump(compiler, bodyJump);
    }


    statement(compiler);
    emitLoop(compiler, loopStart);

    if (exitJump != -1) {
        patchJump(compiler, exitJump);
        emitByte(compiler, OP_POP); // Condition
    }
    endScope(compiler);
}

static void ifStatement(Compiler* compiler)
{
    consume(compiler->parser, TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
    expression(compiler);
    consume(compiler->parser, TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    int thenJump = emitJump(compiler, OP_JUMP_IF_FALSE);
    emitByte(compiler, OP_POP);
    statement(compiler);

    int elseJump = emitJump(compiler, OP_JUMP);
    patchJump(compiler, thenJump);
    emitByte(compiler, OP_POP);

    if (match(compiler->parser, TOKEN_ELSE)) {
        statement(compiler);
    }
    patchJump(compiler, elseJump);
}


// TODO: move to native function / module
static void printStatement(Compiler* compiler)
{
    expression(compiler);
    consume(compiler->parser, TOKEN_SEMICOLON, "Expected ; after value.");
    emitByte(compiler, OP_PRINT);
}

static void returnStatement(Compiler* compiler)
{
    if (compiler->type == TYPE_MODULE) {
        error(compiler->parser, "Can't return from top-level code.");
    }

    if (match(compiler->parser, TOKEN_SEMICOLON)) {
        emitReturn(compiler);
    } else {
        if (compiler->type == TYPE_INITIALIZER) {
            error(compiler->parser, "Can't return a value from an initializer.");
        }
        expression(compiler);
        consume(compiler->parser, TOKEN_SEMICOLON, "Expect ';' after return value.");
        emitByte(compiler, OP_RETURN);
    }
}

static void whileStatement(Compiler* compiler)
{
    int loopStart = currentChunk(compiler)->count;

    consume(compiler->parser, TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
    expression(compiler);
    consume(compiler->parser, TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    int exitJump = emitJump(compiler, OP_JUMP_IF_FALSE);
    emitByte(compiler, OP_POP);
    statement(compiler);

    emitLoop(compiler, loopStart);

    patchJump(compiler, exitJump);
    emitByte(compiler, OP_POP);
}

static void synchronize(Compiler* compiler)
{
    Parser* parser = compiler->parser;
    parser->panicMode = false;

    while (parser->current.type != TOKEN_EOF) {
        if (parser->previous.type == TOKEN_SEMICOLON) {
            return;
        }
        switch (parser->current.type) {
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
        advance(parser);
    }
}

static void declaration(Compiler* compiler)
{
    if (match(compiler->parser, TOKEN_CLASS)) {
        classDeclaration(compiler);
    } else if (match(compiler->parser, TOKEN_FUN)) {
        funDeclaration(compiler);
    } else if (match(compiler->parser, TOKEN_VAR)) {
        varDeclaration(compiler, false);
    } else if (match(compiler->parser, TOKEN_CONST)) {
        varDeclaration(compiler, true);
    } else {
        statement(compiler);
    }

    if (compiler->parser->panicMode)
        synchronize(compiler);
}

static void statement(Compiler* compiler)
{
    if (match(compiler->parser, TOKEN_PRINT)) {
        printStatement(compiler);
    } else if (match(compiler->parser, TOKEN_FOR)) {
        forStatement(compiler);
    } else if (match(compiler->parser, TOKEN_IF)) {
        ifStatement(compiler);
    } else if (match(compiler->parser, TOKEN_RETURN)) {
        returnStatement(compiler);
    } else if (match(compiler->parser, TOKEN_WHILE)) {
        whileStatement(compiler);
    } else if (match(compiler->parser, TOKEN_LEFT_BRACE)) {
        beginScope(compiler);
        block(compiler);
        endScope(compiler);
    } else {
        expressionStatement(compiler);
    }
}

void defineNative(const char* name, NativeFn function)
{
    ObjString* identifier = copyString(name, (int)strlen(name));
    push(OBJ_VAL(identifier));
    ObjNative* native = newNative(function);
    push(OBJ_VAL(native));

    addresstableAdd(&vm.gloablsTable, identifier,
        (Var) {
            .identifier = identifier,
            .readonly = true,
        });
    writeValueArray(&vm.globals, OBJ_VAL(native));

    pop();
    pop();
}

static void initBuild(ObjModule* module, Compiler* compiler, Parser* parser, const char* source)
{
    unnamedModule.fqn = NULL;
    unnamedModule.currentCompiler = NULL;

    initScanner(parser, source);
    initCompiler(module, compiler, parser, NULL, TYPE_MODULE);

    compiler->parser->hadError = false;
    compiler->parser->panicMode = false;
}

static void startBuild(Compiler* compiler)
{
    advance(compiler->parser);

    while (!match(compiler->parser, TOKEN_EOF)) {
        declaration(compiler);
    }
}

ObjFunction* compile(const char* source)
{
    Parser parser;
    Compiler compiler;

    initBuild(&unnamedModule, &compiler, &parser, source);
    unnamedModule.fqn = NULL;
    unnamedModule.currentCompiler = &compiler;

    startBuild(&compiler);

    ObjFunction* scriptFunction = endCompiler(&compiler);
    unnamedModule.currentCompiler = NULL;
    return parser.hadError ? NULL : scriptFunction;
}

ObjFunction* compileModule(ObjString* fqn, const char* source, ObjModule* caller)
{
    ObjModule* module = newModule(fqn);

    temps[tempCount++] = OBJ_VAL(module);
    tableSet(&modules, fqn, OBJ_VAL(module));
    tempCount--; // module

    Parser parser;
    Compiler compiler;

    initBuild(module, &compiler, &parser, source);

    module->currentCompiler = &compiler;

    startBuild(&compiler);

    ObjFunction* scriptFunction = endCompiler(&compiler);
    module->currentCompiler = NULL;
    return parser.hadError ? NULL : scriptFunction;
}

void markModules()
{
    for (int i = 0; i < tempCount; i++) {
        markValue(temps[i]);
    }

    markObject((Obj*)&unnamedModule);
    markTable(&modules);
}

void markCompilerRoots(Compiler* compiler)
{
    while (compiler != NULL) {
        markObject((Obj*)compiler->function);
        markTable(&compiler->locals.addresses);
        compiler = compiler->enclosing;
    }
}
