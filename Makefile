CC = gcc
CFLAGS = -g -Wall
LDFLAGS = -lpthread

all:

	gcc -g -Wall proxy.c cache.c csapp.c -lpthread -o proxy

submit:
	(make clean; cd ..; tar cvf proxylab.tar proxylab-handout)

clean:
	rm -f *~ *.o proxy core

