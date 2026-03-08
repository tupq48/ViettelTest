#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include "worker.h"

static void _initWorkerQueue(WorkerQueue_t* queue);
static void* _worker_thread_main(void *arg);
static void _cleanup_mutex_unlock(void *mutex);
static int32_t _dequeueWorkerMessage(WorkerQueue_t* queue, WorkerQueueItem_t* item);

void workerInit(Worker_t *w, WorkerCallback cb, int num_threads) {
    _initWorkerQueue(&w->queue);
    w->callback = cb;
    w->num_threads = num_threads;
    w->threads = calloc(num_threads, sizeof(pthread_t));
}

int workerStart(Worker_t *w) {
    for (int i = 0; i < w->num_threads; i++) {
        if (pthread_create(&w->threads[i], NULL, _worker_thread_main, w) != 0) {
            perror("pthread_create");
            return -1;
        }
    }
    return 0;
}

void workerEnqueue(Worker_t *w, WorkerQueueItem_t item) {
    WorkerQueue_t *queue = &w->queue;
    pthread_mutex_lock(&queue->mutex);
    
    if (queue->count >= WORKER_QUEUE_SIZE) {
        printf("[Worker] Queue full, dropping message\n");
        pthread_mutex_unlock(&queue->mutex);
        return;
    }

    queue->items[queue->tail] = item;
    queue->tail = (queue->tail + 1) % WORKER_QUEUE_SIZE;
    queue->count++;

    pthread_cond_signal(&queue->cond);
    pthread_mutex_unlock(&queue->mutex);
}


int workerStop(Worker_t *w) {
    for (int i = 0; i < w->num_threads; i++) {
        pthread_cancel(w->threads[i]);
        pthread_join(w->threads[i], NULL);
    }
    free(w->threads);
    return 0;
}

static void _initWorkerQueue(WorkerQueue_t* queue) {
    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;
    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->cond, NULL);
}



static void _cleanup_mutex_unlock(void *mutex) {
    pthread_mutex_unlock((pthread_mutex_t *)mutex);
}

static int32_t _dequeueWorkerMessage(WorkerQueue_t* queue, WorkerQueueItem_t* item) {
    pthread_mutex_lock(&queue->mutex);
    pthread_cleanup_push(_cleanup_mutex_unlock, &queue->mutex);

    while (queue->count == 0) {
        pthread_cond_wait(&queue->cond, &queue->mutex);
    }

    *item = queue->items[queue->head];
    queue->head = (queue->head + 1) % WORKER_QUEUE_SIZE;
    queue->count--;

    pthread_cleanup_pop(1);
    return 0;
}

static void* _worker_thread_main(void *arg) {
    Worker_t *w = (Worker_t*)arg;
    WorkerQueueItem_t queue_item;

    while (1) {
        _dequeueWorkerMessage(&w->queue, &queue_item);
        if (w->callback) {
            w->callback(queue_item);
        }
    }
    return NULL;
}


