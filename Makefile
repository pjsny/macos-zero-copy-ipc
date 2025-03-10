CC = clang
CFLAGS = -Wall -Wextra -g
LIBS = 

# Add optimization flags for SIMD example
SIMD_CFLAGS = $(CFLAGS) -O3 -march=armv8-a -D__MATH__INTRINSICS__IMPLEMENTATION__

SIMD_LIBS = -lm  # Math library for expf function
SIMD_INCLUDE = -I$(CURDIR)/external/math_intrinsics

SRC_DIR = src
EXAMPLES_DIR = examples
BUILD_DIR = build
TEST_DIR = tests

# Shared Memory Library
SHM_SRC = $(SRC_DIR)/shared_memory.c
SHM_OBJ = $(BUILD_DIR)/shared_memory.o
SHM_INCLUDE = -I$(SRC_DIR)

# Standard examples with producer/consumer or process1/process2 pattern
STD_EXAMPLES = countdown:process1:process2 buffer_transfer:producer:consumer ring_buffer:producer:consumer atomic_buffer_transfer:producer:consumer

# Special examples
SPECIAL_EXAMPLES = mmap_file simd_processing benchmark_simd_buffer

# All examples - extract example names from STD_EXAMPLES
STD_EXAMPLE_NAMES = $(foreach ex,$(STD_EXAMPLES),$(firstword $(subst :, ,$(ex))))
EXAMPLES = $(STD_EXAMPLE_NAMES) $(SPECIAL_EXAMPLES)

# Test-related variables
TEST_BUILD_DIR = $(BUILD_DIR)/tests
SIMD_TESTS = vector_functions

# Default target
all: directories $(EXAMPLES)

# Create build directories
directories:
	mkdir -p $(BUILD_DIR)
	mkdir -p $(foreach example,$(EXAMPLES),$(BUILD_DIR)/$(example))
	mkdir -p $(TEST_BUILD_DIR)

# Shared memory implementation
$(SHM_OBJ): $(SHM_SRC)
	$(CC) $(CFLAGS) -c $< -o $@ $(SHM_INCLUDE)

# Process standard examples (with two executables)
define process_example
$(firstword $(subst :, ,$1)): $(SHM_OBJ)
	$(CC) $(CFLAGS) $(EXAMPLES_DIR)/$(firstword $(subst :, ,$1))/$(word 2,$(subst :, ,$1)).c $(SHM_OBJ) -o $(BUILD_DIR)/$(firstword $(subst :, ,$1))/$(word 2,$(subst :, ,$1)) $(LIBS) $(SHM_INCLUDE) -I$(EXAMPLES_DIR)/$(firstword $(subst :, ,$1))
	$(CC) $(CFLAGS) $(EXAMPLES_DIR)/$(firstword $(subst :, ,$1))/$(word 3,$(subst :, ,$1)).c $(SHM_OBJ) -o $(BUILD_DIR)/$(firstword $(subst :, ,$1))/$(word 3,$(subst :, ,$1)) $(LIBS) $(SHM_INCLUDE) -I$(EXAMPLES_DIR)/$(firstword $(subst :, ,$1))
endef

$(foreach ex,$(STD_EXAMPLES),$(eval $(call process_example,$(ex))))

# Special case for mmap_file example with three executables
mmap_file: $(SHM_OBJ)
	$(CC) $(CFLAGS) $(EXAMPLES_DIR)/$@/db_creator.c -o $(BUILD_DIR)/$@/db_creator $(LIBS) -I$(EXAMPLES_DIR)/$@ -pthread
	$(CC) $(CFLAGS) $(EXAMPLES_DIR)/$@/db_reader.c -o $(BUILD_DIR)/$@/db_reader $(LIBS) -I$(EXAMPLES_DIR)/$@ -pthread
	$(CC) $(CFLAGS) $(EXAMPLES_DIR)/$@/db_writer.c -o $(BUILD_DIR)/$@/db_writer $(LIBS) -I$(EXAMPLES_DIR)/$@ -pthread

# Special case for SIMD processing example
simd_processing:
	$(CC) $(SIMD_CFLAGS) $(EXAMPLES_DIR)/$@/producer.c -o $(BUILD_DIR)/$@/producer $(SIMD_LIBS) $(SIMD_INCLUDE) -I$(EXAMPLES_DIR)/$@
	$(CC) $(SIMD_CFLAGS) $(EXAMPLES_DIR)/$@/consumer.c -o $(BUILD_DIR)/$@/consumer $(SIMD_LIBS) $(SIMD_INCLUDE) -I$(EXAMPLES_DIR)/$@

# Special case for benchmark_simd_buffer
benchmark_simd_buffer: $(SHM_OBJ) directories
	mkdir -p $(BUILD_DIR)/$@
	$(CC) $(SIMD_CFLAGS) $(EXAMPLES_DIR)/$@/benchmark.c $(SHM_OBJ) -o $(BUILD_DIR)/$@/benchmark $(SIMD_LIBS) $(SHM_INCLUDE) $(SIMD_INCLUDE) -I$(EXAMPLES_DIR)/buffer_transfer -I$(EXAMPLES_DIR)/simd_processing -pthread

# Tests for SIMD vector functions
test_vector_functions: directories
	$(CC) $(SIMD_CFLAGS) $(TEST_DIR)/simd_processing/test_vector_functions.c -o $(TEST_BUILD_DIR)/test_vector_functions $(SIMD_LIBS) $(SIMD_INCLUDE) -I$(EXAMPLES_DIR)/simd_processing

# Run the tests
run_tests: test_vector_functions
	$(TEST_BUILD_DIR)/test_vector_functions

# Run the benchmark
run_benchmark: benchmark_simd_buffer
	$(BUILD_DIR)/benchmark_simd_buffer/benchmark

# Target to build all tests
tests: test_vector_functions

# Clean targets
clean:
	rm -rf $(BUILD_DIR)
	rm -f /tmp/xpc_connection_name
	rm -f $(MMAP_FILE_PATH)

clean-shm:
	-rm /dev/shm/my_shared_memory /dev/shm/sem.sem_* /dev/shm/*_shared_memory 2>/dev/null || true

.PHONY: all clean clean-shm directories $(EXAMPLES) tests run_tests test_vector_functions benchmark_simd_buffer run_benchmark