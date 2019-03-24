#include "banking.h"
#include "common.h"
#include "ipc.h"
#include "ipc2.h"
#include "pa2345.h"

#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <getopt.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>

int checkQueue(const void *self);

timestamp_t currentTime = 0;
timestamp_t Qi[MAX_PROCESS_ID];
int done = 1;

int main(int argc, char *argv[]) {

    // fetch args

    int proc_count = 0;
    int mutexl = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp("-p", argv[i]) == 0) {
            proc_count = atoi(argv[++i]);
        } else if (strcmp("--mutexl", argv[i]) == 0) {
            mutexl = 1;
        }
    }
//    for (int i = 0; i < argc; i++) {
//        printf("%s \n", argv[i]);
//    }
//    const struct option long_options[] = {{"mutexl", no_argument, &mutexl, 1},
//                                          {NULL,     0,           NULL,    0}};
//    getopt(argc, argv, "p:");
//    int proc_count = atoi(optarg);
//
//    getopt_long(argc, argv, "", long_options, NULL);

    InputOutput io;
    io.procCount = proc_count;
    io.fds = (int ***) calloc((proc_count + 1), sizeof(int **));

    FILE *pipes_logfile = fopen(pipes_log, "a+");
    for (int i = 0; i <= proc_count; ++i) {
        io.fds[i] = (int **) calloc((proc_count + 1), sizeof(int *));
        for (int j = 0; j <= proc_count; ++j) {
            if (i == j) continue;
            io.fds[i][j] = (int *) calloc(2, sizeof(int));
            pipe(io.fds[i][j]);
            fcntl(io.fds[i][j][0], F_SETFL, O_NONBLOCK);
            fcntl(io.fds[i][j][1], F_SETFL, O_NONBLOCK);
            fprintf(pipes_logfile, "%d %d was opened\n", i, j);
        }
    }

    FILE *logfile;
    logfile = fopen(events_log, "a+");

    int pid = 0;
    for (local_id i = 1; i <= proc_count; ++i) {
        pid = fork();
        if (pid == 0) {
            //дочерний процесс
            SelfInputOutput sio = {io, i};
            close_pipes(&sio, i);
//
//            fprintf(logfile, log_started_fmt, get_lamport_time(), i, getpid(), getppid(), 0);
//            fflush(logfile);
//
//            Message msg;
//            sprintf(msg.s_payload, log_started_fmt, get_lamport_time(), i, getpid(), getppid(), 0);
//            createMessageHeader(&msg, STARTED);
//            msg.s_header.s_payload_len = 0;
//            send_multicast(&sio, &msg);
//
//            Message start_msgs[proc_count + 1];
//            receive_all(&sio, start_msgs, STARTED);
//            fprintf(logfile, log_received_all_started_fmt, get_lamport_time(), i);
//            fflush(logfile);

            // полезная работа
            int maxIter = i * 5;
            for (int pr = 1; pr <= maxIter; pr++) {
                char loopStr[MAX_MESSAGE_LEN];
                sprintf(loopStr, log_loop_operation_fmt, i, pr, maxIter);
                if (mutexl) {
                    request_cs(&sio);
                    print(loopStr);
                    release_cs(&sio);
                } else {
                    print(loopStr);
                }
            }

            Message done_msg;
            sprintf(done_msg.s_payload, log_done_fmt, get_lamport_time(), i, 0);
            createMessageHeader(&done_msg, DONE);
            done_msg.s_header.s_payload_len = 0;

            fprintf(logfile, log_done_fmt, get_lamport_time(), i, 0);
            fflush(logfile);
            send_multicast(&sio, &done_msg);
            printf("%d sent done\n", i);

            while (1) {
                if (done == proc_count) break;
                Message workMsg;
                workMsg.s_header.s_type = STARTED;
                int sender = receive_any(&sio, &workMsg);

                if (workMsg.s_header.s_type == CS_RELEASE) {
                    continue;
                } else if (workMsg.s_header.s_type == CS_REQUEST) {
                    Message replyMsg;
                    createMessageHeader(&replyMsg, CS_REPLY);
                    replyMsg.s_header.s_local_time = MAX_TS;
                    replyMsg.s_header.s_payload_len = 0;
                    send(&sio, sender, &replyMsg);
                } else if (workMsg.s_header.s_type == DONE) {
                    done++;
                    printf("%d Done = %d\n", i, done);
                }
            }

            printf("Process %d finished work\n", i);
            return 0;
        }
    }

    SelfInputOutput sio = {io, 0};
    close_pipes(&sio, 0);
