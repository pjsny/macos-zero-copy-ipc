#ifndef RING_BUFFER_SHARED_H
#define RING_BUFFER_SHARED_H

#include <stdint.h>
#include <stdatomic.h>
#include <stdbool.h>

#define RING_BUFFER_SHM_NAME "/ring_buffer_shared_memory"
#define BUFFER_SIZE 1024 // Must be power of 2 for efficient masking

typedef struct
{
    uint8_t data[BUFFER_SIZE];
    atomic_uint_least64_t write_index; // Producer writes here
    atomic_uint_least64_t read_index;  // Consumer reads here
} ring_buffer_t;

// Helper functions for lock-free operations
static inline uint32_t buffer_mask(uint64_t val)
{
    return val & (BUFFER_SIZE - 1); // Efficient modulo for power of 2
}

static inline uint32_t buffer_size(ring_buffer_t *rb)
{
    return atomic_load_explicit(&rb->write_index, memory_order_acquire) -
           atomic_load_explicit(&rb->read_index, memory_order_acquire);
}

static inline uint32_t buffer_free_space(ring_buffer_t *rb)
{
    return BUFFER_SIZE - buffer_size(rb);
}

static inline bool buffer_is_empty(ring_buffer_t *rb)
{
    return buffer_size(rb) == 0;
}

static inline bool buffer_is_full(ring_buffer_t *rb)
{
    return buffer_size(rb) == BUFFER_SIZE;
}

#endif