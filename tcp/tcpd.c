/* Example: server.c receiving and sending datagrams on system generated port in UDP */

#define _REENTRANT
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "tcpd.h"
#include "buffer.h"

#define NUM_THREADS 2


/* ===============Global Variables========= */

uint32_t seq = 0;
uint32_t ack = 0;

circular_buffer cb_r;
sliding_window sw_r;
circular_buffer cb_s;
sliding_window sw_s;

//sockets
unsigned int l_sockfd;
unsigned int r_sockfd;

ssize_t local_recv = 0;
ssize_t local_sent = 0;
ssize_t remote_recv = 0;
ssize_t remote_sent = 0;

//thread variables
pthread_mutex_t mutex;
sem_t full, empty; 
pthread_t tid[NUM_THREADS];


/* ===============Prototypes ========= */
int setup_socket(struct sockaddr_in* addr, int* addrlen,  int port);
void set_fwd_addr( struct sockaddr_in* f_addr, int* f_addrlen,
		   int port
		   );
void* local_listen(void *);
void* remote_listen(void *);
void build_pdu(pdu* p, uint32_t* seq, uint32_t* ack, char* buf, ssize_t buflen);
void unpack_pdu(pdu* p, header* h, char* buf, int buflen);




/*================MAIN ===============*/

/* Daemon service*/
int main(void) 
{
  fd_set read_fd_set;
  
  struct sockaddr_in local_addr;
  int local_len;			
  
  struct sockaddr_in remote_addr;		
  int remote_len;	
  
  uint16_t checksum;
  uint16_t checksum_r;
  
   // initialize semaphores
  i = sem_init(&full, 0, 0);
  if (i<0){
    perror("Error: unable to create semaphore");
    exit(1);
  }
  i = sem_init(&empty, 0,  64);
  if (i<0){
    perror("Error: unable to create semaphore");
    exit(1);
  }

    
  /* create local and remote sockets */
  l_sockfd = setup_socket(&local_addr, &local_len, L_PORT);
  r_sockfd = setup_socket(&remote_addr, &remote_len, R_PORT);
  
  int i = 0;
  int l_thread = pthread_create(&tid[i], NULL, local_listen, &l_sockfd); 
  if (l_thread != 0)
    {
      printf("Error creating local listener#%i\n",i);
      exit(1);
    }
  i++;
  int r_thread = pthread_create(&tid[i], NULL, remote_listen, &r_sockfd); 
  if (r_thread != 0)
    {
      printf("Error creating remote listenter #%i\n",i);
      exit(1);
    }
  
  //wait for any Timeouts
  //join back together threads
  int j;
  for (j = 0;j < NUM_THREADS; j++) 
    {
      i = pthread_join(tid[j],NULL);
      if (i!=0)
	{
	  printf("Error: unable to join thread");
	  exit(1);
	} 
    }
  printf("Threads joined.\n");
}



/* ===========Function Implementations=======*/


int setup_socket(struct sockaddr_in* addr, int* addrlen,  int port)
{
  
  /*create local socket*/
  int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if(sockfd < 0) {
    perror("opening datagram socket");
    exit(1);
  }
  
  /*  lose the pesky "Address already in use" error message (from Beej's)*/
  int yes=1;
  if (setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int)) == -1) {
    perror("setsockopt");
    exit(1);
  } 

  *addrlen = sizeof(struct sockaddr_in);
  
  /* create name with parameters and bind name to socket */
  memset(addr, '\0', *addrlen);
  addr->sin_family = AF_INET;
  addr->sin_port = htons(port);
  addr->sin_addr.s_addr = htonl(INADDR_ANY);

  if(bind(sockfd, (struct sockaddr *) addr, *addrlen) < 0) {
    perror("getting socket name");
    exit(2);
  }

  return sockfd;
}

 
void set_fwd_addr(struct sockaddr_in*f_addr, int* f_addrlen, int port)
{
   
  *f_addrlen = sizeof(struct sockaddr_in);
   
  memset(f_addr, '\0', *f_addrlen);
  f_addr->sin_family = AF_INET;
  f_addr->sin_port = htons(port); 
  f_addr->sin_addr.s_addr = inet_addr(S_ADDR);  

}


/* Listens for incoming local traffic
 */
