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


#include "ubasic.h"
#include "tokenizer.h"

#include "stdio.h"
#include "stdlib.h"
#include "keyboard.h"
#include "kheap.h"
#include "string.h"
#include "timer.h"

static char const *program_ptr;
#define MAX_STRINGLEN 64
static char string[MAX_STRINGLEN];

#define MAX_GOSUB_STACK_DEPTH 10
static int gosub_stack[MAX_GOSUB_STACK_DEPTH];
static int gosub_stack_ptr;

struct for_state {
  int line_after_for;
  int for_variable;
  int to;
};
#define MAX_FOR_STACK_DEPTH 4
static struct for_state for_stack[MAX_FOR_STACK_DEPTH];
static int for_stack_ptr;

#define MAX_VARNUM 26
static int variables[MAX_VARNUM];
static char string_variables[MAX_VARNUM][MAX_STRINGLEN];

typedef struct {
  int* data;
  int max_index;
} int_array_t;
static int_array_t int_arrays[MAX_VARNUM];

static int ended;

static int expr(void);
static void line_statement(void);
static void statement(void);
static void input_statement(void);
static void dim_statement(void);
static void graphics_statement(void);
static void text_statement(void);
static void cls_statement(void);
static void pset_statement(void);
static void line_draw_statement(void);

static bool graphics_mode;

static uint32_t rnd_state;
static bool rnd_seeded;

static uint32_t ubasic_rand_u32(void) {
  if(!rnd_seeded) {
    rnd_seeded = true;
    rnd_state = (uint32_t)timer_get_ticks() ^ (uint32_t)(uint32_t)program_ptr ^ 0xA5A5A5A5u;
    if(rnd_state == 0) {
      rnd_state = 0x12345678u;
    }
  }
  rnd_state = rnd_state * 1664525u + 1013904223u;
  return rnd_state;
}

static int array_get(int var, int index) {
  if(var < 0 || var >= MAX_VARNUM) {
    return 0;
  }
  if(!int_arrays[var].data || index < 0 || index > int_arrays[var].max_index) {
    return 0;
  }
  return int_arrays[var].data[index];
}

static void array_set(int var, int index, int value) {
  if(var < 0 || var >= MAX_VARNUM) {
    return;
  }
  if(!int_arrays[var].data || index < 0 || index > int_arrays[var].max_index) {
    return;
  }
  int_arrays[var].data[index] = value;
}

