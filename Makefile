# Shared Memory Project Makefile

CC = clang
CFLAGS = -Wall -Wextra -g
LIBS = 

# Object files
OBJ = shared_memory.o

# Executables
BINS = process1 process2

# Default target
all: $(BINS)

# Shared memory implementation
shared_memory.o: shared_memory.c shared_memory.h
	$(CC) $(CFLAGS) -c $< -o $@

# Process 1 (writer)
process1: process1.c $(OBJ)
	$(CC) $(CFLAGS) $< $(OBJ) -o $@ $(LIBS)

# Process 2 (reader)
process2: process2.c $(OBJ)
	$(CC) $(CFLAGS) $< $(OBJ) -o $@ $(LIBS)

# Clean build files
clean:
	rm -f $(OBJ) $(BINS)
	rm -f /tmp/xpc_connection_name

# Clean shared memory objects (in case they weren't properly cleaned up)
clean-shm:
	-rm /dev/shm/my_shared_memory 2>/dev/null || true
	-rm /dev/shm/sem.sem_ready 2>/dev/null || true
	-rm /dev/shm/sem.sem_done 2>/dev/null || true

# Run process1
run1:
	./process1

# Run process2
run2:
	./process2

.PHONY: all clean clean-shm run1 run2