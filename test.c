#include <stdio.h>
#include "tcp/pdu.h"
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


int main ()
{
  pdu  p;
  pdu* p_ptr = &p;
  
  memset(&p, 0x00, sizeof(pdu));
  
  p.h.s_port = 1;
  
  printf("Direct reference s_port: %i\n", p.h.s_port);
  printf("Indirect reference s_port: %i\n", p_ptr->h.s_port);
  printf("Indirect reference seq_num: %i\n", p_ptr->h.seq_num);
  return 0;
}
