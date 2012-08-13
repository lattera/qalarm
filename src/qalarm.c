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

static QTHREAD *find_alarm(QALARM *q, pthread_t tid)
{
    QTHREAD *qt;

    for (qt = q->threads; qt != NULL; qt = qt->next)
        if (qt->tid == tid)
            return qt;

    return NULL;
}

static void *main_alarm_handler(void *ctx)
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

            while (q->threads)
                delete_alarm(q, q->threads->tid);

            Free_Queue_Item(item);
            return NULL;
        } else if (!strcmp(item->action, "Wait")) {
            /* Wait for termination */
            for (qt = q->threads; qt != NULL; qt = qt->next)
                pthread_join(qt->tid, NULL);

            while (q->threads)
                delete_alarm(q, q->threads->tid);

            Free_Queue_Item(item);
            return NULL;
        } else if (!strcmp(item->action, "Destroy")) {
            if (item->sz != sizeof(pthread_t)) {
                fprintf(stderr, "Destroy action: unknown size: %u. Expected %u\n", item->sz, sizeof(pthread_t));

                Free_Queue_Item(item);
                continue;
            }

            pthread_join(*((pthread_t *)(item->data)), NULL);
            delete_alarm(q, *((pthread_t *)(item->data)));
        } else {
            fprintf(stderr, "Unknown action: %s\n", item->action);
        }

        Free_Queue_Item(item);
    }

    return NULL;
}

static void *inner_alarm_handler(void *ctx)
{
    QTHREAD *qt = (QTHREAD *)ctx;
    time_t starttime;
    time_t newtime;
    struct timeval t;
    QUEUE_ITEM *item;

    starttime = time(NULL);
    newtime = qt->timeout;
    while (time(NULL) - starttime < newtime) {
        t.tv_sec = newtime < POLLSEC ? newtime : POLLSEC;
        t.tv_usec = 0;

        select(0, NULL, NULL, NULL, &t);
        newtime = qt->timeout - (time(NULL) - starttime);

        if (newtime > 0) {
            if (!Queue_Empty(qt->queue)) {
                item = Get_Queue_Item(qt->queue);
                if (!strcmp(item->action, "Terminate"))
                    return NULL;
            }
        }
    }

    qt->cb(qt->data);
    
    Add_Queue_Item(qt->parent->queue, "Destroy", &(qt->tid), sizeof(pthread_t));

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

void delete_alarm(QALARM *q, pthread_t tid)
{
    QTHREAD *qt;

    pthread_mutex_lock(&(q->q_mutex));

    qt = find_alarm(q, tid);
    if ((qt->prev))
        qt->prev->next = qt->next;
    else
        q->threads = qt->next;

    if ((qt->next))
        qt->next->prev = qt->prev;

    pthread_mutex_unlock(&(q->q_mutex));

    Delete_Queue(qt->queue);
    free(qt);
}

QALARM *qalarm(void)
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

void delete_qalarm(QALARM *q)
{
    terminate_alarms(q);
    free(q);
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
    int i=*((int *)data);

    printf("KABOOM: %d!\n", i);
}

int main(int argc, char *argv[])
{
    QALARM *q;
    QTHREAD *qt;
    int i, *j;

    q = qalarm();
    for (i=0; i < 10; i++) {
        j = malloc(sizeof(int));
        if (j)
            *j = i;

        /* Alarm should activate anywhere between 0-2 seconds. */
        add_alarm(q, i%2, alarmed, j);
    }

    /* Yeah, I'm lazy */
    if (argv[1])
        terminate_alarms(q);
    else
        wait_alarms(q);

    delete_qalarm(q);

    return 0;
}

#endif
