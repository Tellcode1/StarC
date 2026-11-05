#include "../include/parse.h"
#include <string.h>
#include <stdlib.h>
#include <assert.h>

StarNodeID StarNodeCreate(StarParser* p)
{
    if (p->nodeCount + 1 >= p->nodeCapacity) {
        size_t newCapacity = p->nodeCapacity * 2;
        if (newCapacity == 0) newCapacity = 64;
        
        StarNode* newNodes = realloc(p->nodes, newCapacity * sizeof(StarNode));
        if (!newNodes) return -1;
        
        p->nodes = newNodes;
        p->nodeCapacity = newCapacity;
    }
    
    i32 i = (i32)p->nodeCount++;
    
    memset(&p->nodes[i], 0, sizeof(StarNode));
    
    return (StarNodeID)i;
}

u32 StarParserPushDeferScope(StarParser* p)
{
    u32 scope = ++p->currentScope;
    
    p->deferStacks[scope] = (StarDeferStack){
        .defers = calloc(sizeof(StarDefer), 64),
        .deferCount = 0,
        .deferCapacity = 64,
    };
    return scope;
}

void StarParserPopDeferScope(StarParser* p)
{
    u32 scope = p->currentScope;
    if (scope == 0){ p->deferStacks[0].deferCount = 0; return;}

    free(p->deferStacks[scope].defers);
    
    p->currentScope--;
}

int StarParserInit(StarParser* p)
{
    *p = (StarParser){
        .nodes = calloc(sizeof(StarNode), 64),
        .nodeCount = 0,
        .nodeCapacity = 64,    
        .currentScope = 0,
        .deferStacks = calloc(sizeof(StarDeferStack), 8),
        .deferStackCount = 0,
        .deferStackCapacity = 8,
    };
    return p->nodes != NULL && p->deferStacks != NULL ? 0 : -1;
}

void StarParserFree(StarParser* p) {
    free(p->nodes);
    free(p->deferStacks);
}

typedef struct Output
{
    char *buffer;
    size_t bufferSize;
    size_t bufferCapacity;
} Output;

#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) < (b) ? (a) : (b))

static inline int outStr(StarParser* p, Output* o, const char* s, size_t max) {
    size_t slen = MIN(max, strlen(s));

    // fprintf(stderr, "%u %.*s\n", p->currentScope, (int)slen, s);
    if (o->bufferSize + slen >= o->bufferCapacity)
    {
        size_t newCapacity = MAX((o->bufferCapacity * 2), (o->bufferSize + slen));
        char* newS = realloc(o->buffer, newCapacity+1);
        if (!newS) return -1;

        o->buffer = newS;
        o->bufferCapacity = newCapacity;
    }

    memcpy(&o->buffer[o->bufferSize], s, slen);
    o->bufferSize += slen;

    o->buffer[o->bufferSize] = 0;

    // fprintf(stderr, "%s\n", o->buffer);

    return 0;
}

static inline int outChr(Output* o, char c) {
    if (o->bufferSize + 1 >= o->bufferCapacity)
    {
        size_t newCapacity = MAX((o->bufferCapacity * 2), 128);
        char* newS = realloc(o->buffer, newCapacity+1);
        if (!newS) return -1;

        o->buffer = newS;
        o->bufferCapacity = newCapacity;
    }

    o->buffer[o->bufferSize++] = c;
    o->buffer[o->bufferSize] = 0;

    return 0;
}

static inline int instantiateAllDefers(StarParser* p, Output* o) {
    for (i64 i = (i64)p->currentScope; i >= 0; i--) {
        const StarDeferStack* d = &p->deferStacks[i];
        for (i64 j = (i64)d->deferCount-1; j >= 0; j--) {
            outStr(p, o, d->defers[j].begin, d->defers[j].end - d->defers[j].begin);
        }
    }

    return 0;
}

static inline int instantiateCurrentDefers(StarParser* p, Output* o) {
    if (p->currentScope == 0) return -1;
    // fprintf(stderr, "Big sad: %i\n", p->currentScope);
    const StarDeferStack* d = &p->deferStacks[p->currentScope];

    for (i64 j = (i64)d->deferCount-1; j >= 0; j--) {
        if (d->defers[j].scope != p->currentScope) break;

        outStr(p, o, d->defers[j].begin, d->defers[j].end - d->defers[j].begin);
        // fprintf(stderr,
        //  "Emitted: %.*s\n", (int)(d->defers[j].end - d->defers[j].begin), d->defers[j].begin);
    }

    return 0;
}

static inline int instantiateNDefers(StarParser* p, u32 level, Output* o) {
    if (p->currentScope == 0) return -1;
    // fprintf(stderr, "Big sad: %i\n", p->currentScope);

        for (i64 i = (i64)p->currentScope; i > level; i--) {
        const StarDeferStack* d = &p->deferStacks[i];

        for (i64 j = (i64)d->deferCount-1; j >= 0; j--) {

            outStr(p, o, d->defers[j].begin, d->defers[j].end - d->defers[j].begin);
            // fprintf(stderr,
            //  "Emitted: %.*s\n", (int)(d->defers[j].end - d->defers[j].begin), d->defers[j].begin);
        }
    }

    return 0;
}

