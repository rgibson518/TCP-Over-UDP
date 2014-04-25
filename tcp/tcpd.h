#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "pdu.h"

#ifndef TCPD_H
#define TCPD_H

#define L_PORT 10002 //Daemon port (local)
#define R_PORT 15002 //Remote port
#define T_PORT 20002 //Troll port

#define S_ADDR "164.107.112.71"  //epsilon.cse.ohio-state.edu
#define C_ADDR "164.107.112.73"  //eta.cse.ohio-state.edu
#define DATA_LEN 4
#define FILE_NAME_LEN 20
#define MAX_MES_LEN 1000

#define CRC_POLY 0x8005
#define CHKSUM_LEN 2
#define WINDOW_SIZE 20

#define TARGET_DIR "new/" 


/*=========== GLOBAL VARIABLES=================*/



struct sockaddr_in tcpd_addr;
socklen_t tcpd_addr_len;




/*=========== HELPER METHODS==================*/


void build_addr()
{
    tcpd_addr_len = sizeof(struct sockaddr_in);
    memset(&tcpd_addr, '\0', sizeof(tcpd_addr));
    tcpd_addr.sin_family = AF_INET;
    tcpd_addr.sin_port = htons(L_PORT);
    tcpd_addr.sin_addr.s_addr = htonl(INADDR_ANY);
}

/* For this project, we will use a buf of 0 to send req/acks betwen the 
   client/server and daemon.  The intent in derived from the state of the
   connection on the daemon.*/

int wait_for_ack(int sockfd) 
{    
    char buf[PAYLOAD];
    int nbytes_recv;
    
    build_addr();
    
    // block the process until we get an ack from the tcpd
    nbytes_recv = recvfrom(sockfd, buf, PAYLOAD, 0,(struct sockaddr*) &tcpd_addr, &tcpd_addr_len);
    
    //Signal failed and let the client/server handle the error.
    if (nbytes_recv != 0)
    {
	return -1;
    }
    // Successful connection
    else 
    {
	return 0;
    }
    
}


/* Send an empty buffer to TCPD to signal a request
 */
int send_req(int sockfd)
{
    char* buf;
    int nbytes_sent;
    
    build_addr();
    
    // block the process until we get an ack from the tcpd
    nbytes_sent = sendto(sockfd, buf, 0, 0,(struct sockaddr*)&tcpd_addr, tcpd_addr_len);
    return nbytes_sent;
}


uint16_t calc_checksum(char* buf, uint32_t buf_len)
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




/*========== TCP CONVERSION METHODS==========*/



/*  Since we're not really using a TCP, we need to change this to a 
    SOCK_DGRAM for UDP 
*/
int SOCKET(int family, int type, int protocol){
    
    
    return socket(family, SOCK_DGRAM, protocol);
}

/* Doesn't really change I don't think.
 */
int BIND(int sockfd, struct sockaddr *myaddr, int addrlen)
{
    
    return bind(sockfd, myaddr, addrlen);
}

/* Just wait for a connection by waiting for an ack
 */
int ACCEPT(int sockfd, void *peer, socklen_t *addrlen)
{
    if (wait_for_ack(sockfd) ==0)
    {
	peer = &tcpd_addr;
	addrlen = &tcpd_addr_len;
	return sockfd;
    }
    else 
    {
	return -1;
    }
}

/* Establish a connection by sending request and waiting for ack
 */
int CONNECT(int sockfd, struct sockaddr *serv_addr, int addrlen)
{
    return 0;
}

/* Send TCP over UDP, adding wait to slow down process
 */
int SEND(int sockfd, const void *msg, int len, int flags, 
	 const struct sockaddr *to, socklen_t tolen)
{
    //sleep 10 milliseconds between sends
    usleep(10000);
    return sendto(sockfd, msg, len, flags, to, tolen);
}
		 
/* Conversion from TCP to UDP
 */
int RECV(int sockfd, void *buf, size_t  len, int flags,
	 struct sockaddr *from, socklen_t  *fromlen)
{
    return  recvfrom(sockfd, buf, len, flags,from, fromlen);
}

/*Send close to TCPD
 */
int CLOSE(int sockfd)
{
    //trigger close in daemon
    send_req(sockfd);

    //ensure daemon cleaned up everything
    if (wait_for_ack(sockfd)==0)
    {
	//close out socket
	return  close(sockfd);
    } 
    else 
    {
	return -1;
    }
}

		 
		 
		 
#endif
		 