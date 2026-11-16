CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -D_POSIX_C_SOURCE=200809L -std=c11
DEV_FLAGS = -g -fsanitize=address,undefined

TARGET = procmon 

SRCS = main.c

all: program

program: main.o utils.o
	$(CC) $(CFLAGS) $(DEV_FLAGS) $^ -o $(TARGET)

main.o: main.c
	$(CC) $(CFLAGS) $(DEV_FLAGS) -c $< -o $@

utils.o: proc.c proc.h
	$(CC) $(CFLAGS) $(DEV_FLAGS) -c $< -o $@
	
clean:
	rm -f $(TARGET) *.o