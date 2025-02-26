#include "shared_memory.h"

int main()
{
    printf("Starting shared memory writer (process2)\n");

    // Open semaphores
    sem_t *sem_ready = sem_open(SEM_READY_NAME, 0);
    sem_t *sem_done = sem_open(SEM_DONE_NAME, 0);

    if (sem_ready == SEM_FAILED || sem_done == SEM_FAILED)
    {
        perror("sem_open");
        printf("Make sure process1 is running first\n");
        return 1;
    }

    // Open shared memory
    shared_memory_t shm = {0};
    if (shared_memory_open(&shm, SHM_NAME) != 0)
    {
        printf("Failed to open shared memory\n");
        return 1;
    }

    // Access the counter structure in shared memory
    typedef struct
    {
        int counter;
    } shared_counter_t;

    shared_counter_t *counter_data = (shared_counter_t *)shm.addr;

    printf("Connected to shared memory\n");

    // Signal initial connection
    sem_post(sem_ready);

    // Countdown from 10 to 0, sending each number to process1
    printf("Starting countdown from 10 to 0:\n");
    for (int i = 10; i >= 0; i--)
    {
        printf("  Sending %d...\n", i);

        // Write the number directly to shared memory
        counter_data->counter = i;

        // Signal that a new number is available
        sem_post(sem_done);

        if (i > 0)
        {
            // Wait for process1 to process the number
            sem_wait(sem_ready);
            sleep(1);
        }
        else
        {
            // Don't wait after sending 0, as process1 will exit
            printf("Sent 0, process1 should exit\n");
        }
    }

    // Clean up our resources
    shared_memory_destroy(&shm, 0); // Don't unlink, let process1 do it
    sem_close(sem_ready);
    sem_close(sem_done);

    printf("Writer process completed\n");

    return 0;
}