#ifndef _IPC_MSG_H
#define _IPC_MSG_H

#include <ipc/ipc.h>
#include <kernel/types.h>

#define MAX_MSG_QUEUES 16
#define MAX_MSG_PAYLOAD 256
#define MAX_MSGS_PER_QUEUE 16

typedef struct msgbuf {
    long mtype;
    char mtext[MAX_MSG_PAYLOAD];
} msgbuf_t;

typedef struct msg_entry {
    long mtype;
    size_t size;
    uint8_t data[MAX_MSG_PAYLOAD];
    struct msg_entry* next;
} msg_entry_t;

typedef struct msqid_ds {
    int msqid;
    key_t key;
    uint32_t msg_qnum;
    uint32_t msg_qbytes;
    msg_entry_t* head;
    msg_entry_t* tail;
    bool in_use;
} msqid_ds_t;

int msgget(key_t key, int msgflg);
int msgsnd(int msqid, const void* msgp, size_t msgsz, int msgflg);
ssize_t msgrcv(int msqid, void* msgp, size_t msgsz, long msgtyp, int msgflg);
int msgctl(int msqid, int cmd, struct msqid_ds* buf);

#endif // _IPC_MSG_H
