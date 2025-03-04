#include "shared_memory.h"
#include "atomic_buffer_shared.h"
#include <time.h>
#include <math.h>
#include <unistd.h>

int main()
{
    printf("Starting atomic producer (generates 4 float values)\n");
    fflush(stdout);

    // Seed random number generator
    srand(time(NULL));

    // Create shared memory
    shared_memory_t shm = {0};
    if (shared_memory_create(&shm, ATOMIC_BUFFER_SHM_NAME, SHARED_MEM_SIZE) != 0)
    {
        printf("Failed to create shared memory\n");
        return 1;
    }

    printf("Shared memory created at address %p\n", shm.addr);
    fflush(stdout);

    // Initialize the data structure in shared memory
    atomic_point_data_t *point_data = (atomic_point_data_t *)shm.addr;

    // Initialize values
    for (int i = 0; i < 4; i++)
    {
        point_data->values[i] = 0.0f;
    }

    // Initialize version counter to 0
    atomic_store(&point_data->version, 0);

    printf("Shared memory initialized, ready to generate data\n");
    printf("Press Ctrl+C to exit\n");
    fflush(stdout);

    // Animation parameters
    float time = 0.0f;
    const float dt = 0.033f; // ~30 FPS time step
    unsigned int update_count = 0;

    // Continuously generate values
    while (1)
    {
        // Generate some interesting animated values (sine waves with different frequencies)
        point_data->values[0] = sinf(time) * 50.0f + 50.0f;         // 0-100 range
        point_data->values[1] = sinf(time * 1.3f) * 50.0f + 50.0f;  // 0-100 range
        point_data->values[2] = sinf(time * 0.7f) * 50.0f + 50.0f;  // 0-100 range
        point_data->values[3] = (sinf(time * 2.0f) + 1.0f) * 50.0f; // 0-100 range

        // Memory barrier to ensure all writes are visible before updating version
        atomic_thread_fence(memory_order_release);

        // Update version counter atomically
        atomic_fetch_add(&point_data->version, 1);

        update_count++;

        // Print status every 30 updates (approximately once per second)
        if (update_count % 30 == 0)
        {
            printf("Generated values: [%.2f, %.2f, %.2f, %.2f] (update #%u)\n",
                   point_data->values[0], point_data->values[1],
                   point_data->values[2], point_data->values[3],
                   update_count);
            fflush(stdout);
        }

        // Sleep to control update rate (~30 FPS)
        usleep(33333); // 33.333ms = ~30 FPS

        // Update time
        time += dt;
    }

    // This code will never be reached unless interrupted
    shared_memory_destroy(&shm, 1);

    return 0;
}