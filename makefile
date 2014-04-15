objects = server.o client.o tcpd.o

all : client server tcpd

client: client.c tcp/tcpd.h
	gcc -o client client.c

server: server.c tcp/tcpd.h
	gcc -o server server.c

tcpd: tcp/tcpd.c tcp/tcpd.h
	gcc -o tcpd tcp/tcpd.c

.PHONY: clean
clean: 
	-rm server client tcpd
