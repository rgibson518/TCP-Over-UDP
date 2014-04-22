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

#define NUM_THREADS 4


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
sem_t cb_full, cb_empty; 
sem_t sw_full, sw_empty; 

pthread_mutex_t rmutex;
sem_t cbr_full, cbr_empty; 
sem_t swr_full, swr_empty; 
pthread_t tid[NUM_THREADS];


/* ===============Prototypes ========= */
int setup_socket(struct sockaddr_in* addr, int* addrlen,  int port);
void set_fwd_addr( struct sockaddr_in* f_addr, int* f_addrlen,
		   int port
		   );
void set_ack_addr( struct sockaddr_in* f_addr, int* f_addrlen,
		   int port
		   );
void* local_listen(void *);
void* local_send(void *);
void* remote_listen(void *);
void* remote_send(void*);
void build_pdu(pdu* p, uint32_t* seq, uint32_t* ack, char* buf, ssize_t buflen);
void unpack_pdu(pdu* p, header* h, char* buf, int buflen);
void initialize_semaphores();




/*================MAIN ===============*/

/* Daemon service*/
int main(void) 
{
  fd_set read_fd_set;
  
  struct sockaddr_in local_addr;
  int local_len;			
  
  struct sockaddr_in remote_addr;		
  int remote_len;	
  
 
  initialize_semaphores();
    
  /* create local and remote sockets */
  l_sockfd = setup_socket(&local_addr, &local_len, L_PORT);
  r_sockfd = setup_socket(&remote_addr, &remote_len, R_PORT);
  
  int i = 0;
  int l_thread = pthread_create(&tid[i], NULL, local_listen, NULL); 
  if (l_thread != 0)
    {
      printf("Error creating local listener#%i\n",i);
      exit(1);
    }
  i++;
  int ls_thread = pthread_create(&tid[i], NULL, local_send, NULL); 
  if (ls_thread != 0)
    {
      printf("Error creating local sender#%i\n",i);
      exit(1);
    }
  i++;
  int r_thread = pthread_create(&tid[i], NULL, remote_listen, NULL); 
  if (r_thread != 0)
    {
      printf("Error creating remote listener #%i\n",i);
      exit(1);
    }
    i++;
  int rp_thread = pthread_create(&tid[i], NULL, remote_send, NULL); 
  if (rp_thread != 0)
    {
      printf("Error creating remote listener #%i\n",i);
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

}void set_ack_addr(struct sockaddr_in*f_addr, int* f_addrlen, int port)
{
   
  *f_addrlen = sizeof(struct sockaddr_in);
   
  memset(f_addr, '\0', *f_addrlen);
  f_addr->sin_family = AF_INET;
  f_addr->sin_port = htons(port); 
  f_addr->sin_addr.s_addr = inet_addr(C_ADDR);  

}


/* Listens for incoming local traffic
 */
void* local_listen(void *arg)
{

  printf("Local listen created.\n");
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
  sw_init(sw, cb);
  
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
      seq++;
      // wait for space in window
      sem_wait(&cb_empty);
      
      // enter critical section
      pthread_mutex_lock(&mutex);
      printf("Adding to buffer seq#%i\n", seq-1);
      add_to_buffer(sw, cb, &p);
      // exit cirital sectio
      pthread_mutex_unlock(&mutex);    

      // signal pdu has been put in buffer
      sem_post(&cb_full);
    }
}


/* Send data from buffer to remote host
 */
void* local_send(void *arg)
{

  printf("Local send created.\n");
  
  struct sockaddr_in recv_addr;
  int recv_len;
  struct sockaddr_in fwd_addr;
  int fwd_len;
  pdu p;
  
  circular_buffer *cb = &cb_s;
  sliding_window* sw = &sw_s;
  
  set_fwd_addr(&fwd_addr, &fwd_len, R_PORT);
  
  while (1)
    {  
      // wait for pdu's to enter buffer
      printf("Waiting for packets to send\n");
      sem_wait(&cb_full);

      // wait for window to open
      sem_wait(&sw_empty);
      
      // enter critical section
      pthread_mutex_lock(&mutex);
      pdu * ptr = cb->buffer[sw->tail];
      //send a pdu
      printf("Sending pack...#%i\n", ptr->h.seq_num);
      local_sent = sendto(r_sockfd, ptr, MAX_MES_LEN, 0, (struct sockaddr *)&fwd_addr, fwd_len);
      
      // mark as unacked
      sw->packet_acks[sw->count] == UNACKED;
      
      // extend tail of the window
      progress_window_tail(sw,cb);
      sw->count++;
      
      //exit critical section
      pthread_mutex_unlock(&mutex);
      
      // let remote know it can listen for an ack
      sem_post(&sw_full);
      
    }
}


/* Listens on incoming remote traffic
   and ack's pdu's 
 */
