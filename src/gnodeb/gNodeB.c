#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdatomic.h>
#include <errno.h>
#include "gNodeB.h"
#include "worker.h"


atomic_int          gNodeB_sfn = 0;
pthread_t           paging_thread;

int32_t             ue_sockfd;
struct sockaddr_in  ue_addr;

int32_t             amf_sockfd;
struct sockaddr_in  amf_addr;

Worker_t            worker_send_paging;
Worker_t            worker_amf_receiver;
/*
    Biến dùng để tracking việc gNodeB có đang chạy hay không
*/
atomic_int          running = 1;

int32_t _initUE(struct sockaddr_in* ue_addr);
int32_t _initAMF(struct sockaddr_in* server_addr);

void _sendMIB(int32_t sockfd, const struct sockaddr_in* ue_addr, uint16_t sfn);
void _pagingItemHandler(WorkerQueueItem_t item);
void _amfConnectionHandler(WorkerQueueItem_t item);

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

    /* Khởi tạo worker xử lý nhận paging */
    workerInit(&worker_amf_receiver, _amfConnectionHandler, NUM_WORKER_THREADS);
    if (workerStart(&worker_amf_receiver) != 0) {
        printf("Khởi tạo worker threads thất bại\n");
        gNodeBStop();
        return -1;
    }

    /* Khởi tạo worker xử lý paging */
    workerInit(&worker_send_paging, _pagingItemHandler, NUM_WORKER_THREADS);
    if (workerStart(&worker_send_paging) != 0) {
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
    workerStop(&worker_send_paging);
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
        perror("bind AMF socket");
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
        
        /*
            Thực hiện add task vào worker queue để xử lý paging message
            tránh việc block thread chính khi chờ đợi SFN phù hợp để gửi paging đến UE
        */
        workerEnqueue(&worker_amf_receiver, (WorkerQueueItem_t)(intptr_t)client_sockfd);

        
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

void _amfConnectionHandler(WorkerQueueItem_t raw) {
    int32_t client_sockfd = (int32_t)(intptr_t)raw;
    while (1) {
        /* Kiểm tra nếu kết nối đã bị đóng */
        char buffer[1];
        ssize_t recv_result = recv(client_sockfd, buffer, sizeof(buffer), MSG_PEEK | MSG_DONTWAIT);
        if (recv_result == 0) {
            printf("[AMF Receiver] Kết nối đã bị đóng bởi client\n");
            close(client_sockfd);
            return;
        } else if (recv_result < 0 && errno != EWOULDBLOCK && errno != EAGAIN) {
            perror("recv");
            close(client_sockfd);
            return;
        }

        /* Nếu không có dữ liệu mới, tiếp tục chờ */
        if (recv_result < 0) {
            usleep(100000); // Sleep 100ms trước khi kiểm tra lại
            continue;
        }

        /* Nhận paging message từ AMF */
        Paging_t paging;
        ssize_t bytes_received = recv(client_sockfd, &paging, sizeof(paging), 0);
        if (bytes_received < 0) {
            perror("recv");
            close(client_sockfd);
            return;
        }

        if (bytes_received == sizeof(Paging_t) && paging.messageType == NGAP_PAGING_MESSAGE_TYPE) {
            printf("[gNodeB] Nhận NGAP Paging: UE ID=%u, TAC=%u, CN Domain=%u\n",
                   paging.ueId, paging.TAC, paging.cn_domain);

            Paging_t *item = malloc(sizeof(*item));
            if (item) {
                *item = paging;
                workerEnqueue(&worker_send_paging, (WorkerQueueItem_t)item);
            }

            /* response AMF */
            const char* response = "Đã nhận paging message";
            send(client_sockfd, response, strlen(response), 0);
        } else {
            printf("[gNodeB] Nhận dữ liệu không hợp lệ từ AMF\n");
            const char* response = "Dữ liệu không hợp lệ";
            send(client_sockfd, response, strlen(response), 0);
        }
    }
}