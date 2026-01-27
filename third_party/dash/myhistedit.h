/*
 * VOS-specific libedit compatibility layer using linenoise
 * Provides minimal EditLine/History API for dash shell
 */

#ifndef MYHISTEDIT_H
#define MYHISTEDIT_H

#include <stdio.h>

/* History event structure */
typedef struct {
    int num;
    const char *str;
} HistEvent;

/* History commands */
#define H_SETSIZE    1
#define H_ENTER      2
#define H_APPEND     3
#define H_FIRST      4
#define H_NEXT       5
#define H_PREV       6
#define H_LAST       7
#define H_NEXT_EVENT 8
#define H_PREV_STR   9
#define H_LOAD       10
#define H_SAVE       11

/* EditLine commands */
#define EL_PROMPT    1
#define EL_EDITOR    2
#define EL_HIST      3
#define EL_TERMINAL  4

/* Opaque types */
typedef struct vos_editline EditLine;
typedef struct vos_history History;

/* History functions */
History *history_init(void);
void history_end(History *h);
int history(History *h, HistEvent *ev, int op, ...);

/* EditLine functions */
EditLine *el_init(const char *prog, FILE *fin, FILE *fout, FILE *ferr);
void el_end(EditLine *el);
const char *el_gets(EditLine *el, int *count);
int el_set(EditLine *el, int op, ...);
int el_source(EditLine *el, const char *file);

/* Prompt callback type */
typedef char *(*el_pfunc_t)(EditLine *);

/* Dash externs */
extern History *hist;
extern EditLine *el;
extern int displayhist;

void histedit(void);
void sethistsize(const char *);
void setterm(const char *);
int histcmd(int, char **);
int not_fcnumber(char *);
int str_to_event(const char *, int);

#endif /* MYHISTEDIT_H */
