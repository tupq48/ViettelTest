#ifndef GNODEB_H
#define GNODEB_H

#include <stdint.h>
#include <pthread.h>
#include <netinet/in.h>
#include "common.h"

extern atomic_int gNodeB_sfn;
extern int32_t ue_sockfd;
extern struct sockaddr_in ue_addr;
extern int32_t paging_sockfd;
extern struct sockaddr_in paging_server_addr;

typedef struct {
    Paging_t message;
    uint64_t receive_time;
} PagingQueueItem_t;

typedef struct {
    PagingQueueItem_t items[PAGING_QUEUE_SIZE];
    uint32_t head;
    uint32_t tail;
    uint32_t count;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} PagingQueue_t;

extern PagingQueue_t paging_queue;

int32_t initUserAddress(struct sockaddr_in* ue_addr);
void sendMIB(int32_t sockfd, const struct sockaddr_in* ue_addr, uint16_t sfn);
int32_t initPagingServer(struct sockaddr_in* server_addr);
void enqueuePagingMessage(PagingQueue_t* queue, const PagingQueueItem_t* item);
int32_t dequeuePagingMessage(PagingQueue_t* queue, PagingQueueItem_t* item);
void* pagingServerThread(void* arg);
void* pagingWorkerThread(void* arg);

#endif // GNODEB_H