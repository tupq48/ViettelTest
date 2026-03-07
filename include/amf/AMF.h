#ifndef AMF_H
#define AMF_H

#include <netinet/in.h>
#include "common.h"

int32_t AMFInit();
int32_t AMFSendPagingMessage(Paging_t* paging_message);
void    AMFStop();
#endif // AMF_H