void* remote_listen(void *arg)
{
    printf("Remote listen created.\n");
  
  char remote_buf[MAX_MES_LEN];
  uint32_t remote_buflen;
  struct sockaddr_in recv_addr;
  int recv_len;
  struct sockaddr_in fwd_addr;
  int fwd_len;
  struct sockaddr_in return_addr;
  int return_len;
  pdu p;

  
  circular_buffer *cb = &cb_r;
  sliding_window* sw = &sw_r;
  
  // initialize send buffer and window
  cb_init(cb);
  sw_init(sw, cb);

  while (1)
    {
      // clear pdu
      memset(&p, 0x00, sizeof(pdu));
      
      // get next packet
      if ((remote_recv = recvfrom(r_sockfd, remote_buf , MAX_MES_LEN, 0, (struct sockaddr *)&recv_addr, (socklen_t *)&recv_len))<0)
	{
	  perror("recv");
	  exit(1);
	}
      // put into pdu and verify checksum
      memcpy(&p, &remote_buf, remote_recv);
      uint16_t r_checksum =  p.h.chk;
      uint16_t checksum;
      memset(&p.h.chk, 0x00, sizeof(p.h.chk));
      checksum = calc_checksum((char*)&p, MAX_MES_LEN);

      // process packet if checksums match
      if (checksum == r_checksum)
	{
	  // ack from other remote
	  if (p.h.flags & ACK)
	    {
	      cb = &cb_s;
	      sw = &sw_s;
	      int seq = p.h.ack_num;
	      
	      // filter duplicate ACKs
	      if ((markPDUAcked(seq, sw, cb)) ==0)
		{
		  printf("ACK is genuine.\n");
		  //enter critical section
		  pthread_mutex_lock(&mutex);
		  
		  //progress heads and signal window slide
		  update_window(sw, cb, &sw_empty, &cb_empty);
		  
		  //exit critical section
		  pthread_mutex_unlock(&mutex);
		}
	    }
	  // datagram from other remote
	  else
	    {
	      cb = &cb_r;
	      sw = &sw_r;

	      //ensure this isn't a duplicate
      	      if ((markPDUAcked(p.h.seq_num, sw, cb))==0)
		{
		  printf("Packet is genuine.\n");
		  // wait for room in buffer
		  sem_wait(&cbr_empty);
		  
		  // enter critical section
		  pthread_mutex_lock(&rmutex);
		  
		  printf("Adding to receive buffer.\n");
		  add_to_r_buffer(sw, cb, &p);
		  update_r_window(sw, cb, &sw_empty, &cb_empty);
		  
		  // exit critical section
		  pthread_mutex_unlock(&rmutex);
		  
		  // let window know there is more to send
		  sem_post(&cbr_full);

		}
	      
	      //send to
	      set_fwd_addr(&fwd_addr, &fwd_len, L_PORT) ; 
	      remote_sent = sendto(l_sockfd, p.data, PAYLOAD, 0, (struct sockaddr *)&fwd_addr, fwd_len); 
	      
	      //send ack

	      pdu ack_p;
	      memset(&ack_p, 0x00, sizeof(pdu));
	      build_pdu(&ack_p, NULL, &p.h.seq_num, NULL, 0);	      
	      set_ack_addr(&return_addr, &return_len, R_PORT) ; 
	      //printf("Received seq#%i.  Sending ack%i\t Checksum = %i\n",p.h.seq_num, ack_p.h.ack_num, ack_p.h.chk); */
/* 	      remote_sent = sendto(r_sockfd, (char*)&ack_p, MAX_MES_LEN,  0, (struct sockaddr *)&return_addr, return_len);  */

	    }
	}
      else 
	{
	  printf("Checksums did not match\n");
	}
    }
}



/* Send data from buffer to local host
 */
void* remote_send(void *arg)
{

  printf("Remote send created.\n");
  
  struct sockaddr_in recv_addr;
  int recv_len;
  struct sockaddr_in fwd_addr;
  int fwd_len;
  pdu p;
  
  circular_buffer *cb = &cb_r;
  sliding_window* sw = &sw_r;
  
  set_fwd_addr(&fwd_addr, &fwd_len, L_PORT);
  
  while (1)
    {  
      // wait for pdu's to enter buffer
      printf("Waiting for packets to send\n");
      sem_wait(&cbr_full);

      // wait for window to open
      sem_wait(&swr_empty);
      
      // enter critical section
      pthread_mutex_lock(&rmutex);
      pdu * ptr = cb->buffer[sw->head_sequence_num];
      //send a pdu
      printf("Sending pack...#%i\n", ptr->h.seq_num);
      local_sent = sendto(l_sockfd, ptr->data, PAYLOAD, 0, (struct sockaddr *)&fwd_addr, fwd_len);
            
      // extend tail of the window
      progress_window_tail(sw,cb);
      sw->count++;
      
      //exit critical section
      pthread_mutex_unlock(&rmutex);
      
      // let remote know it can listen for an ack
      sem_post(&swr_full);
      
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
      p->h. flags += ACK;
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
  
void initialize_semaphores()
{
  // initialize cb semaphores
  int i = sem_init(&cb_full, 0, 0);
  if (i<0){
    perror("Error: unable to create semaphore");
    exit(1);
  }
  i = sem_init(&cb_empty, 0,  64);
  if (i<0){
    perror("Error: unable to create semaphore");
    exit(1);
  }
  
  // initialize sw semaphores
  i = sem_init(&sw_full, 0, 0);
  if (i<0){
    perror("Error: unable to create semaphore");
    exit(1);
  }
  i = sem_init(&sw_empty, 0,  20);
  if (i<0){
    perror("Error: unable to create semaphore");
    exit(1);
  }

  // initialize receive semaphores

  i = sem_init(&cbr_full, 0, 0);
  if (i<0){
    perror("Error: unable to create semaphore");
    exit(1);
  }
  i = sem_init(&cbr_empty, 0,  64);
  if (i<0){
    perror("Error: unable to create semaphore");
    exit(1);
  }
  
  // initialize sw semaphores
  i = sem_init(&swr_full, 0, 0);
  if (i<0){
    perror("Error: unable to create semaphore");
    exit(1);
  }
  i = sem_init(&swr_empty, 0,  20);
  if (i<0){
    perror("Error: unable to create semaphore");
    exit(1);
  }
}
