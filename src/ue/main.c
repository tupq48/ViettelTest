#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include "userEquipment.h"

void handle_sigint(int sig) {
    printf("\n[UE] Nhận tín hiệu dừng, đang tắt UE...\n");
    userStop();
    exit(0);
}

int main() {
    signal(SIGINT, handle_sigint);
    if (userInit() != 0) {
        printf("Khởi tạo UE thất bại\n");
        return -1;
    }

    if (userStart() != 0) {
        printf("Khởi động UE thất bại\n");
        userStop();
        return -1;
    }

    return 0;
}