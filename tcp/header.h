#include <sys/types.h>
#include <stdint.h>

#ifndef HEADER_H
#define HEADER_H

#define FIN 1
#define SYN 2
#define RST 4
#define PSH 8
#define ACK 16
#define URG 32
#define ECE 64
#define CRW 128

#define NS 1


typedef struct header
{
    uint16_t s_port; 
    uint16_t d_port;
    uint32_t seq_num;
    uint32_t ack_num;
    //data reserved and ns are all on the same int
    uint8_t data_res_ns;
    uint8_t flags;
    uint16_t win; 
    uint16_t chk; 
    uint16_t urg;
    suseconds_t tsopt; //not sure if we need ops.. but could.
}header;

#endif
