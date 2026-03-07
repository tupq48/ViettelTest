#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>

#define GNODEB_UDP_PORT                 5000
#define GNODEB_TCP_PORT                 6000

#define GNODEB_BROADCAST_ADDRESS        "127.0.0.1"
#define GNODEB_SERVER_IP                "127.0.0.1"

#define MAX_SFN                         1023
#define SFN_INCREASE_TIME_MS            10  /* miliseconds */

#define MIB_INCREASE_TIME_MS            80  /* miliseconds */
#define MIB_CYCLE                       (MIB_INCREASE_TIME_MS / SFN_INCREASE_TIME_MS)
#define MIB_SYNC_TIME_MS                800 /* miliseconds */
#define MIB_SYNC_CYCLE                  (MIB_SYNC_TIME_MS / MIB_INCREASE_TIME_MS)

#define UE_ID_DEFAULT                     12345

/* Paging frame configuration */
#define PAGING_FRAME_OFFSET             0
#define PAGING_CYCLE                    64
#define PF_PER_CYCLE                    1

/* Thread pool configuration */
#define NUM_WORKER_THREADS              8
#define PAGING_QUEUE_SIZE               100

/* generic worker queue size; paging uses same default */
#ifndef WORKER_QUEUE_SIZE
#define WORKER_QUEUE_SIZE PAGING_QUEUE_SIZE
#endif

/*
    Bản tin MIB struct
*/
#define MESSAGE_MIB_ID                  0x01
typedef struct {
    uint8_t  messageId;
    uint16_t sfnValue;
} MIB_t;

#define NGAP_PAGING_MESSAGE_TYPE        100
#define TAC_PAGING_VALUE                100

typedef enum {
    CN_DOMAIN_NORMAL_CALL = 100,
    CN_DOMAIN_DATA_CALL   = 101,
} CNDomain;

/*
    Bản tin Paging message struct 
*/
typedef struct {
    uint32_t messageType;
    uint32_t ueId;
    uint32_t TAC;
    uint32_t cn_domain;
} Paging_t;

#endif // COMMON_H
