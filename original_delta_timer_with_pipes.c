#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <fcntl.h>
#include<stdio.h>
#include<stdlib.h>
#include<stdbool.h>
#include<unistd.h>
#include<pthread.h>
#include <time.h>

struct timer
{
	unsigned long duration; //duration of timer in microseconds
	unsigned int packet_number; //sequence number of corresponding packet
	struct timer *next;
};

struct timeout_alert {
    unsigned int packet_number;
    struct timeout_alert *next;
};

struct timer *head = NULL;
struct timer *current = NULL;
struct timer *previous = NULL;


struct timeout_alert *first = NULL;
struct timeout_alert *current_timeout = NULL;
struct timeout_alert *timeout_hold = NULL;


unsigned int s, s2;
struct sockaddr_un local, remote;
int len;

pthread_mutex_t a_mutex = PTHREAD_MUTEX_INITIALIZER;


int add_to_delta_list(unsigned long tDuration, unsigned int tPacket_number){
	struct timer *ptr = (struct timer*)malloc(sizeof(struct timer));
	int rc;
	if (ptr == NULL) {
		//some error handling for timer creation error
	}
	ptr->duration = tDuration;
	ptr->packet_number = tPacket_number;
	ptr->next = NULL;

	//If the head is null, this is the first timer, so set the head and current pointers and return
	if (NULL == head){
		ptr->next = NULL;	
		head = current = ptr;
		return 0;
	}
	
	/*Insertion logic (comparing durations and inserting in appropriate location) occurs here
	Go through list of timer structs
	Compare new struct duration to duration of first timer in list
	If it is less, place new struct at beginning.  Subtract duration of new from old beginning, and make old beginning the "next" in new struct
	otherwise, subtract the duration of the first from the new
	compare to second, and repeat*/
	
	previous = NULL;
	current = head;
	while (1){
		if (current->duration <= ptr->duration){
			//ptr is the timer to be added, current is the timer in the list being compared to
			ptr->duration = ptr->duration - current->duration;
			previous = current;
			
			//Probably need to check that current->next is not NULL, or something, to know when end of list has been reached
			if (NULL == current->next){
				current->next = ptr;
				ptr->next = NULL;
				return 0;
			}
			current = current->next;
			//continue comparing
		}
		else {
			//Adjust the next timer, which is being pushed down the list
			current->duration = current->duration - ptr->duration;
			ptr->next = current;
			if (current == head){
				head = ptr;
				return 0;
			}
			previous->next = ptr;		
			return 0;
		}		
	}
}

int remove_from_list(unsigned int tPacket_number){
	if(head==NULL) return 0; //hack	
	previous = NULL;
	current = head;
	int continu = 1, rc;
	
	//if(current==NULL) return 0;

	while(continu == 1){
		if(current->packet_number == tPacket_number){
			//This is the timer to remove, so remove it
			//First, check if it is the head, and handle accordingly
			if (current == head) {
				if (NULL == head->next){
					free(head);
					head = NULL;
					continu = 0;
					//return 0;
				}

				if (continu == 1){
					head->next->duration += head->duration;
					current = head->next;
					free(head);
					head = current;
				}

				continu = 0;
				//return 0;
			}
			
			else {
				//if timer is at the end of list, just free it
				if (NULL == current->next){
					previous->next = NULL;
					free(current);
					continu = 0;
					return 0;
				}
				//make previous' next point to current next
				previous->next = current->next;
				//add the duration of current to the duration of current next, and free current
				current->next->duration += current->duration;
				free(current);
				continu = 0;
				return 0;
			}
		}
		else {
			//timer to delete has not been found yet, so advance current and advance previous
			previous = current;
			if (NULL == current->next) {return 0;}
			current = current->next;
		}
	}
	return 0;
}

void print_timers(){
	int blah = 0;
	current = head;
	if (NULL==head) {blah = 1;}
	printf("\nList of Timers:\n");
	while (blah == 0){
		printf("%d %d\n", current->packet_number, current->duration);
		if (NULL == current->next){
			blah = 1;
		}
		else { current = current->next;}
	}
}

