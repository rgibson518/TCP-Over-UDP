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

#ifndef BUFFER_H
#define BUFFER_H

#define UNACKED 0
#define ACKED 1
#define PDU_SIZE 1000
#define BUFFER_SIZE 64

typedef struct circular_buffer
{
    char buffer[BUFFER_SIZE][PDU_SIZE];     // data buffer
    unsigned int count;  		//number of pdu's in buffer
    unsigned int capacity;
    int head;       // pointer to head
    int tail;       // pointer to tail
} circular_buffer;

typedef struct sliding_window
{
    //tracks index of first in window
    int head;       
    // tracks index last sent pdu.
    int tail; 
    unsigned int head_sequence_num;

    //number of pdu's in window
    unsigned int count;
    unsigned int capacity;

    // tracks ack'd packets with 1, unack'd with 0 : Use ACKED/UNACKED
    int packet_acks[20];
} sliding_window;


/* ===============Prototypes ========= */

//Buffer functions
void cb_init(circular_buffer *cb);

//Window functions
void sw_init(sliding_window *win, circular_buffer *cb);
int markPDUAcked(int seqNumber, sliding_window *sw, circular_buffer *cb);
void update_window(sliding_window *sw, circular_buffer *cb, sem_t* sw_sem, sem_t* cb_sem);
void send_available_packets(sliding_window *sw, circular_buffer *cb);
void progress_buffer_tail(circular_buffer *cb);
void add_to_buffer(sliding_window *sw,circular_buffer *cb, pdu *packet);
void add_to_r_buffer(sliding_window *sw,circular_buffer *cb, pdu *packet);
void progress_window_tail(sliding_window *sw,circular_buffer *cb);
void progress_heads(sliding_window *sw,circular_buffer *cb);


/* ===============Implementations  ========= */

void cb_init(circular_buffer *cb)
{
    cb->head = 0;
    cb->tail = 0;
    cb->capacity = BUFFER_SIZE;
    cb->count = 0;
}

void sw_init(sliding_window *win, circular_buffer *cb)
{
    int i;
    win->head = cb->head;
    win->tail = cb->head;	
    win->count = 0;
    win->capacity = 20;
    for (i = 0; i < 20; i++)
    {
	win->packet_acks[i] = 0;
    }
}


/* Return -1 if duplicate, 0 if new ack
 * Can be used to filter out duplicates
*/
int markPDUAcked(int seqNumber, sliding_window *sw, circular_buffer *cb)
{
    
    int pdu_index_in_window = seqNumber - sw->head_sequence_num;
    //printf("Window head:\t%i\n", sw->head_sequence_num);
    //printf("Sequence # to ACK:\t%i\n", seqNumber);
    //printf("Acking packet at window index:\t%i\n", pdu_index_in_window);
    // if duplicate ack
    if ( (pdu_index_in_window < 0) ||  (sw->packet_acks[pdu_index_in_window] ==1))
    {
	return  -1;
    }
    else{
	sw->packet_acks[pdu_index_in_window] = 1;
	return 0;
    }
}

void update_window(sliding_window *sw, circular_buffer *cb, sem_t* sw_sem, sem_t* cb_sem)
{
    unsigned int initial_window_count = sw->count;
    int i = 0;
    int frame_acked = sw->packet_acks[0];
    
    while (i < initial_window_count && frame_acked == 1 && sw->head != cb->tail && sw->head != sw->tail)
{
	
	progress_heads(sw, cb);
	sw->count --;
	cb->count --;
	sw->packet_acks[i] = 0;
	i++;
	sem_post(sw_sem);
	sem_post(cb_sem);
	if (i < 20){
	    frame_acked = sw->packet_acks[i];	
	}
    }
}

void update_r_window(sliding_window *sw, circular_buffer *cb, sem_t* sw_sem, sem_t* cb_sem)
{
    unsigned int initial_window_count = sw->count;
    int i = 0;
    int frame_acked = sw->packet_acks[0];
    
    while (i < initial_window_count && frame_acked == 1 && sw->head != cb->tail)
{
	
	progress_heads(sw, cb);
	sw->count --;
	cb->count --;
	sw->packet_acks[i] = 0;
	i++;
	sem_post(sw_sem);
	sem_post(cb_sem);
	if (i < 20){
	    frame_acked = sw->packet_acks[i];	
	}
    }
}

void send_available_packets(sliding_window *sw, circular_buffer *cb)
{
    while ( sw->count < sw->capacity && sw->tail != cb->tail)
{
	/*
	  TODO  SEND PDU IN FRONT OF TAIL
	*/
	int window_count = sw->count;
	sw->packet_acks[window_count] == 0;
	progress_window_tail(sw,cb);
	sw->count++;
    }
}


void progress_buffer_tail(circular_buffer *cb)
{
    cb->tail = (cb->tail +1)%(cb->capacity);
}

void add_to_buffer(sliding_window *sw,circular_buffer *cb, pdu *packet)
{
    if (cb->count == cb->capacity) {
	perror("Buffer is Full");
	exit(1);
    }
    
    int buffer_tail = cb->tail;
    memcpy(cb->buffer[buffer_tail], packet, sizeof(pdu));
    progress_buffer_tail(cb);
    cb->count++;
}


/* Requires that the packet has been checked arleady for duplication
 */
void add_to_r_buffer(sliding_window *sw,circular_buffer *cb, pdu *packet)
{
    if (cb->count == cb->capacity) {
	perror("Buffer is Full");
	exit(1);
    }
    
    int pos = (packet->h.seq_num - sw->head_sequence_num)%BUFFER_SIZE;
    memcpy(cb->buffer[pos], packet, sizeof(pdu));
    cb->count++;
}


void progress_window_tail(sliding_window *sw,circular_buffer *cb)
{
    sw->tail = (sw->tail+1)%BUFFER_SIZE;
}

void progress_heads(sliding_window *sw,circular_buffer *cb)
{
    sw->head = (sw->head ++)%cb->capacity;
    cb->head = sw->head;
    sw->head_sequence_num ++; // or plus PDU_SIZE, depending on our decision
}


#endif