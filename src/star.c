#include "include/lexer.h"
#include "include/parse.h"
#include "include/std.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <errno.h>
#include <stdbool.h>

#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) < (b) ? (a) : (b))

typedef struct Tokens {
    size_t tokenCapacity;
    size_t tokenCount;
    StarToken* tokens;
} Tokens;

#define ADVANCE(str,line,col) do{ char ch = *(str); (str)++; if (ch == '\n') { (line)++; (col) = 1; } else (col)++; }while(0)
#define ADVANCE_N(str,n, line,col) for (size_t _x_ = 0; _x_ < (n); _x_++) { ADVANCE(str,line,col); }

#define TK_APPEND(tokensPtr, ...) do {\
        if ((tokensPtr)->tokenCount + 1 >= (tokensPtr)->tokenCapacity) {\
            size_t newCapacity = (tokensPtr)->tokenCapacity * 2;\
            if (newCapacity == 0) newCapacity = 64;\
            StarToken* newTokens = realloc((tokensPtr)->tokens, newCapacity * sizeof(StarToken));\
            if (!newTokens) return -1;\
            (tokensPtr)->tokenCapacity = newCapacity;\
            (tokensPtr)->tokens = newTokens;\
        }\
        \
        (tokensPtr)->tokens[(tokensPtr)->tokenCount++] = __VA_ARGS__;\
    } while (0)

static inline bool _outExpectError(StarToken tk, StarTokenType type) {
    if (tk.type != type) {
        fprintf(stderr, "[%s:%zu] "TERM_RED"ERROR"TERM_RESET": Expected token %i, but got %i\n", tk.file, tk.line, (type), (tk).type);
        return true;
    }
    return false;
}
#define EXPECT(token, type) _outExpectError(token, type)

/**
  * Compare two tokens, returning 0 when they are equal
  * This protects against matching partial words:
  * like matching does to do, which strncmp would do
  * and using strcmp would always return false (length will almost always be different)
  *
  * This assumes token to always be a valid string (and not something like a space)!
  */
static inline int tokenStrCmp(const char* s, const char* token) {

    /* strcmp */
    while (*s && *token) {
        if (*s != *token) return -1;
        s++;
        token++;
    }
    
    /**
      * This protects from matching does to do.
      * Because: if the matchee continues with a valid identifier character,
      * then it must be an extension of the matchee, and thus should not
      * match with the matchee.
    */
    if (*s && !*token) return !isalnum(*s) && *s != '_' ? 0 : -1;

    return 0; /* Equal! */
}

