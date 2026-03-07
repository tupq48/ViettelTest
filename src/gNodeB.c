#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdatomic.h>
#include "common.h"


atomic_int          gNodeB_sfn = 0;

int32_t             ue_sockfd;
struct sockaddr_in  ue_addr;

int32_t             paging_sockfd;
struct sockaddr_in  paging_server_addr;

/* Paging queue structure */
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

PagingQueue_t paging_queue;

int32_t initUserAddress(struct sockaddr_in* ue_addr);
int32_t initPagingServer(struct sockaddr_in* server_addr);

void    sendMIB(int32_t sockfd, const struct sockaddr_in* ue_addr, uint16_t sfn);

void*   pagingServerThread(void* arg);
void*   pagingWorkerThread(void* arg);

void    enqueuePagingMessage(PagingQueue_t* queue, const PagingQueueItem_t* item);
int32_t dequeuePagingMessage(PagingQueue_t* queue, PagingQueueItem_t* item);

int main() {

    /* Khởi tạo paging queue */
    paging_queue.head = 0;
    paging_queue.tail = 0;
    paging_queue.count = 0;
    pthread_mutex_init(&paging_queue.mutex, NULL);
    pthread_cond_init(&paging_queue.cond, NULL);

    ue_sockfd = initUserAddress(&ue_addr);
    if (ue_sockfd < 0) {
        printf("Khởi tạo địa chỉ UE thất bại\n");
        return -1;
    }

    paging_sockfd = initPagingServer(&paging_server_addr);
    if (paging_sockfd < 0) {
        printf("Khởi tạo paging server thất bại\n");
        close(ue_sockfd);
        return -1;
    }
    
    /* Khởi tạo paging server thread */
    pthread_t paging_server_thread;
    if (pthread_create(&paging_server_thread, NULL, pagingServerThread, &paging_sockfd) != 0) {
        printf("Khởi tạo paging server thread thất bại\n");
        close(ue_sockfd);
        close(paging_sockfd);
        return -1;
    }

    /* Khởi tạo worker thread pool */
    pthread_t worker_threads[NUM_WORKER_THREADS];
    for (int i = 0; i < NUM_WORKER_THREADS; i++) {
        if (pthread_create(&worker_threads[i], NULL, pagingWorkerThread, NULL) != 0) {
            printf("Khởi tạo worker thread %d thất bại\n", i);
            close(ue_sockfd);
            close(paging_sockfd);
            return -1;
        }
    }

    printf("[gNodeB] Bắt đầu phát sóng...\n");

    while (1) {
        if (atomic_load(&gNodeB_sfn) % MIB_CYCLE == 0) {
            sendMIB(ue_sockfd, &ue_addr, atomic_load(&gNodeB_sfn));
        }

        usleep(SFN_INCREASE_TIME_MS * 1000); // Đợi 10ms
        atomic_store(&gNodeB_sfn, (atomic_load(&gNodeB_sfn) + 1) % (MAX_SFN + 1));
        
        if (atomic_load(&gNodeB_sfn) % 100 == 0)
            printf("[gNodeB] SFN hiện tại: %d\n", atomic_load(&gNodeB_sfn));
    }

    pthread_join(paging_server_thread, NULL);
    for (int i = 0; i < NUM_WORKER_THREADS; i++) {
        pthread_join(worker_threads[i], NULL);
    }
    close(ue_sockfd);
    close(paging_sockfd);
    return 0;
}

int32_t initUserAddress(struct sockaddr_in* ue_addr) {
    int32_t sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return -1;
    }

    memset(ue_addr, 0, sizeof(*ue_addr));
    ue_addr->sin_family = AF_INET;
    ue_addr->sin_port = htons(GNODEB_UDP_PORT);
    ue_addr->sin_addr.s_addr = inet_addr(GNODEB_BROADCAST_ADDRESS);

    return sockfd;
}

void sendMIB(int32_t sockfd, const struct sockaddr_in* ue_addr, uint16_t sfn) {
    MIB_t mib = {MESSAGE_MIB_ID, htons(sfn)};
    sendto(sockfd, &mib,
           sizeof(mib), 0,
           (struct sockaddr*) ue_addr,
           sizeof(*ue_addr)
    );
}

