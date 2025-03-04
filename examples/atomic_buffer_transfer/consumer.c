#include "shared_memory.h"
#include "atomic_buffer_shared.h"
#include <unistd.h>
#include <time.h>
#include <stdio.h>
#include <string.h>

int main()
{
    printf("Starting atomic consumer (visualizer for variable-length strings)\n");
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
    atomic_string_data_t *string_data = (atomic_string_data_t *)shm.addr;

    // Keep track of the last version we processed
    unsigned int last_version = 0;
    unsigned int frames_processed = 0;
    unsigned int updates_received = 0;

    // For calculating FPS
    time_t last_time = time(NULL);
    int fps = 0;

    printf("Connected to shared memory, ready to visualize data\n");
    printf("Press Ctrl+C to exit\n");
    printf("--------------------------------------------------------------\n");
    printf("| Frame # | Updates | FPS | String Length | String Content   |\n");
    printf("--------------------------------------------------------------\n");
    fflush(stdout);

    // Visualization loop
    while (1)
    {
        // Check if there's a new version available (non-blocking)
        unsigned int current_version = atomic_load(&string_data->version);

        if (current_version != last_version)
        {
            // Memory barrier to ensure we read fresh values
            atomic_thread_fence(memory_order_acquire);

            // Read the string
            char string_copy[MAX_STRING_LENGTH];
            strcpy(string_copy, string_data->string_data);
            size_t string_length = strlen(string_copy);

            // Update our last processed version
            last_version = current_version;
            updates_received++;

            // Display the string (truncate if too long for display)
            char display_string[41]; // 40 chars + null terminator
            if (string_length > 40)
            {
                strncpy(display_string, string_copy, 37);
                display_string[37] = '.';
                display_string[38] = '.';
                display_string[39] = '.';
                display_string[40] = '\0';
            }
            else
            {
                strcpy(display_string, string_copy);
            }

            // Visualize the data
            printf("| %7u | %7u | %3d | %13zu | %-16s |\n",
                   frames_processed, updates_received, fps,
                   string_length, display_string);
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