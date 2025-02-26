#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <time.h>
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

int main()
{
    printf("Starting SIMD-accelerated shared memory producer\n");

    // Set up signal handler for clean shutdown
    signal(SIGINT, handle_sigint);

    // Calculate the size we need (use a huge page)
    size_t shm_size = HUGE_PAGE_SIZE;

    // Create shared memory
    shm_unlink(SIMD_SHM_NAME);
    int fd = shm_open(SIMD_SHM_NAME, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    if (fd == -1)
    {
        perror("shm_open");
        return 1;
    }

    // Set the size
    if (ftruncate(fd, shm_size) == -1)
    {
        perror("ftruncate");
        close(fd);
        shm_unlink(SIMD_SHM_NAME);
        return 1;
    }

    // Try to use huge pages for better performance (macOS doesn't support MAP_ALIGNED_SUPER)
    void *addr = mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED)
    {
        perror("mmap");
        close(fd);
        shm_unlink(SIMD_SHM_NAME);
        return 1;
    }

    printf("Shared memory mapped successfully\n");

    // Initialize shared memory
    simd_shared_t *shm = (simd_shared_t *)addr;
    atomic_init(&shm->write_index, 0);
    atomic_init(&shm->read_index, 0);
    atomic_init(&shm->total_batches_produced, 0);
    atomic_init(&shm->total_batches_consumed, 0);
    atomic_init(&shm->producer_cycles, 0);
    atomic_init(&shm->consumer_cycles, 0);
    atomic_init(&shm->shutdown_flag, false);

    // Calculate how many batches we can fit
    uint32_t max_batches = MAX_BATCHES(shm_size);
    printf("Shared memory initialized with capacity for %u batches\n", max_batches);

    // Seed random number generator
    srand(time(NULL));

    // Generate and send batches
    uint32_t batch_counter = 0;
    uint64_t total_ns = 0;
    uint64_t total_batches = 0;

    printf("Producing data batches. Press Ctrl+C to exit.\n");

    while (running)
    {
        // Check if buffer is full
        if (buffer_is_full(shm, max_batches))
        {
            // Buffer full, wait a bit
            usleep(100);
            continue;
        }

        // Get current write position
        uint64_t write_idx = atomic_load_explicit(&shm->write_index, memory_order_relaxed);
        uint32_t buffer_idx = write_idx % max_batches;

        // Start timing
        uint64_t start_time = get_time_ns();

        // Fill the batch with random data and process it with SIMD
        simd_batch_t *batch = &shm->batches[buffer_idx];
        batch->batch_id = batch_counter++;

        // Generate random data and pre-process it with SIMD
        for (int i = 0; i < 1024; i += 4)
        {
            // Generate 4 random values at once (fix float conversion)
            float32x4_t random_data = {
                ((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f,
                ((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f,
                ((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f,
                ((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f};

            // Apply SIMD processing (tanh activation)
            float32x4_t processed = vector_tanh(random_data);

            // Store the result
            vst1q_f32(&batch->data[i], processed);
        }

        batch->processed = 0; // Mark as not processed by consumer

        // Ensure all writes to the batch are visible before updating write_index
        atomic_thread_fence(memory_order_release);

        // Update write index
        atomic_store_explicit(&shm->write_index, write_idx + 1, memory_order_release);

        // Update statistics
        atomic_fetch_add_explicit(&shm->total_batches_produced, 1, memory_order_relaxed);

        // End timing
        uint64_t end_time = get_time_ns();
        uint64_t batch_ns = end_time - start_time;
        total_ns += batch_ns;
        total_batches++;

        // Update performance metrics
        atomic_store_explicit(&shm->producer_cycles, batch_ns, memory_order_relaxed);

        // Print statistics every 1000 batches
        if (batch_counter % 1000 == 0)
        {
            printf("\rProduced %u batches, Avg time: %.2f µs/batch",
                   batch_counter, (double)total_ns / total_batches / 1000.0);
            fflush(stdout);
        }

        // Don't produce too fast
        usleep(10);
    }

    printf("\nShutting down...\n");

    // Signal consumer to shut down
    atomic_store_explicit(&shm->shutdown_flag, true, memory_order_release);

    // Wait a bit for consumer to notice
    usleep(100000);

    // Clean up
    munmap(addr, shm_size);
    close(fd);
    // Don't unlink, let consumer do it

    printf("Producer completed. Processed %u batches at %.2f µs/batch\n",
           batch_counter, (double)total_ns / total_batches / 1000.0);

    return 0;
}