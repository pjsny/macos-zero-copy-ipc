#include "shared_memory.h"
#include "ring_buffer_shared.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

// Flag for clean shutdown
volatile sig_atomic_t running = 1;

// Signal handler for Ctrl+C
void handle_sigint(int sig)
{
    (void)sig; // Suppress unused parameter warning
    running = 0;
}

// Function to read data from the ring buffer without blocking
int read_from_buffer(ring_buffer_t *rb, uint8_t *data, uint32_t max_size)
{
    // Check if there's data to read
    uint32_t available = buffer_size(rb);
    if (available == 0)
    {
        return 0; // Buffer empty, can't read
    }

    // Read at most max_size bytes
    uint32_t to_read = (available < max_size) ? available : max_size;
    uint64_t read_idx = atomic_load_explicit(&rb->read_index, memory_order_relaxed);

    // Copy data from buffer
    for (uint32_t i = 0; i < to_read; i++)
    {
        data[i] = rb->data[buffer_mask(read_idx + i)];
    }

    // Ensure all reads from the buffer are complete before updating read_index
    atomic_thread_fence(memory_order_acquire);

    // Update read index
    atomic_store_explicit(&rb->read_index, read_idx + to_read, memory_order_release);

    return to_read;
}

int main()
{
    printf("Starting lock-free ring buffer consumer\n");

    // Set up signal handler for clean shutdown
    signal(SIGINT, handle_sigint);

    // Open shared memory
    shared_memory_t shm = {0};
    if (shared_memory_open(&shm, RING_BUFFER_SHM_NAME) != 0)
    {
        printf("Failed to open shared memory. Make sure producer is running first.\n");
        return 1;
    }

    // Get ring buffer from shared memory
    ring_buffer_t *rb = (ring_buffer_t *)shm.addr;

    printf("Connected to ring buffer\n");
    printf("Press Ctrl+C to exit\n");

    // Process data
    uint8_t data[128];
    uint32_t counter = 0;
    uint64_t total_bytes = 0;

    while (running)
    {
        // Try to read from buffer (non-blocking)
        int bytes_read = read_from_buffer(rb, data, sizeof(data));

        if (bytes_read > 0)
        {
            counter++;
            total_bytes += bytes_read;

            printf("\rConsumed %u packets, %llu total bytes", counter, total_bytes);
            fflush(stdout);

            // Process the data (in this example, we just print the first few bytes)
            if (counter % 100 == 0)
            {
                printf("\nSample data: ");
                for (int i = 0; i < 8 && i < bytes_read; i++)
                {
                    printf("%02X ", data[i]);
                }
                printf("\n");
            }
        }
        else
        {
            // No data available, wait a bit
            usleep(1000); // 1ms
        }
    }

    printf("\nShutting down...\n");

    // Clean up
    shared_memory_destroy(&shm, 0); // Don't unlink, let producer do it

    printf("Consumer process completed\n");

    return 0;
}