#include "auction.h"

sem_t *g_sem_bid_lock = NULL;
sem_t *g_sem_admission = NULL;
sem_t *g_sem_vip = NULL;
int g_shm_id = -1;
SharedBlock *g_shm = NULL;

int ipc_init(double starting_bid)
{
    //cleanup stale segments/sems just in case it crashed last time
    shmctl(shmget(SHM_KEY, sizeof(SharedBlock), 0666), IPC_RMID, NULL);
    sem_unlink(SEM_BID_LOCK);
    sem_unlink(SEM_ADMISSION);
    sem_unlink(SEM_VIP);

    g_shm_id = shmget(SHM_KEY, sizeof(SharedBlock), IPC_CREAT | IPC_EXCL | SHM_PERMS);
    if(g_shm_id == -1)
    {
        return -1;
    }
    
    //attach and zero out the memory
    g_shm = (SharedBlock *)shmat(g_shm_id, NULL, 0);
    memset(g_shm, 0, sizeof(SharedBlock));
    
    g_shm->auction.highest_bid = starting_bid;
    g_shm->auction.status = AUCTION_WAITING;
    g_shm->parent_pid = getpid();

    //setup our semaphores
    g_sem_bid_lock = sem_open(SEM_BID_LOCK, O_CREAT, SHM_PERMS, 1);
    g_sem_admission = sem_open(SEM_ADMISSION, O_CREAT, SHM_PERMS, MAX_BIDDERS);
    g_sem_vip = sem_open(SEM_VIP, O_CREAT, SHM_PERMS, MAX_VIP_SLOTS);

    return 0;
}

int ipc_attach(void)
{
    g_shm_id = shmget(SHM_KEY, sizeof(SharedBlock), SHM_PERMS);
    if(g_shm_id == -1)
    {
        return -1;
    }
    g_shm = (SharedBlock *)shmat(g_shm_id, NULL, 0);
    
    g_sem_bid_lock = sem_open(SEM_BID_LOCK, 0);
    g_sem_admission = sem_open(SEM_ADMISSION, 0);
    g_sem_vip = sem_open(SEM_VIP, 0);
    return 0;
}

void ipc_detach(void)
{
    sem_close(g_sem_bid_lock);
    sem_close(g_sem_admission);
    sem_close(g_sem_vip);
    shmdt(g_shm);
}

void ipc_cleanup(void)
{
    sem_close(g_sem_bid_lock);
    sem_unlink(SEM_BID_LOCK);
    sem_close(g_sem_admission);
    sem_unlink(SEM_ADMISSION);
    sem_close(g_sem_vip);
    sem_unlink(SEM_VIP);
    if(g_shm)
    {
        shmdt(g_shm);
    }
    if(g_shm_id != -1)
    {
        shmctl(g_shm_id, IPC_RMID, NULL);
    }
}
