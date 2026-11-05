#include "include/lexer.h"
#include "include/parse.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <errno.h>

#define MAX(a,b) ((a) > (b) ? (a) : (b))

typedef struct Tokens {
    size_t tokenCapacity;
    size_t tokenCount;
    StarToken* tokens;
} Tokens;

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

int StarLexicalize(const char* str, size_t maxStr, StarToken** outTokens, size_t* outNtokens) {
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

    /* Track text we don't care about. Flushed and set to NULL if we matched with anything */
    const char* miscTextBegin = NULL;
    while (*str && str < max)
    {
        // fprintf(stderr, "%s\n", str);
        while (isspace(*str) && *str && str < max) str++;

        if (*str == '#') {
            /* Don't flush Misc */
            if (!miscTextBegin) miscTextBegin = str;
            
            str++;

            while (*str && *str != '\n' && *str != '\r') {
                str++;
                if (*str == '\\') {
                    str++;
                    while (isspace(*str) && str && str < max) str++;
                    str--;
                }
            }
            str++; // \n
            while (isspace(*str) && *str && str < max) str++;
        }

        else if (*str == ';')
        {
            if (miscTextBegin != NULL) {
                const char *miscTextEnd = str;
                TK_APPEND(&tks, (StarToken){ .type = STAR_TOKEN_TYPE_MISC, .begin = miscTextBegin, .end = miscTextEnd });
                
                miscTextBegin = NULL;
            }
            TK_APPEND(&tks, (StarToken){ .type = STAR_TOKEN_TYPE_SEMI_COLON });
            str++;
        }
        else if (*str == '(')
        {
            if (miscTextBegin != NULL) {
                const char *miscTextEnd = str;
                TK_APPEND(&tks, (StarToken){ .type = STAR_TOKEN_TYPE_MISC, .begin = miscTextBegin, .end = miscTextEnd });
                
                miscTextBegin = NULL;
            }
            TK_APPEND(&tks, (StarToken){ .type = STAR_TOKEN_TYPE_OPEN_PAREN });
            str++;
        }
        else if (*str == ')')
        {
            if (miscTextBegin != NULL) {
                const char *miscTextEnd = str;
                TK_APPEND(&tks, (StarToken){ .type = STAR_TOKEN_TYPE_MISC, .begin = miscTextBegin, .end = miscTextEnd });
                
                miscTextBegin = NULL;
            }
            TK_APPEND(&tks, (StarToken){ .type = STAR_TOKEN_TYPE_CLOSE_PAREN });
            str++;
        }
        else if (*str == '{')
        {
            if (miscTextBegin != NULL) {
                const char *miscTextEnd = str;
                TK_APPEND(&tks, (StarToken){ .type = STAR_TOKEN_TYPE_MISC, .begin = miscTextBegin, .end = miscTextEnd });
                
                miscTextBegin = NULL;
            }
            TK_APPEND(&tks, (StarToken){ .type = STAR_TOKEN_TYPE_OPEN_BRACE });
            str++;
        }
        else if (*str == '}')
        {
            if (miscTextBegin != NULL) {
                const char *miscTextEnd = str;
                TK_APPEND(&tks, (StarToken){ .type = STAR_TOKEN_TYPE_MISC, .begin = miscTextBegin, .end = miscTextEnd });
                
                miscTextBegin = NULL;
            }
            TK_APPEND(&tks, (StarToken){ .type = STAR_TOKEN_TYPE_CLOSE_BRACE });
            str++;
        }
        
        else if (strncmp(str, "break", 5) == 0)
        {
            if (miscTextBegin != NULL) {
                const char *miscTextEnd = str;
                TK_APPEND(&tks, (StarToken){ .type = STAR_TOKEN_TYPE_MISC, .begin = miscTextBegin, .end = miscTextEnd });
                
                miscTextBegin = NULL;
            }
            
            str += 5;
            while (*str && isspace(*str)) str++;

            TK_APPEND(&tks, (StarToken){ .type = STAR_TOKEN_TYPE_BREAK });
        }
        
        else if (strncmp(str, "continue", strlen("continue")) == 0)
        {
            if (miscTextBegin != NULL) {
                const char *miscTextEnd = str;
                TK_APPEND(&tks, (StarToken){ .type = STAR_TOKEN_TYPE_MISC, .begin = miscTextBegin, .end = miscTextEnd });
                
                miscTextBegin = NULL;
            }
            
            str += strlen("continue");
            while (*str && isspace(*str)) str++;
            
            TK_APPEND(&tks, (StarToken){ .type = STAR_TOKEN_TYPE_CONTINUE });
        }
        
        else if (strncmp(str, "return", strlen("return")) == 0)
        {
            if (miscTextBegin != NULL) {
                const char *miscTextEnd = str;
                TK_APPEND(&tks, (StarToken){ .type = STAR_TOKEN_TYPE_MISC, .begin = miscTextBegin, .end = miscTextEnd });
                
                miscTextBegin = NULL;
            }

            str += strlen("return");
            while (*str && isspace(*str)) str++;

            TK_APPEND(&tks, (StarToken){ .type = STAR_TOKEN_TYPE_RETURN });
        }
        
        else if (*str == '"') {
            /* Don't flush Misc */
            if (!miscTextBegin) miscTextBegin = str;

            str++;
            while (*str && str < max) {
                if (*str == '\\' && (str+1) < max) {
                    str++;
                }
                str++;
                if (*str == '"') {
                    break;
                }
            }
            str++;
        }
        else if (strncmp(str, "//", 2) == 0) {
            /* Don't flush Misc */
            if (!miscTextBegin) miscTextBegin = str;
            
            str += 2;

            while (*str && *str != '\n' && *str != '\r') str++;
            while (*str && isspace(*str)) str++;
        }
        else if (strncmp(str, "/*", 2) == 0) {
            /* Don't flush Misc */
            if (!miscTextBegin) miscTextBegin = str;
            
            str += 2;

            i64 depth = 0;
            while (*str) {
                if (strncmp(str, "*/", 2) == 0) {
                    str++;
                    str++;
                    if (depth == 0) break;
                    else depth--;
                }
                else if (strncmp(str, "/*", 2) == 0) {
                    depth++;
                    str++;
                    str++;
                }
                else str++;
            }
            while (*str && isspace(*str)) str++;
        }
        
        else if (strncmp(str, "for", 3) == 0) {
            if (miscTextBegin != NULL) {
                const char *miscTextEnd = str;
                TK_APPEND(&tks, (StarToken){ .type = STAR_TOKEN_TYPE_MISC, .begin = miscTextBegin, .end = miscTextEnd });
                
                miscTextBegin = NULL;
            }

            str += 3;
            
            while (isspace(*str) && *str && str < max) str++;
            

            TK_APPEND(&tks,
                (StarToken){
                    .type = STAR_TOKEN_TYPE_FOR_LOOP,
                    /*
                    .forInitializersBegin = initializersBegin,
                    .forInitializersEnd = initializersEnd,
                    .conditionsBegin = conditionsBegin,
                    .conditionsEnd = conditionsEnd,
                    .forIteratorsBegin = iteratorsBegin,
                    .forIteratorsEnd = iteratorsEnd,
                    .begin = bodyBegin,
                    .end = bodyEnd,
                    */
             });
        }

        else if (strncmp(str, "if", 2) == 0) {
            if (miscTextBegin != NULL) {
                const char *miscTextEnd = str;
                TK_APPEND(&tks, (StarToken){ .type = STAR_TOKEN_TYPE_MISC, .begin = miscTextBegin, .end = miscTextEnd });
                
                miscTextBegin = NULL;
            }

            str += 2;
            
            while (isspace(*str) && *str && str < max) str++;
            

            TK_APPEND(&tks,
                (StarToken){
                    .type = STAR_TOKEN_TYPE_IF,
             });
        }
        else if (strncmp(str, "else", 4) == 0) {
            if (miscTextBegin != NULL) {
                const char *miscTextEnd = str;
                TK_APPEND(&tks, (StarToken){ .type = STAR_TOKEN_TYPE_MISC, .begin = miscTextBegin, .end = miscTextEnd });
                
                miscTextBegin = NULL;
            }

            str += 4;
            
            while (isspace(*str) && *str && str < max) str++;
            

            TK_APPEND(&tks,
                (StarToken){
                    .type = STAR_TOKEN_TYPE_ELSE,
             });
        }
        else if (strncmp(str, "defer", 5) == 0) {
            if (miscTextBegin != NULL) {
                const char *miscTextEnd = str;
                TK_APPEND(&tks, (StarToken){ .type = STAR_TOKEN_TYPE_MISC, .begin = miscTextBegin, .end = miscTextEnd });
                
                miscTextBegin = NULL;
            }

            str += 5;
            
            while (isspace(*str) && *str && str < max) str++;

            const char* begin = str;
            
            i64 depth = 0;
            if (*str == '{')
            {
                while (*str && str < max) {
                    str++;

                    if (*str == '{') { depth++; }
                    else if (*str == '}') { 
                        if (depth == 0) break;
                        else depth--;
                    }
                }
            } else {
                while (*str && str < max && *str != ';') str++;
                if (!*str) return -1;
            }

            str++; // ;/}, include it in the defer text
            const char* end = str;

            TK_APPEND(&tks,
                (StarToken){
                    .type = STAR_TOKEN_TYPE_DEFER,
                    .begin = begin,
                    .end = end,
             });
        }
        else if (strncmp(str, "while", 5) == 0) {
            if (miscTextBegin != NULL) {
                const char *miscTextEnd = str;
                TK_APPEND(&tks, (StarToken){ .type = STAR_TOKEN_TYPE_MISC, .begin = miscTextBegin, .end = miscTextEnd });
                
                miscTextBegin = NULL;
            }

            str += 5;
            
            while (isspace(*str) && *str && str < max) str++;
            
            TK_APPEND(&tks,
                (StarToken){
                    .type = STAR_TOKEN_TYPE_WHILE_LOOP,
                    /*
                    .conditionsBegin = conditionsBegin,
                    .conditionsEnd = conditionsEnd,
                    .begin = bodyBegin,
                    .end = bodyEnd,
                    */
             });
        }
        else if (strncmp(str, "do", 2) == 0) {
            if (miscTextBegin != NULL) {
                const char *miscTextEnd = str;
                TK_APPEND(&tks, (StarToken){ .type = STAR_TOKEN_TYPE_MISC, .begin = miscTextBegin, .end = miscTextEnd });
                
                miscTextBegin = NULL;
            }

            str += 2;
            
            while (isspace(*str) && *str && str < max) str++;
            
            TK_APPEND(&tks,
                (StarToken){
                    .type = STAR_TOKEN_TYPE_DO,
             });
        }

        /* Must always be after the keyword cases */
        else if (isalpha(*str) || *str == '_') {
            if (miscTextBegin != NULL) {
                const char *miscTextEnd = str;
                TK_APPEND(&tks, (StarToken){ .type = STAR_TOKEN_TYPE_MISC, .begin = miscTextBegin, .end = miscTextEnd });
                
                miscTextBegin = NULL;
            }

            const char* beg = str;
            const char* end = str;
            
            while (*end && end < max && (isalnum(*end) || *end == '_')) {
                end++;
            }
            
            TK_APPEND(&tks, (StarToken){ .type = STAR_TOKEN_TYPE_IDENT, .begin = beg, .end = end });
            
            str = end;
        }

        else { /* Don't set if we're already tracking */
            if (!miscTextBegin) miscTextBegin = str;
            str++;
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
            u32 j = i+1;
            while (tks.tokens[j].type != STAR_TOKEN_TYPE_SEMI_COLON) j++;
            j++;

            assert(j < tks.tokenCount);
            assert(tks.tokens[j].type == STAR_TOKEN_TYPE_WHILE_LOOP);

            tks.tokens[j].type = STAR_TOKEN_TYPE_WHILE_PATCHED; /* patched */
        } else {
            /**
              * Do a simple depth search of braces
              */
            i64 depth = 0;
            u32 j = i+1;
            for (; j < tks.tokenCount; j++) {
                StarToken* fwd = &tks.tokens[j];
    
                if (fwd->type == STAR_TOKEN_TYPE_OPEN_BRACE) {
                    depth++;
                }
                else if (fwd->type == STAR_TOKEN_TYPE_CLOSE_BRACE) {
                    depth--;
                    if (depth == 0) break;
                }
            }
            // fprintf(stderr, "glooby: ");
            // StarTokenPrint(stderr, &tks.tokens[j+1]);

            assert(tks.tokens[j+1].type == STAR_TOKEN_TYPE_WHILE_LOOP);
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
            // StarTokenPrint(stderr, &tks.tokens[i+1]);
            // fputc('\n', stderr);

            u32 j = i+2;
            while (tks.tokens[j].type != STAR_TOKEN_TYPE_SEMI_COLON && j < tks.tokenCount) j++;
            tks.tokens[j].flags |= STAR_TOKEN_SUFFIX_CLOSE_BRACE;

            continue;
        }
        if (type != STAR_TOKEN_TYPE_IF && type != STAR_TOKEN_TYPE_WHILE_LOOP && type != STAR_TOKEN_TYPE_FOR_LOOP) { continue; }

        i++;
        assert(tks.tokens[i].type == STAR_TOKEN_TYPE_OPEN_PAREN);

        /* Find index of closing parenthesis*/
        i64 depth = 0;
        u32 j = i+1;
        for (; j < tks.tokenCount; j++) {
            StarToken* peek = &tks.tokens[j];
            if (peek->type == STAR_TOKEN_TYPE_OPEN_PAREN)
                depth++;
            else if (peek->type == STAR_TOKEN_TYPE_CLOSE_PAREN) {
                if (depth == 0) break;
                else depth--;
            }
        }

        j++;
        assert(j < tks.tokenCount);

        /* already braced, skip */
        if (tks.tokens[j].type == STAR_TOKEN_TYPE_OPEN_BRACE) continue;
        
        /* else, suffix the close parenthesis with an open brace */
        tks.tokens[j-1].flags |= STAR_TOKEN_SUFFIX_OPEN_BRACE;

        // StarTokenPrint(stderr, &tks.tokens[j-1]);
        // fputc('\n', stderr);
        
        /* find the semicolon (statement end) and suffix it with the close brace*/
        while (tks.tokens[j].type != STAR_TOKEN_TYPE_SEMI_COLON && j < tks.tokenCount) j++;
        if (j >= tks.tokenCount) return -1;
        
        // StarTokenPrint(stderr, &tks.tokens[j]);
        // fputc('\n', stderr);

        /* suffix semicolon with close brace */
        tks.tokens[j].flags |= STAR_TOKEN_SUFFIX_CLOSE_BRACE;
    }
    
    if (outTokens) *outTokens = tks.tokens; else free(tks.tokens);
    if (outNtokens) *outNtokens = tks.tokenCount;
    
    if (miscTextBegin != NULL) {
        const char *miscTextEnd = str;
        TK_APPEND(&tks, (StarToken){ .type = STAR_TOKEN_TYPE_MISC, .begin = miscTextBegin, .end = miscTextEnd });
        
        miscTextBegin = NULL;
    }
    
    return 0;
}

