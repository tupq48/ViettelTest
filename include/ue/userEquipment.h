#ifndef USEREQUIPMENT_H
#define USEREQUIPMENT_H

#include <stdint.h>
#include <stdatomic.h>
#include <netinet/in.h>
#include "common.h"

extern atomic_int UE_sfn;
extern atomic_bool is_synced;
extern int8_t mib_received_count;

void* tick_thread(void* arg);
int32_t initUserAddress(struct sockaddr_in* ue_addr);

#endif // USEREQUIPMENT_H