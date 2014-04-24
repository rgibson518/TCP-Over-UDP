/*
Buffer being used incorrectly.  should be sending as many as possible from after the tail.  the head is the lower index
should receive acks and progress head, send pdu's and progress tail
*/



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
#include <errno.h>
#include <sys/un.h>

#include "tcpd.h"
#include "buffer.h"

#define NUM_THREADS 5


/* ===============Global Variables========= */

//uint32_t seq = 0;
uint32_t ack = 0;

circular_buffer cb_r;
sliding_window sw_r;
circular_buffer cb_s;
sliding_window sw_s;

//sockets
unsigned int l_sockfd;
unsigned int r_sockfd;
unsigned int dt_sockfd;
unsigned int len;

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

// socket helpers
int setup_socket(struct sockaddr_in* addr, int* addrlen,  int port);
void set_fwd_addr( struct sockaddr_in* f_addr, int* f_addrlen,
		   int port
		   );
void set_ack_addr( struct sockaddr_in* f_addr, int* f_addrlen,
		   int port
		   );

// thread methods
void* local_listen(void *);
void* local_send(void *);
void* remote_listen(void *);
void* remote_send(void *);
void* listen_to_timer_socket(void *);


//pdu methods
void build_pdu(pdu* p, uint32_t* seq, uint32_t* ack, char* buf, ssize_t buflen);

// initidalization helpers
void initialize_semaphores();




/*================MAIN ===============*/

