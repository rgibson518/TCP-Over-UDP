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
	//	unsigned int port_number; //number of the corresponding port
	struct timer *next;
};

struct timeout_alert {
	unsigned int packet_number;
	//    unsigned int port_number;
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
pthread_mutex_t b_mutex = PTHREAD_MUTEX_INITIALIZER;


struct timer* add_to_delta_list(unsigned long tDuration, unsigned int tPacket_number){    //, unsigned int tPort_number){
	int rc;	
	struct timer *ptr = (struct timer*)malloc(sizeof(struct timer));
	if (ptr == NULL) {
		//some error handling for timer creation error
	}
	ptr->duration = tDuration;
	ptr->packet_number = tPacket_number;
	//	ptr->port_number = tPort_number;
	ptr->next = NULL;

	//If the head is null, this is the first timer, so set the head and current pointers and return
	rc = pthread_mutex_lock(&a_mutex);				
	if (rc) {
		perror("pthread_mutex_lock");
		pthread_exit(NULL);
	}
	
	if (NULL == head){
		ptr->next = NULL;
		
		head = current = ptr;
		return ptr;
	}
	
	rc = pthread_mutex_unlock(&a_mutex);				
	if (rc) {
		perror("pthread_mutex_unlock");
		pthread_exit(NULL);
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
		rc = pthread_mutex_lock(&a_mutex);				
		if (rc) {
			perror("pthread_mutex_lock");
			pthread_exit(NULL);
		}
		if (current->duration <= ptr->duration){
			//ptr is the timer to be added, current is the timer in the list being compared to

			ptr->duration = ptr->duration - current->duration;
			previous = current;
			
			//Probably need to check that current->next is not NULL, or something, to know when end of list has been reached
			if (NULL == current->next){
				current->next = ptr;
				ptr->next = NULL;
				return ptr;
			}
			current = current->next;

			rc = pthread_mutex_unlock(&a_mutex);				
			if (rc) {
				perror("pthread_mutex_unlock");
				pthread_exit(NULL);
			}

			//continue comparing
		}
		else {
			//Adjust the next timer, which is being pushed down the list

			current->duration = current->duration - ptr->duration;
			ptr->next = current;
			if (current == head){
				head = ptr;
				return head;
			}
			previous->next = ptr;
			rc = pthread_mutex_unlock(&a_mutex);				
			if (rc) {
				perror("pthread_mutex_unlock");
				pthread_exit(NULL);
			}
			return ptr;
		}		
	}
}

int remove_from_list(unsigned int tPacket_number){      //, unsigned int tPort_number){
	if(head==NULL) return 0; //hack	
	previous = NULL;
	current = head;
	int continu = 1;
	int rc;
	
	//if(current==NULL) return 0;

	while(continu == 1){
		if(current->packet_number == tPacket_number){
			//This is the timer to remove, so remove it
			//First, check if it is the head, and handle accordingly
			if (current == head) {
				rc = pthread_mutex_lock(&a_mutex);				
				if (rc) {
					perror("pthread_mutex_unlock");
					pthread_exit(NULL);
				}
				if (NULL == head->next){
					free(head);
					head = NULL;
					continu = 0;
					return 0;
				}
				//May need mutex here
				head->next->duration += head->duration;
				current = head->next;
				free(head);
				head = current;
				rc = pthread_mutex_unlock(&a_mutex);				
				if (rc) {
					perror("pthread_mutex_unlock");
					pthread_exit(NULL);
				}
				continu = 0;
				return 0;
			}
			
			else {
				rc = pthread_mutex_lock(&a_mutex);				
				if (rc) {
					perror("pthread_mutex_unlock");
					pthread_exit(NULL);
				}
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

				rc = pthread_mutex_unlock(&a_mutex);				
				if (rc) {
					perror("pthread_mutex_unlock");
					pthread_exit(NULL);
				}
				continu = 0;
				return 0;
			}
		}
		else {
			//timer to delete has not been found yet, so advance current and advance previous
			rc = pthread_mutex_lock(&a_mutex);				
			if (rc) {
				perror("pthread_mutex_lock");
				pthread_exit(NULL);
			}
			previous = current;
			if (NULL == current->next) {return 0;}
			current = current->next;
			rc = pthread_mutex_unlock(&a_mutex);				
			if (rc) {
				perror("pthread_mutex_unlock");
				pthread_exit(NULL);
			}
		}
	}
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
	int n, rc;
	unsigned int port, packet, duration;
	char *token, *search = " ";
	char str[100];
	
	while(1){
		//receive a string from the socket
		n = recv(s2, str, 100, 0);
		//parse the mode
		token = strtok(str, search);
		mode = atoi(token);	
		//parse the port number	
		//		token = strtok(NULL, search);
		//		port = atoi(token);
		//parse the packet number
		token = strtok(NULL, search);
		packet = atoi(token);
		//parse the duration
		token = strtok(NULL, search);
		duration = atoi(token);

		if (mode == 1){ // add timer
			//rc = pthread_mutex_lock(&a_mutex);
			struct timer* new = add_to_delta_list(duration,packet);
			//rc = pthread_mutex_unlock(&a_mutex);
			printf("\nAdded Timer: packet %d  duration %d\n", packet, duration);
			//print_timers();
			sprintf(str, "Added Timer: packet %d duration %d\n\n", port, packet, duration);
			
			n = send(s2, str, 100, 0);
		}
		else if (mode == 2){ // remove timer
			//			rc = pthread_mutex_lock(&a_mutex);
			//			if (rc) {
			//				perror("pthread_mutex_lock");
			//				pthread_exit(NULL);
			//			}
			remove_from_list(packet);//, port);
			//			rc = pthread_mutex_unlock(&a_mutex);
			//			if (rc) {
			//			    perror("pthread_mutex_unlock");
			//			    pthread_exit(NULL);
			//			}	
			printf("\nRemoved Timer: packet %d \n", port, packet);	
			//print_timers();
			//sprintf(str, "Removed Timer: port %d  packet %d\n\n", port, packet);
			
			//n = send(s2, str, 100, 0);
		}
	
		/*str[0] = 'a';
		if (send(s2, str, strlen(str), 0)==-1) {
		perror("send");
		exit(1);
		}*/
	}
}

void* do_timer()
{
	int rc;
	while(1){
		//if head is NULL, do nothing 
		//mutex whole thing	
		if (NULL == head) {
			/*
			if(sleep(1) < 0 ){
			fprintf(stderr,"sleep fail");
			exit(1);
			}*/
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
			head->duration--;
			rc = pthread_mutex_unlock(&a_mutex);				
			if (rc) {
				perror("pthread_mutex_unlock");
				pthread_exit(NULL);
			}
			//if head duration is 0 after decrement, remove from list

			
			//This if was copied from the section below for making timeout notifications, for testing without timeouts
			if(head->duration <= 0 && NULL != head){							
				remove_from_list(head->packet_number);				
			}

		/*	if(head->duration<=0){
				struct timeout_alert *ptr = (struct timeout_alert*)malloc(sizeof(struct timeout_alert));
				ptr->packet_number = head->packet_number;
				//			ptr->port_number = head->port_number;
				current_timeout = first;
				if(NULL == current_timeout){
					first = ptr;
				}
				else {
					int exit = 0;
					while(exit == 0){
						current_timeout = current_timeout->next;
						if(NULL == current_timeout->next){
							current_timeout->next = ptr;
							exit = 1;
						}
					}	
				}

				if(head->duration <= 0 && NULL != head){							
					remove_from_list(head->packet_number);				
				}

			}*/
		}

	}
	pthread_exit(NULL);
}

int main(int argc, char *argv[])
{	
	int	 	thread_id, thread_id2; //thread_id2 and pthread2 are for thread that is handling socket
	pthread_t	p_thread, p_thread2;
	int 		a = 1;
	
	
	s = socket(AF_UNIX, SOCK_STREAM, 0);
	
	local.sun_family = AF_UNIX;
	strcpy(local.sun_path, "mysocket2");
	unlink(local.sun_path);
	len = strlen(local.sun_path) + sizeof(local.sun_family);
	bind(s, (struct sockaddr *)&local, len);
	
	listen(s, 5);
	
	len = sizeof(remote);

	if ((s2 = accept(s, (struct sockaddr *)&remote, &len)) == -1) {
		perror("accept");
		exit(1);
	}

	//*******************************************************************************
	//Creating threads for listening to socket and for creating timer to decrement timer struct
	thread_id2 = pthread_create(&p_thread2, NULL, listen_to_socket, NULL);
	thread_id = pthread_create(&p_thread, NULL, do_timer, NULL);


	int packet_holder, port_holder, n;
	char str[100];
	while(1){
	
		//***********CHECK FOR TIMEOUT ALERTS, AND SEND TO TCPD IF ONE OCCURS**********
		//The timeout list probably needs its own mutex, maybe own thread.
		/*
		if (NULL!= first){
			packet_holder = first->packet_number;
			//		port_holder = first->port_number;
			//print_timers();
			
			//Removed port information, since we are only concerned with sequence # now
			sprintf(str, "%d\0", packet_holder);
			
			n = send(s2, str, 100, 0);
				
			if (NULL!=first->next){
				timeout_hold = first->next;
				free(first);
				first = timeout_hold;
				timeout_hold = NULL;
				//hold the next, free the first, make first the old next			
			}
			//just one so just free and make first NULL
			free(first);
			first = NULL;
		}*/
		//**********************************************************************
	}
}



