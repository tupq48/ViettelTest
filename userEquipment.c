#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <stdatomic.h>
#include <stdbool.h>
#include "common.h"

 
atomic_int  UE_sfn = 0;
atomic_bool is_synced = false;
int8_t      mib_received_count = 0;

void* receive_mib_thread(void* arg) {
    int sockfd = *(int*)arg;
    free(arg);
    struct sockaddr_in gnb_addr;
    socklen_t addr_len = sizeof(gnb_addr);
    MIB_t mib;

    printf("[UE] Starting, listening on port %d\n", GNODEB_UDP_PORT);
    while (1) {
        int ret = recvfrom(sockfd,
                           &mib, sizeof(mib),
                           0,
                           (struct sockaddr*)&gnb_addr, &addr_len);
        if (ret < 0) {
            perror("recvfrom failed");
            continue;
        }

        uint16_t received_sfn = ntohs(mib.sfnValue);
        
        if (false == atomic_load(&is_synced)) {
            atomic_store(&UE_sfn, received_sfn);
            atomic_store(&is_synced, true);
            printf("[UE] Đồng bộ lần đầu: SFN = %d\n", received_sfn);
        } else {
            mib_received_count++;
            if (mib_received_count >= MIB_SYNC_CYCLE) {
                int16_t oldValue = atomic_load(&UE_sfn);
                atomic_store(&UE_sfn, received_sfn);
                mib_received_count = 0;
                if (oldValue != received_sfn) {
                    printf("[UE] Hiệu chỉnh SFN từ %d -> %d\n", oldValue, received_sfn);
                }
            }
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
    pthread_t thread_id;

    sockfd = initUserAddress(&my_addr);
    if (sockfd < 0) {
        fprintf(stderr, "Khởi tạo địa chỉ UE thất bại\n");
        return -1;
    }

    int* sockfd_ptr = malloc(sizeof(int));
    *sockfd_ptr = sockfd;
    pthread_create(&thread_id, NULL, receive_mib_thread, sockfd_ptr);
    printf("[UE] Đang đợi MIB từ gNodeB...\n");

    while (1) {
        usleep(SFN_INCREASE_TIME_MS * 1000);
        
        if (atomic_load(&is_synced)) {
            int current = atomic_load(&UE_sfn);
            atomic_store(&UE_sfn, (current + 1) % (MAX_SFN + 1));
            
            int display_sfn = atomic_load(&UE_sfn);
            if (display_sfn % 100 == 0) {
                printf("[UE] Tick - SFN: %d\n", display_sfn);
            }
        }
    }
    
    close(sockfd);
    pthread_join(thread_id, NULL);
    return 0;
}
