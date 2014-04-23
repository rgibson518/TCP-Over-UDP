objects = server.o client.o dt.o tcpd.o 

all : client server dt tcpd 

client: client.c tcp/tcpd.h
	gcc -o client client.c

server: server.c tcp/tcpd.h
	gcc -o server server.c

dt:   original_delta_timer_with_pipes.c
	gcc -o dt original_delta_timer_with_pipes.c -lpthread

tcpd: tcp/tcpd.c tcp/tcpd.h
	gcc -o tcpd tcp/tcpd.c -lpthread

.PHONY: clean
clean: 
	-rm server client tcpd dt
