#ifndef COMMAND_H
#define COMMAND_H

typedef void *Command;

#include "Tree.h"
#include "Jobs.h"
#include "Sequence.h"

extern Command newCommand(T_command command);

extern void execCommand(Command command, Pipeline pipeline, Jobs jobs,
			int *jobbed, int *eof, int fg);

extern void freeCommand(Command command);
extern void freestateCommand();

extern void execCommandChild(Command command);

extern int isBuiltinCommand(Command command);
extern int execCommandParent(Command command, int *eof, Jobs jobs);
extern void execCommandChild(Command command);

#endif
