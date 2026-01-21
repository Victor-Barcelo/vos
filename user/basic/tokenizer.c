/*
 * Copyright (c) 2006, Adam Dunkels
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#define DEBUG 0

#if DEBUG
#define DEBUG_PRINTF(...)  printf(__VA_ARGS__)
#else
#define DEBUG_PRINTF(...)
#endif

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "tokenizer.h"

static char const *ptr, *nextptr;

#define MAX_NUMLEN 10

struct keyword_token {
  char *keyword;
  int token;
};

static int current_token = TOKENIZER_ERROR;

static const struct keyword_token keywords[] = {
  {"let", TOKENIZER_LET},
  {"print", TOKENIZER_PRINT},
  {"input", TOKENIZER_INPUT},
  {"dim", TOKENIZER_DIM},
  {"if", TOKENIZER_IF},
  {"then", TOKENIZER_THEN},
  {"else", TOKENIZER_ELSE},
  {"for", TOKENIZER_FOR},
  {"to", TOKENIZER_TO},
  {"next", TOKENIZER_NEXT},
  {"goto", TOKENIZER_GOTO},
  {"gosub", TOKENIZER_GOSUB},
  {"return", TOKENIZER_RETURN},
  {"call", TOKENIZER_CALL},
  {"rnd", TOKENIZER_RND},
  {"graphics", TOKENIZER_GRAPHICS},
  {"text", TOKENIZER_TEXT},
  {"cls", TOKENIZER_CLS},
  {"pset", TOKENIZER_PSET},
  {"line", TOKENIZER_LINE},
  {"end", TOKENIZER_END},
  {NULL, TOKENIZER_ERROR}
};

static bool is_kw_boundary(char c) {
  if(c == 0) return true;
  if(c == ' ' || c == '\t' || c == '\n' || c == '\r') return true;
  if(c == ',' || c == ';') return true;
  if(c == '(' || c == ')') return true;
  if(c == '"' || c == '=' || c == '+' || c == '-' || c == '*' || c == '/' || c == '%') return true;
  if(c == '<' || c == '>' || c == '&' || c == '|') return true;
  return false;
}

static int kw_match(const char* s, const char* kw) {
  int i = 0;
  for(; kw[i] != 0; i++) {
    char a = (char)tolower((unsigned char)s[i]);
    char b = kw[i];
    if(a != b) {
      return 0;
    }
  }
  return is_kw_boundary(s[i]);
}

/*---------------------------------------------------------------------------*/
static int
singlechar(void)
{
  if(*ptr == '\n') {
    return TOKENIZER_CR;
  } else if(*ptr == ',') {
    return TOKENIZER_COMMA;
  } else if(*ptr == ';') {
    return TOKENIZER_SEMICOLON;
  } else if(*ptr == '+') {
    return TOKENIZER_PLUS;
  } else if(*ptr == '-') {
    return TOKENIZER_MINUS;
  } else if(*ptr == '&') {
    return TOKENIZER_AND;
  } else if(*ptr == '|') {
    return TOKENIZER_OR;
  } else if(*ptr == '*') {
    return TOKENIZER_ASTR;
  } else if(*ptr == '/') {
    return TOKENIZER_SLASH;
  } else if(*ptr == '%') {
    return TOKENIZER_MOD;
  } else if(*ptr == '(') {
    return TOKENIZER_LEFTPAREN;
  } else if(*ptr == ')') {
    return TOKENIZER_RIGHTPAREN;
  } else if(*ptr == '<') {
    return TOKENIZER_LT;
  } else if(*ptr == '>') {
    return TOKENIZER_GT;
  } else if(*ptr == '=') {
    return TOKENIZER_EQ;
  }
  return 0;
}
/*---------------------------------------------------------------------------*/
static int
get_next_token(void)
{
  struct keyword_token const *kt;
  int i;

  DEBUG_PRINTF("get_next_token(): '%s'\n", ptr);

  if(*ptr == 0) {
    return TOKENIZER_ENDOFINPUT;
  }
  
  if(isdigit((unsigned char)*ptr)) {
    for(i = 0; i < MAX_NUMLEN; ++i) {
      if(!isdigit((unsigned char)ptr[i])) {
	if(i > 0) {
	  nextptr = ptr + i;
	  return TOKENIZER_NUMBER;
	} else {
	  DEBUG_PRINTF("get_next_token: error due to too short number\n");
	  return TOKENIZER_ERROR;
	}
      }
      if(!isdigit((unsigned char)ptr[i])) {
	DEBUG_PRINTF("get_next_token: error due to malformed number\n");
	return TOKENIZER_ERROR;
      }
    }
    DEBUG_PRINTF("get_next_token: error due to too long number\n");
    return TOKENIZER_ERROR;
  } else if(singlechar()) {
    nextptr = ptr + 1;
    return singlechar();
  } else if(*ptr == '"') {
    nextptr = ptr;
    do {
      ++nextptr;
    } while(*nextptr != '"' && *nextptr != 0);
    if(*nextptr != '"') {
      return TOKENIZER_ERROR;
    }
    ++nextptr;
    return TOKENIZER_STRING;
  } else {
    for(kt = keywords; kt->keyword != NULL; ++kt) {
      if(kw_match(ptr, kt->keyword)) {
        nextptr = ptr + strlen(kt->keyword);
        return kt->token;
      }
    }
  }

  if(isalpha((unsigned char)*ptr)) {
    if(ptr[1] == '$') {
      nextptr = ptr + 2;
      return TOKENIZER_STRINGVAR;
    }
    nextptr = ptr + 1;
    return TOKENIZER_VARIABLE;
  }

  
  return TOKENIZER_ERROR;
}
/*---------------------------------------------------------------------------*/
void
tokenizer_init(const char *program)
{
  ptr = program;
  current_token = get_next_token();
}
/*---------------------------------------------------------------------------*/
int
tokenizer_token(void)
{
  return current_token;
}
/*---------------------------------------------------------------------------*/
void
tokenizer_next(void)
{

  if(tokenizer_finished()) {
    return;
  }

  DEBUG_PRINTF("tokenizer_next: %p\n", nextptr);
  ptr = nextptr;
  while(*ptr == ' ' || *ptr == '\t') {
    ++ptr;
  }
  current_token = get_next_token();
  DEBUG_PRINTF("tokenizer_next: '%s' %d\n", ptr, current_token);
  return;
}
/*---------------------------------------------------------------------------*/
int
tokenizer_num(void)
{
  return atoi(ptr);
}
/*---------------------------------------------------------------------------*/
void
tokenizer_string(char *dest, int len)
{
  char *string_end;
  int string_len;
  
  if(tokenizer_token() != TOKENIZER_STRING) {
    return;
  }
  string_end = strchr(ptr + 1, '"');
  if(string_end == NULL) {
    return;
  }
  string_len = string_end - ptr - 1;
  if(len < string_len) {
    string_len = len;
  }
  memcpy(dest, ptr + 1, string_len);
  dest[string_len] = 0;
}
/*---------------------------------------------------------------------------*/
void
tokenizer_error_print(void)
{
  DEBUG_PRINTF("tokenizer_error_print: '%s'\n", ptr);
}
/*---------------------------------------------------------------------------*/
int
tokenizer_finished(void)
{
  return *ptr == 0 || current_token == TOKENIZER_ENDOFINPUT;
}
/*---------------------------------------------------------------------------*/
int
tokenizer_variable_num(void)
{
  return tolower((unsigned char)*ptr) - 'a';
}
/*---------------------------------------------------------------------------*/
