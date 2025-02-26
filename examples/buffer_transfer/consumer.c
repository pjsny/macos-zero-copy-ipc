#include "shared_memory.h"
#include "buffer_shared.h"

int main()
{
    printf("Starting consumer (point receiver)\n");
    fflush(stdout);

    // Open semaphores
    sem_t *sem_ready = sem_open(BUFFER_SEM_READY_NAME, 0);
    sem_t *sem_done = sem_open(BUFFER_SEM_DONE_NAME, 0);

    if (sem_ready == SEM_FAILED || sem_done == SEM_FAILED)
    {
        perror("sem_open");
        printf("Make sure producer is running first\n");
        return 1;
    }

    printf("Semaphores opened\n");
    fflush(stdout);

    // Open shared memory
    shared_memory_t shm = {0};
    if (shared_memory_open(&shm, BUFFER_SHM_NAME) != 0)
    {
        printf("Failed to open shared memory\n");
        return 1;
    }

    printf("Shared memory opened at address %p\n", shm.addr);
    fflush(stdout);

    // Access the data structure in shared memory
    point_data_t *point_data = (point_data_t *)shm.addr;

    printf("Connected to shared memory, signaling producer...\n");
    fflush(stdout);

    // Signal initial connection
    sem_post(sem_ready);

    // Continuously receive and print points
    printf("Ready to receive points. Press Ctrl+C to exit.\n");
    printf("-----------------------------------------\n");
    printf("| Update # |    X    |    Y    |\n");
    printf("-----------------------------------------\n");
    fflush(stdout);

    int last_update_count = -1;

    while (1)
    {
        // Wait for new data
        sem_wait(sem_done);

        // Check if we have a new update
        if (point_data->update_count != last_update_count)
        {
            // Print the received point
            printf("| %8d | %7.2f | %7.2f |\n",
                   point_data->update_count, point_data->x, point_data->y);
            fflush(stdout);

            last_update_count = point_data->update_count;
        }

        // Signal that we're ready for more data
        sem_post(sem_ready);
    }

    // This code will never be reached unless interrupted
    shared_memory_destroy(&shm, 0);
    sem_close(sem_ready);
    sem_close(sem_done);

    return 0;
}