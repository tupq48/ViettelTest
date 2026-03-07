#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "common.h"

#define BUFFER_SIZE 1024

int32_t initGNodeBAddress(struct sockaddr_in* gnb_addr) {
    int32_t sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return -1;
    }

    memset(gnb_addr, 0, sizeof(*gnb_addr));
    gnb_addr->sin_family = AF_INET;
    gnb_addr->sin_port = htons(GNODEB_TCP_PORT);
    gnb_addr->sin_addr.s_addr = inet_addr(GNODEB_SERVER_IP);

    if (connect(sockfd, (struct sockaddr*)gnb_addr, sizeof(*gnb_addr)) < 0) {
        perror("connect");
        close(sockfd);
        return -1;
    }
    return sockfd;
}

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};
    
    sock = initGNodeBAddress(&serv_addr);
    if (sock < 0) {
        printf("Failed to initialize gNodeB address\n");
        return -1;
    }
    
    printf("Connected to gNodeB server at %s:%d\n", GNODEB_SERVER_IP, GNODEB_TCP_PORT);
    
    // Prepare NGAP Paging message
    Paging_t paging_message = {
        .messageType = NGAP_PAGING_MESSAGE_TYPE,
        .ueId        = UE_ID_DEFAULT,
        .TAC         = TAC_PAGING_VALUE,
        .cn_domain   = CN_DOMAIN_NORMAL_CALL
    };
    const size_t message_len = sizeof(paging_message);

    // Send paging message to gNodeB
    if (send(sock, &paging_message, message_len, 0) < 0) {
        perror("send");
        close(sock);
        return -1;
    }

    printf("Sent NGAP Paging message to gNodeB\n");

    // Receive response from gNodeB
    const size_t recv_len = recv(sock, buffer, BUFFER_SIZE - 1, 0);
    if (recv_len < 0) {
        perror("recv");
        close(sock);
        return -1;
    }

    buffer[recv_len] = '\0';
    printf("Received response: %s\n", buffer);
    close(sock);
    return 0;
}