/* Daemon service*/
int main(int argc, char *argv[]) 
{

  //***************Fork and connect to delta timer ***************************/
  
  int pid;
  int count = 0;
  char *argv2[] = { NULL };
  char *newarg[] = {"./dt", NULL};
  
  
  circular_buffer *cb = &cb_r;
  sliding_window* sw = &sw_r;
  
  // initialize send buffer and window
  cb_init(cb);
  sw_init(sw, cb);
  
  pid = fork();
  if (pid == 0){
    
    if ( execve("./dt", newarg, argv2) )
      {
	printf("execv failed with error %d %s\n",errno,strerror(errno));
	return 254;  
      }
    
  }
  
  sleep(1);
  struct sockaddr_un remote;
  char str[100];
  int thread_id;
  //	pthread_t	p_thread;
  
  if ((dt_sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
    perror("socket");
    exit(1);
  }
  
  printf("Trying to connect to timer...\n");
  
  remote.sun_family = AF_UNIX;
  strcpy(remote.sun_path, "mysocket2");
  len = strlen(remote.sun_path) + sizeof(remote.sun_family);
  if (connect(dt_sockfd, (struct sockaddr *)&remote, len) == -1) {
    perror("connect");
    exit(1);
  }
  
  printf("Connected to timer.\n");
  
  
  
  /*As of now, this is setting up a thread to listen to the socket on 
    dt_sockfd.  We need to make that listening occur somewhere in one of
    our existing threads, or make the thread that is created here interact with
    them somehow.  this thread receives messages identifying which 
    packets have timed outs*/
  int i = 0;
  thread_id = pthread_create(&tid[i], NULL, listen_to_timer_socket, NULL);
  i++;
  
  
  //*****************************END OF CONNECTING TO DELTA TIMER***************
      
      
      
      fd_set read_fd_set;
  
  struct sockaddr_in local_addr;
  int local_len;			
  
  struct sockaddr_in remote_addr;		
  int remote_len;	
  
 
  initialize_semaphores();
    
  /* create local and remote sockets */
  l_sockfd = setup_socket(&local_addr, &local_len, L_PORT);
  r_sockfd = setup_socket(&remote_addr, &remote_len, R_PORT);
  
  
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


/*Listens to socket to make connection with delta timer
*/

void* listen_to_timer_socket(void *arg){
/*	int mode;
	int sequence, rc;
	unsigned int t, port, packet, duration;
	char *token, *search = " ";
	char str[100];
	ssize_t pdu_resent = 0;
	int fwd_len;
	struct sockaddr_in fwd_addr;
	set_fwd_addr(&fwd_addr, &fwd_len, R_PORT);
	circular_buffer *cb = &cb_s;

	while(1){
		if ((t=recv(dt_sockfd, str, 100, 0)) > 0) {
			str[t] = '\0';
			printf("timeout received : %s", str);

			//take the packet number modded with the size of buffer
			//resend the packet in that position
			sscanf(str, "%d", &sequence);
			sequence = sequence % BUFFER_SIZE;
			pdu_resent = sendto(r_sockfd, cb->buffer[sequence], MAX_MES_LEN, 0, (struct sockaddr *)&fwd_addr, fwd_len);
			} else {
				if (t < 0) perror("recv");
				else printf("Server closed connection\n");
				exit(1);
			}
		}*/
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
  int seq = 0;
  
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
      build_pdu(&p, &seq, NULL , local_buf, PAYLOAD);
      seq++;
      // wait for space in buffer
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
  char str[100];
  unsigned int send_check;
  
  circular_buffer *cb = &cb_s;
  sliding_window *sw = &sw_s;
  
  set_fwd_addr(&fwd_addr, &fwd_len, R_PORT);
  
  while (1)
    {  
      // wait for pdu's to enter buffer
      printf("Waiting for packets to send\n");
      sem_wait(&cb_full);

      // wait for window to have contents
      sem_wait(&sw_empty);
      
      // enter critical section
      pthread_mutex_lock(&mutex);

      pdu * ptr = (pdu*)cb->buffer[sw->tail];
      //send a pdu
      printf("Sending pack...#%i\n", ptr->h.seq_num);
      local_sent = sendto(r_sockfd, ptr, MAX_MES_LEN, 0, (struct sockaddr *)&fwd_addr, fwd_len);
      
      // mark as unacked
      sw->packet_acks[sw->count] == UNACKED;
      
      //START PACKET TRANSMISSION TIMER  ONLY IF IN CLIENT
      /*  IGNORE TIMER FOR NOW
	  printf("Adding to delta list seq# %i duration %i msec\n", ptr->h.seq_num, 10);
	  add_to_buffer(sw, cb, &p);
	  sprintf(str, "1 %i %i", ptr->h.seq_num, 100000);
	  
	  send_check = send(dt_sockfd, str, 100, 0);*/
      
      
      
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
  char str[100];
  unsigned int send_check;

  
  circular_buffer *cb = &cb_r;
  sliding_window* sw = &sw_r;


  
  // initialize receive buffer and window
  cb_init(cb);
  sw_init(sw, cb);
  sw->tail = sw->head + 20;
  cb->tail = cb->head + 64;
  
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
	      int ack_seq = p.h.ack_num;
	      
	      // filter duplicate ACKs
	      if ((markPDUAcked(ack_seq, sw, cb)) ==0)
		{
		  //printf("ACK is genuine.\n");
		  
		  /*
		    IGNORE TIMER FOR NOW
		    sprintf(str, "2 %i", ack_seq);
		    send_check = send(dt_sockfd, str, 100, 0);
		  */		  
		  //progress heads and signal window slide
		  sem_wait(&sw_full);
		  update_window(sw, cb, &sw_empty, &cb_empty);
		  
		  //exit critical section
		}

	    }
	  // datagram from other remote
	  else
	    {
	      cb = &cb_r;
	      sw = &sw_r;
	      
	      //send ack		  
	      pdu ack_p;
	      memset(&ack_p, 0x00, sizeof(pdu));
	      build_pdu(&ack_p, NULL, &p.h.seq_num, NULL, 0);	      
	      set_ack_addr(&return_addr, &return_len, R_PORT) ; 
	      printf("Received seq#%i.  Sending ack%i\t Checksum = %i\n",p.h.seq_num, ack_p.h.ack_num, ack_p.h.chk);
	      remote_sent = sendto(r_sockfd, (char*)&ack_p, MAX_MES_LEN,  0, (struct sockaddr *)&return_addr, return_len);  
	      
	      //ensure this isn't a duplicate
      	      if ((markPDURecv(p.h.seq_num, sw, cb))==0)
			{
				//printf("Packet is genuine.\n");
		  
				// ensure open slot in window and buffer
				//sem_wait(&swr_empty);	
				sem_wait(&cbr_empty);  
				// enter critical section
				// printf("waiting on mutex\n");
				pthread_mutex_lock(&rmutex);
		  
				printf("Adding to receive buffer.\n");
				add_to_r_buffer(sw, cb, &p);
				// exit critical section
				pthread_mutex_unlock(&rmutex);
				//sem_post(&swr_full);
				sem_post(&cbr_full);
			}
	      else 
			{
			printf("\t\t\tPACK SKIPPED %i\n", p.h.seq_num);
			}
	      
	    }
	}
      else 
	{
	  perror("Checksums did not match\n");
	  exit(1);
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
		// wait for window to have contents (waiting on buffer full sem should be redundant here)
		/*changed to cbr, thinking that the while check below will be ensuring that the
		window has valid contents before trying to read them in, and in the remote receive function,
		we might be adding a pdu into the buffer that is not necessarily going into the window, so, I
		did not want to post to the window sem there*/
		sem_wait(&cbr_full);
      while (sw->packet_acks[sw->packet_acks_head_index]==ACKED)
		{
			// enter critical section
			pthread_mutex_lock(&rmutex);
		
			pdu * ptr = (pdu*)cb->buffer[sw->head];
			//send a pdu
			printf("Reading pack...#%i\n", ptr->h.seq_num);
			local_sent = sendto(l_sockfd, ptr->data, PAYLOAD, 0, (struct sockaddr *)&fwd_addr, fwd_len);
		
			sw->packet_acks[sw->packet_acks_head_index]= 0;
	  
			//progress_heads(sw, cb);
			sw->head = (sw->head +1)%64;
			progress_window_tail(sw, cb);
	  
			printf("sw->head% i\n",sw->head);
			// printf("Adding space to sw\n");
			//sw->count--;
			cb->count--;
			pthread_mutex_unlock(&rmutex);
			//sem_post(&swr_empty);
			sem_post(&cbr_empty);
		}
    }
}



/* Builds pdu from buffer
 */
void build_pdu(pdu* p, uint32_t* seq, uint32_t* ack, char* buf, ssize_t buflen)
{
  p->h. s_port = R_PORT;
  p->h. d_port = T_PORT;
  p->h.win = WINDOW_SIZE;
  if (seq == NULL)
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
