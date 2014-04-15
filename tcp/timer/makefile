all: dt td

dt: original_delta_timer_with_pipes.c
	gcc -o dt original_delta_timer_with_pipes.c -lpthread

td: timer_driver.c
	gcc -o td timer_driver.c -lpthread

clean:
	rm -rf mysocket2 dt td *.o *~
