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

void fill_balance(BalanceHistory *history, int currentBalance, timestamp_t currentTime);

int main(int argc, char *argv[]) {

    // fetch args
    getopt(argc, argv, "p:");
    int proc_count = atoi(optarg);
    balance_t balances[proc_count + 1];
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
            //C Process
            BalanceHistory history;
            history.s_id = i;
            BalanceState initState = {balances[i], get_physical_time(), 0};
            history.s_history[0] = initState;
            history.s_history_len = 1;
            SelfInputOutput sio = {io, i};
            close_pipes(&sio, i);

            fprintf(logfile, log_started_fmt, get_physical_time(), i, getpid(), getppid(), balances[i]);
            fflush(logfile);

            Message msg;
            sprintf(msg.s_payload, log_started_fmt, get_physical_time(), i, getpid(), getppid(), balances[i]);
            createMessageHeader(&msg, STARTED);
            send_multicast(&sio, &msg);

            Message start_msgs[proc_count + 1];
            receive_all(&sio, start_msgs, STARTED);
            fprintf(logfile, log_received_all_started_fmt, get_physical_time(), i);
            fflush(logfile);

            // Полезная работа

            int done = 1;
            Message workMsg;
            while (1) {
                receive_any(&sio, &workMsg);
                if (workMsg.s_header.s_type == STOP) {
                    Message done_msg;
                    sprintf(done_msg.s_payload, log_done_fmt, get_physical_time(), i, balances[i]);
                    createMessageHeader(&done_msg, DONE);
                    fprintf(logfile, log_done_fmt, get_physical_time(), i, balances[i]);
                    send_multicast(&sio, &done_msg);
                }
                if (workMsg.s_header.s_type == TRANSFER) {
                    TransferOrder order;
                    memcpy(&order, &workMsg.s_payload, workMsg.s_header.s_payload_len);
                    if (order.s_src == i) {
                        workMsg.s_header.s_local_time = get_physical_time();
                        fill_balance(&history, -order.s_amount, workMsg.s_header.s_local_time);
                        send(&sio, order.s_dst, &workMsg);
                        fprintf(logfile, log_transfer_out_fmt, get_physical_time(), order.s_src, order.s_amount,
                                order.s_dst);
                        fflush(logfile);
                    } else if (order.s_dst == i) {
                        fill_balance(&history, order.s_amount, workMsg.s_header.s_local_time);
                        printf("history written %d %d\n", history.s_history[history.s_history_len - 1].s_balance,
                               history.s_history[history.s_history_len - 1].s_time);
                        fprintf(logfile, log_transfer_in_fmt, get_physical_time(), order.s_src, order.s_amount,
                                order.s_dst);
                        fflush(logfile);
                        Message ackMsg;
                        createMessageHeader(&ackMsg, ACK);
                        send(&sio, 0, &ackMsg);
                        printf("sent %d\n", i);
                        fflush(stdout);
                    }
                }
                if (workMsg.s_header.s_type == DONE) {
                    done++;
                    if (done == proc_count) {
                        fill_balance(&history, 0, get_physical_time());
                        break;
                    }
                }
            }

//            Message done_msgs[proc_count + 1];
//            receive_all(&sio, done_msgs, DONE);
//            fprintf(logfile, log_received_all_done_fmt, get_physical_time(), i);
//            fill_balance(&history, 0);

            Message historyMsg;
            memcpy(historyMsg.s_payload, &history, sizeof(BalanceHistory));
            createMessageHeader(&historyMsg, BALANCE_HISTORY);
            historyMsg.s_header.s_payload_len = sizeof(BalanceHistory);
            printf("%d\n", history.s_history[2].s_balance);

            send(&sio, 0, &historyMsg);
            return 0;
        }
    }

    SelfInputOutput sio = {io, 0};
    close_pipes(&sio, 0);
    Message msgs[proc_count + 1];
    receive_all(&sio, msgs, STARTED);
    fprintf(logfile, log_received_all_started_fmt, get_physical_time(), 0);
    fflush(logfile);

    int max_id = proc_count;
    bank_robbery(&sio, max_id);

    Message stopMsg;
    createMessageHeader(&stopMsg, STOP);
    stopMsg.s_header.s_payload_len = 0;
    send_multicast(&sio, &stopMsg);

    receive_all(&sio, msgs, DONE);
    fflush(logfile);
    fprintf(logfile, log_received_all_done_fmt, get_physical_time(), 0);
    for (int i = 0; i < sio.io.procCount; i++)
        wait(NULL);
    fflush(logfile);

    Message history_msgs[proc_count + 1];
    receive_all(&sio, history_msgs, BALANCE_HISTORY);

    AllHistory allHistory;
    allHistory.s_history_len = proc_count;
    for (int i = 0; i < proc_count; i++) {
        memcpy(&allHistory.s_history[i], &history_msgs[i + 1].s_payload, sizeof(BalanceHistory));
        printf("%d\n", allHistory.s_history[i].s_history[2].s_balance);
    }
    print_history(&allHistory);

    return 0;
}

void fill_balance(BalanceHistory *history, int amount, timestamp_t current_time) {
    balance_t balance = history->s_history[history->s_history_len - 1].s_balance;
    for (timestamp_t t = history->s_history_len; t <= current_time; t++) {
        BalanceState state = {balance, t, 0};
        history->s_history[t] = state;
        history->s_history_len++;
    }
    printf("balance %d amount %d\n", history->s_history[current_time].s_balance, amount);
    history->s_history[current_time].s_balance += amount;
}

void transfer(void *parent_data, local_id src, local_id dst, balance_t amount) {
    Message msg;
    TransferOrder order = {src, dst, amount};
    memcpy(msg.s_payload, &order, sizeof(order));
    createMessageHeader(&msg, TRANSFER);
    msg.s_header.s_payload_len = sizeof(order);
    send(parent_data, src, &msg);

    Message ackMsg;
    ackMsg.s_header.s_type = DONE;
    while (ackMsg.s_header.s_type != ACK)
        receive(parent_data, dst, &ackMsg);
    printf("received %d\n", dst);
    fflush(stdout);
    usleep(1000);
}