int StarLexicalize(const char* str, size_t maxStr, const char* debugFileName, StarToken** outTokens, size_t* outNtokens) {
    if (!str || maxStr == 0) return 0;
    
    Tokens tks = {
        .tokenCount = 0,
        .tokenCapacity = 64,
        .tokens = calloc(64, sizeof(StarToken)),
    };
    if (!tks.tokens) return -1;
    
    const char* max = str + maxStr;
    if (maxStr == SIZE_MAX) {
        max = str + strlen(str);
    }

    size_t line = 1;
    size_t col =  1;

    /* Track text we don't care about. Flushed and set to NULL if we matched with anything */
    const char* miscTextBegin = NULL;

    while (*str && str < max)
    {
        while (isspace(*str) && *str && str < max) ADVANCE(str,line,col);
        if (str >= max || !*str) break;

        if (strncmp(str, "//", 2) == 0) {
            if (!miscTextBegin) miscTextBegin = str;

            ADVANCE_N(str,2,line,col);

            while (*str && *str != '\n') ADVANCE(str,line,col);
            while (*str && isspace(*str)) ADVANCE(str,line,col);
        }
        else if (strncmp(str, "/*", 2) == 0) {
            // if (miscTextBegin != NULL) {
            //     const char *miscTextEnd = str;
            //     TK_APPEND(&tks, (StarToken){ .type = STAR_TOKEN_TYPE_MISC, .begin = miscTextBegin, .end = miscTextEnd, .line = line, .column = col, .file = debugFileName  });
                
            //     miscTextBegin = NULL;
            // }
            if (!miscTextBegin) miscTextBegin = str;
            
            ADVANCE_N(str,2,line,col);

            i64 depth = 0;
            while (*str) {
                if (strncmp(str, "*/", 2) == 0) {
                    ADVANCE(str,line,col);
                    ADVANCE(str,line,col);
                    if (depth == 0) break;
                    else depth--;
                }
                else if (strncmp(str, "/*", 2) == 0) {
                    depth++;
                    ADVANCE(str,line,col);
                    ADVANCE(str,line,col);
                }
                else ADVANCE(str,line,col);
            }
            fprintf(stderr,"PISS: %c\n", *str);
            while (*str && isspace(*str)) ADVANCE(str,line,col);
        }
        else if (*str == '"') {
            /**
              * Extend the misc selection to include
              * this string
              */
            if (!miscTextBegin) miscTextBegin = str;
            
            ADVANCE(str,line,col);

            while (*str && *str != '"' && str < max) {
                if (*str == '\\' && (str+1) < max) {
                    ADVANCE(str,line,col);
                }
                ADVANCE(str,line,col);
                if (*str == '"') {
                    break;
                }
            }
            ADVANCE(str,line,col);
        }
        else if (*str == '#') {
            /* Don't flush Misc */
            if (!miscTextBegin) miscTextBegin = str;
            
            ADVANCE(str,line,col); // #

            while (*str && *str != '\n' && *str != '\r') {
                ADVANCE(str,line,col);
                if (*str == '\\') {
                    ADVANCE(str,line,col);
                    while (isspace(*str) && str && str < max) ADVANCE(str,line,col);
                }
            }
            while (isspace(*str) && *str && str < max) ADVANCE(str,line,col); // \n
        }
        else if (*str == ';')
        {
            if (miscTextBegin != NULL) {
                const char *miscTextEnd = str;
                TK_APPEND(&tks, (StarToken){ .type = STAR_TOKEN_TYPE_MISC, .begin = miscTextBegin, .end = miscTextEnd, .line = line, .column = col, .file = debugFileName  });
                
                miscTextBegin = NULL;
            }
            TK_APPEND(&tks, (StarToken){ .type = STAR_TOKEN_TYPE_SEMI_COLON, .line = line, .column = col, .file = debugFileName  });
            ADVANCE(str,line,col);
        }
        else if (*str == '(')
        {
            if (miscTextBegin != NULL) {
                const char *miscTextEnd = str;
                TK_APPEND(&tks, (StarToken){ .type = STAR_TOKEN_TYPE_MISC, .begin = miscTextBegin, .end = miscTextEnd, .line = line, .column = col, .file = debugFileName  });
                
                miscTextBegin = NULL;
            }
            TK_APPEND(&tks, (StarToken){ .type = STAR_TOKEN_TYPE_OPEN_PAREN, .line = line, .column = col, .file = debugFileName  });
            ADVANCE(str,line,col);
        }
        else if (*str == ')')
        {
            if (miscTextBegin != NULL) {
                const char *miscTextEnd = str;
                TK_APPEND(&tks, (StarToken){ .type = STAR_TOKEN_TYPE_MISC, .begin = miscTextBegin, .end = miscTextEnd, .line = line, .column = col, .file = debugFileName  });
                
                miscTextBegin = NULL;
            }
            TK_APPEND(&tks, (StarToken){ .type = STAR_TOKEN_TYPE_CLOSE_PAREN, .line = line, .column = col, .file = debugFileName  });
            ADVANCE(str,line,col);
        }
        else if (*str == '{')
        {
            if (miscTextBegin != NULL) {
                const char *miscTextEnd = str;
                TK_APPEND(&tks, (StarToken){ .type = STAR_TOKEN_TYPE_MISC, .begin = miscTextBegin, .end = miscTextEnd, .line = line, .column = col, .file = debugFileName  });
                
                miscTextBegin = NULL;
            }
            TK_APPEND(&tks, (StarToken){ .type = STAR_TOKEN_TYPE_OPEN_BRACE, .line = line, .column = col, .file = debugFileName  });
            ADVANCE(str,line,col);
        }
        else if (*str == '}')
        {
            if (miscTextBegin != NULL) {
                const char *miscTextEnd = str;
                TK_APPEND(&tks, (StarToken){ .type = STAR_TOKEN_TYPE_MISC, .begin = miscTextBegin, .end = miscTextEnd, .line = line, .column = col, .file = debugFileName  });
                
                miscTextBegin = NULL;
            }
            TK_APPEND(&tks, (StarToken){ .type = STAR_TOKEN_TYPE_CLOSE_BRACE, .line = line, .column = col, .file = debugFileName  });
            ADVANCE(str,line,col);
        }
        
        else if (tokenStrCmp(str, "break") == 0)
        {
            if (miscTextBegin != NULL) {
                const char *miscTextEnd = str;
                TK_APPEND(&tks, (StarToken){ .type = STAR_TOKEN_TYPE_MISC, .begin = miscTextBegin, .end = miscTextEnd, .line = line, .column = col, .file = debugFileName  });
                
                miscTextBegin = NULL;
            }
            
            ADVANCE_N(str,5,line,col);
            while (*str && isspace(*str)) ADVANCE(str,line,col);

            TK_APPEND(&tks, (StarToken){ .type = STAR_TOKEN_TYPE_BREAK, .line = line, .column = col, .file = debugFileName  });
        }
        
        else if (tokenStrCmp(str, "continue") == 0)
        {
            if (miscTextBegin != NULL) {
                const char *miscTextEnd = str;
                TK_APPEND(&tks, (StarToken){ .type = STAR_TOKEN_TYPE_MISC, .begin = miscTextBegin, .end = miscTextEnd, .line = line, .column = col, .file = debugFileName  });
                
                miscTextBegin = NULL;
            }
            
            ADVANCE_N(str,strlen("continue"),line,col);
            while (*str && isspace(*str)) ADVANCE(str,line,col);
            
            TK_APPEND(&tks, (StarToken){ .type = STAR_TOKEN_TYPE_CONTINUE, .line = line, .column = col, .file = debugFileName  });
        }
        
        else if (tokenStrCmp(str, "return") == 0)
        {
            if (miscTextBegin != NULL) {
                const char *miscTextEnd = str;
                TK_APPEND(&tks, (StarToken){ .type = STAR_TOKEN_TYPE_MISC, .begin = miscTextBegin, .end = miscTextEnd, .line = line, .column = col, .file = debugFileName  });
                
                miscTextBegin = NULL;
            }

            ADVANCE_N(str,strlen("return"),line,col);
            while (*str && isspace(*str)) ADVANCE(str,line,col);

            TK_APPEND(&tks, (StarToken){ .type = STAR_TOKEN_TYPE_RETURN, .line = line, .column = col, .file = debugFileName  });
        }
        else if (tokenStrCmp(str, "for") == 0) {
            if (miscTextBegin != NULL) {
                const char *miscTextEnd = str;
                TK_APPEND(&tks, (StarToken){ .type = STAR_TOKEN_TYPE_MISC, .begin = miscTextBegin, .end = miscTextEnd, .line = line, .column = col, .file = debugFileName  });
                
                miscTextBegin = NULL;
            }

            ADVANCE_N(str,3,line,col);
            
            while (isspace(*str) && *str && str < max) ADVANCE(str,line,col);
            

            TK_APPEND(&tks,
                (StarToken){
                    .type = STAR_TOKEN_TYPE_FOR_LOOP,
                    .line = line,
                    .column = col, .file = debugFileName ,
             });
        }

        else if (tokenStrCmp(str, "if") == 0) {
            if (miscTextBegin != NULL) {
                const char *miscTextEnd = str;
                TK_APPEND(&tks, (StarToken){ .type = STAR_TOKEN_TYPE_MISC, .begin = miscTextBegin, .end = miscTextEnd, .line = line, .column = col, .file = debugFileName  });
                
                miscTextBegin = NULL;
            }

            ADVANCE_N(str,2,line,col);
            
            while (isspace(*str) && *str && str < max) ADVANCE(str,line,col);
            

            TK_APPEND(&tks,
                (StarToken){
                    .type = STAR_TOKEN_TYPE_IF,
                    .line = line,
                    .column = col, .file = debugFileName ,
             });
        }
        else if (tokenStrCmp(str, "else") == 0) {
            if (miscTextBegin != NULL) {
                const char *miscTextEnd = str;
                TK_APPEND(&tks, (StarToken){ .type = STAR_TOKEN_TYPE_MISC, .begin = miscTextBegin, .end = miscTextEnd, .line = line, .column = col, .file = debugFileName  });
                
                miscTextBegin = NULL;
            }

            ADVANCE_N(str,4,line,col);
            
            while (isspace(*str) && *str && str < max) ADVANCE(str,line,col);
            

            TK_APPEND(&tks,
                (StarToken){
                    .type = STAR_TOKEN_TYPE_ELSE,
                    .line = line, .column = col,
                    .file = debugFileName,
             });
        }
        else if (tokenStrCmp(str, "defer") == 0) {
            if (miscTextBegin != NULL) {
                const char *miscTextEnd = str;
                TK_APPEND(&tks, (StarToken){ .type = STAR_TOKEN_TYPE_MISC, .begin = miscTextBegin, .end = miscTextEnd, .line = line, .column = col, .file = debugFileName  });
                
                miscTextBegin = NULL;
            }

            ADVANCE_N(str,5,line,col);
            
            while (isspace(*str) && *str && str < max) ADVANCE(str,line,col);

            const char* begin = str;
            
            i64 depth = 0;
            if (*str == '{')
            {
                while (*str && str < max) {
                    ADVANCE(str,line,col);

                    if (*str == '{') { depth++; }
                    else if (*str == '}') { 
                        if (depth == 0) break;
                        else depth--;
                    }
                }
            } else {
                while (*str && str < max && *str != ';') ADVANCE(str,line,col);
                if (!*str) {
                    fprintf(stderr, TERM_RED"ERROR"TERM_RESET": Unexpected EOF\n");
                    return -1;
                }
            }

            ADVANCE(str,line,col); // ;/}, include it in the defer text
            const char* end = str;

            TK_APPEND(&tks,
                (StarToken){
                    .type = STAR_TOKEN_TYPE_DEFER,
                    .begin = begin,
                    .end = end,
                    .line = line, .column = col, .file = debugFileName,
             });
        }
        else if (tokenStrCmp(str, "while") == 0) {
            if (miscTextBegin != NULL) {
                const char *miscTextEnd = str;
                TK_APPEND(&tks, (StarToken){ .type = STAR_TOKEN_TYPE_MISC, .begin = miscTextBegin, .end = miscTextEnd, .line = line, .column = col, .file = debugFileName  });
                
                miscTextBegin = NULL;
            }

            ADVANCE_N(str,5,line,col);
            
            while (isspace(*str) && *str && str < max) ADVANCE(str,line,col);
            
            TK_APPEND(&tks,
                (StarToken){
                    .type = STAR_TOKEN_TYPE_WHILE_LOOP,
                    .line = line, .column = col, .file = debugFileName,
             });
        }
        else if (tokenStrCmp(str, "do") == 0) {
            if (miscTextBegin != NULL) {
                const char *miscTextEnd = str;
                TK_APPEND(&tks, (StarToken){ .type = STAR_TOKEN_TYPE_MISC, .begin = miscTextBegin, .end = miscTextEnd, .line = line, .column = col, .file = debugFileName  });
                
                miscTextBegin = NULL;
            }

            ADVANCE_N(str,2,line,col);
            
            while (isspace(*str) && *str && str < max) ADVANCE(str,line,col);
            
            TK_APPEND(&tks,
                (StarToken){
                    .type = STAR_TOKEN_TYPE_DO,
                    .line = line, .column = col, .file = debugFileName,
             });
        }

        /* Must always be after the keyword cases */
        else if (isalpha(*str) || *str == '_') {
            if (miscTextBegin != NULL) {
                const char *miscTextEnd = str;
                TK_APPEND(&tks, (StarToken){ .type = STAR_TOKEN_TYPE_MISC, .begin = miscTextBegin, .end = miscTextEnd, .line = line, .column = col, .file = debugFileName  });
                
                miscTextBegin = NULL;
            }

            const char* beg = str;
            const char* end = str;
            
            while (*end && end < max && (isalnum(*end) || *end == '_')) {
                end++;
            }
            
            TK_APPEND(&tks, (StarToken){ .type = STAR_TOKEN_TYPE_IDENT, .begin = beg, .end = end, .line = line, .column = col, .file = debugFileName  });
            
            str = end;
        }

        else { /* Don't set if we're already tracking */
            if (!miscTextBegin) miscTextBegin = str;
            if (*str == '"') abort();
            ADVANCE(str,line,col);
        }
    }

    for (u32 i = 0; i < tks.tokenCount; i++) {
        if (tks.tokens[i].type != STAR_TOKEN_TYPE_DO) continue;
        /* Find its corresponding while and patch it */

        /**
          * We have to patch do while loops so
          * that our parser can intuitively read them.
          *
          * We do this by finding each do keyword, looking forward
          * to find its while counterpart and then setting it to a
          * special WHILE_PATCHED token type: which only prints "while"
          * and does not serve any special purposes.
          */

          bool singleStatement = tks.tokens[i+1].type != STAR_TOKEN_TYPE_OPEN_BRACE;

          /**
            * If it is a single statement loop:
            * Like do j++ while (j > 0);
            * Simply look forward until the first semicolon
            */
        if (singleStatement) {
            u32 j = i;
            bool found = false;
            while (j < tks.tokenCount) {
                if (tks.tokens[j].type == STAR_TOKEN_TYPE_SEMI_COLON) {
                    found = true;
                    break;
                }
                j++;
            }

            if (!found) {
                fprintf(stderr, "[%s:%zu] "TERM_RED"ERROR"TERM_RESET": Unexpected EOF while looking for the semicolon for single statement do while that starts here: [%s:%zu:%zu]\n", tks.tokens[j-1].file, tks.tokens[j-1].line, tks.tokens[i].file, tks.tokens[i].line, tks.tokens[i].column);
                return -1;
            }

            found = false;
            while (j < tks.tokenCount) {
                if (tks.tokens[j].type == STAR_TOKEN_TYPE_WHILE_LOOP) {
                    found = true;
                    break;
                }
                j++;
            }
            if (!found) {
                fprintf(stderr, "[%s:%zu] "TERM_RED"ERROR"TERM_RESET": No matching while for this do\n", tks.tokens[i].file, tks.tokens[i].line);
                return -1;
            }
            if (EXPECT(tks.tokens[j], STAR_TOKEN_TYPE_WHILE_LOOP)) return -1;

            tks.tokens[j].type = STAR_TOKEN_TYPE_WHILE_PATCHED; /* patched */
        } else {
            /**
              * Do a simple depth search of braces
              */
            i64 depth = 0;
            u32 j = i+1;

            bool found = false;
            for (; j < tks.tokenCount; j++) {
                StarToken* fwd = &tks.tokens[j];
    
                if (fwd->type == STAR_TOKEN_TYPE_OPEN_BRACE) {
                    depth++;
                }
                else if (fwd->type == STAR_TOKEN_TYPE_CLOSE_BRACE) {
                    depth--;
                    if (depth == 0) {found=true; break;}
                }
            }

            if (!found) {
                fprintf(stderr, "[%s:%zu] "TERM_RED"ERROR"TERM_RESET": Unexpected EOF\n", tks.tokens[i+1].file, tks.tokens[i+1].line);
                return -1;
            }
            // fprintf(stderr, "glooby: ");
            // StarTokenPrint(stderr, &tks.tokens[j+1]);

            if (EXPECT(tks.tokens[j+1], STAR_TOKEN_TYPE_WHILE_LOOP)) return -1;
            tks.tokens[j+1].type = STAR_TOKEN_TYPE_WHILE_PATCHED; /* patched */
        }
    }

    /**
      * This must be after the do while patch because this can incorrectly
      * insert braces after the while in do {} while(1); (do {} while (1) {;})
    */

    /* Patch all single line statements without braces to include braces */
    for (u32 i = 0; i < tks.tokenCount; i++) {
        StarTokenType type = tks.tokens[i].type;

        /* do while statements need to be specially handled */
        if (type == STAR_TOKEN_TYPE_DO) {
            /* already braced */
            if (tks.tokens[i+1].type == STAR_TOKEN_TYPE_OPEN_BRACE) { continue; }

            tks.tokens[i].flags |= STAR_TOKEN_SUFFIX_OPEN_BRACE;

            u32 j = i+1; // after the do
            while (tks.tokens[j].type != STAR_TOKEN_TYPE_SEMI_COLON && j < tks.tokenCount) j++;
            tks.tokens[j].flags |= STAR_TOKEN_SUFFIX_CLOSE_BRACE;

            continue;
        }
        if (type != STAR_TOKEN_TYPE_IF && type != STAR_TOKEN_TYPE_WHILE_LOOP && type != STAR_TOKEN_TYPE_FOR_LOOP) { continue; }

        i++;
        if (EXPECT(tks.tokens[i], STAR_TOKEN_TYPE_OPEN_PAREN)) return -1;

        /* Find index of closing parenthesis*/
        i64 depth = 0;
        u32 j = i+1;

        bool found = false;
        for (; j < tks.tokenCount; j++) {
            StarToken* peek = &tks.tokens[j];
            if (peek->type == STAR_TOKEN_TYPE_OPEN_PAREN)
                depth++;
            else if (peek->type == STAR_TOKEN_TYPE_CLOSE_PAREN) {
                if (depth == 0) {found = true; break;}
                else depth--;
            }
        }

        j++;
        if (!found) {
            fprintf(stderr, "[%s:%zu] "TERM_RED"ERROR"TERM_RESET": Unexpected EOF\n", tks.tokens[i+1].file, tks.tokens[i+1].line);
            return -1;
        }

        /* already braced, skip */
        if (tks.tokens[j].type == STAR_TOKEN_TYPE_OPEN_BRACE) continue;
        
        /* else, suffix the close parenthesis with an open brace */
        tks.tokens[j-1].flags |= STAR_TOKEN_SUFFIX_OPEN_BRACE;

        /* find the semicolon (statement end) and suffix it with the close brace*/
        found = false;
        while (j < tks.tokenCount) {
            if (tks.tokens[j].type == STAR_TOKEN_TYPE_SEMI_COLON) {
                found = true;
                break;
            }
            j++;
        }

        if (!found) {
            fprintf(stderr, "[%s:%zu] "TERM_RED"ERROR"TERM_RESET": Unexpected EOF\n", tks.tokens[i+1].file, tks.tokens[i+1].line);
            return -1;
        }
        
        /* suffix semicolon with close brace */
        tks.tokens[j].flags |= STAR_TOKEN_SUFFIX_CLOSE_BRACE;
    }
    
    if (outTokens) *outTokens = tks.tokens; else free(tks.tokens);
    if (outNtokens) *outNtokens = tks.tokenCount;
    
    if (miscTextBegin != NULL) {
        const char *miscTextEnd = str;
        TK_APPEND(&tks, (StarToken){ .type = STAR_TOKEN_TYPE_MISC, .begin = miscTextBegin, .end = miscTextEnd, .line = line, .column = col, .file = debugFileName  });
        
        miscTextBegin = NULL;
    }
    
    return 0;
}

