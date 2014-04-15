#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include<pthread.h>

unsigned int s,t, len;

void* listen_to_socket(){
	int mode;
	int n, rc;
	unsigned int port, packet, duration;
	char *token, *search = " ";
	char str[100];

	while(1){
        if ((t=recv(s, str, 100, 0)) > 0) {
            str[t] = '\0';
            printf("%s", str);
        } else {
            if (t < 0) perror("recv");
            else printf("Server closed connection\n");
            exit(1);
        }
	}
}

int main(void)
{
    struct sockaddr_un remote;
    char str[100];
	int thread_id;
	pthread_t	p_thread;

    if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }

    printf("Trying to connect...\n");

    remote.sun_family = AF_UNIX;
    strcpy(remote.sun_path, "mysocket2");
    len = strlen(remote.sun_path) + sizeof(remote.sun_family);
    if (connect(s, (struct sockaddr *)&remote, len) == -1) {
        perror("connect");
        exit(1);
    }

    printf("Connected.\n");

	thread_id = pthread_create(&p_thread, NULL, listen_to_socket, NULL);

	/*
    while(printf(""), fgets(str, 100, stdin), !feof(stdin)) {
        if (send(s, str, strlen(str), 0) == -1) {
            perror("send");
            exit(1);
        }
    }*/
	
	int n;
	int a =1;
	while(a == 1){
	
		printf("\nTimer Create: port 1  duration 30\n");
		sprintf(str, "1 1 1 30");		
		n = send(s, str, 100, 0);
		
		printf("\nTimer Create: port 2  duration 15\n");
		sprintf(str, "1 2 1 15");		
		n = send(s, str, 100, 0);
		
		printf("\nTimer Create: port 3  duration 20\n");
		sprintf(str, "1 3 1 20");		
		n = send(s, str, 100, 0);
		
		sleep(5);
		
		printf("\nTimer Cancel: port 2 \n");
		sprintf(str, "2 2 1 20");		
		n = send(s, str, 100, 0);
		
		printf("\nTimer Create: port 4  duration 40\n");
		sprintf(str, "1 4 1 40");		
		n = send(s, str, 100, 0);
		
		sleep(5);
		
		printf("\nTimer Create: port 5  duration 5\n");
		sprintf(str, "1 5 1 50");		
		n = send(s, str, 100, 0);
		
		printf("\nTimer Cancel: port 4\n");
		sprintf(str, "2 4 1 1");		
		n = send(s, str, 100, 0);
		
		printf("\nTimer Cancel: port 8\n");
		sprintf(str, "2 8 1 2");		
		n = send(s, str, 100, 0);
		
		a=0;
	}
	
    while(printf(""), fgets(str, 100, stdin), !feof(stdin)) {
        if (send(s, str, strlen(str), 0) == -1) {
            perror("send");
	    close(s);
            exit(1);
        }
    }
    //close(s);

    return 0;
}
