#if !defined(_QALARM_H)
#define _QALARM_H

#define POLLSEC 5

#define QALARM_DEFAULT  0
#define QALARM_RECUR    1

struct _qthread;
struct _qalarm;

typedef void (*qalarm_cb)(void *);

typedef struct _qthread {
    struct _qalarm *parent;

    pthread_t tid;
    unsigned long flags;
    qalarm_cb cb;
    void *data;

    QUEUE *queue;
    int timeout;

    struct _qthread *prev;
    struct _qthread *next;
} QTHREAD;

typedef struct _qalarm {
    pthread_t tid;
    pthread_mutex_t q_mutex;

    QTHREAD *threads;

    QUEUE *queue;
} QALARM;

QALARM *qalarm(void);
int add_alarm(QALARM *, int, qalarm_cb, void *, unsigned long);
void terminate_alarms(QALARM *);
void delete_alarm(QALARM *, pthread_t);
void delete_qalarm(QALARM *);

#endif
