#include "shared_memory.h"
#include "ring_buffer_shared.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

// Function to write data to the ring buffer without blocking
int write_to_buffer(ring_buffer_t *rb, const uint8_t *data, uint32_t size)
{
    // Check if there's enough space
    if (buffer_free_space(rb) < size)
    {
        return 0; // Buffer full, can't write
    }

    uint64_t write_idx = atomic_load_explicit(&rb->write_index, memory_order_relaxed);

    // Copy data to buffer
    for (uint32_t i = 0; i < size; i++)
    {
        rb->data[buffer_mask(write_idx + i)] = data[i];
    }

    // Ensure all writes to the buffer are visible before updating write_index
    atomic_thread_fence(memory_order_release);

    // Update write index
    atomic_store_explicit(&rb->write_index, write_idx + size, memory_order_release);

    return size;
}

int main()
{
    printf("Starting lock-free ring buffer producer\n");

    // Create shared memory
    shared_memory_t shm = {0};
    if (shared_memory_create(&shm, RING_BUFFER_SHM_NAME, sizeof(ring_buffer_t)) != 0)
    {
        printf("Failed to create shared memory\n");
        return 1;
    }

    // Initialize ring buffer
    ring_buffer_t *rb = (ring_buffer_t *)shm.addr;
    atomic_init(&rb->write_index, 0);
    atomic_init(&rb->read_index, 0);

    printf("Ring buffer initialized, waiting for consumer...\n");
    printf("Press Ctrl+C to exit\n");

    // Seed random number generator
    srand(time(NULL));

    // Generate and send random data
    uint32_t counter = 0;
    while (1)
    {
        // Create some test data (random bytes)
        uint8_t data[64];
        for (int i = 0; i < 64; i++)
        {
            data[i] = rand() % 256;
        }

        // Try to write to buffer (non-blocking)
        int bytes_written = write_to_buffer(rb, data, 64);

        if (bytes_written > 0)
        {
            counter++;
            printf("\rProduced %u packets of data", counter);
            fflush(stdout);
        }
        else
        {
            // Buffer is full, wait a bit
            usleep(1000); // 1ms
        }

        // Slow down a bit to make it easier to observe
        usleep(10000); // 10ms
    }

    // Clean up (this won't be reached due to infinite loop)
    shared_memory_destroy(&shm, 1);

    return 0;
}