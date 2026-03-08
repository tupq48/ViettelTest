#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "AMF.h"
#include "common.h"

static int32_t              gNodeB_sockfd = -1;
static struct sockaddr_in   gNodeB_addr;
#define BUFFER_SIZE 1024

static int32_t _initGNodeBAddress();

int32_t AMFInit() {
    if (_initGNodeBAddress() < 0) {
        printf("Failed to initialize gNodeB address\n");
        return -1;
    }

    if (gNodeB_sockfd < 0) {
        printf("Failed to connect to gNodeB\n");
        return -1;
    }
    printf("AMF initialized and connected to gNodeB at %s:%d\n", GNODEB_SERVER_IP, GNODEB_TCP_PORT);
    return 0;
}

int32_t AMFSendPagingMessage(Paging_t* paging_message) {
    if (gNodeB_sockfd < 0) {
        printf("AMF is not initialized\n");
        return -1;
    }

    const size_t message_len = sizeof(*paging_message);

    if (send(gNodeB_sockfd, paging_message, message_len, 0) < 0) {
        perror("send");
        return -1;
    }
    printf("Đã gửi Paging Message đến gNodeB, chờ ACK.... \n");
    // Wait for ACK from gNodeB
    char buffer[BUFFER_SIZE] = {0};
    const ssize_t recv_len = recv(gNodeB_sockfd, buffer, BUFFER_SIZE - 1, 0);
    if (recv_len < 0) {
        perror("AMF recv");
        return -1;
    }
    buffer[recv_len] = '\0';
    printf("Received ACK from gNodeB: %s\n", buffer);
    return 0;
}

void AMFStop() {
    if (gNodeB_sockfd >= 0) {
        close(gNodeB_sockfd);
        gNodeB_sockfd = -1;
    }
    printf("AMF stopped and disconnected from gNodeB\n");
}

static int32_t _initGNodeBAddress() {
    gNodeB_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (gNodeB_sockfd < 0) {
        perror("socket");
        return -1;
    }

    memset(&gNodeB_addr, 0, sizeof(gNodeB_addr));
    gNodeB_addr.sin_family = AF_INET;
    gNodeB_addr.sin_port = htons(GNODEB_TCP_PORT);
    gNodeB_addr.sin_addr.s_addr = inet_addr(GNODEB_SERVER_IP);

    if (connect(gNodeB_sockfd, (struct sockaddr*)&gNodeB_addr, sizeof(gNodeB_addr)) < 0) {
        perror("connect");
        close(gNodeB_sockfd);
        gNodeB_sockfd = -1;
        return -1;
    }

    return 0;
}