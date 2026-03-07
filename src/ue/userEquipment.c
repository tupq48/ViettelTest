#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <sys/time.h>
#include <sys/epoll.h>
#include <errno.h>
#include <pthread.h>
#include "userEquipment.h"
#include "common.h"

 
atomic_int  UE_sfn = 0;                     // SFN nội bộ của UE, tự cập nhật + sync MIB cập nhật
atomic_bool is_synced = false;              // track việc đã sync với gNodeB hay chưa
int8_t      mib_received_count = 0;         // dùng để tracking số lần nhận MIB
                                            // Theo dõi để wake up UE nhận paging

int32_t     gnodeb_sockfd;                  // Socket để UE nhận MIB và paging từ gNodeB
pthread_t   ue_sfn_auto_update_thread_id;   // Thread để tự động tăng SFN nội bộ

int32_t     ue_epfd;                        // epoll fd để UE chờ MIB và Paging
#define MAX_EVENTS 10
struct epoll_event events[MAX_EVENTS];      // Dùng chung cho epoll_wait
struct sockaddr_in ue_addr;                 // Địa chỉ UE

static void _handleMIB(MIB_t* mib);
static void _handlePaging(Paging_t* paging);

void* _ueSFNIncrease(void* arg);

atomic_bool running = true;                 // Biến dùng để tracking việc UE có đang chạy hay không, dùng để dừng thread khi cần

int32_t userInit() {
    gnodeb_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (gnodeb_sockfd < 0) {
        perror("socket");
        return -1;
    }

    memset(&ue_addr, 0, sizeof(ue_addr));
    ue_addr.sin_family = AF_INET;
    ue_addr.sin_port = htons(GNODEB_UDP_PORT);
    ue_addr.sin_addr.s_addr = inet_addr(GNODEB_BROADCAST_ADDRESS);

    if (bind(gnodeb_sockfd, (struct sockaddr*)&ue_addr, sizeof(ue_addr)) < 0) {
        perror("bind");
        close(gnodeb_sockfd);
        return -1;
    }

    ue_epfd = epoll_create1(0);
    if (ue_epfd < 0) {
        perror("epoll_create1");
        close(gnodeb_sockfd);
        return -1;
    }
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = gnodeb_sockfd;
    epoll_ctl(ue_epfd, EPOLL_CTL_ADD, gnodeb_sockfd, &ev);
    return 0;
}

int32_t userStart() {
    if (pthread_create(&ue_sfn_auto_update_thread_id, NULL, _ueSFNIncrease, NULL) != 0) {
        printf("Khởi tạo tick thread thất bại\n");
        return -1;
    }
    int32_t counter = 0;
    while (atomic_load(&running)) {
        int current_sfn = atomic_load(&UE_sfn);
        bool synced = atomic_load(&is_synced);

        // Tính toán điều kiện Wake-up cho Paging theo công thức
        // (SFN + PF_offset) mod T = (T div N) * (UE_ID mod N)
        int32_t left = (current_sfn + PAGING_FRAME_OFFSET) % PAGING_CYCLE;
        int32_t right = (PAGING_CYCLE / PF_PER_CYCLE) * (UE_ID_DEFAULT % PF_PER_CYCLE);
        bool is_paging_occasion = (left == right);

        // Điều kiện thức dậy: 
        // 1. Chưa đồng bộ (phải thức để tìm MIB)
        // 2. Đến kỳ Paging (DRX Wake-up)
        // 3. Đến kỳ cập nhật MIB - Kiểm tra mỗi khi SFN chia hết cho MIB_CYCLE
        bool should_wake_up = !synced || is_paging_occasion || (current_sfn % MIB_CYCLE == 0);

        int timeout = should_wake_up ? MIB_INCREASE_TIME_MS : MIB_INCREASE_TIME_MS * MIB_CYCLE;

        int n = epoll_wait(ue_epfd, events, MAX_EVENTS, timeout); 
        
        if (n == 0) {
            counter++;
            if (counter >= 10*MIB_CYCLE && synced) { // Nếu đã timeout 10 lần liên tiếp
                printf("[UE] Đang mất kết nối với gNodeB, chưa nhận được sự kiện nào trong thời gian dài\n");
                atomic_store(&is_synced, false);
                counter = 0;
            }
            continue; // Timeout, quay lại vòng lặp check SFN tiếp theo
        } else {
            counter = 0; // Reset counter nếu có sự kiện
        }
        for (int i = 0; i < n; i++) {
            if (events[i].data.fd == gnodeb_sockfd) {
                char buffer[1024];
                struct sockaddr_in gnb_addr;
                socklen_t addr_len = sizeof(gnb_addr);
                int ret = recvfrom(gnodeb_sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&gnb_addr, &addr_len);
                
                if (ret <= 0) continue;

                // Xử lý MIB
                if (ret == sizeof(MIB_t)) {
                    _handleMIB((MIB_t*)buffer);
                } 
                // Xử lý Paging
                else if (ret == sizeof(Paging_t) && synced && is_paging_occasion) {
                    _handlePaging((Paging_t*)buffer);
                }
            }
        }
    }

    return 0;
}

void userStop() {
    atomic_store(&running, false);
    pthread_cancel(ue_sfn_auto_update_thread_id);
    pthread_join(ue_sfn_auto_update_thread_id, NULL);
    close(gnodeb_sockfd);
    close(ue_epfd);
}

void* _ueSFNIncrease(void* arg) {
    while (atomic_load(&running)) {
        usleep(SFN_INCREASE_TIME_MS * 1000);
        if (false == atomic_load(&is_synced)) {
            // Nếu chưa sync, skip và chờ sync
            continue;
        }
        int current = atomic_load(&UE_sfn);
        current = (current + 1) % (MAX_SFN + 1);
        atomic_store(&UE_sfn, current);
        if (current % 100 == 0) {
            printf("[UE] SFN nội bộ hiện tại: %d\n", current);
        }
    }
    return NULL;
}

static void _handleMIB(MIB_t* mib) {
    uint16_t received_sfn = ntohs(mib->sfnValue);

    atomic_store(&UE_sfn, received_sfn);
    if (!atomic_load(&is_synced)) {
        atomic_store(&is_synced, true);
        printf("[UE] Đã đồng bộ SFN = %d\n", received_sfn);
    }
}

static void _handlePaging(Paging_t* paging) {
    if (paging->messageType == NGAP_PAGING_MESSAGE_TYPE && paging->ueId == UE_ID_DEFAULT) {
        printf("[UE] Nhận Paging thành công tại SFN=%d\n", atomic_load(&UE_sfn));
        printf("[UE] NgAP CN Domain: %s\n", paging->cn_domain == CN_DOMAIN_NORMAL_CALL ? "Normal Call" : "Data App Call");
    }
}