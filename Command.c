
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <readline/history.h>

#include "Command.h"
#include "error.h"

typedef struct {
    char *file;
    char **argv;
    char *input;
    char *output;
} *CommandRep;

static char *owd = NULL;

/* Convert the parse-tree words into argv. */
static char **getargs(T_words words) {
    int n = 0;
    for (T_words p = words; p; p = p->words)
        n++;

    char **argv = calloc(n + 1, sizeof(*argv));
    if (!argv)
        ERROR("calloc() failed");

    int i = 0;
    for (T_words p = words; p; p = p->words) {
        argv[i] = strdup(p->word->s);
        if (!argv[i])
            ERROR("strdup() failed");
        i++;
    }

    return argv;
}

extern Command newCommand(T_command t) {
    CommandRep r = calloc(1, sizeof(*r));
    if (!r)
        ERROR("calloc() failed");

    r->argv = getargs(t->words);
    r->file = r->argv[0];

    if (t->input)
        r->input = strdup(t->input);

    if (t->output)
        r->output = strdup(t->output);

    return r;
}

/* Apply command redirections in the current process. */
static int redirectCommand(CommandRep r) {
    int fd;

    if (r->input) {
        fd = open(r->input, O_RDONLY);
        if (fd < 0) {
            perror(r->input);
            return -1;
        }

        if (dup2(fd, STDIN_FILENO) < 0) {
            perror("dup2 input");
            close(fd);
            return -1;
        }
        close(fd);
    }

    if (r->output) {
        fd = open(r->output,
                  O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (fd < 0) {
            perror(r->output);
            return -1;
        }

        if (dup2(fd, STDOUT_FILENO) < 0) {
            perror("dup2 output");
            close(fd);
            return -1;
        }
        close(fd);
    }

    return 0;
}

extern int isBuiltinCommand(Command command) {
    CommandRep r = command;

    if (!r || !r->file)
        return 0;

    return strcmp(r->file, "exit") == 0 ||
           strcmp(r->file, "pwd") == 0 ||
           strcmp(r->file, "cd") == 0 ||
           strcmp(r->file, "history") == 0;
}

static int argcCommand(CommandRep r) {
    int n = 0;
    while (r->argv[n])
        n++;
    return n;
}

/* Return 1 if a builtin was executed. */
static int builtin(CommandRep r, int *eof, Jobs jobs) {
    int argc = argcCommand(r);

    if (strcmp(r->file, "exit") == 0) {
        if (argc != 1) {
            fprintf(stderr, "exit: wrong number of arguments\n");
            return 1;
        }
        *eof = 1;
        waitJobs(jobs);
        return 1;
    }

    if (strcmp(r->file, "pwd") == 0) {
        if (argc != 1) {
            fprintf(stderr, "pwd: wrong number of arguments\n");
            return 1;
        }

        char *dir = getcwd(NULL, 0);
        if (!dir) {
            perror("getcwd");
        } else {
            printf("%s\n", dir);
            free(dir);
        }
        return 1;
    }

    if (strcmp(r->file, "cd") == 0) {
        if (argc != 2) {
            fprintf(stderr, "cd: expected one directory\n");
            return 1;
        }

        char *previous = getcwd(NULL, 0);
        if (!previous) {
            perror("getcwd");
            return 1;
        }

        const char *destination = r->argv[1];

        if (strcmp(destination, "-") == 0) {
            if (!owd) {
                fprintf(stderr, "cd: no previous directory\n");
                free(previous);
                return 1;
            }
            destination = owd;
        }

        if (chdir(destination) < 0) {
            perror("cd");
            free(previous);
            return 1;
        }

        free(owd);
        owd = previous;
        return 1;
    }

    if (strcmp(r->file, "history") == 0) {
        if (argc != 1) {
            fprintf(stderr, "history: wrong number of arguments\n");
            return 1;
        }

        HIST_ENTRY **entries = history_list();

        if (entries) {
            for (int i = 0; entries[i]; i++)
                printf("%d %s\n",
                       history_base + i,
                       entries[i]->line);
        }
        return 1;
    }

    return 0;
}

/*
 * Execute a standalone foreground builtin in the shell.
 * Save and restore descriptors so redirection does not
 * permanently change the shell's stdin or stdout.
 */
extern int execCommandParent(Command command, int *eof, Jobs jobs) {
    CommandRep r = command;

    if (!isBuiltinCommand(command))
        return 0;

    fflush(stdout);

    int savedIn = dup(STDIN_FILENO);
    int savedOut = dup(STDOUT_FILENO);

    if (savedIn < 0 || savedOut < 0) {
        perror("dup");
        if (savedIn >= 0) close(savedIn);
        if (savedOut >= 0) close(savedOut);
        return 1;
    }

    if (redirectCommand(r) == 0)
        builtin(r, eof, jobs);

    fflush(stdout);

    if (dup2(savedIn, STDIN_FILENO) < 0)
        perror("restore stdin");

    if (dup2(savedOut, STDOUT_FILENO) < 0)
        perror("restore stdout");

    close(savedIn);
    close(savedOut);

    return 1;
}

/*
 * Called only inside an already-forked child.
 * This function never returns.
 */
extern void execCommandChild(Command command) {
    CommandRep r = command;
    int eof = 0;

    if (redirectCommand(r) < 0)
        _exit(1);

    if (isBuiltinCommand(command)) {
        /*
         * Child builtins must not wait for or modify
         * the parent's background job tracking.
         * exit in a child simply terminates that child.
         */
        if (strcmp(r->file, "exit") != 0) {
            Jobs childJobs = newJobs();
            builtin(r, &eof, childJobs);
            freeJobs(childJobs);
        }

        fflush(stdout);
        _exit(0);
    }

    execvp(r->file, r->argv);

    perror(r->file);
    _exit(127);
}

extern void freeCommand(Command command) {
    CommandRep r = command;
    if (!r)
        return;

    if (r->argv) {
        for (int i = 0; r->argv[i]; i++)
            free(r->argv[i]);
        free(r->argv);
    }

    free(r->input);
    free(r->output);
    free(r);
}

extern void freestateCommand() {
    free(owd);
    owd = NULL;
}

