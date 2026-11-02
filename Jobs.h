#ifndef JOBS_H
#define JOBS_H

#include <sys/types.h>

typedef void *Jobs;

#include "Pipeline.h"

extern Jobs newJobs();
extern void addJobs(Jobs jobs, Pipeline pipeline);
extern int sizeJobs(Jobs jobs);
extern void freeJobs(Jobs jobs);

// New functions
extern void addJobPid(Jobs jobs, pid_t pid);
extern void reapJobs(Jobs jobs);
extern void waitJobs(Jobs jobs);

#endif
