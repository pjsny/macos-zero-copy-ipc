#ifndef SIMD_SHARED_H
#define SIMD_SHARED_H

#include <stdint.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <arm_neon.h>
#include <arm_sve.h>
#include <math.h>

// Use 2MB huge pages for better TLB efficiency
#define HUGE_PAGE_SIZE (2 * 1024 * 1024)
#define SIMD_SHM_NAME "/simd_shared_memory"

// Align to cache line size to prevent false sharing
#define CACHE_LINE_SIZE 64
#define ALIGN_TO_CACHE __attribute__((aligned(CACHE_LINE_SIZE)))

// Structure for a batch of data to process with SIMD
typedef struct
{
    float data[1024]; // 4KB of data (1024 floats)
    uint32_t batch_id;
    uint32_t processed;
} simd_batch_t;

// Structure for our shared memory region
typedef struct
{
    // Control variables (each on its own cache line to prevent false sharing)
    atomic_uint_least64_t write_index ALIGN_TO_CACHE;
    atomic_uint_least64_t read_index ALIGN_TO_CACHE;

    // Statistics
    atomic_uint_least64_t total_batches_produced ALIGN_TO_CACHE;
    atomic_uint_least64_t total_batches_consumed ALIGN_TO_CACHE;

    // Performance metrics
    atomic_uint_least64_t producer_cycles ALIGN_TO_CACHE;
    atomic_uint_least64_t consumer_cycles ALIGN_TO_CACHE;

    // Flags
    atomic_bool shutdown_flag ALIGN_TO_CACHE;

    // Padding to ensure batches start at a cache line boundary
    uint8_t padding[CACHE_LINE_SIZE];

    // Actual data batches - must be at the end
    simd_batch_t batches[];
} simd_shared_t;

// Calculate the number of batches that fit in a given size
#define MAX_BATCHES(size) (((size) - sizeof(simd_shared_t)) / sizeof(simd_batch_t))

// Helper functions for the ring buffer
static inline uint32_t buffer_size(simd_shared_t *shm)
{
    return atomic_load_explicit(&shm->write_index, memory_order_acquire) -
           atomic_load_explicit(&shm->read_index, memory_order_acquire);
}

static inline bool buffer_is_empty(simd_shared_t *shm)
{
    return buffer_size(shm) == 0;
}

static inline bool buffer_is_full(simd_shared_t *shm, uint32_t capacity)
{
    return buffer_size(shm) >= capacity;
}

// Custom implementation of vector exp since vexpq_f32 isn't available
static inline float32x4_t custom_vexpq_f32(float32x4_t x)
{
    // Extract the 4 float values
    float values[4];
    vst1q_f32(values, x);

    // Apply exp to each value
    for (int i = 0; i < 4; i++)
    {
        values[i] = expf(values[i]);
    }

    // Load back into a vector
    return vld1q_f32(values);
}

// SIMD helper functions
// Exponential polynomial coefficients
static const float exp_tab[] = {1.0f, 1.0f, 0.5f, 0.166666667f, 0.041666667f, 0.008333333f};

static inline float32x4_t vexpq_f32(
    float32x4_t x)
{
    // Perform range reduction [-log(2),log(2)]
    int32x4_t m = vcvtq_s32_f32(vmulq_n_f32(x, 1.4426950408f));
    float32x4_t val = vfmsq_f32(x, vcvtq_f32_s32(m), vdupq_n_f32(0.6931471805f));

    // Polynomial Approximation
    // Replace with custom implementation since vtaylor_polyq_f32 isn't available
    float32x4_t z = val;
    float32x4_t result = vdupq_n_f32(exp_tab[0]);

    for (int i = 1; i < 6; i++)
    {
        result = vaddq_f32(result, vmulq_n_f32(z, exp_tab[i]));
        z = vmulq_f32(z, val);
    }

    // Reconstruct
    int32x4_t shifted = vqshlq_n_s32(m, 23);
    result = vmulq_f32(result, vreinterpretq_f32_s32(vaddq_s32(shifted, vdupq_n_s32(127 << 23))));

    // Handle underflow
    uint32x4_t mask = vcltq_s32(m, vdupq_n_s32(-126));
    return vbslq_f32(mask, vdupq_n_f32(0.0f), result);
}

static inline float32x4_t vector_sigmoid(float32x4_t x)
{
    // Vectorized sigmoid approximation: 1/(1+exp(-x))
    float32x4_t one = vdupq_n_f32(1.0f);
    float32x4_t neg_x = vnegq_f32(x);
    float32x4_t exp_neg_x = vexpq_f32(neg_x);
    return vdivq_f32(one, vaddq_f32(one, exp_neg_x));
}

static inline float32x4_t vector_tanh(float32x4_t x)
{
    // Vectorized tanh approximation using sigmoid: 2*sigmoid(2*x) - 1
    float32x4_t two = vdupq_n_f32(2.0f);
    float32x4_t one = vdupq_n_f32(1.0f);
    float32x4_t two_x = vmulq_f32(two, x);
    float32x4_t sigmoid_two_x = vector_sigmoid(two_x);
    return vsubq_f32(vmulq_f32(two, sigmoid_two_x), one);
}

#endif // SIMD_SHARED_H