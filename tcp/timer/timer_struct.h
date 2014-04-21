typedef struct timer
{
	unsigned long duration; //duration of timer in microseconds
	unsigned int packet_number; //sequence number of corresponding packet
	unsigned int port_number; //number of the corresponding port
	struct timer *next;
} timer;