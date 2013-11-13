CC     = gcc
CFLAGS = -g -Wall

all:
	$(CC) $(CFLAGS) watchmonkey.c -o watchmonkey

clean:
	rm -rf *.o *~ watchmonkey
