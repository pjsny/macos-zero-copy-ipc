#include "shared_memory.h"
#include "atomic_buffer_shared.h"
#include <unistd.h>
#include <time.h>

int main()
{
    printf("Starting atomic consumer (visualizer for 4 float values)\n");
    fflush(stdout);

    // Open shared memory
    shared_memory_t shm = {0};
    if (shared_memory_open(&shm, ATOMIC_BUFFER_SHM_NAME) != 0)
    {
        printf("Failed to open shared memory. Make sure producer is running first.\n");
        return 1;
    }

    printf("Shared memory opened at address %p\n", shm.addr);
    fflush(stdout);

    // Access the data structure in shared memory
    atomic_point_data_t *point_data = (atomic_point_data_t *)shm.addr;

    // Keep track of the last version we processed
    unsigned int last_version = 0;
    unsigned int frames_processed = 0;
    unsigned int updates_received = 0;

    // For calculating FPS
    time_t last_time = time(NULL);
    int fps = 0;

    printf("Connected to shared memory, ready to visualize data\n");
    printf("Press Ctrl+C to exit\n");
    printf("---------------------------------------------------------\n");
    printf("| Frame # | Updates | FPS |   Value1   Value2   Value3   Value4  |\n");
    printf("---------------------------------------------------------\n");
    fflush(stdout);

    // Visualization loop
    while (1)
    {
        // Check if there's a new version available (non-blocking)
        unsigned int current_version = atomic_load(&point_data->version);

        if (current_version != last_version)
        {
            // Memory barrier to ensure we read fresh values
            atomic_thread_fence(memory_order_acquire);

            // Read the values
            float v1 = point_data->values[0];
            float v2 = point_data->values[1];
            float v3 = point_data->values[2];
            float v4 = point_data->values[3];

            // Update our last processed version
            last_version = current_version;
            updates_received++;

            // Visualize the data (here we just print it, but in a real app you'd render it)
            printf("| %7u | %7u | %3d | %7.2f  %7.2f  %7.2f  %7.2f |\n",
                   frames_processed, updates_received, fps,
                   v1, v2, v3, v4);
            fflush(stdout);
        }

        // Update FPS counter once per second
        time_t current_time = time(NULL);
        if (current_time > last_time)
        {
            fps = frames_processed - fps;
            last_time = current_time;
        }

        frames_processed++;

        // Sleep to control visualization rate (~30 FPS)
        // In a real visualization app, this would be your render loop timing
        usleep(33333); // 33.333ms = ~30 FPS
    }

    // This code will never be reached unless interrupted
    shared_memory_destroy(&shm, 0);

    return 0;
}