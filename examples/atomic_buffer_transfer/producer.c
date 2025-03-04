#include "shared_memory.h"
#include "atomic_buffer_shared.h"
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// Function to generate a random string of variable length
void generate_random_string(char *buffer, int max_length)
{
    static const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!@#$%^&*()-_=+[]{}|;:,.<>?";
    int charset_size = sizeof(charset) - 1;

    // Generate a random length between 5 and max_length-1
    int length = 5 + rand() % (max_length - 5);

    // Generate random characters
    for (int i = 0; i < length; i++)
    {
        buffer[i] = charset[rand() % charset_size];
    }

    // Null-terminate the string
    buffer[length] = '\0';
}

int main()
{
    printf("Starting atomic producer (generates random variable-length strings)\n");
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
    atomic_string_data_t *string_data = (atomic_string_data_t *)shm.addr;

    // Initialize string to empty
    string_data->string_data[0] = '\0';

    // Initialize version counter to 0
    atomic_store(&string_data->version, 0);

    printf("Shared memory initialized, ready to generate data\n");
    printf("Press Ctrl+C to exit\n");
    fflush(stdout);

    unsigned int update_count = 0;

    // Continuously generate strings
    while (1)
    {
        // Generate a random variable-length string
        char temp_buffer[MAX_STRING_LENGTH];
        generate_random_string(temp_buffer, 32);

        // Copy the string to shared memory
        strcpy(string_data->string_data, temp_buffer);

        // Memory barrier to ensure all writes are visible before updating version
        atomic_thread_fence(memory_order_release);

        // Update version counter atomically
        atomic_fetch_add(&string_data->version, 1);

        update_count++;

        // Print status every 10 updates
        if (update_count % 10 == 0)
        {
            printf("Generated string (length %zu): \"%s\" (update #%u)\n",
                   strlen(string_data->string_data),
                   string_data->string_data,
                   update_count);
            fflush(stdout);
        }

        // Sleep to control update rate (~3 updates per second)
        usleep(333333); // 333.333ms = ~3 updates/sec
    }

    // This code will never be reached unless interrupted
    shared_memory_destroy(&shm, 1);

    return 0;
}