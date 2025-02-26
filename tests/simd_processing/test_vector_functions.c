#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include "simd_shared.h"

// Helper function to calculate scalar sigmoid for comparison
float scalar_sigmoid(float x)
{
    return 1.0f / (1.0f + expf(-x));
}

// Helper function to calculate scalar tanh for comparison
float scalar_tanh(float x)
{
    return 2.0f * scalar_sigmoid(2.0f * x) - 1.0f;
    // Alternative: return tanhf(x);
}

// Test vector_sigmoid against scalar implementation
void test_vector_sigmoid()
{
    printf("Testing vector_sigmoid...\n");

    // Create test values
    float test_values[4] = {-5.0f, -0.5f, 0.5f, 5.0f};
    float32x4_t input = vld1q_f32(test_values);

    // Apply vector sigmoid
    float32x4_t result = vector_sigmoid(input);

    // Store results
    float output[4];
    vst1q_f32(output, result);

    // Compare with scalar implementation
    const float epsilon = 1e-5f;
    for (int i = 0; i < 4; i++)
    {
        float expected = scalar_sigmoid(test_values[i]);
        printf("Input: %f, Vector: %f, Scalar: %f\n", test_values[i], output[i], expected);
        assert(fabsf(output[i] - expected) < epsilon);
    }

    printf("vector_sigmoid test passed!\n\n");
}

// Test vector_tanh against scalar implementation
void test_vector_tanh()
{
    printf("Testing vector_tanh...\n");

    // Create test values
    float test_values[4] = {-5.0f, -0.5f, 0.5f, 5.0f};
    float32x4_t input = vld1q_f32(test_values);

    // Apply vector tanh
    float32x4_t result = vector_tanh(input);

    // Store results
    float output[4];
    vst1q_f32(output, result);

    // Compare with scalar implementation
    const float epsilon = 1e-5f;
    for (int i = 0; i < 4; i++)
    {
        float expected = scalar_tanh(test_values[i]);
        float stdlib_tanh = tanhf(test_values[i]);
        printf("Input: %f, Vector: %f, Scalar: %f, stdlib: %f\n",
               test_values[i], output[i], expected, stdlib_tanh);
        assert(fabsf(output[i] - expected) < epsilon);
    }

    printf("vector_tanh test passed!\n\n");
}

// Random input test with many values
void test_with_random_inputs(int num_tests)
{
    printf("Testing with %d random inputs...\n", num_tests);

    srand(time(NULL));
    const float epsilon = 1e-5f;

    for (int test = 0; test < num_tests; test += 4)
    {
        // Generate 4 random values
        float test_values[4];
        for (int i = 0; i < 4; i++)
        {
            // Generate values between -10 and 10
            test_values[i] = ((float)rand() / (float)RAND_MAX) * 20.0f - 10.0f;
        }

        float32x4_t input = vld1q_f32(test_values);

        // Test sigmoid
        float32x4_t sigmoid_result = vector_sigmoid(input);
        float sigmoid_output[4];
        vst1q_f32(sigmoid_output, sigmoid_result);

        // Test tanh
        float32x4_t tanh_result = vector_tanh(input);
        float tanh_output[4];
        vst1q_f32(tanh_output, tanh_result);

        // Verify results
        for (int i = 0; i < 4; i++)
        {
            float expected_sigmoid = scalar_sigmoid(test_values[i]);
            float expected_tanh = scalar_tanh(test_values[i]);

            assert(fabsf(sigmoid_output[i] - expected_sigmoid) < epsilon);
            assert(fabsf(tanh_output[i] - expected_tanh) < epsilon);
        }
    }

    printf("Random input test passed!\n\n");
}

int main()
{
    printf("Running SIMD vector function unit tests\n");
    printf("=======================================\n\n");

    test_vector_sigmoid();
    test_vector_tanh();
    test_with_random_inputs(1000);

    printf("All tests passed successfully!\n");
    return 0;
}