//    Message msgs[proc_count + 1];
//    receive_all(&sio, msgs, STARTED);
//    fprintf(logfile, log_received_all_started_fmt, get_lamport_time(), 0);
//    fflush(logfile);

//    receive_all(&sio, msgs, DONE);
//    fflush(logfile);
//    fprintf(logfile, log_received_all_done_fmt, get_lamport_time(), 0);
    for (int i = 0; i < sio.io.procCount; i++)
        wait(NULL);

    return 0;
}

int request_cs(const void *self) {
    SelfInputOutput *sio = (SelfInputOutput *) self;

    Message requestMsg;
    createMessageHeader(&requestMsg, CS_REQUEST);
    requestMsg.s_header.s_payload_len = 0;
    Qi[sio->self] = requestMsg.s_header.s_local_time;
    send_multicast(sio, &requestMsg);

    fflush(stdout);

//    if (checkQueue(sio) == 0) return 0;

    int replies = 1;
    while (1) {
        Message workMsg;
        workMsg.s_header.s_type = STARTED;
        int sender = receive_any(sio, &workMsg);
//        printf("%d Received from %d, time %d, type %d\n", sio->self, sender, workMsg.s_header.s_local_time,
//               workMsg.s_header.s_type);

        if (workMsg.s_header.s_type == CS_RELEASE) {
//            printf("%d Received release from %d\n", sio->self, sender);
            Qi[sender] = MAX_TS;
            if (checkQueue(sio) == 0) {
                return 0;
            }
        } else if (workMsg.s_header.s_type == CS_REQUEST) {
            Qi[sender] = workMsg.s_header.s_local_time;
//            printf("%d local time - %d");
            Message replyMsg;
            createMessageHeader(&replyMsg, CS_REPLY);
            replyMsg.s_header.s_payload_len = 0;
            send(sio, sender, &replyMsg);
//            printf("%d sent release to %d\n", sio->self, sender);
        } else if (workMsg.s_header.s_type == CS_REPLY) {
            replies++;
            if (replies == sio->io.procCount && checkQueue(sio) == 0)
                return 0;
        } else if (workMsg.s_header.s_type == DONE) {
            done++;
            printf("%d Done = %d\n", sio->self, done);
        }
    }
}

int checkQueue(const void *self) {
    SelfInputOutput *sio = (SelfInputOutput *) self;
    for (int i = 1; i <= sio->io.procCount; i++) {
//        printf("%d Qi: 3 - %d; 4 - %d\n", sio->self, Qi[3], Qi[4]);
        if (i == sio->self) continue;
//        printf("processes %d with time %d and %d with time %d want to go to cs\n", sio->self, Qi[sio->self], i, Qi[i]);
        if (Qi[sio->self] > Qi[i]) return i;
        if (Qi[i] == Qi[sio->self] && i > sio->self) return i;
    }
//    printf("process %d going to cs\n", sio->self);
    return 0;
}

int release_cs(const void *self) {
    SelfInputOutput *sio = (SelfInputOutput *) self;
    Qi[sio->self] = MAX_TS;
    Message releaseMsg;
    createMessageHeader(&releaseMsg, CS_RELEASE);
    releaseMsg.s_header.s_payload_len = 0;
    send_multicast(sio, &releaseMsg);

    return 0;
}

timestamp_t get_lamport_time() {
    return currentTime;
}

void transfer(void *parent_data, local_id src, local_id dst, balance_t amount) {}
