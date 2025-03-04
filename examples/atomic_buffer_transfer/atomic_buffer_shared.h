#ifndef ATOMIC_BUFFER_SHARED_H
#define ATOMIC_BUFFER_SHARED_H

#include <stdatomic.h>

#define ATOMIC_BUFFER_SHM_NAME "/atomic_buf_shared_mem"
#define MAX_STRING_LENGTH 256 // Maximum length of the string we can store

// Structure to hold a variable-length string and an atomic version counter
typedef struct
{
    char string_data[MAX_STRING_LENGTH]; // Buffer for the string
    atomic_uint version;                 // Atomic counter for tracking updates
} atomic_string_data_t;

#endif // ATOMIC_BUFFER_SHARED_H