void StarTokenPrint(FILE* f, const StarToken *tk) {
    fprintf(f, "[%s:%zu:%zu] ", tk->file, tk->line, tk->column); 
    if (tk->type == STAR_TOKEN_TYPE_FOR_LOOP) {
        fprintf(f, "for ");
    }
    else if (tk->type == STAR_TOKEN_TYPE_WHILE_LOOP) {
        fprintf(f, "while ");
    }
    else if (tk->type == STAR_TOKEN_TYPE_RETURN) {
        fprintf(f,
            " return "
        );
    }
    else if (tk->type == STAR_TOKEN_TYPE_DEFER) {
        fprintf(f,
            "defer %.*s;\n",
            (int)(tk->begin == NULL ? 0 : (tk->end - tk->begin)),
            tk->begin
        );
    }
    else if (tk->type == STAR_TOKEN_TYPE_CONTINUE) {
        fprintf(f, "continue");
    }
    else if (tk->type == STAR_TOKEN_TYPE_BREAK) {
        fprintf(f, "break");
    }
    else if (tk->type == STAR_TOKEN_TYPE_IF) {
        fprintf(f, "if ");
    }
    else if (tk->type == STAR_TOKEN_TYPE_ELSE) {
        fprintf(f, "else ");
    }
    else if (tk->type == STAR_TOKEN_TYPE_DO) {
        fprintf(f, "do ");
    }
    else if (tk->type == STAR_TOKEN_TYPE_IDENT || tk->type == STAR_TOKEN_TYPE_MISC) {
        fprintf(f, "(%s)%.*s ", tk->type == STAR_TOKEN_TYPE_IDENT ? "IDENT" : "MISC", (int)(tk->end - tk->begin), tk->begin);
    }
    else if (tk->type == STAR_TOKEN_TYPE_SEMI_COLON) { fprintf(f, ";"); }
    else if (tk->type == STAR_TOKEN_TYPE_OPEN_PAREN) { fprintf(f, " ( "); }
    else if (tk->type == STAR_TOKEN_TYPE_CLOSE_PAREN) { fprintf(f, " ) "); }
    else if (tk->type == STAR_TOKEN_TYPE_OPEN_BRACE) { fprintf(f, " {\n"); }
    else if (tk->type == STAR_TOKEN_TYPE_CLOSE_BRACE) { fprintf(f, " }\n"); }
    else {
        fprintf(stderr, "Unknown token type: %i\n", tk->type);
    }
    fputc('\n', f);
}

