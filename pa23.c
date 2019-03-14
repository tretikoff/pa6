#include "banking.h"
#include "common.h"
#include "ipc.h"
#include "ipc2.h"
#include "pa2345.h"

#include <unistd.h>
#include <stdio.h>

#include <sys/types.h>
#include <getopt.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>

int main(int argc, char *argv[]) {
    //bank_robbery(parent_data);
    //print_history(all);

    // fetch args
    getopt(argc, argv, "p:");
    int proc_count = atoi(optarg);
    int balances[proc_count + 1];
    for (int i = 1; i <= proc_count; ++i)
        balances[i] = atoi(argv[i + 2]);

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
            fprintf(pipes_logfile, "%d %d was opened\n", i, j);
        }
    }

    FILE *logfile;
    logfile = fopen(events_log, "a+");

    int pid = 0;
    for (local_id i = 1; i <= proc_count; ++i) {
        pid = fork();
        if (pid == 0) {
            //C Process
            BalanceHistory history;
            history.s_id = i;
            SelfInputOutput sio = {io, i};
            close_pipes(&sio, i);

            fprintf(logfile, log_started_fmt, get_physical_time(), i, getpid(), getppid(), balances[i]);
            fflush(logfile);

            Message msg;
            sprintf(msg.s_payload, log_started_fmt, get_physical_time(), i, getpid(), getppid(), balances[i]);
            createMessageHeader(&msg, STARTED);

            send_multicast(&sio, &msg);

            Message msgs[proc_count + 1];
            receive_all(&sio, msgs, STARTED);
            fprintf(logfile, log_received_all_started_fmt, get_physical_time(), i);
            fflush(logfile);
//
//            Message msg2;
//            sprintf(msg2.s_payload, log_done_fmt, i);
//            createMessageHeader(&msg2, DONE);
//
//            fprintf(logfile, log_done_fmt, i);
//            send_multicast(&sio, &msg2);
//
//            receive_all(&sio, msgs, DONE);
//            fprintf(logfile, log_received_all_done_fmt, get_physical_time(), i);
//


//            Message msgHistory;
//            msg.s_payload = history;
//            createMessageHeader(&msgHistory, BALANCE_HISTORY);
//            send(&sio, 0, &msgHistory);
            return 0;
        }
    }

    SelfInputOutput sio = {io, 0};
    close_pipes(&sio, 0);
    Message msgs[proc_count + 1];
    receive_all(&sio, msgs, STARTED);
    fprintf(logfile, log_received_all_started_fmt, get_physical_time(), 0);
//    TODO parent data
    int max_id = proc_count - 1;
    bank_robbery(&sio, max_id);

    Message doneMsg;
    createMessageHeader(&doneMsg, STOP);
    send_multicast(&sio, &doneMsg);
    receive_all(&sio, msgs, DONE);
    fflush(logfile);
    fprintf(logfile, log_received_all_done_fmt, get_physical_time(), 0);
    for (int i = 0; i < sio.io.procCount; i++)
        wait(NULL);
    fflush(logfile);

    return 0;
}

void transfer(void *parent_data, local_id src, local_id dst, balance_t amount) {
    // student, please implement me
}

//TODO remove
timestamp_t get_physical_time() {
    return 1;
}