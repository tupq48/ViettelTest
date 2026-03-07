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
#include "common.h"

 
atomic_int  UE_sfn = 0;
atomic_bool is_synced = false;
int8_t      mib_received_count = 0;

void* tick_thread(void* arg) {
    while (1) {
        usleep(10 * 1000); // 10ms theo tài liệu
        
        // Luôn tăng SFN nội bộ để giữ nhịp, 
        // nhưng chỉ bắt đầu chuẩn hóa khi đã nhận MIB đầu tiên
        int current = atomic_load(&UE_sfn);
        current = (current + 1) % 1024;
        atomic_store(&UE_sfn, current);
        
        if (current % 100 == 0 && atomic_load(&is_synced)) {
            printf("[UE] Tick - SFN: %d\n", current);
        }
    }
    return NULL;
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

    bind(sockfd, (struct sockaddr*)ue_addr, sizeof(*ue_addr));
    return sockfd;
}

int main() {
    int sockfd;
    struct sockaddr_in my_addr;

    sockfd = initUserAddress(&my_addr);
    if (sockfd < 0) {
        fprintf(stderr, "Khởi tạo địa chỉ UE thất bại\n");
        return -1;
    }

    printf("[UE] Starting epoll event loop\n");

    int epfd = epoll_create1(0);
    if (epfd < 0) {
        perror("epoll_create1");
        close(sockfd);
        return -1;
    }

    struct epoll_event ev, events[2];
    ev.events = EPOLLIN;
    ev.data.fd = sockfd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, sockfd, &ev);

    // spawn tick thread
    pthread_t tick_tid;
    pthread_create(&tick_tid, NULL, tick_thread, NULL);

    while (1) {
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
        // 3. Đến kỳ cập nhật MIB (800ms) - Giả sử kiểm tra mỗi khi SFN chia hết cho 80
        bool should_wake_up = !synced || is_paging_occasion || (current_sfn % 80 == 0);

        int timeout = should_wake_up ? 10 : 100; // Thức thì đợi 10ms, ngủ thì đợi lâu hơn

        int n = epoll_wait(epfd, events, 2, timeout); 
        
        if (n == 0) continue; // Timeout, quay lại vòng lặp check SFN tiếp theo

        for (int i = 0; i < n; i++) {
            if (events[i].data.fd == sockfd) {
                char buffer[1024];
                struct sockaddr_in gnb_addr;
                socklen_t addr_len = sizeof(gnb_addr);
                int ret = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&gnb_addr, &addr_len);
                
                if (ret <= 0) continue;

                // Xử lý MIB
                if (ret == sizeof(MIB_t)) {
                    MIB_t* mib = (MIB_t*)buffer;
                    uint16_t received_sfn = ntohs(mib->sfnValue);
                    // Cập nhật SFN theo chu kỳ 80ms nếu chưa sync, hoặc 800ms nếu đã sync
                    atomic_store(&UE_sfn, received_sfn);
                    if (!synced) {
                        atomic_store(&is_synced, true);
                        printf("[UE] Đã đồng bộ SFN = %d\n", received_sfn);
                    }
                } 
                // Xử lý Paging
                else if (ret == sizeof(Paging_t) && synced && is_paging_occasion) {
                    Paging_t* paging = (Paging_t*)buffer;
                    if (paging->messageType == NGAP_PAGING_MESSAGE_TYPE && paging->ueId == UE_ID_DEFAULT) {
                        printf("[UE] Nhận Paging thành công tại SFN=%d\n", current_sfn);
                    }
                }
            }
        }
    }

    close(epfd);
    close(sockfd);
    return 0;
}
