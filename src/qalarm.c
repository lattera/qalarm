#define _BSD_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/time.h>
#include <sys/select.h>

#include "misc.h"
#include "queue.h"
#include "qalarm.h"

QTHREAD *find_alarm(QALARM *q, pthread_t tid)
{
    QTHREAD *qt;

    for (qt = q->threads; qt != NULL; qt = qt->next)
        if (qt->tid == tid)
            return qt;

    return NULL;
}

void *main_alarm_handler(void *ctx)
{
    QALARM *q = (QALARM *)ctx;
    QUEUE_ITEM *item;
    pthread_t tid;
    QTHREAD *qt;

    while ((item = Get_Queue_Item(q->queue))) {
        if (!strcmp(item->action, "Terminate")) {
            /* Send termination signal */
            for (qt = q->threads; qt != NULL; qt = qt->next)
                Add_Queue_Item(qt->queue, "Terminate", NULL, 0);

            /* Wait for termination */
            for (qt = q->threads; qt != NULL; qt = qt->next)
                pthread_join(qt->tid, NULL);

            Free_Queue_Item(item);
            return NULL;
        } else if (!strcmp(item->action, "Wait")) {
            /* Wait for termination */
            for (qt = q->threads; qt != NULL; qt = qt->next)
                pthread_join(qt->tid, NULL);

            Free_Queue_Item(item);
            return NULL;
        } else {
            fprintf(stderr, "Unknown action: %s\n", item->action);
        }

        Free_Queue_Item(item);
    }

    return NULL;
}

void *inner_alarm_handler(void *ctx)
{
    QTHREAD *qt = (QTHREAD *)ctx;
    unsigned long nsecs;
    struct timeval t;

    t.tv_sec = qt->timeout;
    t.tv_usec = 0;

    printf("New alarm, inner loop, tid: %d\n", qt->tid);
    select(0, NULL, NULL, NULL, &t);

    qt->cb(qt->data);

    return NULL;
}

int add_alarm(QALARM *q, int nsecs, qalarm_cb cb, void *data)
{
    QTHREAD *qt;

    pthread_mutex_lock(&(q->q_mutex));

    if (!(q->threads)) {
        q->threads = qt = calloc(1, sizeof(QTHREAD));
        if (!(q->threads))
            return -1;
    } else {
        for (qt = q->threads; qt->next != NULL; qt = qt->next)
            ;

        qt->next = calloc(1, sizeof(QTHREAD));
        if (!(qt->next))
            return -1;

        qt->next->prev = qt;
        qt = qt->next;
    }

    pthread_mutex_unlock(&(q->q_mutex));

    qt->parent = q;
    qt->cb = cb;
    qt->data = data;
    qt->timeout = nsecs;

    qt->queue = Initialize_Queue();
    if (!(qt->queue)) {
        pthread_mutex_lock(&(q->q_mutex));
        qt->prev->next = NULL;
        pthread_mutex_unlock(&(q->q_mutex));

        return -1;
    }

    pthread_create(&(qt->tid), NULL, inner_alarm_handler, qt);

    return 0;
}

QALARM *new_alarm(void)
{
    QALARM *q;

    q = calloc(1, sizeof(QALARM));
    if (!(q))
        return NULL;

    q->queue = Initialize_Queue();
    if (!(q->queue)) {
        free(q);
        return NULL;
    }

    pthread_mutex_init(&(q->q_mutex), NULL);

    pthread_create(&(q->tid), NULL, main_alarm_handler, q);

    return q;
}

void terminate_alarms(QALARM *q)
{
    Add_Queue_Item(q->queue, "Terminate", NULL, 0);
    pthread_join(q->tid, NULL);
}

void wait_alarms(QALARM *q)
{
    Add_Queue_Item(q->queue, "Wait", NULL, 0);
    pthread_join(q->tid, NULL);
}

#if defined(TEST_CODE)

void alarmed(void *data)
{
    printf("KABOOM!\n");
}

int main(int argc, char *argv[])
{
    QALARM *qalarm;
    QTHREAD *qt;

    qalarm = new_alarm();
    add_alarm(qalarm, 5, alarmed, NULL);

    terminate_alarms(qalarm);

    return 0;
}

#endif
