#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifndef TCPD_H
#define TCPD_H

#define L_PORT 10001 //Daemon port (local)
#define R_PORT 15001 //Remote port
#define T_PORT 20001 //Troll port

#define S_ADDR "164.107.112.71"  //epsilon.cse.ohio-state.edu
#define C_ADDR "164.107.112.73"  //eta.cse.ohio-state.edu

#define DATA_LEN 4
#define FILE_NAME_LEN 20
#define MAX_MES_LEN 1000

#define CRC_POLY 0x8005
#define CHKSUM_LEN 2

#define TARGET_DIR "new/" 

#define FIN 1
#define SYN 2
#define RST 4
#define PSH 8
#define ACK 16
#define URG 32
#define ECE 64
#define CRW 128

#define NS 1


typedef struct 
{
    uint16_t s_port; 
    uint16_t d_port;
    uint32_t seq_num;
    uint32_t ack_num;
    uint4_t data_offset;
    uint4_t res_plus_ns;
    uint8_t flags;
    uint16_t s_port; 
    uint16_t s_port; 
    uint16_t s_port; 
    
}


int SOCKET(int family, int type, int protocol){
  
  return socket(family, SOCK_DGRAM, protocol);
}

int BIND(int sockfd, struct sockaddr *myaddr, int addrlen)
{
    
  return bind(sockfd, myaddr, addrlen);
}

int ACCEPT(int sockfd, void *peer, int *addrlen)
{
    //wait for connection request from TCPD
  return 0;
}

int CONNECT(int sockfd, struct sockaddr *serv_addr, int addrlen)
{
    //request connection from TCPD
    return 0;
}

int SEND(int sockfd, const void *msg, int len, int flags, 
	 const struct sockaddr *to, int tolen)
{
    //sleep 10 milliseconds between sends
    usleep(10000);
    return sendto(sockfd, msg, len, flags, to, tolen);
}

int RECV(int sockfd, void *buf, size_t  len, int flags,
	 struct sockaddr *from, socklen_t  *fromlen)
{
    return  recvfrom(sockfd, buf, len, flags,from, fromlen);
}

int CLOSE(int sockfd)
{
    //send close to TCPD 
    return close(sockfd);
}

unsigned short calc_checksum(char* buf, uint32_t buf_len)
{
    uint16_t checksum = 0;
    int i = 0;
    int isbit;
    
    while(buf_len > 0)
    {
        isbit = checksum >> 15;
	
	checksum <<= 1;
	checksum |= (*buf >> i) & 1;
		
	i++;
	if(i > 7)
	{
            i = 0;
            buf++;
            buf_len--;
        }
	
        
        if(isbit)
            checksum ^= CRC_POLY;
    }
    
    for (i = 0; i < 16; ++i) {
        isbit = checksum >> 15;
        checksum <<= 1;
        if(isbit)
            checksum ^= CRC_POLY;
    }
    
    uint16_t crc = 0;
    i = 0x8000;
    int j = 0x0001;
    for (; i != 0; i >>=1, j <<= 1) {
        if (i & checksum){
	    crc = crc | j;
	}
    }

    return crc;
    }

#endif
