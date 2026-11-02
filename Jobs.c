#include "Jobs.h"
#include "deq.h"
#include "error.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>

typedef struct JobPid {
    pid_t pid;
    struct JobPid *next;
} JobPid;

static JobPid *backgroundJobs = NULL;

extern void addJobPid(Jobs jobs, pid_t pid) {
    (void)jobs;

    JobPid *job = malloc(sizeof(*job));

    if (!job) {
        perror("malloc");
        return;
    }

    job->pid = pid;
    job->next = backgroundJobs;
    backgroundJobs = job;
}

extern void reapJobs(Jobs jobs) {
    (void)jobs;

    JobPid **current = &backgroundJobs;

    while (*current != NULL) {
        JobPid *job = *current;
        int status;

        pid_t result = waitpid(job->pid, &status, WNOHANG);

        if (result == 0) {
            // Process is still running.
            current = &job->next;
        }
        else if (result == job->pid) {
            // Process finished. Remove it from the list.

            if (WIFEXITED(status)) {
                printf("[background %d] exited with status %d\n",
                       job->pid, WEXITSTATUS(status));
            }
            else if (WIFSIGNALED(status)) {
                printf("[background %d] terminated by signal %d\n",
                       job->pid, WTERMSIG(status));
            }

            *current = job->next;
            free(job);
        }
        else {
            // waitpid() failed.
            if (errno == EINTR) {
                continue;
            }

            if (errno == ECHILD) {
                // Child was already reaped.
                *current = job->next;
                free(job);
            }
            else {
                perror("waitpid");
                current = &job->next;
            }
        }
    }
}

extern void waitJobs(Jobs jobs) {
    (void)jobs;

    while (backgroundJobs != NULL) {
        JobPid *job = backgroundJobs;
        int status;
        pid_t result;

        do {
            result = waitpid(job->pid, &status, 0);
        } while (result == -1 && errno == EINTR);

        if (result == -1 && errno != ECHILD) {
            perror("waitpid");
        }

        backgroundJobs = job->next;
        free(job);
    }
}


extern Jobs newJobs() {
  return deq_new();
}

extern void addJobs(Jobs jobs, Pipeline pipeline) {
  deq_tail_put(jobs,pipeline);
}

extern int sizeJobs(Jobs jobs) {
  return deq_len(jobs);
}

extern void freeJobs(Jobs jobs) {
  deq_del(jobs,freePipeline);
}