void* local_listen(void *arg)
{
  char local_buf[PAYLOAD];
  uint32_t local_buflen;
  struct sockaddr_in recv_addr;
  int recv_len;
  struct sockaddr_in fwd_addr;
  int fwd_len;
  pdu p;
  
  circular_buffer *cb = &cb_s;
  sliding_window* sw = &sw_s;
  
  // initialize send buffer and window
  cb_init(cb);
  sw_init(sw);
  
  // liste for incoming payloads from local host
  while (1)
    {  
      //clear pdu
      memset(&p, 0x00, sizeof(pdu));
      
      // get next payload from the local host
      local_recv = recvfrom(l_sockfd, local_buf , PAYLOAD , 0, (struct sockaddr *)&recv_addr, &recv_len);
      if (local_recv < 0)
	{
	  perror("TCPD local:  recvfrom error");
	  exit(1);
	}
      else if (local_recv == 0) 
	{
	  //send FIN
	}

      // pack buffer into pdu
      build_pdu(&p,&seq, NULL , local_buf, PAYLOAD);
      
      // wait for space in window
      sem_wait
      // enter critical section
      pthread_lock(mutex);
      
      add_to_buffer(sw, cb, &p);
      
      // exit cirital sectio
      pthread_unlock(mutex);       
    }
}


/* Listens for incoming local traffic
 */
void* local_send(void *arg)
{
  struct sockaddr_in recv_addr;
  int recv_len;
  struct sockaddr_in fwd_addr;
  int fwd_len;
  pdu p;
  
  circular_buffer *cb = &cb_s;
  sliding_window* sw = &sw_s;
  
  // initialize send buffer and window
  cb_init(cb);
  sw_init(sw);
  
  // liste for incoming payloads from local host
  while (1)
    {  
    
      sem_wait(full);
      // enter critical section
      pthread_lock(mutex);
      
      add_to_buffer(sw, cb, &p);
      
      // exit cirital sectio
      pthread_unlock(mutex);      
      
      set_fwd_addr(&fwd_addr, &fwd_len, R_PORT);
      local_sent = sendto(r_sockfd, (char*)&p, MAX_MES_LEN, 0, (struct sockaddr *)&fwd_addr, fwd_len);
    }
}


/* Listens on incoming remote traffic
 */
void* remote_listen(void *arg)
{
    printf("Local remote created.\n");
  
  char remote_buf[MAX_MES_LEN];
  uint32_t remote_buflen;
  struct sockaddr_in recv_addr;
  int recv_len;
  struct sockaddr_in fwd_addr;
  int fwd_len;
  pdu p;
  
  while (1)
    {
      //clear pdu
      memset(&p, 0x00, sizeof(pdu));
    
      if ((remote_recv = recvfrom(r_sockfd, remote_buf , MAX_MES_LEN, 0, (struct sockaddr *)&recv_addr, (socklen_t *)&recv_len))<0)
	{
	  perror("recv");
	  exit(1);
	}
      printf("Bytes received:\t%i\n", remote_recv);
      memcpy(&p, &remote_buf, MAX_MES_LEN);
      uint16_t r_checksum =  p.h.chk;
      uint16_t checksum;
      memset(&p.h.chk, 0x00, sizeof(p.h.chk));
      checksum = calc_checksum((char*)&p, MAX_MES_LEN);
      if (checksum)
	{
	  printf("Checksums did not match(rec:calc):\t%d:%d\n", r_checksum, checksum);
	}
      
      set_fwd_addr(&fwd_addr, &fwd_len, L_PORT) ;      
      remote_sent = sendto(l_sockfd, p.data, PAYLOAD, 0, (struct sockaddr *)&fwd_addr, fwd_len);
    }
}



/* Builds pdu from buffer
 */
void build_pdu(pdu* p, uint32_t* seq, uint32_t* ack, char* buf, ssize_t buflen)
{
  p->h. s_port = R_PORT;
  p->h. d_port = T_PORT;
  p->h.win = WINDOW_SIZE;
  if (seq ==NULL)
    {
      p->h. flags = ACK;
      p->h.ack_num = *ack;
    } 
  else if (ack == NULL)
    {
      p->h.seq_num = *seq;
      memcpy(p->data, buf, buflen);
    }
  else 
    {
      // must ACK or SEQ for this program
      perror("TCPD: error building pdu Must be ACK or SEQ.");
      exit(1);
    }
  
  p->h.chk = calc_checksum((char*)p, MAX_MES_LEN);
}
  
