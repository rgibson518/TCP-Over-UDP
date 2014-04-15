#include <sys/types.h>

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
    uint8_t data_res__ns;
    uint8_t flags;
    uint16_t win_size; 
    uint16_t checksum; 
    uint16_t urg_ptr;
    //    uint32_t opts[10]; //not sure if we need ops.. but could.
}header;

#endif
