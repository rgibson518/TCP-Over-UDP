objects = server.o client.o tcpd.o

all : client server tcpd

client: client.c tcpd.h
	gcc -o client client.c

server: server.c tcpd.h
	gcc -o server server.c

tcpd: tcpd.c tcpd.h
	gcc -o tcpd tcpd.c

.PHONY: clean
clean: 
	-rm server client tcpd
