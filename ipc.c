#include "common.h"
#include "ipc.h"
#include "ipc2.h"
#include "banking.h"

#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

#include <sys/types.h>
#include <getopt.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>


int send(void *self, local_id dst, const Message *msg) {
    SelfInputOutput *sio = (SelfInputOutput *) self;
    if (sio->self == dst) {
        return -1;
    }

    write(sio->io.fds[sio->self][dst][1], msg, sizeof msg->s_header + msg->s_header.s_payload_len);
    return 0;
}

int send_multicast(void *self, const Message *msg) {
    SelfInputOutput *sio = (SelfInputOutput *) self;
    for (int i = 0; i <= sio->io.procCount; ++i) {
        if (i != sio->self)
            send(self, i, msg);
    }
    return 0;
}

int receive(void *self, local_id from, Message *msg) {
    SelfInputOutput *sio = (SelfInputOutput *) self;
    int fd = sio->io.fds[from][sio->self][0];
//    printf("%d started\n", sio->self);
    while (1) {
        int sum, sum1;
        if ((sum = read(fd, &msg->s_header, sizeof(MessageHeader))) == -1) {
            sleep(0);
            continue;
        }
        if (msg->s_header.s_payload_len > 0) {
            sum1 = read(fd, msg->s_payload, msg->s_header.s_payload_len);
        }
//        printf("%d received %s\n", sio->self, msg->s_payload);
        return 0;
    }
}

int receive_any(void *self, Message *msg) {
    SelfInputOutput *sio = (SelfInputOutput *) self;
    while (1) {
        for (int i = 0; i <= sio->io.procCount; ++i) {
            if (i == sio->self) continue;

            int fd = sio->io.fds[i][sio->self][0];
            int sum, sum1;
            sum = read(fd, &msg->s_header, sizeof(MessageHeader));
//            if (msg->s_header.)
            if (sum == -1) {
                continue;
            }
            if (msg->s_header.s_payload_len > 0) {
                sum1 = read(fd, msg->s_payload, msg->s_header.s_payload_len);
            }
            fflush(stdout);
            return 0;
        }
        usleep(1000);
    }
}

void close_pipes(void *self, int proc) {
    SelfInputOutput *sio = (SelfInputOutput *) self;
    for (int i = 0; i <= sio->io.procCount; ++i) {
        for (int j = 0; j <= sio->io.procCount; ++j) {
            if (i == j) continue;
            if (proc == i) {
                close(sio->io.fds[i][j][0]);
            } else if (proc == j) {
                close(sio->io.fds[i][j][1]);
            } else {
                close(sio->io.fds[i][j][0]);
                close(sio->io.fds[i][j][1]);
            }
        }
    }
}

int receive_all(void *self, Message msgs[], MessageType type) {
    SelfInputOutput *sio = (SelfInputOutput *) self;
    for (int i = 1; i <= sio->io.procCount; ++i) {
        if (i == sio->self) continue;
        do {
            receive(self, i, &msgs[i]);
        } while (msgs[i].s_header.s_type != type);
    }

    return 0;
}

void createMessageHeader(Message *msg, MessageType messageType) {
    msg->s_header.s_magic = MESSAGE_MAGIC;
    msg->s_header.s_type = messageType;
//    msg->s_header.s_local_time = get_physical_time();
//    msg->s_header.s_payload_len = strlen(msg->s_payload) + 1;
}
