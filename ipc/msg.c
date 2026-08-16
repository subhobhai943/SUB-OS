#include <ipc/msg.h>
#include <mm/kmalloc.h>
#include <lib/string.h>

static msqid_ds_t msg_queues[MAX_MSG_QUEUES];

int msgget(key_t key, int msgflg) {
    (void)msgflg;
    for (int i = 0; i < MAX_MSG_QUEUES; i++) {
        if (msg_queues[i].in_use && msg_queues[i].key == key) {
            return i;
        }
    }

    for (int i = 0; i < MAX_MSG_QUEUES; i++) {
        if (!msg_queues[i].in_use) {
            msg_queues[i].in_use = true;
            msg_queues[i].key = key;
            msg_queues[i].msqid = i;
            msg_queues[i].msg_qnum = 0;
            msg_queues[i].msg_qbytes = MAX_MSG_PAYLOAD * MAX_MSGS_PER_QUEUE;
            msg_queues[i].head = NULL;
            msg_queues[i].tail = NULL;
            return i;
        }
    }
    return -1;
}

int msgsnd(int msqid, const void* msgp, size_t msgsz, int msgflg) {
    (void)msgflg;
    if (msqid < 0 || msqid >= MAX_MSG_QUEUES || !msg_queues[msqid].in_use || !msgp) return -1;
    if (msgsz > MAX_MSG_PAYLOAD) return -1;

    msqid_ds_t* q = &msg_queues[msqid];
    if (q->msg_qnum >= MAX_MSGS_PER_QUEUE) return -1;

    msg_entry_t* entry = (msg_entry_t*)kmalloc(sizeof(msg_entry_t));
    if (!entry) return -1;

    const msgbuf_t* mb = (const msgbuf_t*)msgp;
    entry->mtype = mb->mtype;
    entry->size = msgsz;
    memcpy(entry->data, mb->mtext, msgsz);
    entry->next = NULL;

    if (!q->tail) {
        q->head = entry;
        q->tail = entry;
    } else {
        q->tail->next = entry;
        q->tail = entry;
    }
    q->msg_qnum++;
    return 0;
}

ssize_t msgrcv(int msqid, void* msgp, size_t msgsz, long msgtyp, int msgflg) {
    (void)msgtyp; (void)msgflg;
    if (msqid < 0 || msqid >= MAX_MSG_QUEUES || !msg_queues[msqid].in_use || !msgp) return -1;

    msqid_ds_t* q = &msg_queues[msqid];
    if (!q->head) return -1;

    msg_entry_t* entry = q->head;
    q->head = entry->next;
    if (!q->head) q->tail = NULL;
    q->msg_qnum--;

    msgbuf_t* mb = (msgbuf_t*)msgp;
    mb->mtype = entry->mtype;
    size_t copy_len = entry->size < msgsz ? entry->size : msgsz;
    memcpy(mb->mtext, entry->data, copy_len);

    kfree(entry);
    return (ssize_t)copy_len;
}

int msgctl(int msqid, int cmd, struct msqid_ds* buf) {
    if (msqid < 0 || msqid >= MAX_MSG_QUEUES || !msg_queues[msqid].in_use) return -1;

    if (cmd == IPC_RMID) {
        msqid_ds_t* q = &msg_queues[msqid];
        msg_entry_t* curr = q->head;
        while (curr) {
            msg_entry_t* next = curr->next;
            kfree(curr);
            curr = next;
        }
        memset(q, 0, sizeof(msqid_ds_t));
        return 0;
    } else if (cmd == IPC_STAT && buf) {
        memcpy(buf, &msg_queues[msqid], sizeof(msqid_ds_t));
        return 0;
    }
    return -1;
}
