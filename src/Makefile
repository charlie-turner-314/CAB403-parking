CC = gcc
CFLAGS = -Wall -pedantic -I../libs -I../src # Show all reasonable warnings
LDFLAGS = -lrt -lm -lpthread
HEADERS = ../libs/delay.h ../libs/hashtable.h ../libs/queue.h ../libs/shm_parking.h simulator.h config.h

all: manager simulator

manager: manager.o ../libs/delay.o ../libs/hashtable.o ../libs/queue.o ../libs/shm_parking.o
	$(CC) $(LDFLAGS) -o $@ $^

simulator: simulator.o ../libs/delay.o ../libs/hashtable.o ../libs/queue.o ../libs/shm_parking.o
	$(CC) $(LDFLAGS) -o $@ $^

# dependencies for each file
manager.o: manager.c $(HEADERS)
simulator.o: simulator.c $(HEADERS)
../libs/delay.o: ../libs/delay.c $(HEADERS)
../libs/hashtable.o: ../libs/hashtable.c $(HEADERS)
../libs/queue.o: ../libs/queue.c $(HEADERS)
../libs/shm_parking.o: ../libs/shm_parking.c $(HEADERS)

clean:
	rm -f manager simulator *.o ../libs/*.o
	
.PHONY: all clean