void* listen_to_socket(){
	int mode;
	int n, rc, add;
	unsigned int packet, duration;
	char *token, *search = " ";
	char str[100];
	//CHECK
	n = recv(s2, str, 100, 0);
	printf(str);
	
	while(1){
		//receive a string from the socket
		n = recv(s2, str, 100, 0);
		//parse the mode
		token = strtok(str, search);
		mode = atoi(token);	
		//parse the token number
		token = strtok(NULL, search);
		packet = atoi(token);
		//parse the duration
		token = strtok(NULL, search);
		duration = atoi(token);

		if (mode == 1){ // add timer
			add = add_to_delta_list(duration,packet);
			printf("\nAdded Timer: packet %d  duration %d\n", packet, duration);
		}
		else if (mode == 2){ // remove timer
			remove_from_list(packet);

			printf("\nRemoved Timer: packet %d \n", packet);	
		}
	}
}

void* do_timer()
{
	int rc, head_duration=1, sequence, n;
	char str[100];	
	while(1){
		//if head is NULL, do nothing 
		//mutex whole thing	
		if (NULL == head) {
			//Do nothing
		 }
		//otherwise, sleep then decrement
		else {
			if(usleep(1000) < 0 ){
				fprintf(stderr,"sleep fail");
				exit(1);
			}
			rc = pthread_mutex_lock(&a_mutex);
			if (rc) {
				perror("pthread_mutex_lock");
				pthread_exit(NULL);
			}
			sequence = decrement_head();
			rc = pthread_mutex_unlock(&a_mutex);
			if (rc) {
				perror("pthread_mutex_unlock");
				pthread_exit(NULL);
			}
			if (sequence >=0){
				sprintf(str, "%i", sequence);
				n = send(s2, str, 100, 0);
			}
		}
	}
	pthread_exit(NULL);
}

//From Beej's Guide
int32_t decrement_head(){
	struct timer *ptr;
	int32_t seq = 0;
	
	ptr = head;
	
	if(ptr == NULL){
		//printf("Delta List is Empty\n");
		return -1;
	}
	
	if((*ptr).duration == 0){
		seq = (*ptr).packet_number;
		
		// shift the head right
		head = (*head).next;
		free(ptr);
		return seq;
	}else{
		(*ptr).duration--;
		if((*ptr).duration == 0){
			seq = (*ptr).packet_number;
		
			// shift the head right
			head = (*head).next;
			free(ptr);
			return seq;
		}
	}
	
	return -1;
}

int main(int argc, char *argv[])
{	
	pthread_t	p_thread;
	int mode;
	int n, rc, add;
	unsigned int packet, duration;
	char *token, *search = " ";
	char str[100];	
	
	s = socket(AF_UNIX, SOCK_STREAM, 0);
	
	local.sun_family = AF_UNIX;
	strcpy(local.sun_path, "mysocket");
	unlink(local.sun_path);
	len = strlen(local.sun_path) + sizeof(local.sun_family);
	bind(s, (struct sockaddr *)&local, len);
	
	listen(s, 5);
	
	len = sizeof(remote);

        if ((s2 = accept(s, (struct sockaddr *)&remote, &len)) == -1) {
            perror("accept");
            exit(1);
        }

        printf("Connected.\n");
	//*******************************************************************************
	//Creating threads for listening to socket and for creating timer to decrement timer struct
	if (pthread_create(&p_thread, NULL, do_timer, NULL) != 0) printf("problem with second thread\n");

	//sprintf(str,"Threads Created.\n");
	/*sprintf(str, "%i", 10);
	n = send(s2, str, 100, 0);*/

	
	while(1){
		//receive a string from the socket
		n = recv(s2, str, 100, 0);
		//parse the mode
		token = strtok(str, search);
		mode = atoi(token);	
		//parse the token number
		token = strtok(NULL, search);
		packet = atoi(token);
		//parse the duration
		token = strtok(NULL, search);
		duration = atoi(token);

		if (mode == 1){ // add timer
			rc = pthread_mutex_lock(&a_mutex);
			if (rc) {
				perror("pthread_mutex_lock");
				pthread_exit(NULL);
			}
			add = add_to_delta_list(duration,packet);
			rc = pthread_mutex_unlock(&a_mutex);
			if (rc) {
				perror("pthread_mutex_unlock");
				pthread_exit(NULL);
			}
			printf("\nAdded Timer: packet %d  duration %d\n", packet, duration);
		}
		else if (mode == 2){ // remove timer
		
			rc = pthread_mutex_lock(&a_mutex);
			if (rc) {
				perror("pthread_mutex_lock");
				pthread_exit(NULL);
			}
			remove_from_list(packet);
			rc = pthread_mutex_unlock(&a_mutex);
			if (rc) {
				perror("pthread_mutex_unlock");
				pthread_exit(NULL);
			}
			printf("\nRemoved Timer: packet %d \n", packet);	
		}
	}
}


