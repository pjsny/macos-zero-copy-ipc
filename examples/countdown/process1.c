#include "shared_memory.h"

int main()
{
    printf("Starting shared memory reader (process1)\n");

    // Create semaphores
    sem_unlink(SEM_READY_NAME);
    sem_unlink(SEM_DONE_NAME);

    sem_t *sem_ready = sem_open(SEM_READY_NAME, O_CREAT, S_IRUSR | S_IWUSR, 0);
    sem_t *sem_done = sem_open(SEM_DONE_NAME, O_CREAT, S_IRUSR | S_IWUSR, 0);

    if (sem_ready == SEM_FAILED || sem_done == SEM_FAILED)
    {
        perror("sem_open");
        return 1;
    }

    // Create shared memory
    shared_memory_t shm = {0};
    if (shared_memory_create(&shm, SHM_NAME, SHARED_MEM_SIZE) != 0)
    {
        printf("Failed to create shared memory\n");
        return 1;
    }

    // Initialize the shared memory with a counter structure
    typedef struct
    {
        int counter;
    } shared_counter_t;

    shared_counter_t *counter_data = (shared_counter_t *)shm.addr;
    counter_data->counter = -1; // Initialize to an invalid value

    printf("Shared memory created, waiting for writer...\n");

    // Wait for writer to connect
    sem_wait(sem_ready);

    printf("Connection established, processing numbers...\n");

    // Process numbers until we get a zero
    while (1)
    {
        // Wait for the signal that a new number is available
        sem_wait(sem_done);

        // Read the number directly from shared memory
        int number = counter_data->counter;
        printf("Received number: %d\n", number);

        if (number == 0)
        {
            printf("Received 0, processing complete\n");
            break;
        }

        // Signal that we're ready for the next number
        sem_post(sem_ready);
    }

    // Clean up
    printf("Cleaning up resources\n");
    shared_memory_destroy(&shm, 1);
    sem_close(sem_ready);
    sem_close(sem_done);
    sem_unlink(SEM_READY_NAME);
    sem_unlink(SEM_DONE_NAME);

    printf("Reader process completed\n");

    return 0;
}