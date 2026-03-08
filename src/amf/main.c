#include <stdio.h>
#include "AMF.h"
#include "common.h"

static void _handleSendPagingMessage();

int main() {
    if (AMFInit() < 0) {
        printf("Failed to initialize AMF\n");
        return -1;
    }

    while (1) {
        printf("\n--- AMF Simulator ---\n");
        printf("1. Send Paging Message (UE ID: %d)\n", UE_ID_DEFAULT);
        printf("0. Exit\n");
        printf("Enter your choice: ");
        int choice;
        scanf("%d", &choice);
        switch (choice) {
            case 1:
                _handleSendPagingMessage();
                break;
            case 0:
                AMFStop();
                printf("Exiting AMF Simulator...\n");
                return 0;
            default:
                printf("Invalid choice. Please try again.\n");
        }
    }
}

static void _handleSendPagingMessage() {
    printf("Choose CNDomain type:\n");
    printf("1. Normal Call\n");
    printf("2. Data App Call\n");
    printf("Enter your choice: ");
    int type_choice;
    scanf("%d", &type_choice);
    Paging_t paging_message;
    switch (type_choice) {
        case 1:
            paging_message.cn_domain = CN_DOMAIN_NORMAL_CALL;
            break;
        case 2:
            paging_message.cn_domain = CN_DOMAIN_DATA_CALL;
            break;
        default:
            printf("Invalid choice. Returning to main menu.\n");
            return;
    }
    paging_message.TAC = TAC_PAGING_VALUE;
    paging_message.ueId = UE_ID_DEFAULT;
    paging_message.messageType = NGAP_PAGING_MESSAGE_TYPE;
    if (AMFSendPagingMessage(&paging_message) < 0) {
        printf("Failed to send Paging Message\n");
    }
}