#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <signal.h>
#include <mach/mach_time.h> // For high-precision timing on macOS
#include "simd_shared.h"

// Flag for clean shutdown
volatile sig_atomic_t running = 1;

// Signal handler for Ctrl+C
void handle_sigint(int sig)
{
    (void)sig; // Suppress unused parameter warning
    running = 0;
}

// Get current time in nanoseconds
uint64_t get_time_ns()
{
    static mach_timebase_info_data_t timebase_info;
    if (timebase_info.denom == 0)
    {
        mach_timebase_info(&timebase_info);
    }

    uint64_t time = mach_absolute_time();
    return time * timebase_info.numer / timebase_info.denom;
}

// Process a batch using SIMD instructions
void process_batch_simd(simd_batch_t *batch)
{
    // Apply a SIMD operation to the entire batch
    // Here we're doing a simple vector operation: square each value
    for (int i = 0; i < 1024; i += 4)
    {
        // Load 4 floats at once
        float32x4_t data = vld1q_f32(&batch->data[i]);

        // Square each value
        float32x4_t squared = vmulq_f32(data, data);

        // Store the result back
        vst1q_f32(&batch->data[i], squared);
    }

    // Mark as processed
    batch->processed = 1;
}

int main()
{
    printf("Starting SIMD-accelerated shared memory consumer\n");

    // Set up signal handler for clean shutdown
    signal(SIGINT, handle_sigint);

    // Open shared memory
    int fd = shm_open(SIMD_SHM_NAME, O_RDWR, 0);
    if (fd == -1)
    {
        perror("shm_open");
        printf("Make sure producer is running first\n");
        return 1;
    }

    // Get the size
    size_t shm_size = HUGE_PAGE_SIZE;

    // Map the shared memory (macOS doesn't support MAP_ALIGNED_SUPER)
    void *addr = mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED)
    {
        perror("mmap");
        close(fd);
        return 1;
    }

    printf("Shared memory mapped successfully\n");

    // Get shared memory pointer
    simd_shared_t *shm = (simd_shared_t *)addr;

    // Calculate how many batches we can fit
    uint32_t max_batches = MAX_BATCHES(shm_size);
    printf("Connected to shared memory with capacity for %u batches\n", max_batches);

    // Process batches
    uint32_t batch_counter = 0;
    uint64_t total_ns = 0;
    uint64_t total_batches = 0;

    printf("Consuming data batches. Press Ctrl+C to exit.\n");

    while (running)
    {
        // Check if there's data to process
        if (buffer_is_empty(shm))
        {
            // Check if producer has signaled shutdown
            if (atomic_load_explicit(&shm->shutdown_flag, memory_order_acquire))
            {
                printf("\nProducer has shut down, exiting...\n");
                break;
            }

            // No data, wait a bit
            usleep(100);
            continue;
        }

        // Get current read position
        uint64_t read_idx = atomic_load_explicit(&shm->read_index, memory_order_relaxed);
        uint32_t buffer_idx = read_idx % max_batches;

        // Start timing
        uint64_t start_time = get_time_ns();

        // Process the batch with SIMD
        simd_batch_t *batch = &shm->batches[buffer_idx];
        process_batch_simd(batch);

        // Ensure all reads from the batch are complete before updating read_index
        atomic_thread_fence(memory_order_acquire);

        // Update read index
        atomic_store_explicit(&shm->read_index, read_idx + 1, memory_order_release);

        // Update statistics
        atomic_fetch_add_explicit(&shm->total_batches_consumed, 1, memory_order_relaxed);
        batch_counter++;

        // End timing
        uint64_t end_time = get_time_ns();
        uint64_t batch_ns = end_time - start_time;
        total_ns += batch_ns;
        total_batches++;

        // Update performance metrics
        atomic_store_explicit(&shm->consumer_cycles, batch_ns, memory_order_relaxed);

        // Print statistics every 1000 batches
        if (batch_counter % 1000 == 0)
        {
            // Calculate producer-consumer ratio
            uint64_t producer_ns = atomic_load_explicit(&shm->producer_cycles, memory_order_relaxed);
            double ratio = producer_ns > 0 ? (double)batch_ns / producer_ns : 0;

            printf("\rConsumed %u batches, Avg time: %.2f µs/batch, P/C ratio: %.2f",
                   batch_counter, (double)total_ns / total_batches / 1000.0, ratio);
            fflush(stdout);
        }
    }

    printf("\nShutting down...\n");

    // Clean up
    munmap(addr, shm_size);
    close(fd);
    shm_unlink(SIMD_SHM_NAME);

    printf("Consumer completed. Processed %u batches at %.2f µs/batch\n",
           batch_counter, (double)total_ns / total_batches / 1000.0);

    return 0;
}