# Lock-Free Ring Buffer Example

This example demonstrates a lock-free ring buffer implementation using shared memory for high-performance inter-process communication on macOS.

## Overview

The ring buffer is a circular data structure that allows a producer process to write data and a consumer process to read data without requiring locks or mutexes. This implementation uses atomic operations to ensure thread safety and data consistency.

## Key Features

- **Lock-free design**: Uses atomic operations instead of mutexes for better performance
- **Zero-copy**: Data is shared directly through memory mapping without copying
- **Non-blocking I/O**: Both producer and consumer operate without blocking
- **Power-of-2 sized buffer**: Enables efficient masking for index calculations

## How It Works

1. The producer creates a shared memory region and initializes the ring buffer
2. The consumer connects to the same shared memory region
3. The producer writes random data to the buffer
4. The consumer reads and processes the data
5. Both processes use atomic operations to update read and write indices

## Memory Ordering

The implementation uses specific memory ordering constraints to ensure correct behavior:

- `memory_order_relaxed` for local operations
- `memory_order_acquire` for reading shared data
- `memory_order_release` for publishing updates to shared data

## Running the Example

1. Build the example:

   ```bash
   make ring_buffer
   ```

2. Start the producer in one terminal:

   ```bash
   ./build/ring_buffer/producer
   ```

3. Start the consumer in another terminal:

   ```bash
   ./build/ring_buffer/consumer
   ```

4. Press Ctrl+C in either terminal to exit

## Files

- `ring_buffer_shared.h`: Shared definitions and helper functions
- `producer.c`: Generates random data and writes to the buffer
- `consumer.c`: Reads and processes data from the buffer

## Performance Considerations

This implementation is optimized for throughput rather than latency. The buffer size (1024 bytes) can be adjusted in `ring_buffer_shared.h` to suit different workloads.
