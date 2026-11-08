CC = gcc
CFLAGS = -Wall -g

TARGET = procmon 

SRCS = main.c

all: program

program: main.o utils.o
	$(CC) $(CFLAGS) $^ -o $(TARGET)

main.o: main.c
	$(CC) $(CFLAGS) -c $< -o $@

utils.o: utils.c utils.h
	$(CC) $(CFLAGS) -c $< -o $@
	
clean:
	rm -f $(TARGET) *.o