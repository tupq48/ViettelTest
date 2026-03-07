#include <stdint.h>

#define MESSAGE_MIB_ID                  0x01

#define GNODEB_UDP_PORT                 5000
#define GNODEB_TCP_PORT                 6000

#define GNODEB_BROADCAST_ADDRESS        "127.0.0.1"

#define MAX_SFN                         1023
#define SFN_INCREASE_TIME_MS            10  /* miliseconds */

#define MIB_INCREASE_TIME_MS            80  /* miliseconds */
#define MIB_CYCLE                       (MIB_INCREASE_TIME_MS / SFN_INCREASE_TIME_MS)
#define MIB_SYNC_TIME_MS                800 /* miliseconds */
#define MIB_SYNC_CYCLE                  (MIB_SYNC_TIME_MS / MIB_INCREASE_TIME_MS)



/*
    Bản tin MIB struct
*/
typedef struct {
    uint8_t  messageId;
    uint16_t sfnValue;
} MIB_t;