int32_t initPagingServer(struct sockaddr_in* server_addr) {
    int32_t sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return -1;
    }

    memset(server_addr, 0, sizeof(*server_addr));
    server_addr->sin_family = AF_INET;
    server_addr->sin_port = htons(GNODEB_TCP_PORT);
    server_addr->sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (struct sockaddr*) server_addr, sizeof(*server_addr)) < 0) {
        perror("bind");
        close(sockfd);
        return -1;
    }

    if (listen(sockfd, 5) < 0) {
        perror("listen");
        close(sockfd);
        return -1;
    }

    printf("[gNodeB] Paging server đã sẵn sàng tại port %d\n", GNODEB_TCP_PORT);
    return sockfd;
}

void enqueuePagingMessage(PagingQueue_t* queue, const PagingQueueItem_t* item) {
    pthread_mutex_lock(&queue->mutex);
    
    if (queue->count >= PAGING_QUEUE_SIZE) {
        printf("[gNodeB] Queue đầy, bỏ qua paging message\n");
        pthread_mutex_unlock(&queue->mutex);
        return;
    }

    queue->items[queue->tail] = *item;
    queue->tail = (queue->tail + 1) % PAGING_QUEUE_SIZE;
    queue->count++;

    pthread_cond_signal(&queue->cond);
    pthread_mutex_unlock(&queue->mutex);
}

int32_t dequeuePagingMessage(PagingQueue_t* queue, PagingQueueItem_t* item) {
    pthread_mutex_lock(&queue->mutex);
    
    while (queue->count == 0) {
        pthread_cond_wait(&queue->cond, &queue->mutex);
    }

    *item = queue->items[queue->head];
    queue->head = (queue->head + 1) % PAGING_QUEUE_SIZE;
    queue->count--;

    pthread_mutex_unlock(&queue->mutex);
    return 0;
}

void* pagingServerThread(void* arg) {
    int32_t server_sockfd = *(int32_t*) arg;
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int32_t client_sockfd = accept(server_sockfd, (struct sockaddr*) &client_addr, &addr_len);
        if (client_sockfd < 0) {
            perror("accept");
            continue;
        }

        printf("[gNodeB] Nhận kết nối từ %s:%d\n",
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        /* Nhận paging message */
        Paging_t paging_message;
        ssize_t recv_len = recv(client_sockfd, &paging_message, sizeof(paging_message), 0);
        if (recv_len < 0) {
            perror("recv");
            close(client_sockfd);
            continue;
        }

        if (recv_len == sizeof(Paging_t) && paging_message.messageType == NGAP_PAGING_MESSAGE_TYPE) {
            printf("[gNodeB] Nhận NGAP Paging: UE ID=%u, TAC=%u, CN Domain=%u\n",
                   paging_message.ueId, paging_message.TAC, paging_message.cn_domain);

            /* Đưa vào queue để worker xử lý */
            PagingQueueItem_t queue_item = {
                .message = paging_message,
                .receive_time = 0
            };
            enqueuePagingMessage(&paging_queue, &queue_item);

            /* Gửi ACK */
            const char* response = "Paging message received";
            send(client_sockfd, response, strlen(response), 0);
        }

        close(client_sockfd);
    }
    return NULL;
}

void* pagingWorkerThread(void* arg) {
    (void)arg;
    PagingQueueItem_t queue_item;

    while (1) {
        /* Lấy paging message từ queue */
        dequeuePagingMessage(&paging_queue, &queue_item);
        
        Paging_t* paging = &queue_item.message;
        int32_t ue_id = paging->ueId;

        printf("[Worker] Xử lý paging cho UE ID=%u\n", ue_id);

        /* Chờ khớp điều kiện SFN */
        while (1) {
            int32_t current_sfn = atomic_load(&gNodeB_sfn);
            int32_t left_side = (current_sfn + PAGING_FRAME_OFFSET) % PAGING_CYCLE;
            int32_t right_side = (PAGING_CYCLE / PF_PER_CYCLE) * (ue_id % PF_PER_CYCLE);

            if (left_side == right_side) {
                /* Gửi RRC Paging tới UE */
                sendto(ue_sockfd, paging, sizeof(Paging_t), 0,
                       (struct sockaddr*) &ue_addr, sizeof(ue_addr));
                printf("[Worker] Gửi RRC Paging tới UE ID=%u tại SFN=%d\n", 
                       ue_id, current_sfn);
                break;
            }

            usleep(1000); /* Chờ 1ms trước khi kiểm tra lại */
        }
    }
    return NULL;
}