static void array_dim(int var, int max_index) {
  if(var < 0 || var >= MAX_VARNUM || max_index < 0) {
    return;
  }

  if(int_arrays[var].data) {
    kfree(int_arrays[var].data);
    int_arrays[var].data = NULL;
    int_arrays[var].max_index = 0;
  }

  uint32_t count = (uint32_t)max_index + 1u;
  int* data = (int*)kcalloc(count, sizeof(int));
  if(!data) {
    return;
  }
  int_arrays[var].data = data;
  int_arrays[var].max_index = max_index;
}
/*---------------------------------------------------------------------------*/
void
ubasic_init(const char *program)
{
  program_ptr = program;
  for_stack_ptr = gosub_stack_ptr = 0;
  for(int i = 0; i < MAX_VARNUM; i++) {
    variables[i] = 0;
    string_variables[i][0] = 0;
    if(int_arrays[i].data) {
      kfree(int_arrays[i].data);
      int_arrays[i].data = NULL;
      int_arrays[i].max_index = 0;
    }
  }
  rnd_state = 0;
  rnd_seeded = false;
  graphics_mode = false;
  tokenizer_init(program);
  ended = 0;
}
/*---------------------------------------------------------------------------*/
static void
accept(int token)
{
  if(token != tokenizer_token()) {
    DEBUG_PRINTF("Token not what was expected (expected %d, got %d)\n",
		 token, tokenizer_token());
    tokenizer_error_print();
    ended = 1;
    return;
  }
  DEBUG_PRINTF("Expected %d, got it\n", token);
  tokenizer_next();
}
/*---------------------------------------------------------------------------*/
static int
varfactor(void)
{
  int r;
  int var = tokenizer_variable_num();
  accept(TOKENIZER_VARIABLE);

  if(tokenizer_token() == TOKENIZER_LEFTPAREN) {
    accept(TOKENIZER_LEFTPAREN);
    int idx = expr();
    accept(TOKENIZER_RIGHTPAREN);
    r = array_get(var, idx);
    return r;
  }

  DEBUG_PRINTF("varfactor: obtaining %d from variable %d\n", variables[var], var);
  r = ubasic_get_variable(var);
  return r;
}
/*---------------------------------------------------------------------------*/
static int
factor(void)
{
  int r;

  DEBUG_PRINTF("factor: token %d\n", tokenizer_token());
  switch(tokenizer_token()) {
  case TOKENIZER_NUMBER:
    r = tokenizer_num();
    DEBUG_PRINTF("factor: number %d\n", r);
    accept(TOKENIZER_NUMBER);
    break;
  case TOKENIZER_RND: {
    accept(TOKENIZER_RND);
    int max = 32768;
    if(tokenizer_token() == TOKENIZER_LEFTPAREN) {
      accept(TOKENIZER_LEFTPAREN);
      max = expr();
      accept(TOKENIZER_RIGHTPAREN);
    }
    if(max <= 0) {
      r = 0;
    } else {
      r = (int)(ubasic_rand_u32() % (uint32_t)max);
    }
    break;
  }
  case TOKENIZER_LEFTPAREN:
    accept(TOKENIZER_LEFTPAREN);
    r = expr();
    accept(TOKENIZER_RIGHTPAREN);
    break;
  default:
    r = varfactor();
    break;
  }
  return r;
}
/*---------------------------------------------------------------------------*/
static int
term(void)
{
  int f1, f2;
  int op;

  f1 = factor();
  op = tokenizer_token();
  DEBUG_PRINTF("term: token %d\n", op);
  while(op == TOKENIZER_ASTR ||
	op == TOKENIZER_SLASH ||
	op == TOKENIZER_MOD) {
    tokenizer_next();
    f2 = factor();
    DEBUG_PRINTF("term: %d %d %d\n", f1, op, f2);
    switch(op) {
    case TOKENIZER_ASTR:
      f1 = f1 * f2;
      break;
    case TOKENIZER_SLASH:
      f1 = f1 / f2;
      break;
    case TOKENIZER_MOD:
      f1 = f1 % f2;
      break;
    }
    op = tokenizer_token();
  }
  DEBUG_PRINTF("term: %d\n", f1);
  return f1;
}
/*---------------------------------------------------------------------------*/
static int
expr(void)
{
  int t1, t2;
  int op;
  
  t1 = term();
  op = tokenizer_token();
  DEBUG_PRINTF("expr: token %d\n", op);
  while(op == TOKENIZER_PLUS ||
	op == TOKENIZER_MINUS ||
	op == TOKENIZER_AND ||
	op == TOKENIZER_OR) {
    tokenizer_next();
    t2 = term();
    DEBUG_PRINTF("expr: %d %d %d\n", t1, op, t2);
    switch(op) {
    case TOKENIZER_PLUS:
      t1 = t1 + t2;
      break;
    case TOKENIZER_MINUS:
      t1 = t1 - t2;
      break;
    case TOKENIZER_AND:
      t1 = t1 & t2;
      break;
    case TOKENIZER_OR:
      t1 = t1 | t2;
      break;
    }
    op = tokenizer_token();
  }
  DEBUG_PRINTF("expr: %d\n", t1);
  return t1;
}
/*---------------------------------------------------------------------------*/
static int
relation(void)
{
  int r1, r2;
  int op;
  
  r1 = expr();
  op = tokenizer_token();
  DEBUG_PRINTF("relation: token %d\n", op);
  while(op == TOKENIZER_LT ||
	op == TOKENIZER_GT ||
	op == TOKENIZER_EQ) {
    tokenizer_next();
    r2 = expr();
    DEBUG_PRINTF("relation: %d %d %d\n", r1, op, r2);
    switch(op) {
    case TOKENIZER_LT:
      r1 = r1 < r2;
      break;
    case TOKENIZER_GT:
      r1 = r1 > r2;
      break;
    case TOKENIZER_EQ:
      r1 = r1 == r2;
      break;
    }
    op = tokenizer_token();
  }
  return r1;
}
/*---------------------------------------------------------------------------*/
static void
jump_linenum(int linenum)
{
  tokenizer_init(program_ptr);
  while(tokenizer_num() != linenum) {
    do {
      do {
	tokenizer_next();
      } while(tokenizer_token() != TOKENIZER_CR &&
	      tokenizer_token() != TOKENIZER_ENDOFINPUT);
      if(tokenizer_token() == TOKENIZER_CR) {
	tokenizer_next();
      }
    } while(tokenizer_token() != TOKENIZER_NUMBER);
    DEBUG_PRINTF("jump_linenum: Found line %d\n", tokenizer_num());
  }
}
/*---------------------------------------------------------------------------*/
static void
goto_statement(void)
{
  accept(TOKENIZER_GOTO);
  jump_linenum(tokenizer_num());
}
/*---------------------------------------------------------------------------*/
static void
print_statement(void)
{
  accept(TOKENIZER_PRINT);
  do {
    DEBUG_PRINTF("Print loop\n");
    if(tokenizer_token() == TOKENIZER_STRING) {
      tokenizer_string(string, sizeof(string));
      printf("%s", string);
      tokenizer_next();
    } else if(tokenizer_token() == TOKENIZER_STRINGVAR) {
      int var = tokenizer_variable_num();
      accept(TOKENIZER_STRINGVAR);
      printf("%s", string_variables[var]);
    } else if(tokenizer_token() == TOKENIZER_COMMA) {
      printf(" ");
      tokenizer_next();
    } else if(tokenizer_token() == TOKENIZER_SEMICOLON) {
      tokenizer_next();
    } else if(tokenizer_token() == TOKENIZER_VARIABLE ||
	      tokenizer_token() == TOKENIZER_NUMBER) {
      printf("%d", expr());
    } else {
      break;
    }
  } while(tokenizer_token() != TOKENIZER_CR &&
	  tokenizer_token() != TOKENIZER_ENDOFINPUT);
  printf("\n");
  DEBUG_PRINTF("End of print\n");
  tokenizer_next();
}
/*---------------------------------------------------------------------------*/
static void
if_statement(void)
{
  int r;
  
  accept(TOKENIZER_IF);

  r = relation();
  DEBUG_PRINTF("if_statement: relation %d\n", r);
  accept(TOKENIZER_THEN);
  if(r) {
    statement();
  } else {
    do {
      tokenizer_next();
    } while(tokenizer_token() != TOKENIZER_ELSE &&
	    tokenizer_token() != TOKENIZER_CR &&
	    tokenizer_token() != TOKENIZER_ENDOFINPUT);
    if(tokenizer_token() == TOKENIZER_ELSE) {
      tokenizer_next();
      statement();
    } else if(tokenizer_token() == TOKENIZER_CR) {
      tokenizer_next();
    }
  }
}
/*---------------------------------------------------------------------------*/
static void
assign_statement(void)
{
  if(tokenizer_token() == TOKENIZER_STRINGVAR) {
    int var = tokenizer_variable_num();
    accept(TOKENIZER_STRINGVAR);
    accept(TOKENIZER_EQ);

    if(tokenizer_token() == TOKENIZER_STRING) {
      tokenizer_string(string, sizeof(string));
      accept(TOKENIZER_STRING);
      strncpy(string_variables[var], string, MAX_STRINGLEN - 1);
      string_variables[var][MAX_STRINGLEN - 1] = 0;
    } else if(tokenizer_token() == TOKENIZER_STRINGVAR) {
      int src = tokenizer_variable_num();
      accept(TOKENIZER_STRINGVAR);
      strncpy(string_variables[var], string_variables[src], MAX_STRINGLEN - 1);
      string_variables[var][MAX_STRINGLEN - 1] = 0;
    } else {
      ended = 1;
      return;
    }

    accept(TOKENIZER_CR);
    return;
  }

  int var = tokenizer_variable_num();
  accept(TOKENIZER_VARIABLE);

  bool is_array = false;
  int idx = 0;
  if(tokenizer_token() == TOKENIZER_LEFTPAREN) {
    is_array = true;
    accept(TOKENIZER_LEFTPAREN);
    idx = expr();
    accept(TOKENIZER_RIGHTPAREN);
  }

  accept(TOKENIZER_EQ);
  int value = expr();
  if(is_array) {
    array_set(var, idx, value);
  } else {
    ubasic_set_variable(var, value);
  }

  DEBUG_PRINTF("assign_statement: assign %d to %d\n", value, var);
  accept(TOKENIZER_CR);

}
/*---------------------------------------------------------------------------*/
static void
input_statement(void)
{
  accept(TOKENIZER_INPUT);

  if(tokenizer_token() == TOKENIZER_STRING) {
    tokenizer_string(string, sizeof(string));
    printf("%s", string);
    accept(TOKENIZER_STRING);
    if(tokenizer_token() == TOKENIZER_COMMA) {
      accept(TOKENIZER_COMMA);
    }
  }

  do {
    printf("? ");
    char line[64];
    keyboard_getline(line, sizeof(line));

    if(tokenizer_token() == TOKENIZER_STRINGVAR) {
      int var = tokenizer_variable_num();
      accept(TOKENIZER_STRINGVAR);
      strncpy(string_variables[var], line, MAX_STRINGLEN - 1);
      string_variables[var][MAX_STRINGLEN - 1] = 0;
    } else {
      int var = tokenizer_variable_num();
      accept(TOKENIZER_VARIABLE);
      bool is_array = false;
      int idx = 0;
      if(tokenizer_token() == TOKENIZER_LEFTPAREN) {
        is_array = true;
        accept(TOKENIZER_LEFTPAREN);
        idx = expr();
        accept(TOKENIZER_RIGHTPAREN);
      }
      int value = atoi(line);
      if(is_array) {
        array_set(var, idx, value);
      } else {
        ubasic_set_variable(var, value);
      }
    }

    if(tokenizer_token() == TOKENIZER_COMMA) {
      accept(TOKENIZER_COMMA);
    } else {
      break;
    }
  } while(1);

  accept(TOKENIZER_CR);
}
/*---------------------------------------------------------------------------*/
static void
dim_statement(void)
{
  accept(TOKENIZER_DIM);

  do {
    int var = tokenizer_variable_num();
    accept(TOKENIZER_VARIABLE);
    accept(TOKENIZER_LEFTPAREN);
    int size = expr();
    accept(TOKENIZER_RIGHTPAREN);
    if(size < 0) {
      ended = 1;
      return;
    }
    array_dim(var, size);

    if(tokenizer_token() == TOKENIZER_COMMA) {
      accept(TOKENIZER_COMMA);
    } else {
      break;
    }
  } while(1);

  accept(TOKENIZER_CR);
}
/*---------------------------------------------------------------------------*/
static void
graphics_statement(void)
{
  accept(TOKENIZER_GRAPHICS);

  int bg = 0;
  if(tokenizer_token() != TOKENIZER_CR) {
    bg = expr();
  }
  screen_graphics_clear((uint8_t)bg);
  graphics_mode = true;
  screen_cursor_set_enabled(false);
  accept(TOKENIZER_CR);
}
/*---------------------------------------------------------------------------*/
static void
text_statement(void)
{
  accept(TOKENIZER_TEXT);
  graphics_mode = false;
  screen_cursor_set_enabled(true);
  accept(TOKENIZER_CR);
}
/*---------------------------------------------------------------------------*/
static void
cls_statement(void)
{
  accept(TOKENIZER_CLS);
  if(graphics_mode && screen_is_framebuffer()) {
    screen_graphics_clear(0);
  } else {
    screen_clear();
  }
  accept(TOKENIZER_CR);
}
/*---------------------------------------------------------------------------*/
static void
pset_statement(void)
{
  accept(TOKENIZER_PSET);

  int x = expr();
  accept(TOKENIZER_COMMA);
  int y = expr();
  int c = 15;
  if(tokenizer_token() == TOKENIZER_COMMA) {
    accept(TOKENIZER_COMMA);
    c = expr();
  }

  (void)screen_graphics_putpixel(x, y, (uint8_t)c);
  accept(TOKENIZER_CR);
}
/*---------------------------------------------------------------------------*/
static void
line_draw_statement(void)
{
  accept(TOKENIZER_LINE);

  int x0 = expr();
  accept(TOKENIZER_COMMA);
  int y0 = expr();
  accept(TOKENIZER_COMMA);
  int x1 = expr();
  accept(TOKENIZER_COMMA);
  int y1 = expr();
  int c = 15;
  if(tokenizer_token() == TOKENIZER_COMMA) {
    accept(TOKENIZER_COMMA);
    c = expr();
  }

  (void)screen_graphics_line(x0, y0, x1, y1, (uint8_t)c);
  accept(TOKENIZER_CR);
}
/*---------------------------------------------------------------------------*/
static void
gosub_statement(void)
{
  int linenum;
  accept(TOKENIZER_GOSUB);
  linenum = tokenizer_num();
  accept(TOKENIZER_NUMBER);
  accept(TOKENIZER_CR);
  if(gosub_stack_ptr < MAX_GOSUB_STACK_DEPTH) {
    gosub_stack[gosub_stack_ptr] = tokenizer_num();
    gosub_stack_ptr++;
    jump_linenum(linenum);
  } else {
    DEBUG_PRINTF("gosub_statement: gosub stack exhausted\n");
  }
}
/*---------------------------------------------------------------------------*/
static void
return_statement(void)
{
  accept(TOKENIZER_RETURN);
  if(gosub_stack_ptr > 0) {
    gosub_stack_ptr--;
    jump_linenum(gosub_stack[gosub_stack_ptr]);
  } else {
    DEBUG_PRINTF("return_statement: non-matching return\n");
  }
}
/*---------------------------------------------------------------------------*/
static void
next_statement(void)
{
  int var;
  
  accept(TOKENIZER_NEXT);
  var = tokenizer_variable_num();
  accept(TOKENIZER_VARIABLE);
  if(for_stack_ptr > 0 &&
     var == for_stack[for_stack_ptr - 1].for_variable) {
    ubasic_set_variable(var,
			ubasic_get_variable(var) + 1);
    if(ubasic_get_variable(var) <= for_stack[for_stack_ptr - 1].to) {
      jump_linenum(for_stack[for_stack_ptr - 1].line_after_for);
    } else {
      for_stack_ptr--;
      accept(TOKENIZER_CR);
    }
  } else {
    DEBUG_PRINTF("next_statement: non-matching next (expected %d, found %d)\n", for_stack[for_stack_ptr - 1].for_variable, var);
    accept(TOKENIZER_CR);
  }

}
/*---------------------------------------------------------------------------*/
static void
for_statement(void)
{
  int for_variable, to;
  
  accept(TOKENIZER_FOR);
  for_variable = tokenizer_variable_num();
  accept(TOKENIZER_VARIABLE);
  accept(TOKENIZER_EQ);
  ubasic_set_variable(for_variable, expr());
  accept(TOKENIZER_TO);
  to = expr();
  accept(TOKENIZER_CR);

  if(for_stack_ptr < MAX_FOR_STACK_DEPTH) {
    for_stack[for_stack_ptr].line_after_for = tokenizer_num();
    for_stack[for_stack_ptr].for_variable = for_variable;
    for_stack[for_stack_ptr].to = to;
    DEBUG_PRINTF("for_statement: new for, var %d to %d\n",
		 for_stack[for_stack_ptr].for_variable,
		 for_stack[for_stack_ptr].to);
		 
    for_stack_ptr++;
  } else {
    DEBUG_PRINTF("for_statement: for stack depth exceeded\n");
  }
}
/*---------------------------------------------------------------------------*/
static void
end_statement(void)
{
  accept(TOKENIZER_END);
  ended = 1;
}
/*---------------------------------------------------------------------------*/
static void
statement(void)
{
  int token;
  
  token = tokenizer_token();
  
  switch(token) {
  case TOKENIZER_PRINT:
    print_statement();
    break;
  case TOKENIZER_GRAPHICS:
    graphics_statement();
    break;
  case TOKENIZER_TEXT:
    text_statement();
    break;
  case TOKENIZER_CLS:
    cls_statement();
    break;
  case TOKENIZER_PSET:
    pset_statement();
    break;
  case TOKENIZER_LINE:
    line_draw_statement();
    break;
  case TOKENIZER_INPUT:
    input_statement();
    break;
  case TOKENIZER_DIM:
    dim_statement();
    break;
  case TOKENIZER_IF:
    if_statement();
    break;
  case TOKENIZER_GOTO:
    goto_statement();
    break;
  case TOKENIZER_GOSUB:
    gosub_statement();
    break;
  case TOKENIZER_RETURN:
    return_statement();
    break;
  case TOKENIZER_FOR:
    for_statement();
    break;
  case TOKENIZER_NEXT:
    next_statement();
    break;
  case TOKENIZER_END:
    end_statement();
    break;
  case TOKENIZER_LET:
    accept(TOKENIZER_LET);
    /* Fall through. */
  case TOKENIZER_VARIABLE:
  case TOKENIZER_STRINGVAR:
    assign_statement();
    break;
  default:
    DEBUG_PRINTF("ubasic.c: statement(): not implemented %d\n", token);
    ended = 1;
    break;
  }
}
/*---------------------------------------------------------------------------*/
static void
line_statement(void)
{
  DEBUG_PRINTF("----------- Line number %d ---------\n", tokenizer_num());
  /*    current_linenum = tokenizer_num();*/
  accept(TOKENIZER_NUMBER);
  statement();
  return;
}
/*---------------------------------------------------------------------------*/
void
ubasic_run(void)
{
  if(tokenizer_finished()) {
    DEBUG_PRINTF("uBASIC program finished\n");
    return;
  }

  line_statement();
}
/*---------------------------------------------------------------------------*/
int
ubasic_finished(void)
{
  return ended || tokenizer_finished();
}
/*---------------------------------------------------------------------------*/
void
ubasic_set_variable(int varnum, int value)
{
  if(varnum >= 0 && varnum < MAX_VARNUM) {
    variables[varnum] = value;
  }
}
/*---------------------------------------------------------------------------*/
int
ubasic_get_variable(int varnum)
{
  if(varnum >= 0 && varnum < MAX_VARNUM) {
    return variables[varnum];
  }
  return 0;
}
/*---------------------------------------------------------------------------*/
