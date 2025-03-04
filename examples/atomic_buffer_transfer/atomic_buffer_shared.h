#ifndef ATOMIC_BUFFER_SHARED_H
#define ATOMIC_BUFFER_SHARED_H

#include <stdatomic.h>

#define ATOMIC_BUFFER_SHM_NAME "/atomic_buf_shared_mem"

// Structure to hold our 4 float values and an atomic version counter
typedef struct
{
    float values[4];     // The 4 float values to be visualized
    atomic_uint version; // Atomic counter for tracking updates
} atomic_point_data_t;

#endif // ATOMIC_BUFFER_SHARED_H