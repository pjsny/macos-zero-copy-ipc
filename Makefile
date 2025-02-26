# Shared Memory Project Makefile

CC = clang
CFLAGS = -Wall -Wextra -g
LIBS = 

SRC_DIR = src
EXAMPLES_DIR = examples
BUILD_DIR = build

# Shared Memory Library
SHM_SRC = $(SRC_DIR)/shared_memory.c
SHM_OBJ = $(BUILD_DIR)/shared_memory.o
SHM_INCLUDE = -I$(SRC_DIR)

# Examples
EXAMPLES = countdown buffer_transfer

# Default target
all: directories $(EXAMPLES)

# Create build directories
directories:
	mkdir -p $(BUILD_DIR)
	mkdir -p $(foreach example,$(EXAMPLES),$(BUILD_DIR)/$(example))

# Shared memory implementation
$(SHM_OBJ): $(SHM_SRC)
	$(CC) $(CFLAGS) -c $< -o $@ $(SHM_INCLUDE)

# Countdown example
countdown: $(SHM_OBJ) $(EXAMPLES_DIR)/countdown/process1.c $(EXAMPLES_DIR)/countdown/process2.c
	$(CC) $(CFLAGS) $(EXAMPLES_DIR)/countdown/process1.c $(SHM_OBJ) -o $(BUILD_DIR)/countdown/process1 $(LIBS) $(SHM_INCLUDE)
	$(CC) $(CFLAGS) $(EXAMPLES_DIR)/countdown/process2.c $(SHM_OBJ) -o $(BUILD_DIR)/countdown/process2 $(LIBS) $(SHM_INCLUDE)

# Buffer transfer example
buffer_transfer: $(SHM_OBJ) $(EXAMPLES_DIR)/buffer_transfer/consumer.c $(EXAMPLES_DIR)/buffer_transfer/producer.c $(EXAMPLES_DIR)/buffer_transfer/buffer_shared.h
	$(CC) $(CFLAGS) $(EXAMPLES_DIR)/buffer_transfer/consumer.c $(SHM_OBJ) -o $(BUILD_DIR)/buffer_transfer/consumer $(LIBS) $(SHM_INCLUDE) -I$(EXAMPLES_DIR)/buffer_transfer
	$(CC) $(CFLAGS) $(EXAMPLES_DIR)/buffer_transfer/producer.c $(SHM_OBJ) -o $(BUILD_DIR)/buffer_transfer/producer $(LIBS) $(SHM_INCLUDE) -I$(EXAMPLES_DIR)/buffer_transfer

# Clean build files
clean:
	rm -rf $(BUILD_DIR)
	rm -f /tmp/xpc_connection_name

# Clean shared memory objects
clean-shm:
	-rm /dev/shm/my_shared_memory 2>/dev/null || true
	-rm /dev/shm/sem.sem_ready 2>/dev/null || true
	-rm /dev/shm/sem.sem_done 2>/dev/null || true

.PHONY: all clean clean-shm directories $(EXAMPLES) run-countdown-1 run-countdown-2 run-buffer-consumer run-buffer-producer