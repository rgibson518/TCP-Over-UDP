#include <stdio.h>
#include "tcp/tcpd.h"
#include <string.h>
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


void build_pdu(pdu* p, uint32_t* seq, uint32_t* ack, char* buf, ssize_t buflen);
int main ()
{
  uint32_t seq = 0xFFFFFFFF;
  char local_buf[PAYLOAD];
  
  
  pdu  p;
  pdu* p_ptr = &p;
  
  memset(&p, 0x00, sizeof(pdu));
  
  p.h.s_port = 1;

  build_pdu(&p,&seq, NULL , local_buf, PAYLOAD);
  printf("Direct reference s_port: %i\n", p.h.s_port);
  printf("Indirect reference s_port: %i\n", p_ptr->h.s_port);
  printf("Indirect reference seq_num: %u\n", p_ptr->h.seq_num);
  printf("Indirect reference chk: %4u\n", p_ptr->h.chk);
  return 0;
}

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
}
