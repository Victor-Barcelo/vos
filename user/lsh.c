#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "linenoise.h"

static void completion_cb(const char* buf, linenoiseCompletions* lc) {
    if (!buf || !lc) {
        return;
    }

    if (strncmp(buf, "e", 1) == 0) {
        linenoiseAddCompletion(lc, "exit");
        linenoiseAddCompletion(lc, "emoji");
        linenoiseAddCompletion(lc, "eliza");
    }
    if (strncmp(buf, "h", 1) == 0) {
        linenoiseAddCompletion(lc, "help");
    }
    if (strncmp(buf, "z", 1) == 0) {
        linenoiseAddCompletion(lc, "zork");
    }
}

static void cmd_help(void) {
    puts("Commands:");
    puts("  help   - show this help");
    puts("  emoji  - print some unicode symbols");
    puts("  exit   - exit this program");
    puts("");
    puts("This is a linenoise demo (line editing/history/completion).");
    puts("Run user programs from the kernel shell with: run /bin/<name>");
}

static void cmd_emoji(void) {
    puts("Unicode symbols test:");
    puts("  Ballot: \xE2\x98\x90 \xE2\x98\x91 \xE2\x98\x92");
    puts("  Boxes:  \xE2\x96\xA0 \xE2\x96\xAE \xE2\x97\x8F");
    puts("  Lines:  \xE2\x94\x80 \xE2\x94\x82 \xE2\x94\x8C \xE2\x94\x90 \xE2\x94\x94 \xE2\x94\x98");
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    linenoiseSetCompletionCallback(completion_cb);
    linenoiseHistorySetMaxLen(64);

    puts("VOS linenoise demo. Type 'help' for help.");

    for (;;) {
        char* line = linenoise("lsh> ");
        if (!line) {
            break;
        }

        if (line[0] != '\0') {
            linenoiseHistoryAdd(line);
        }

        if (strcmp(line, "exit") == 0) {
            free(line);
            break;
        } else if (strcmp(line, "help") == 0) {
            cmd_help();
        } else if (strcmp(line, "emoji") == 0) {
            cmd_emoji();
        } else if (line[0] != '\0') {
            printf("You typed: %s\n", line);
        }

        free(line);
    }

    return 0;
}

