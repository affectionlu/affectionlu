/* basic-usage.c - Basic usage examples for mem_shared keyword
   Copyright (C) 2024 Free Software Foundation, Inc.

This example demonstrates the basic usage of the mem_shared keyword
for declaring shared memory variables in a multi-core system.

Compile with:
  gcc -fmem-shared -fmem-shared-cores=4 basic-usage.c -o basic-usage

For debugging information:
  gcc -fmem-shared -fmem-shared-cores=4 -fdump-mem-shared basic-usage.c
*/

#include <stdio.h>
#include <string.h>

// Basic type shared variables - compiler decides placement
mem_shared int global_counter = 0;
mem_shared float coefficient = 3.14159f;
mem_shared double precision_value = 2.718281828;
mem_shared char status_flag = 'A';

// Small arrays - stored on single core
mem_shared int small_buffer[256];          // 1KB - single core storage
mem_shared float coordinate_array[500];    // 2KB - single core storage
mem_shared char message_buffer[1024];      // 1KB - single core storage

// Large arrays - distributed across cores
mem_shared int large_array[3000];          // 12KB - distributed storage
mem_shared double matrix[100][100];        // 80KB - distributed storage
mem_shared float signal_data[5000];        // 20KB - distributed storage

// Structure definitions
typedef struct {
    int x, y, z;
    float velocity;
    char name[16];
} particle_t;

// Shared structures
mem_shared particle_t particle_data[100];  // ~3KB - single core storage

void basic_operations_demo(void)
{
    printf("=== Basic Operations Demo ===\n");
    
    // Basic variable access
    global_counter = 42;
    coefficient = 2.5f;
    status_flag = 'B';
    
    printf("Global counter: %d\n", global_counter);
    printf("Coefficient: %.2f\n", coefficient);
    printf("Status flag: %c\n", status_flag);
    
    // Increment operations
    global_counter++;
    coefficient *= 1.5f;
    precision_value += 1.0;
    
    printf("After operations:\n");
    printf("Global counter: %d\n", global_counter);
    printf("Coefficient: %.2f\n", coefficient);
    printf("Precision value: %.6f\n", precision_value);
}

void array_operations_demo(void)
{
    printf("\n=== Array Operations Demo ===\n");
    
    // Initialize small buffer
    for (int i = 0; i < 256; i++) {
        small_buffer[i] = i * 2;
    }
    
    // Initialize coordinate array
    for (int i = 0; i < 500; i++) {
        coordinate_array[i] = (float)i / 10.0f;
    }
    
    // String operations on message buffer
    strcpy(message_buffer, "Hello from shared memory!");
    printf("Message: %s\n", message_buffer);
    
    // Display some array values
    printf("Small buffer[10]: %d\n", small_buffer[10]);
    printf("Coordinate array[50]: %.2f\n", coordinate_array[50]);
}

void large_data_demo(void)
{
    printf("\n=== Large Data Operations Demo ===\n");
    
    // Initialize large array (distributed across cores)
    printf("Initializing large array (distributed)...\n");
    for (int i = 0; i < 3000; i++) {
        large_array[i] = i * i;
    }
    
    // Initialize matrix (distributed across cores)
    printf("Initializing matrix (distributed)...\n");
    for (int i = 0; i < 100; i++) {
        for (int j = 0; j < 100; j++) {
            matrix[i][j] = (double)(i * j) / 100.0;
        }
    }
    
    // Initialize signal data
    printf("Initializing signal data (distributed)...\n");
    for (int i = 0; i < 5000; i++) {
        signal_data[i] = sinf((float)i / 100.0f);
    }
    
    // Display some values
    printf("Large array[1000]: %d\n", large_array[1000]);
    printf("Matrix[50][50]: %.4f\n", matrix[50][50]);
    printf("Signal data[1000]: %.4f\n", signal_data[1000]);
}

void structure_operations_demo(void)
{
    printf("\n=== Structure Operations Demo ===\n");
    
    // Initialize particle data
    for (int i = 0; i < 100; i++) {
        particle_data[i].x = i;
        particle_data[i].y = i * 2;
        particle_data[i].z = i * 3;
        particle_data[i].velocity = (float)i / 10.0f;
        snprintf(particle_data[i].name, sizeof(particle_data[i].name), 
                "P%03d", i);
    }
    
    // Display some particle data
    printf("Particle 0: (%d, %d, %d) vel=%.2f name=%s\n",
           particle_data[0].x, particle_data[0].y, particle_data[0].z,
           particle_data[0].velocity, particle_data[0].name);
    
    printf("Particle 50: (%d, %d, %d) vel=%.2f name=%s\n",
           particle_data[50].x, particle_data[50].y, particle_data[50].z,
           particle_data[50].velocity, particle_data[50].name);
}

void pointer_operations_demo(void)
{
    printf("\n=== Pointer Operations Demo ===\n");
    
    // Pointers to shared memory variables
    mem_shared int *ptr_counter = &global_counter;
    mem_shared float *ptr_coeff = &coefficient;
    mem_shared int *ptr_array = small_buffer;
    
    // Access through pointers
    *ptr_counter = 100;
    *ptr_coeff = 1.414f;
    ptr_array[10] = 999;
    
    printf("Via pointer - counter: %d\n", *ptr_counter);
    printf("Via pointer - coefficient: %.3f\n", *ptr_coeff);
    printf("Via pointer - array[10]: %d\n", ptr_array[10]);
    
    // Pointer arithmetic
    ptr_array += 20;
    *ptr_array = 777;
    printf("Via pointer arithmetic - array[20]: %d\n", small_buffer[20]);
}

void performance_test_demo(void)
{
    printf("\n=== Performance Test Demo ===\n");
    
    // Measure access time for different data types
    volatile int dummy = 0;
    
    printf("Performing access tests...\n");
    
    // Basic type access (should be very fast)
    for (int i = 0; i < 1000000; i++) {
        dummy += global_counter;
        global_counter = dummy % 1000;
    }
    
    // Small array access (single core)
    for (int i = 0; i < 100000; i++) {
        dummy += small_buffer[i % 256];
    }
    
    // Large array access (distributed)
    for (int i = 0; i < 50000; i++) {
        dummy += large_array[i % 3000];
    }
    
    printf("Performance test completed (dummy = %d)\n", dummy);
}

int main(void)
{
    printf("GCC mem_shared Basic Usage Demo\n");
    printf("===============================\n");
    
    // Run all demonstration functions
    basic_operations_demo();
    array_operations_demo();
    large_data_demo();
    structure_operations_demo();
    pointer_operations_demo();
    performance_test_demo();
    
    printf("\n=== Summary ===\n");
    printf("Demonstrated mem_shared features:\n");
    printf("- Basic type variables (compiler-assigned cores)\n");
    printf("- Small arrays (<10KB, single core storage)\n");
    printf("- Large arrays (>=10KB, distributed storage)\n");
    printf("- Structure arrays\n");
    printf("- Pointer operations\n");
    printf("- Performance characteristics\n");
    
    return 0;
}