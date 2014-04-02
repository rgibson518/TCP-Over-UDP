#include <netdb.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "tcpd.h"

void set_header(char* cli_buf, char* arg);

/* client program called with file path*/
int main(int argc, char *argv[])
{
  int sockfd, addrlen, buflen, fido, file_len, nbytes_read, nbytes_sent, nbytes_recv;
  int header_len = DATA_LEN+FILE_NAME_LEN;
  char cli_buf[MAX_MES_LEN];
  char srv_buf[MAX_MES_LEN];
  struct sockaddr_in addr;
  struct sockaddr_in srv_addr;
  int srv_len;
  

  //check proper arguments
  if(argc < 2 ) {
    printf("usage: file path requiredr\n");
    exit(1);
  }
  printf("File name \t%s\n", argv[1]);
  
  /* create socket for connecting to server */
  if ((sockfd = SOCKET(AF_INET, SOCK_STREAM,0))==-1){
    perror("client:socket");
    exit(1);
  }  
  
  /* construct addr for connecting to server */
  memset(&addr, '\0', sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(L_PORT);
  addr.sin_addr.s_addr = inet_addr(C_ADDR);
 
  addrlen = sizeof(struct sockaddr_in);
  
  /* TODO: Implement CONNECT()
  //attempt connection
  if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) ==-1){
  close(sockfd);cl
  perror("client:connect");
  exit(1);
  }
 */
    
  //open the file
  if ((fido = open(argv[1], O_RDONLY)) <0){
   close(sockfd);
   perror("client:file open");
   exit(1);
  }
  // get the length of the file
  if ((file_len = lseek(fido, 0, SEEK_END)) == -1){
    close(sockfd);
    perror("client:lseek : file size");
    exit(1);
  }
  //reset the cursor to the start of the file
  if (lseek(fido, 0, SEEK_SET) == -1){
    perror("client:lseek : beginning");
    exit(1);
  }
  printf("File Length\t%d\n", file_len);
 
  // header include the total number of bytes and the name of the file
  printf("Header length:\t%d\n",header_len);
  file_len += FILE_NAME_LEN;
  
  //add header to buffer
  memset(cli_buf, '\0', MAX_MES_LEN);
  memcpy(cli_buf, &file_len, DATA_LEN);
  memcpy(cli_buf+DATA_LEN, argv[1], FILE_NAME_LEN);
  
  //read file into headered buffer
  nbytes_read = read(fido, cli_buf+header_len, MAX_MES_LEN - header_len); 
  buflen = nbytes_read + header_len;
  
  int total_sent = 0;
  while (nbytes_read > 0){
    //reset the byes sent from last buffer
    nbytes_sent = 0;
    
    //ensure the entire buffer is sent
    while (nbytes_sent < buflen){
      nbytes_sent = SEND(sockfd, cli_buf+nbytes_sent, buflen - nbytes_sent, 0, (struct sockaddr *)&addr, addrlen);
      //printf("Nbytes_sent:%d\n", nbytes_sent);
      total_sent = total_sent + nbytes_sent;
    } 
    //read remaining chunks into headerless buffer
    nbytes_read = read(fido, cli_buf, MAX_MES_LEN); 
    //sleep(1);
  }
  printf ("Sending complete.\n");
  
  close(fido);
  printf("File closed.\n");

  close(sockfd);
  printf("Socket closed.\n");
  
  return 0;
}
