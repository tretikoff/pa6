#ifndef __IFMO_DISTRIBUTED_CLASS_IPC2__H
#define __IFMO_DISTRIBUTED_CLASS_IPC2__H

#include "ipc.h"
#include <unistd.h>

int usleep(__useconds_t usec);

void close_pipes(void *self, int proc);

int receive_all(void *self, Message *msgs, MessageType type);

void createMessageHeader(Message *msg, MessageType type);

typedef struct {
    int read;
    int write;
} fd;

typedef struct {
    int procCount;
    int ***fds;
} InputOutput;

typedef struct {
    InputOutput io;
    local_id self;
} SelfInputOutput;

#endif
