#ifndef BUFFER_SHARED_H
#define BUFFER_SHARED_H

#define BUFFER_SHM_NAME "/buffer_example_shared_memory"
#define BUFFER_SEM_READY_NAME "/buffer_example_sem_ready"
#define BUFFER_SEM_DONE_NAME "/buffer_example_sem_done"

typedef struct
{
    float x;
    float y;
    int update_count;
} point_data_t;

#endif