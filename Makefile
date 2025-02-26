# Shared Memory Project Makefile

CC = clang
CFLAGS = -Wall -Wextra -g
LIBS = 

# Add optimization flags for SIMD example
SIMD_CFLAGS = -Wall -Wextra -O3 -march=armv8-a
SIMD_LIBS = -lm  # Add math library for expf function

SRC_DIR = src
EXAMPLES_DIR = examples
BUILD_DIR = build

# Shared Memory Library
SHM_SRC = $(SRC_DIR)/shared_memory.c
SHM_OBJ = $(BUILD_DIR)/shared_memory.o
SHM_INCLUDE = -I$(SRC_DIR)

# List of examples - add new examples here
EXAMPLES = countdown buffer_transfer ring_buffer mmap_file simd_processing

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

# Special case for mmap_file example with three executables
mmap_file: $(SHM_OBJ) $(EXAMPLES_DIR)/mmap_file/mmap_shared.h $(EXAMPLES_DIR)/mmap_file/db_creator.c $(EXAMPLES_DIR)/mmap_file/db_reader.c $(EXAMPLES_DIR)/mmap_file/db_writer.c
	$(CC) $(CFLAGS) $(EXAMPLES_DIR)/mmap_file/db_creator.c -o $(BUILD_DIR)/mmap_file/db_creator $(LIBS) -I$(EXAMPLES_DIR)/mmap_file -pthread
	$(CC) $(CFLAGS) $(EXAMPLES_DIR)/mmap_file/db_reader.c -o $(BUILD_DIR)/mmap_file/db_reader $(LIBS) -I$(EXAMPLES_DIR)/mmap_file -pthread
	$(CC) $(CFLAGS) $(EXAMPLES_DIR)/mmap_file/db_writer.c -o $(BUILD_DIR)/mmap_file/db_writer $(LIBS) -I$(EXAMPLES_DIR)/mmap_file -pthread

# Special case for SIMD processing example
simd_processing: $(EXAMPLES_DIR)/simd_processing/simd_shared.h $(EXAMPLES_DIR)/simd_processing/producer.c $(EXAMPLES_DIR)/simd_processing/consumer.c
	$(CC) $(SIMD_CFLAGS) $(EXAMPLES_DIR)/simd_processing/producer.c -o $(BUILD_DIR)/simd_processing/producer $(SIMD_LIBS) -I$(EXAMPLES_DIR)/simd_processing
	$(CC) $(SIMD_CFLAGS) $(EXAMPLES_DIR)/simd_processing/consumer.c -o $(BUILD_DIR)/simd_processing/consumer $(SIMD_LIBS) -I$(EXAMPLES_DIR)/simd_processing

# Clean build files
clean:
	rm -rf $(BUILD_DIR)
	rm -f /tmp/xpc_connection_name
	rm -f $(MMAP_FILE_PATH)

# Clean shared memory objects
clean-shm:
	-rm /dev/shm/my_shared_memory 2>/dev/null || true
	-rm /dev/shm/sem.sem_ready 2>/dev/null || true
	-rm /dev/shm/sem.sem_done 2>/dev/null || true
	-rm /dev/shm/ring_buffer_shared_memory 2>/dev/null || true
	-rm /dev/shm/simd_shared_memory 2>/dev/null || true

.PHONY: all clean clean-shm directories $(EXAMPLES)