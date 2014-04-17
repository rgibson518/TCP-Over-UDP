typedef struct circular_buffer
{
    char buffer[64][1000];     // data buffer
	unsigned int count;  		//number of pdu's in buffer
	unsigned int capacity;
    int head;       // pointer to head
	int tail;       // pointer to tail
} circular_buffer;

typedef struct sliding_window
{
    int head;       // index of head pdu
	unsigned int head_sequence_num;
    int tail;       // index of tail pdu
	
	unsigned int count;		//number of pdu's in window
	unsigned int capacity;
	int packet_acks[20];
} sliding_window;

void cb_init(circular_buffer *cb)
{
	cb->head = 0;
	cb->tail = 0;
	cb->capacity = 64;
	cb->count = 0;
}

void sw_init(sliding_window *win, circular_buffer *cb)
{
	win->head = cb->head;
	win->tail = cb->head;	
	win->count = 0;
	win->capacity = 20;
	win->packet_acks = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
}

//**************************************WINDOW MANAGEMENT******************************

/*
Pseudocode for markPDUAcked( int seqNumber)
	index = (calculate which frame is being acked, by comparing seqNumber to window head sequence Number)
	
	Set Window-> packet_acks[index] to 1
*/

void markPDUAcked(int seqNumber, sliding_window *sw, circular_buffer *cb){
	int pdu_index_in_window = seqNumber - sw->head_sequence_num;
	
	sw->packet_acks[pdu_index_in_window] = 1;
}

/*
Pseudocode for update window (*sliding window, *circularbuf, which is to be called whenever an ack is received and the pdu is marked

		*Before trying to move the head or do anything with the pdu there,
		this makes sure that the head is not at the tail of the window or the buffer


		set int initial count to window count
		set frame acked to value in index 0 of packet_acks
		while int i (= 0) and less than initial count AND frame is acked AND window head is not at buffer tail AND window head is not at window tail
				progress heads of window and buffer(FUNCTION)
				decrement window->count
				decrement buffer->count
				set packet_acks[i] = 0
				i ++;
				frame acked = packet_acks[i]
		endwhile
	
		*Before moving the tail, this checks that the window can be expanded and also checks
		that the window tail has not reached the buffer tail.
	
		Now while count < capacity AND window tail is not equal to buffer tail
			send PDU from new frame in front of tail (FUNCTION)
			set packet_acks[window count] == 0
			progress tail of window (FUNCTION)
			increment window count
		endwhile	
	
*/
void update_window(sliding_window *sw, circular_buffer *cb){
	unsigned int initial_window_count = sw->count;
	int i = 0;
	int frame_acked = sw->packet_acks[0];
	
	while (i < initial_window_count && frame_acked == 1 && sw->head != cb->tail && sw->head != sw->tail){
			
			progress_heads(sw, cb);
			sw->count --;
			cb->count --;
			sw->packet_acks[i] = 0;
			i++;
			if (i < 20){
				frame_acked = sw->packet_acks[i];	
			}
		}
}

void send_available_packets(sliding_window *sw, circular_buffer *cb){
		while ( sw->count < sw->capacity && sw->tail != cb->tail){
		/*
		TODO  SEND PDU IN FRONT OF TAIL
		*/
		int window_count = sw->count;
		sw->packet_acks[window_count] == 0;
		progress_window_tail(sw,cb);
		sw->count++;
	}
}

 /*
Pseudo for addding pdu to buffer (circular_buffer *cb, const *PDU)
	if count = 64, then error - cannot add another
	
	memcpy(cb->tail, PDU, 1000*sizeof(char))   //hard coding copying 1000 bytes from PDU into buffer right now.
									//Might need some sort of handling like adding an end symbol
									//if data does not take up entire alloted space, so that receiver knows when to stop
	cb->tail = (char *) cb->tail + 1000*sizeof(char)
	if (cb->tail == cb-> end)
		cb->tail = cb-> buffer
	cb-> count ++
*/

void add_to_buffer(sliding_window *sw,circular_buffer *cb, pdu *packet){
	if (cb->count == cb->capacity) {
		fprintf(stderr,"Buffer is Full");
		exit();
	}
	
	int buffer_tail = cb->tail;
	memcpy(cb->buffer[buffer_tail], packet, sizeof(pdu));
	progress_buffer_tail(cb);
	cb->count++;
}

void progress_buffer_tail(circular_buffer *cb){
	cb->tail = (cb->tail +1)%(cb->capacity);
}


/*
Pseudo for progress tail (sliding_window *sw, circular_buffer *cb)
	sw->tail = (char *)sw->head + 1000 * sizeof(char)
	if sw->tail == cb->end
		sw->tail = cb->beginning
	endif
*/

void progress_window_tail(sliding_window *sw,circular_buffer *cb){
	sw->tail = sw->tail+1;
	if (sw->tail == 64){
		sw->tail = 0;
	}	
}

/*
Pseudo for progress head (sliding_window *sw, circular_buffer *cb)
	(maybe set mem in head frame to null but might not be needed)
	sw->head = (char *)sw->head + 1000 * sizeof(char)
	set new value for current_head_seq in sw
	if sw->head == cb->end
		sw->head = cb->beginning
	cb->head = sw->head
	endif
*/

void progress_heads(sliding_window *sw,circular_buffer *cb){
	sw->head = (sw->head ++)%cb->capacity;
	cb->head = sw->head;
	sw->head_sequence_number ++; // or plus 1000, depending on our decision
}