void StarTokenPrint(FILE* f, const StarToken *tk) {
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
    // STAR_TOKEN_TYPE_DEFER
    else if (tk->type == STAR_TOKEN_TYPE_IDENT || tk->type == STAR_TOKEN_TYPE_MISC) {
        fprintf(f, " %.*s ", (int)(tk->end - tk->begin), tk->begin);
    }
    else if (tk->type == STAR_TOKEN_TYPE_SEMI_COLON) { fprintf(f, ";"); }
    else if (tk->type == STAR_TOKEN_TYPE_OPEN_PAREN) { fprintf(f, " OPEN PAREN( "); }
    else if (tk->type == STAR_TOKEN_TYPE_CLOSE_PAREN) { fprintf(f, " ) "); }
    else if (tk->type == STAR_TOKEN_TYPE_OPEN_BRACE) { fprintf(f, " {\n"); }
    else if (tk->type == STAR_TOKEN_TYPE_CLOSE_BRACE) { fprintf(f, " }\n"); }
    else {
        fprintf(stderr, "Unknown token type: %i\n", tk->type);
    }
}

void StarTokensFree(StarToken* tokens, u32 nTokens)
{
    /* Tokens are just read only copies :3 */
    free(tokens);
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

#define STARC_MAJOR 0
#define STARC_MINOR 1
#define STARC_PATCH 0

int main(int argc, char *argv[]) {
    fprintf(stderr, "StarC parser v%i.%i.%i\n"TERM_YELLOW"WARNING"TERM_RESET": Ensure your main function ends with a return 0, can result in weird behavior otherwise.\n", STARC_MAJOR, STARC_MINOR, STARC_PATCH);

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
        int e = StarLexicalize(s, SIZE_MAX, &tokens, &nTokens);
        assert(e == 0);
        
        // fprintf(stderr, "%zu GLOOBY WARNING GLOOBY WARNING \n", nTokens);
        // for (u32 j = 0; j < nTokens; j++) {
        //     StarTokenPrint(stderr, &tokens[j]);
        // }
        // fprintf(stderr, "%zu GLOOBY NO NO NO GLOOBY NO NO NO n", nTokens);
        
        StarParser p;
        StarParserInit(&p);
    
        char *buffer = NULL;
        assert(StarParse(tokens, nTokens, &p, &buffer, NULL) == 0);

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