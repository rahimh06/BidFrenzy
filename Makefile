#ifndef AUCTION_H
#define AUCTION_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <semaphore.h>
#include <ncurses.h>

//basic limits for the shell
#define MAX_CMD_LEN 128
#define MAX_ARGS 3
#define MAX_ARG_LEN 64

//auction rules
#define MAX_BIDDERS 10
#define MAX_VIP_SLOTS 3
#define AUCTION_DURATION 60

//keys and perms for shared memory
#define SHM_KEY 0xABC1
#define SHM_PERMS 0666

//semaphore names
#define SEM_BID_LOCK "/auc_bid_lock"
#define SEM_ADMISSION "/auc_admission"
#define SEM_VIP "/auc_vip"

typedef enum
{
    AUCTION_WAITING = 0,
    AUCTION_ACTIVE = 1,
    AUCTION_SOLD = 2
} AuctionStatus;

//this is what all processes fight over
typedef struct
{
    double highest_bid;
    pid_t winning_pid;
    int total_bids;
    int active_bidders;
    AuctionStatus status;
    char product_name[64];
}AuctionItem;

//master block for shared memory
typedef struct
{
    AuctionItem auction;
    pid_t parent_pid;
    pid_t logger_pid;
    pid_t timer_pid;
}SharedBlock;

typedef struct
{
    char argv[MAX_ARGS][MAX_ARG_LEN];
    int argc;
}Command;

extern sem_t *g_sem_bid_lock;
extern sem_t *g_sem_admission;
extern sem_t *g_sem_vip;
extern int g_shm_id;
extern SharedBlock *g_shm;
extern int g_log_pipe_write;

int ipc_init(double starting_bid);
void ipc_cleanup(void);
int ipc_attach(void);
void ipc_detach(void);

pid_t spawn_logger(int pipe_read_fd);
void log_event(int pipe_fd, const char *fmt, ...);

pid_t spawn_bidder(int is_vip, double budget);
void spawn_timer(int duration);

#endif
