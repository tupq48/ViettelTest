#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "common.h"

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

int main() {
    int32_t sockfd;
    struct sockaddr_in ue_addr;
    uint16_t gNodeB_sfn = 0;

    sockfd = initUserAddress(&ue_addr);
    if (sockfd < 0) {
        fprintf(stderr, "Khởi tạo địa chỉ UE thất bại\n");
        return -1;
    }
    printf("[gNodeB] Bắt đầu phát sóng...\n");

    while (1) {
        if (gNodeB_sfn % MIB_CYCLE == 0) {
            sendMIB(sockfd, &ue_addr, gNodeB_sfn);
        }

        usleep(SFN_INCREASE_TIME_MS * 1000); // Đợi 10ms
        gNodeB_sfn = (gNodeB_sfn + 1) % (MAX_SFN + 1);
        
        if (gNodeB_sfn % 100 == 0)
            printf("[gNodeB] SFN hiện tại: %d\n", gNodeB_sfn);
    }
    return 0;
}
