
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "Pipeline.h"
#include "deq.h"
#include "error.h"

typedef struct {
    Deq processes;
    int fg;                 // 1 = foreground, 0 = background
} *PipelineRep;

extern Pipeline newPipeline(int fg) {
    PipelineRep r = malloc(sizeof(*r));

    if (!r)
        ERROR("malloc() failed");

    r->processes = deq_new();
    r->fg = fg;

    return r;
}

extern void addPipeline(Pipeline pipeline, Command command) {
    PipelineRep r = pipeline;
    deq_tail_put(r->processes, command);
}

extern int sizePipeline(Pipeline pipeline) {
    PipelineRep r = pipeline;
    return deq_len(r->processes);
}

extern void freePipeline(Pipeline pipeline) {
    PipelineRep r = pipeline;
    if (!r)
        return;

    deq_del(r->processes, freeCommand);
    free(r);
}

/* Wait for one foreground child, retrying if interrupted. */
static void waitForChild(pid_t pid) {
    int status;
    pid_t result;

    do {
        result = waitpid(pid, &status, 0);
    } while (result == -1 && errno == EINTR);

    if (result == -1)
        perror("waitpid");
}

extern void execPipeline(Pipeline pipeline, Jobs jobs, int *eof) {
    PipelineRep r = pipeline;
    int count = sizePipeline(pipeline);

    if (count == 0) {
        freePipeline(pipeline);
        return;
    }

    /*
     * A standalone foreground builtin runs in the
     * parent shell. This is necessary for cd and exit.
     */
    if (r->fg && count == 1) {
        Command command = deq_head_ith(r->processes, 0);

        if (isBuiltinCommand(command)) {
            execCommandParent(command, eof, jobs);
            freePipeline(pipeline);
            return;
        }
    }

    /*
     * Foreground PIDs are stored here.
     * Background PIDs go into Jobs.c instead.
     */
    pid_t *pids = calloc(count, sizeof(*pids));

    if (!pids) {
        perror("calloc");
        freePipeline(pipeline);
        return;
    }

    int launched = 0;
    int previousRead = -1;

    for (int i = 0; i < count; i++) {
        int fd[2] = {-1, -1};
        int hasNext = (i < count - 1);

        /*
         * Create a pipe only if another command follows.
         */
        if (hasNext) {
            if (pipe(fd) == -1) {
                perror("pipe");
                break;
            }
        }

        Command command = deq_head_ith(r->processes, i);

        pid_t pid = fork();

        if (pid == -1) {
            perror("fork");

            if (fd[0] != -1) close(fd[0]);
            if (fd[1] != -1) close(fd[1]);

            break;
        }

        if (pid == 0) {
            /*
             * CHILD PROCESS
             */

            /* Connect input from the previous pipe. */
            if (previousRead != -1) {
                if (dup2(previousRead, STDIN_FILENO) == -1) {
                    perror("dup2 stdin");
                    _exit(1);
                }
            }

            /* Connect output to the next pipe. */
            if (hasNext) {
                if (dup2(fd[1], STDOUT_FILENO) == -1) {
                    perror("dup2 stdout");
                    _exit(1);
                }
            }

            /* Close pipe descriptors no longer needed. */
            if (previousRead != -1)
                close(previousRead);

            if (fd[0] != -1)
                close(fd[0]);

            if (fd[1] != -1)
                close(fd[1]);

            /*
             * Apply file redirection and run builtin
             * or external command. Never returns.
             */
            execCommandChild(command);
            _exit(127);
        }

        /*
         * PARENT PROCESS
         */

        pids[launched++] = pid;

        if (!r->fg)
            addJobPid(jobs, pid);

        /*
         * Parent doesn't write to the new pipe.
         * It only keeps the read end for the next child.
         */
        if (previousRead != -1)
            close(previousRead);

        if (fd[1] != -1)
            close(fd[1]);

        previousRead = fd[0];
    }

    if (previousRead != -1)
        close(previousRead);

    /*
     * All children have now been launched.
     * Wait for foreground processes only.
     */
    if (r->fg) {
        for (int i = 0; i < launched; i++)
            waitForChild(pids[i]);
    }

    free(pids);

    /*
     * Safe to free in the parent even for background
     * children: fork() gave each child its own memory.
     */
    freePipeline(pipeline);
}

