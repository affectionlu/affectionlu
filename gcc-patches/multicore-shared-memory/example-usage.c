/* example-usage.c - Example usage of mem_shared keyword
   This file demonstrates the new mem_shared keyword functionality
   for multicore shared memory programming.
   
   Compile with: gcc -fmem-shared -fmem-shared-cores=4 -fdump-mem-shared example-usage.c
*/

#include <stdio.h>

/* Basic type shared variables - automatically allocated by compiler */
mem_shared int global_counter = 0;
mem_shared float shared_coefficient = 3.14159f;
mem_shared double precision_value = 2.718281828459045;

/* Small arrays - compiler decides core placement */
mem_shared int small_buffer[256];  /* 1KB array */
mem_shared char message_buffer[4096];  /* 4KB buffer */

/* Large arrays - automatically distributed across cores */
mem_shared double large_matrix[1000][1000];  /* 8MB matrix - distributed */
mem_shared float processing_buffer[1024*1024];  /* 4MB buffer - distributed */

/* Structure with mem_shared */
typedef struct {
    int id;
    float coordinates[3];
    double timestamp;
} data_point_t;

mem_shared data_point_t shared_data_points[1000];

/* Function demonstrating basic shared variable access */
void update_global_state(int increment)
{
    /* These accesses generate special ld/st instructions with core bits */
    global_counter += increment;
    shared_coefficient *= 1.01f;
    
    printf("Global counter: %d\n", global_counter);
    printf("Shared coefficient: %f\n", shared_coefficient);
}

/* Function demonstrating array access */
void process_small_buffer(void)
{
    /* Compiler generates optimized access for single-core allocated array */
    for (int i = 0; i < 256; i++) {
        small_buffer[i] = i * i;
    }
    
    /* Message buffer access */
    sprintf(message_buffer, "Processing complete, counter = %d", global_counter);
}

/* Function demonstrating distributed large array access */
void matrix_computation(void)
{
    /* Compiler automatically calculates target core for each access */
    for (int i = 0; i < 1000; i++) {
        for (int j = 0; j < 1000; j++) {
            /* Each access may target different cores based on address */
            large_matrix[i][j] = (double)(i * j) / 1000.0;
        }
    }
}

/* Function demonstrating structure array access */
void update_data_points(void)
{
    for (int i = 0; i < 1000; i++) {
        shared_data_points[i].id = i;
        shared_data_points[i].coordinates[0] = (float)i;
        shared_data_points[i].coordinates[1] = (float)(i * 2);
        shared_data_points[i].coordinates[2] = (float)(i * 3);
        shared_data_points[i].timestamp = precision_value + i;
    }
}

/* Parallel processing simulation */
void parallel_work(int core_id)
{
    /* Each core can access shared data */
    int local_start = core_id * 250;  /* Assume 4 cores */
    int local_end = (core_id + 1) * 250;
    
    for (int i = local_start; i < local_end && i < 1000; i++) {
        for (int j = 0; j < 1000; j++) {
            /* Distributed access - compiler handles core routing */
            large_matrix[i][j] *= shared_coefficient;
        }
    }
    
    /* Update shared counter (requires synchronization in real use) */
    global_counter += local_end - local_start;
}

/* Example of different data size categories */
void demonstrate_allocation_strategies(void)
{
    /* Basic types - compiler uses round-robin or optimal placement */
    mem_shared int basic_var = 42;
    mem_shared float basic_float = 1.5f;
    
    /* Small data - compiler chooses best-fit core */
    mem_shared int small_array[1024];  /* 4KB */
    mem_shared char small_string[2048]; /* 2KB */
    
    /* Large data - automatically distributed */
    mem_shared double huge_array[100000]; /* 800KB - distributed */
    
    /* Use the variables to prevent optimization */
    basic_var += 1;
    basic_float += 0.1f;
    small_array[0] = basic_var;
    small_string[0] = 'A';
    huge_array[0] = basic_float;
    
    printf("Basic var: %d\n", basic_var);
    printf("Small array[0]: %d\n", small_array[0]);
    printf("Huge array[0]: %f\n", huge_array[0]);
}

/* Main function */
int main(void)
{
    printf("=== mem_shared Multicore Example ===\n");
    
    /* Initialize shared data */
    update_global_state(10);
    process_small_buffer();
    
    printf("Message: %s\n", message_buffer);
    
    /* Demonstrate matrix computation */
    printf("Starting matrix computation...\n");
    matrix_computation();
    printf("Matrix[500][500] = %f\n", large_matrix[500][500]);
    
    /* Update data points */
    update_data_points();
    printf("Data point 100: id=%d, coords=[%f,%f,%f], time=%f\n",
           shared_data_points[100].id,
           shared_data_points[100].coordinates[0],
           shared_data_points[100].coordinates[1], 
           shared_data_points[100].coordinates[2],
           shared_data_points[100].timestamp);
    
    /* Simulate parallel work */
    printf("Simulating parallel work...\n");
    for (int core = 0; core < 4; core++) {
        parallel_work(core);
    }
    
    printf("Final counter: %d\n", global_counter);
    
    /* Demonstrate different allocation strategies */
    demonstrate_allocation_strategies();
    
    return 0;
}

/* Expected compiler behavior:
 *
 * 1. Basic types (global_counter, shared_coefficient, precision_value):
 *    - Allocated to specific cores using round-robin or optimal placement
 *    - Generate ld/st instructions with core number in bit 21
 *
 * 2. Small arrays (small_buffer, message_buffer, shared_data_points):
 *    - Allocated to single core with best available space
 *    - Fast access with single core number
 *
 * 3. Large arrays (large_matrix, processing_buffer):
 *    - Automatically distributed across all available cores
 *    - Compiler calculates target core for each access
 *    - Uses chunk-based distribution algorithm
 *
 * 4. Memory layout example for 4 cores:
 *    - Core 0: global_counter, small_buffer, large_matrix[0-249][*]
 *    - Core 1: shared_coefficient, message_buffer, large_matrix[250-499][*]  
 *    - Core 2: precision_value, shared_data_points, large_matrix[500-749][*]
 *    - Core 3: processing_buffer, large_matrix[750-999][*]
 *
 * 5. Generated assembly will include:
 *    - Special ld/st instructions with core encoding
 *    - Automatic address calculation for distributed data
 *    - Optimized access patterns for single-core data
 */