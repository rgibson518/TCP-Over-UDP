#include <sys/types.h>
#include "header.h"

#ifndef PDU_H
#define PDU_H

#define PAYLOAD 980

typedef struct pdu
{
    header h;
    char data[PAYLOAD];
}pdu;

#endif
