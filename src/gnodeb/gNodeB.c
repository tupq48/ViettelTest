#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdatomic.h>
#include "gNodeB.h"
#include "worker.h"


atomic_int          gNodeB_sfn = 0;
pthread_t           paging_thread;

int32_t             ue_sockfd;
struct sockaddr_in  ue_addr;

int32_t             amf_sockfd;
struct sockaddr_in  amf_addr;

Worker_t            worker;
/*
    Biến dùng để tracking việc gNodeB có đang chạy hay không
*/
atomic_int          running = 1;

int32_t _initUE(struct sockaddr_in* ue_addr);
int32_t _initAMF(struct sockaddr_in* server_addr);

void _sendMIB(int32_t sockfd, const struct sockaddr_in* ue_addr, uint16_t sfn);
void _pagingItemHandler(WorkerQueueItem_t item);

void* _pagingReceiverThread(void* arg);

int32_t gNodeBInit() {
    ue_sockfd = _initUE(&ue_addr);
    if (ue_sockfd < 0) {
        printf("Khởi tạo địa chỉ UE thất bại\n");
        return -1;
    }

    amf_sockfd = _initAMF(&amf_addr);
    if (amf_sockfd < 0) {
        printf("Khởi tạo AMF thất bại\n");
        close(ue_sockfd);
        return -1;
    }
    
    return 0;
}

int32_t gNodeBStart() {
    /* Khởi tạo paging server thread */
    if (pthread_create(&paging_thread, NULL, _pagingReceiverThread, &amf_sockfd) != 0) {
        printf("Khởi tạo paging thread thất bại\n");
        gNodeBStop();
        return -1;
    }

    /* Khởi tạo worker module (queue + callback) */
    workerInit(&worker, _pagingItemHandler, NUM_WORKER_THREADS);
    if (workerStart(&worker) != 0) {
        printf("Khởi tạo worker threads thất bại\n");
        gNodeBStop();
        return -1;
    }

    printf("[gNodeB] Bắt đầu phát sóng...\n");

    while (atomic_load(&running)) {
        if (atomic_load(&gNodeB_sfn) % MIB_CYCLE == 0) {
            _sendMIB(ue_sockfd, &ue_addr, atomic_load(&gNodeB_sfn));
        }

        usleep(SFN_INCREASE_TIME_MS * 1000);
        atomic_store(&gNodeB_sfn,
                     (atomic_load(&gNodeB_sfn) + 1) % (MAX_SFN + 1));
        
        if (atomic_load(&gNodeB_sfn) % 100 == 0)
            printf("[gNodeB] SFN hiện tại: %d\n", atomic_load(&gNodeB_sfn));
    }

    return 0;
}

void gNodeBStop() {
    atomic_store(&running, 0);

    /* Đóng socket và dừng thread */
    close(ue_sockfd);
    close(amf_sockfd);
    pthread_cancel(paging_thread);
    pthread_join(paging_thread, NULL);
    workerStop(&worker);
}

int32_t _initUE(struct sockaddr_in* ue_addr) {
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

int32_t _initAMF(struct sockaddr_in* server_addr) {
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

    printf("[gNodeB] AMF listener server đã sẵn sàng tại port %d\n", GNODEB_TCP_PORT);
    return sockfd;
}

void _sendMIB(int32_t sockfd, const struct sockaddr_in* ue_addr, uint16_t sfn) {
    MIB_t mib = {MESSAGE_MIB_ID, htons(sfn)};
    sendto(sockfd, &mib,
           sizeof(mib), 0,
           (struct sockaddr*) ue_addr,
           sizeof(*ue_addr)
    );
}



void* _pagingReceiverThread(void* arg) {
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

            Paging_t *item = malloc(sizeof(*item));
            if (item) {
                *item = paging_message;
                workerEnqueue(&worker, item);
            }

            /* response AMF */
            const char* response = "Đã nhận paging message";
            send(client_sockfd, response, strlen(response), 0);
        }

        close(client_sockfd);
    }
    return NULL;
}

void _pagingItemHandler(WorkerQueueItem_t raw) {
    Paging_t *paging = (Paging_t*)raw;
    int32_t ue_id = paging->ueId;

    printf("[Worker] Processing paging for UE ID=%u\n", ue_id);

    /* wait for the correct SFN */
    while (1) {
        int32_t current_sfn = atomic_load(&gNodeB_sfn);
        int32_t left_side = (current_sfn + PAGING_FRAME_OFFSET) % PAGING_CYCLE;
        int32_t right_side = (PAGING_CYCLE / PF_PER_CYCLE) * (ue_id % PF_PER_CYCLE);

        if (left_side == right_side) {
            sendto(ue_sockfd, paging, sizeof(Paging_t), 0,
                   (struct sockaddr*) &ue_addr, sizeof(ue_addr));
            printf("[Worker] Sent RRC Paging to UE ID=%u at SFN=%d\n", ue_id, current_sfn);
            break;
        }

        usleep(1000);
    }

    free(paging);
}