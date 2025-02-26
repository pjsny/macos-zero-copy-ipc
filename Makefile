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

# List of examples - add new examples here
EXAMPLES = countdown buffer_transfer ring_buffer

# Default target
all: directories $(EXAMPLES)

# Create build directories
directories:
	mkdir -p $(BUILD_DIR)
	mkdir -p $(foreach example,$(EXAMPLES),$(BUILD_DIR)/$(example))

# Shared memory implementation
$(SHM_OBJ): $(SHM_SRC)
	$(CC) $(CFLAGS) -c $< -o $@ $(SHM_INCLUDE)

# Generic rule for examples with two executables
# The first parameter is the example name
# The second parameter is the first executable name (default: process1)
# The third parameter is the second executable name (default: process2)
define build_example_pair
$(1): $(SHM_OBJ) $(wildcard $(EXAMPLES_DIR)/$(1)/*.h) $(EXAMPLES_DIR)/$(1)/$(2).c $(EXAMPLES_DIR)/$(1)/$(3).c
	$(CC) $(CFLAGS) $(EXAMPLES_DIR)/$(1)/$(2).c $(SHM_OBJ) -o $(BUILD_DIR)/$(1)/$(2) $(LIBS) $(SHM_INCLUDE) -I$(EXAMPLES_DIR)/$(1)
	$(CC) $(CFLAGS) $(EXAMPLES_DIR)/$(1)/$(3).c $(SHM_OBJ) -o $(BUILD_DIR)/$(1)/$(3) $(LIBS) $(SHM_INCLUDE) -I$(EXAMPLES_DIR)/$(1)
endef

# Apply the template for each example
$(eval $(call build_example_pair,countdown,process1,process2))
$(eval $(call build_example_pair,buffer_transfer,producer,consumer))
$(eval $(call build_example_pair,ring_buffer,producer,consumer))

# To add a new example, just add its name to the EXAMPLES list and call the template:
# $(eval $(call build_example_pair,new_example_name,executable1_name,executable2_name))

# Clean build files
clean:
	rm -rf $(BUILD_DIR)
	rm -f /tmp/xpc_connection_name

# Clean shared memory objects
clean-shm:
	-rm /dev/shm/my_shared_memory 2>/dev/null || true
	-rm /dev/shm/sem.sem_ready 2>/dev/null || true
	-rm /dev/shm/sem.sem_done 2>/dev/null || true
	-rm /dev/shm/ring_buffer_shared_memory 2>/dev/null || true

.PHONY: all clean clean-shm directories $(EXAMPLES)