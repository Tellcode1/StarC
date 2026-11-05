#ifndef STAR_C_PARSER_H
#define STAR_C_PARSER_H

#include "lexer.h"

typedef int StarNodeID;

typedef enum StarNodeType {
    /* Arbitrary text region */
    STAR_NODE_TYPE_MISC,
    STAR_NODE_TYPE_IDENT,
    
    STAR_NODE_TYPE_CONTINUE,
    STAR_NODE_TYPE_BREAK,
    STAR_NODE_TYPE_RETURN,
    
    STAR_NODE_TYPE_FOR_LOOP,
    STAR_NODE_TYPE_WHILE_LOOP,
    STAR_NODE_TYPE_DO_WHILE_LOOP,
    STAR_NODE_TYPE_IF,

    /**
        * Only function definitions.
        * ident ident(...) {}
    */
    STAR_NODE_TYPE_FUNCTION,
} StarNodeType;

typedef union StarNodeValue {
    struct {
        /* Token indices */
        u32 begin;
        u32 end;
    } text, ident, defers;
    
    struct {
        /* Token indices */
        u32 initBegin; // inclusive
        u32 initEnd; // exclusive

        u32 cndBegin; // inclusive
        u32 cndEnd; // exclusive

        u32 iterBegin; // inclusive
        u32 iterEnd; // exclusive

        u32 bodyBegin;
        u32 bodyEnd;
    } for_stmt;
    
    struct {
        u32 cndBegin;
        u32 cndEnd;
        u32 bodyBegin;
        u32 bodyEnd;
    } while_stmt, do_while_stmt;
    
    struct {
        u32 cndBegin;
        u32 cndEnd;

        u32* stmts; // body, token indices
        u32 nstmts;

        StarNodeID nextElseIf; // -1 if no else if. IF node ID.
        StarNodeID elseNode;
    } if_stmt;
    
    struct {
        StarNodeID returnValueType;
        const char* name;
    } func;
} StarNodeValue;


typedef struct StarNode {
    StarNodeType type;
    StarNodeValue as;
} StarNode;

/* The text region to copy over everytime a defer needs to be instantiated */
typedef struct StarDefer {
    const char* begin;
    const char* end;
    u32 scope;
} StarDefer;

typedef struct StarDeferStack {
    StarDefer* defers;
    u32 deferCount;
    u32 deferCapacity;
} StarDeferStack;

typedef struct StarParser {
    StarNode* nodes;
    u32 nodeCount;
    u32 nodeCapacity;

    const StarToken* tokens;
    u32 tokenCount;

    u32 i; // token index
    
    u32 currentScope; // 0 == global, 1 == inside function.
    
    /* Array of defer stacks, first defer stack is for the function. */
    StarDeferStack *deferStacks;
    u32 deferStackCount;
    u32 deferStackCapacity;
} StarParser;

StarNodeID StarNodeCreate(StarParser* p);

/* returns the scope level. 0 == global, pushed for every statement (func/for/while/dowhile/if/whatever) */
u32 StarParserPushDeferScope(StarParser* p);
void StarParserPopDeferScope(StarParser* p);

int StarParserInit(StarParser* p);
void StarParserFree(StarParser* p);

/**
  * Parse the token list to an output buffer
  * The output buffer contains the parsed file (and can be passed
  * straight to the compiler).
  * outBufferSize can be NULL.
*/
int StarParse(const StarToken* tokens, u32 nTokens, StarParser* p, char** outBuffer, size_t* outBufferSize);

#endif//STAR_C_PARSER_H
