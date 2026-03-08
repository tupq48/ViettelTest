#include <signal.h>
#include <stdio.h>
#include "gNodeB.h"

static void handle_sigint(int sig) {
    (void)sig;
    printf("received SIGINT, stopping...\n");
    gNodeBStop();
}

int main() {
    signal(SIGINT, handle_sigint);

    if (gNodeBInit() != 0) {
        printf("Khởi tạo gNodeB thất bại\n");
        return -1;
    }

    if (gNodeBStart() != 0) {
        printf("Khởi động gNodeB thất bại\n");
        return -1;
    }

    return 0;
}