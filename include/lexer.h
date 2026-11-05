#ifndef STAR_C_LEXER_H
#define STAR_C_LEXER_H

#include <stdint.h>
#include <stdio.h>

#include "std.h"

typedef enum StarTokenType {
    STAR_TOKEN_TYPE_FOR_LOOP,
    STAR_TOKEN_TYPE_WHILE_LOOP,
    STAR_TOKEN_TYPE_DO,
    STAR_TOKEN_TYPE_IF,
    STAR_TOKEN_TYPE_ELSE,
    STAR_TOKEN_TYPE_RETURN,
    STAR_TOKEN_TYPE_CONTINUE,
    STAR_TOKEN_TYPE_BREAK,
    STAR_TOKEN_TYPE_DEFER,
    STAR_TOKEN_TYPE_OPEN_PAREN, // (
    STAR_TOKEN_TYPE_CLOSE_PAREN, // )
    STAR_TOKEN_TYPE_OPEN_BRACE, // {
    STAR_TOKEN_TYPE_CLOSE_BRACE, // }
    STAR_TOKEN_TYPE_SEMI_COLON, // ;
    
    // Can start with anything. Arbitrary text region.
    STAR_TOKEN_TYPE_MISC,

    // Must start with alphabets or _
    STAR_TOKEN_TYPE_IDENT,

    /* The while in do{}while. Isn't supposed to act as a loop. */
    STAR_TOKEN_TYPE_WHILE_PATCHED,
} StarTokenType;

#define STAR_TOKEN_SUFFIX_OPEN_BRACE 0x1
#define STAR_TOKEN_SUFFIX_CLOSE_BRACE 0x2

typedef struct StarToken {
    StarTokenType type;
    const char* begin; /* The end of the text region: IDENT/MISC */
    const char* end; /* The end of the text region: IDENT/MISC */
    u32 flags; /* Used by the post processor to insert braces */
    size_t line;
    size_t column;
    const char* file;
} StarToken;

/**
  * Lexicalize the given string, reading no more than maxStr.
  * The outTokens array is allocated with malloc and must be freed.
  */
int StarLexicalize(const char* str, size_t maxStr, const char* debugFileName, StarToken** outTokens, size_t* nTokens);

/**
  * Print a given token
  */
void StarTokenPrint(FILE* f, const StarToken *tk);

/* Frees the array too! */
void StarTokensFree(StarToken* tokens, u32 nTokens);

#endif//STAR_C_LEXER_H
