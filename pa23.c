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

int check_forks(const void *self);

int request_fork(const void *self);

timestamp_t currentTime = 0;

int done = 1;
int doneArr[MAX_PROCESS_ID];
int forks[MAX_PROCESS_ID];
int dirty[MAX_PROCESS_ID];
int reqf[MAX_PROCESS_ID];
int priorities[MAX_PROCESS_ID];

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
            for (int j = 1; j <= proc_count; j++) {
                if (j == i) continue;
                doneArr[j] = 0;
                if (i < j) {
                    forks[j] = 1;
                    reqf[j] = 0;
                    dirty[j] = 1;
                    priorities[j] = 0;
                } else {
                    forks[j] = 0;
                    reqf[j] = 1;
                    dirty[j] = 0;
                    priorities[j] = 1;
                }
            }
            fprintf(logfile, log_started_fmt, get_lamport_time(), i, getpid(), getppid(), 0);
            fflush(logfile);

            Message msg;
            createMessageHeader(&msg, STARTED);
            msg.s_header.s_payload_len = 0;
            send_multicast(&sio, &msg);

            Message start_msgs[proc_count + 1];
            receive_all(&sio, start_msgs, STARTED);
            fprintf(logfile, log_received_all_started_fmt, get_lamport_time(), i);
            fflush(logfile);

            // полезная работа
            int maxIter = i * 5;
            for (int pr = 1; pr <= maxIter; pr++) {
                char loopStr[MAX_MESSAGE_LEN];
                sprintf(loopStr, log_loop_operation_fmt, i, pr, maxIter);
                if (mutexl) {
//                    if (i == 4)
//                        printf("%d before request", i);
//                    fflush(stdout);
                    request_cs(&sio);
                    print(loopStr);
                    release_cs(&sio);
                } else {
                    print(loopStr);
                }
            }

            Message done_msg;
            createMessageHeader(&done_msg, DONE);
            done_msg.s_header.s_payload_len = 0;

            fprintf(logfile, log_done_fmt, get_lamport_time(), i, 0);
            fflush(logfile);
            send_multicast(&sio, &done_msg);

//            printf("%d finished \n", i);
            fflush(stdout);
            while (1) {
                if (done == proc_count) break;
                Message workMsg;
                workMsg.s_header.s_type = -1;
                int sender = receive_any(&sio, &workMsg);
//                printf("%d received \n", i);

                if (workMsg.s_header.s_type == CS_RELEASE) {
                    continue;
                } else if (workMsg.s_header.s_type == CS_REQUEST) {
                    Message replyMsg;
                    createMessageHeader(&replyMsg, CS_REPLY);
//                    replyMsg.s_header.s_local_time = MAX_TS;
                    replyMsg.s_header.s_payload_len = 0;
                    send(&sio, sender, &replyMsg);
//                    printf("%d replied \n", i);
                    printf("%d sent reply\n", i);
                    fflush(stdout);
                } else if (workMsg.s_header.s_type == DONE) {
                    doneArr[sender] = 1;
                    done++;
                }
            }
            return 0;
        }
    }

    SelfInputOutput sio = {io, 0};
    close_pipes(&sio, 0);
    Message msgs[proc_count + 1];
    receive_all(&sio, msgs, STARTED);
    fprintf(logfile, log_received_all_started_fmt, get_lamport_time(), 0);
    fflush(logfile);

    receive_all(&sio, msgs, DONE);
    fflush(logfile);
    fprintf(logfile, log_received_all_done_fmt, get_lamport_time(), 0);
    for (int i = 0; i < sio.io.procCount; i++)
        wait(NULL);

    return 0;
}

int request_cs(const void *self) {
    SelfInputOutput *sio = (SelfInputOutput *) self;

    while (1) {
        request_fork(sio);
        //make request for forks
        Message workMsg;
        workMsg.s_header.s_type = -1;
        int sender = receive_any(sio, &workMsg);

        if (workMsg.s_header.s_type == CS_RELEASE) {
            priorities[sender] = 1;
            if (reqf[sender] == 0) {
                forks[sender] = 1;
                dirty[sender] = 0;
                if (check_forks(sio))
                    break;
            }
        } else if (workMsg.s_header.s_type == CS_REQUEST) {
            reqf[sender] = 1;
            if (priorities[sender] == 0) {
                forks[sender] = 0;
                dirty[sender] = 0;
                // Reply значит то, что мы отправили вилку
                Message replyMsg;
                createMessageHeader(&replyMsg, CS_REPLY);
                replyMsg.s_header.s_payload_len = 0;
                send(sio, sender, &replyMsg);
            }
        } else if (workMsg.s_header.s_type == CS_REPLY) {
            // Нам отправили вилку, радуемся
            forks[sender] = 1;
            dirty[sender] = 0;
            if (check_forks(sio))
                break;
        } else if (workMsg.s_header.s_type == DONE) {
            doneArr[sender] = 1;
//            printf("%d d done from %d\n", sio->self, sender);
//            fflush(stdout);
            done++;
        }
    }

    return 0;
}

int request_fork(const void *self) {
    SelfInputOutput *sio = (SelfInputOutput *) self;

    for (int i = 1; i <= sio->io.procCount; i++) {
        if (reqf[i] == 1) {
            Message requestMsg;
            createMessageHeader(&requestMsg, CS_REQUEST);
            requestMsg.s_header.s_payload_len = 0;
            send(sio, i, &requestMsg);
            reqf[i] = 0;
        }
    }
    return 1;
}

int check_forks(const void *self) {
    SelfInputOutput *sio = (SelfInputOutput *) self;
    for (int i = 1; i <= sio->io.procCount; i++) {
        if (i == sio->self) continue;
        if (forks[i] != 1 || dirty[i] == 1) {
            if (doneArr[i] == 1) continue;
//            if (sio->self == 4)
//                printf("dirty %d %d %d\n", dirty[1], dirty[2], dirty[3]);
//            fflush(stdout);
            return 0;
        }
    }
    fflush(stdout);
    return 1;
}

int release_cs(const void *self) {
    SelfInputOutput *sio = (SelfInputOutput *) self;
    for (int i = 1; i <= sio->io.procCount; i++) {
        if (i == sio->self) continue;
        Message receiveMsg;
        receiveMsg.s_header.s_type = -1;
        if (receive(sio, i, &receiveMsg) && receiveMsg.s_header.s_type == CS_REQUEST) {
            reqf[i] = 1;
            forks[i] = 0;
            dirty[i] = 0;
        } else if (receiveMsg.s_header.s_type == DONE) {
            doneArr[i] = 1;
            done++;
        } else
            dirty[i] = 1;

        priorities[i] = 0;
        Message releaseMsg;
        createMessageHeader(&releaseMsg, CS_RELEASE);
        send(sio, i, &releaseMsg);
    }

    return 0;
}

timestamp_t get_lamport_time() {
    return currentTime;
}

void transfer(void *parent_data, local_id src, local_id dst, balance_t amount) {}

