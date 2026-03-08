#ifndef WORKER_H
#define WORKER_H

#include <stdint.h>
#include <pthread.h>
#include "common.h"

typedef void *WorkerQueueItem_t;
typedef void (*WorkerCallback)(WorkerQueueItem_t item);

typedef struct {
    WorkerQueueItem_t items[WORKER_QUEUE_SIZE];
    uint32_t head;
    uint32_t tail;
    uint32_t count;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} WorkerQueue_t;


typedef struct {
    WorkerQueue_t queue;
    WorkerCallback callback;
    pthread_t *threads;
    int num_threads;
} Worker_t;


void workerInit(Worker_t *w, WorkerCallback cb, int num_threads);
int  workerStart(Worker_t *w);
void workerEnqueue(Worker_t *w, WorkerQueueItem_t item);
int  workerStop(Worker_t *w);
#endif // WORKER_H
