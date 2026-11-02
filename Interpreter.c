#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Interpreter.h"
#include "Sequence.h"
#include "Pipeline.h"
#include "Command.h"

static Command i_command(T_command t);
static void i_pipeline(T_pipeline t, Pipeline pipeline);
static void i_sequence(T_sequence t, Sequence sequence);

static Command i_command(T_command t) {
    if (!t)
        return 0;

    if (t->words)
        return newCommand(t);

    return 0;
}

static void i_pipeline(T_pipeline t, Pipeline pipeline) {
  if (!t)
    return;
  addPipeline(pipeline,i_command(t->command));
  i_pipeline(t->pipeline,pipeline);
}

static void i_sequence(T_sequence t, Sequence sequence) {
    if (!t)
        return;

    int fg = 1;

    if (t->op && strcmp(t->op, "&") == 0)
        fg = 0;

    Pipeline pipeline = newPipeline(fg);

    i_pipeline(t->pipeline, pipeline);
    addSequence(sequence, pipeline);

    i_sequence(t->sequence, sequence);
}

extern void interpretSequence(T_sequence s, int *eof, Jobs jobs) {
  if (!s)
    return;
  Sequence sequence=newSequence();
  i_sequence(s,sequence);
  execSequence(sequence,jobs,eof);
}
