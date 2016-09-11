#Viziru Luciana - 332 CA
CFLAGS = -Wall -g -Wno-unused -lpthread

.PHONY: all clean

build: libscheduler.so

libscheduler.so: so_scheduler.o task_queue.o linkedlist.o
	gcc -shared so_scheduler.o task_queue.o linkedlist.o -o libscheduler.so

so_scheduler.o: so_scheduler.h so_scheduler.c util.h
	gcc -c -fPIC so_scheduler.c

task_queue.o: task_queue.h task_queue.c util.h
	gcc -c -fPIC task_queue.c

linkedlist.o: linkedlist.c linkedlist.h util.h
	gcc -c -fPIC linkedlist.c

clean:
	-rm -f *~ *.o *.so