int StarParse(const StarToken* tokens, u32 nTokens, StarParser* p, char** outBuffer, size_t* outBufferSize) {
    Output o = {
        .buffer = calloc(1,1024),
        .bufferCapacity = 1023,
        .bufferSize = 0,
    };

    u32 loopScopeLevel[1024];
    u32 x = 0;

    for (u32 i = 0; i < nTokens; i++) {
        const StarToken* tk = &tokens[i];

        // StarTokenPrint(stderr, tk);
        switch (tokens[i].type) {

            case STAR_TOKEN_TYPE_DEFER: {
                // fprintf(stderr, "Pushed defer at scope %u\n", p->currentScope);
                StarDeferStack* scope = &p->deferStacks[p->currentScope];
                if (scope->deferCount + 1 >= scope->deferCapacity) {
                    size_t newCapacity = scope->deferCapacity * 2;
                    if (newCapacity == 0) newCapacity = 64;

                    StarDefer* newDefers = realloc(scope->defers, newCapacity * sizeof(StarDefer));
                    if (!newDefers) return -1;

                    scope->defers = newDefers;
                    scope->deferCapacity = newCapacity;
                }

                scope->defers[scope->deferCount++] = (StarDefer){ .begin = tk->begin, .end = tk->end, .scope = p->currentScope };
                break;
            }

            case STAR_TOKEN_TYPE_FOR_LOOP: {
                outStr(p, &o, " for ", SIZE_MAX);

                loopScopeLevel[x++] = p->currentScope;
                // p->currentScope = StarParserPushDeferScope(p);
                break;
            }

            /* scopes for if/else/do/while/for are handled by the braces */ 
            case STAR_TOKEN_TYPE_IF: {
                outStr(p, &o, " if ", SIZE_MAX);
                break;
            }
            case STAR_TOKEN_TYPE_ELSE: {
                outStr(p, &o, " else ", SIZE_MAX);
                break;
            }
            case STAR_TOKEN_TYPE_WHILE_LOOP: {
                outStr(p, &o, " while ", SIZE_MAX);
                loopScopeLevel[x++] = p->currentScope;
                break;
            }

            /**
              * do pushes the scope, while just parses normally. We run
              * an additional pass to patch all do {} while loops;
              */
            case STAR_TOKEN_TYPE_WHILE_PATCHED: {
                outStr(p, &o, " while ", SIZE_MAX);
                break;
            }
            case STAR_TOKEN_TYPE_DO: {
                outStr(p, &o, " do ", SIZE_MAX);
                loopScopeLevel[x++] = p->currentScope;
                break;
            }

            case STAR_TOKEN_TYPE_RETURN: {
                instantiateAllDefers(p, &o);

                outStr(p, &o, " return ", SIZE_MAX);
                break;
            }
            case STAR_TOKEN_TYPE_CONTINUE: {
                instantiateNDefers(p, loopScopeLevel[x-1], &o);
                // StarParserPopDeferScope(p);

                outStr(p, &o, " continue ", SIZE_MAX);
                break;
            }

            case STAR_TOKEN_TYPE_BREAK: {
                // fprintf(stderr, "loop scope level: %u, Current level: %u\n", loopScopeLevel[x-1], p->currentScope);

                instantiateNDefers(p, loopScopeLevel[x-1], &o);
                // StarParserPopDeferScope(p);

                outStr(p, &o, " break ", SIZE_MAX);
                break;
            }

            case STAR_TOKEN_TYPE_OPEN_PAREN: outStr(p, &o, "(", SIZE_MAX); break;
            case STAR_TOKEN_TYPE_SEMI_COLON: outStr(p, &o, ";", SIZE_MAX); break;
            
            case STAR_TOKEN_TYPE_CLOSE_PAREN:
                outStr(p, &o, ")", SIZE_MAX);
                break;

            case STAR_TOKEN_TYPE_OPEN_BRACE:
                p->currentScope = StarParserPushDeferScope(p);
                outStr(p, &o, "{", SIZE_MAX);
                break;

            case STAR_TOKEN_TYPE_CLOSE_BRACE: {
                // fprintf(stderr, "Current scope: %u\n", p->currentScope);
                instantiateCurrentDefers(p, &o);
                StarParserPopDeferScope(p);
                outStr(p, &o, "}", SIZE_MAX);

                while (loopScopeLevel[x] >= p->currentScope && x > 0) {
                    // fprintf(stderr, "Rolled back to %u\n", x);
                    x--;
                }
                break;
            }
            case STAR_TOKEN_TYPE_MISC:
                outChr(&o, ' ');
                outStr(p, &o, tk->begin, tk->end - tk->begin);
                outChr(&o, ' ');
                break;
            case STAR_TOKEN_TYPE_IDENT:
                outChr(&o, ' ');
                outStr(p, &o, tk->begin, tk->end - tk->begin);
                outChr(&o, ' ');
                break;
        }
        if ((tk->flags & STAR_TOKEN_SUFFIX_OPEN_BRACE) != 0) outChr(&o, '{');
        if ((tk->flags & STAR_TOKEN_SUFFIX_CLOSE_BRACE) != 0) outChr(&o, '}');
    }
    
    
    if (outBuffer) *outBuffer = o.buffer; else free(o.buffer);
    if (outBufferSize) *outBufferSize = o.bufferSize;

    return 0;
}