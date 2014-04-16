typedef struct circular_buffer
{
    void *buffer;     // data buffer
    void *buffer_end; // end of data buffer
	size_t count;  		//number of pdu's in buffer
    void *head;       // pointer to head
    void *tail;       // pointer to tail
} circular_buffer;

typedef struct sliding_window
{
    void *head;       // pointer to head pdu
	unsigned int head_sequence_num;
	//void *current;		//(???Maybe not)pointer to pdu currently being used
    void *tail;       // pointer to tail pdu
	
	size_t count;		//number of pdu's in window
	size_t capacity;
	int packet_acks[20];
} sliding_window;


//**************************************WINDOW MANAGEMENT******************************

/*
Pseudocode for update window (*sliding window, *circularbuf, which is to be called whenever an ack is received and the pdu is marked

		*Before trying to move the head or do anything with the pdu there,
		this makes sure that the head is not at the tail of the window or the buffer


		set int initial count to window count
		while int i (= 0) and less than initial count AND head frame not unacked AND window head is not at buffer tail AND window head is not at window tail
			if packet_acks[i] == 1	
				progress head of window (FUNCTION)
				decrement window->count
				set packet_acks[i] = 0
				i ++;
				progress head of buffer (FUNCTION)
				decrement buffer->count
			endif
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

/*
Pseudocode for markPDUAcked( int seqNumber)
	index = (calculate which frame is being acked, by comparing seqNumber to window head sequence Number)
	
	Set Window-> packet_acks[index] to 1
*/

/*
Pseudo for progress tail (sliding_window *sw, circular_buffer *cb)
	sw->tail = (char *)sw->head + 1000 * sizeof(char)
	if sw->tail == cb->end
		sw->tail = cb->beginning
	endif
*/

void progress_window_tail(sliding_window *sw,circular_buffer *cb){
	sw->tail = (char *)sw->head + 1000 * sizeof(char);
	if (sw->tail == cb->buffer_end){
		sw->tail = cb->buffer;
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
	sw->head = (char *)sw->head + 1000 * sizeof(char);
	sw->head_sequence_number ++; // or plus 1000, depending on our decision
	if (sw->head == cb->buffer_end){
		sw->head = cb->buffer;
	}
	//advance the head of the buffer, as well
	cb->head = sw->head;
}

//**********************************************BUFFER MANAGEMENT*************************

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








void cb_init(circular_buffer *cb)
{
    cb->buffer = malloc(64000*sizeof (char));
    if(cb->buffer == NULL)
        // handle error
    cb->buffer_end = (char *)cb->buffer + 64000*sizeof (char);
    cb->head = cb->buffer;
    cb->tail = cb->buffer;
}

void sw_init(sliding_window *win, circular_buffer *cb)
{
	win->head = cb->head;
	win->tail = cb->head;	
	win->count = 0;
	win->capacity = 20;
	win->packet_acks = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
}
















void cb_free(circular_buffer *cb)
{
    free(cb->buffer);
    // clear out other fields too, just to be safe
}

void cb_push_back(circular_buffer *cb, const void *item)
{
    if(cb->count == cb->capacity)
        // handle error
    memcpy(cb->head, item, cb->sz);
    cb->head = (char*)cb->head + cb->sz;
    if(cb->head == cb->buffer_end)
        cb->head = cb->buffer;
    cb->count++;
}

void cb_pop_front(circular_buffer *cb, void *item)
{
    if(cb->count == 0)
        // handle error
    memcpy(item, cb->tail, cb->sz);
    cb->tail = (char*)cb->tail + cb->sz;
    if(cb->tail == cb->buffer_end)
        cb->tail = cb->buffer;
    cb->count--;
}