void StarTokensFree(StarToken* tokens, u32 nTokens)
{
    /* Tokens are just read only copies :3 */
    free(tokens);
}


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

static inline int outStr(StarParser* p, Output* o, const char* s, size_t max) {
    size_t slen = MIN(max, strlen(s));

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

/* Instantiate defers declared in the current scope */
static inline int instantiateCurrentDefers(StarParser* p, Output* o) {
    if (p->currentScope == 0) return -1;

    const StarDeferStack* d = &p->deferStacks[p->currentScope];
    for (i64 j = (i64)d->deferCount-1; j >= 0; j--) {
        if (d->defers[j].scope != p->currentScope) break;

        outStr(p, o, d->defers[j].begin, d->defers[j].end - d->defers[j].begin);
    }

    return 0;
}

/* Instantiate all defers from current level to the specific level (excluding it) */
static inline int instantiateNDefers(StarParser* p, u32 level, Output* o) {
    if (p->currentScope == 0) return -1;
        for (i64 i = (i64)p->currentScope; i > level; i--) {
        const StarDeferStack* d = &p->deferStacks[i];

        for (i64 j = (i64)d->deferCount-1; j >= 0; j--) {

            outStr(p, o, d->defers[j].begin, d->defers[j].end - d->defers[j].begin);
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

    /**
      * The scope levels which loops reside in
      */
    u32 loopScopeLevelStack[1024];
    u32 loopScope = 0;

    for (u32 i = 0; i < nTokens; i++) {
        const StarToken* tk = &tokens[i];

        switch (tokens[i].type) {

            case STAR_TOKEN_TYPE_DEFER: {
                StarDeferStack* scope = &p->deferStacks[p->currentScope];
                if (scope->deferCount + 1 >= scope->deferCapacity) {
                    size_t newCapacity = scope->deferCapacity * 2;
                    if (newCapacity == 0) newCapacity = 64;

                    StarDefer* newDefers = realloc(scope->defers, newCapacity * sizeof(StarDefer));
                    if (!newDefers) return -1;

                    scope->defers = newDefers;
                    scope->deferCapacity = newCapacity;
                }

                /* Track the defer */
                scope->defers[scope->deferCount++] = (StarDefer){ .begin = tk->begin, .end = tk->end, .scope = p->currentScope };
                break;
            }

            case STAR_TOKEN_TYPE_FOR_LOOP: {
                outStr(p, &o, " for ", SIZE_MAX);

                loopScopeLevelStack[loopScope++] = p->currentScope;
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
                loopScopeLevelStack[loopScope++] = p->currentScope;
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
                loopScopeLevelStack[loopScope++] = p->currentScope;
                break;
            }

            case STAR_TOKEN_TYPE_RETURN: {
                instantiateAllDefers(p, &o);

                outStr(p, &o, " return ", SIZE_MAX);
                break;
            }

            /* Instantiate all defers declared in the latest loop */
            case STAR_TOKEN_TYPE_CONTINUE: {
                instantiateNDefers(p, loopScopeLevelStack[loopScope-1], &o);

                outStr(p, &o, " continue ", SIZE_MAX);
                break;
            }

            /* Instantiate all defers declared in the latest loop */
            case STAR_TOKEN_TYPE_BREAK: {
                instantiateNDefers(p, loopScopeLevelStack[loopScope-1], &o);

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
                instantiateCurrentDefers(p, &o);
                StarParserPopDeferScope(p);
                outStr(p, &o, "}", SIZE_MAX);

                while (loopScopeLevelStack[loopScope] >= p->currentScope && loopScope > 0) {
                    loopScope--;
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

static inline size_t
kit_strlcat(char* d, const char* s, size_t dsize)
{
  size_t dl = strlen(d);
  size_t sl = strlen(s);
  size_t i  = 0;

  if (dsize <= dl) { return dsize + sl; }

  for (; i < sl && (dl + i) < dsize - 1; i++) { d[dl + i] = s[i]; }
  d[dl + i] = 0;

  return dl + sl;
}

static inline int
create_and_cat(const char* s, size_t* to_capacity, char** to_ptr)
{
  if (!*to_ptr) {
    size_t new_cap = strlen(s) + 1;
    *to_ptr        = calloc(1, new_cap);
    *to_capacity   = new_cap;

    memset(*to_ptr, 0, new_cap);
  }
  if (!*to_ptr) return -1;

  const size_t s_len  = strlen(s);
  const size_t to_len = strlen(*to_ptr);

  const size_t new_size = (s_len + to_len + 1);

  if (new_size >= *to_capacity) {
    const size_t new_cap = MAX(new_size, *to_capacity * 2);
    char*        new_to  = realloc(*to_ptr, new_cap); // to_len+1 -> to_len+s_len+1
    if (!new_to) return -1;

    *to_ptr      = new_to;
    *to_capacity = new_cap;
  }

  kit_strlcat(*to_ptr, s, *to_capacity);

  return 0;
}

static inline char*
read_file_arena_better(FILE* f)
{
  char tmp[128];

  const size_t init_capacity = 128;
  size_t       capacity      = init_capacity;

  char* buffer = calloc(1, init_capacity);
  if (!buffer) return NULL;

  while (fgets(tmp, sizeof(tmp), f)) {
    int e = create_and_cat(tmp, &capacity, &buffer);
    if (e) {
      free(buffer);
      return NULL;
    }
  }

  return buffer;
}

#ifndef STARC_NO_MAIN

int main(int argc, char *argv[]) {
    fprintf(stderr, "StarC parser v%i.%i.%i\n", STARC_MAJOR, STARC_MINOR, STARC_PATCH);

    const char* file = NULL;
    const char* outf = NULL;

    FILE* infile = NULL;
    FILE* outfile = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0) {
            i++;
            assert(i < argc);

            outf = argv[i];
        } else {
            file = argv[i];
        }
    }

    if (file == NULL) {
        fprintf(stderr, TERM_YELLOW"WARNING"TERM_RESET": No input files\n");
        return 0;
    }

    if (strcmp(file, "-") == 0) {
        infile = stdin;
    } else {
        infile = fopen(file, "r");
    }

    if (!outf || strcmp(outf, "-") == 0) {
        outfile = stdout;
    } else {
        outfile = fopen(outf, "w");
    }

    if (!infile) {
        fprintf(stderr, TERM_RED"Error"TERM_RESET": Failed to open file %s: %s\n", file, strerror(errno));
        return -1;
    }

    for (int i = 1; i < argc; i++) {
        char* s = read_file_arena_better(infile);
        
        StarToken* tokens = NULL;
        size_t     nTokens = 0;
        int e = StarLexicalize(s, SIZE_MAX, file, &tokens, &nTokens);
        if (e != 0) {
            fprintf(stderr, TERM_RED"ERROR"TERM_RESET": Failed to tokenize file: %s\n", file);
            continue;
        }

        StarParser p;
        StarParserInit(&p);
    
        char *buffer = NULL;
        if (StarParse(tokens, nTokens, &p, &buffer, NULL) != 0) {
            fprintf(stderr, TERM_RED"ERROR"TERM_RESET": Failed to parse %s\n", file);
            continue;
        }

        fputs(buffer, outfile);

        StarParserFree(&p);
        
        StarTokensFree(tokens, nTokens);
        free(s);
        free(buffer);
    }

    if (infile != stdin) {
        fclose(infile);
    }

    if (outfile != stdout) {
        fclose(outfile);
    }

    return 0;
}

#endif // STARC_NO_MAIN