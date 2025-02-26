#include "shared_memory.h"
#include "buffer_shared.h"
#include <time.h>

int main()
{
    printf("Starting producer (random point generator)\n");
    fflush(stdout);

    // Seed random number generator
    srand(time(NULL));

    // Remove any existing semaphores with these names
    sem_unlink(BUFFER_SEM_READY_NAME);
    sem_unlink(BUFFER_SEM_DONE_NAME);

    // Create semaphores
    sem_t *sem_ready = sem_open(BUFFER_SEM_READY_NAME, O_CREAT, S_IRUSR | S_IWUSR, 0);
    sem_t *sem_done = sem_open(BUFFER_SEM_DONE_NAME, O_CREAT, S_IRUSR | S_IWUSR, 0);

    if (sem_ready == SEM_FAILED || sem_done == SEM_FAILED)
    {
        perror("sem_open");
        return 1;
    }

    printf("Semaphores created\n");
    fflush(stdout);

    // Remove any existing shared memory with this name
    shm_unlink(BUFFER_SHM_NAME);

    // Create shared memory
    shared_memory_t shm = {0};
    if (shared_memory_create(&shm, BUFFER_SHM_NAME, SHARED_MEM_SIZE) != 0)
    {
        printf("Failed to create shared memory\n");
        return 1;
    }

    printf("Shared memory created at address %p\n", shm.addr);
    fflush(stdout);

    // Initialize the data structure in shared memory
    point_data_t *point_data = (point_data_t *)shm.addr;
    point_data->x = 0.0f;
    point_data->y = 0.0f;
    point_data->update_count = 0;

    printf("Shared memory initialized, waiting for consumer...\n");
    fflush(stdout);

    // Wait for consumer to connect
    printf("Waiting for consumer to connect...\n");
    fflush(stdout);
    sem_wait(sem_ready);

    printf("Consumer connected, beginning to generate random points\n");
    fflush(stdout);

    // Continuously generate random points
    while (1)
    {
        // Generate random X,Y values between 0-100
        point_data->x = (float)(rand() % 10000) / 100.0f; // 0.00 to 99.99
        point_data->y = (float)(rand() % 10000) / 100.0f; // 0.00 to 99.99
        point_data->update_count++;

        printf("Generated point: (%.2f, %.2f) [update #%d]\n",
               point_data->x, point_data->y, point_data->update_count);
        fflush(stdout);

        // Signal that new data is available
        sem_post(sem_done);

        // Wait for consumer to process the data
        sem_wait(sem_ready);

        // Sleep a short time to control update rate (100ms)
        usleep(100000);
    }

    // This code will never be reached unless interrupted
    shared_memory_destroy(&shm, 1);
    sem_close(sem_ready);
    sem_close(sem_done);
    sem_unlink(BUFFER_SEM_READY_NAME);
    sem_unlink(BUFFER_SEM_DONE_NAME);

    return 0;
}