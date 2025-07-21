/* intrinsic-usage.c - Example of mem_shared intrinsic function usage
   Copyright (C) 2024 Free Software Foundation, Inc.

This example demonstrates how the compiler automatically replaces
intrinsic functions like memset/memcpy when used with mem_shared variables.

Compile with:
  gcc -fmem-shared -fmem-shared-cores=4 -fdump-mem-shared intrinsic-usage.c -o intrinsic-usage

For detailed replacement information:
  gcc -fmem-shared -fmem-shared-cores=4 -fdump-mem-shared -fdump-tree-mem_shared_intrinsics intrinsic-usage.c
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Various mem_shared data types for testing */

// Small arrays - single core storage
mem_shared int small_buffer[256];        // 1KB - single core
mem_shared char message[1024];           // 1KB - single core  
mem_shared float coefficients[500];      // 2KB - single core

// Large arrays - distributed storage
mem_shared int large_array[3000];        // 12KB - distributed across cores
mem_shared double matrix[100][100];      // 80KB - distributed across cores
mem_shared char big_buffer[20000];       // 20KB - distributed across cores

// Mixed size data
mem_shared short medium_array[2000];     // 4KB - single core
mem_shared long huge_array[5000];        // 40KB - distributed

void demonstrate_memset_operations(void)
{
    printf("=== memset Operations Demo ===\n");
    
    /* Small data memset - single core operation */
    printf("Setting small_buffer to 0...\n");
    memset(small_buffer, 0, sizeof(small_buffer));
    // Compiler generates: single memset call with encoded address
    
    printf("Setting message to 'A'...\n");
    memset(message, 'A', sizeof(message));
    // Compiler generates: single memset call
    
    /* Large data memset - distributed operation */
    printf("Setting large_array to 42...\n");
    memset(large_array, 42, sizeof(large_array));
    // Compiler generates: multiple memset calls, one per core
    // Example generated calls:
    // memset((void*)(0 << 21 | offset0), 42, chunk_size);
    // memset((void*)(1 << 21 | offset1), 42, chunk_size);
    // memset((void*)(2 << 21 | offset2), 42, chunk_size);
    // memset((void*)(3 << 21 | offset3), 42, chunk_size);
    
    printf("Setting matrix to 0...\n");
    memset(matrix, 0, sizeof(matrix));
    // Compiler generates: 4 memset calls for 4-core system
    
    printf("Partial memset on distributed data...\n");
    memset(big_buffer, 0xFF, 5000);  // Only first 5KB
    // Compiler calculates affected cores and generates appropriate calls
}

void demonstrate_memcpy_operations(void)
{
    printf("\n=== memcpy Operations Demo ===\n");
    
    /* Source data in regular memory */
    int source_data[3000];
    double source_matrix[100][100];
    char source_string[1024];
    
    /* Initialize source data */
    for (int i = 0; i < 3000; i++) {
        source_data[i] = i * 2;
    }
    
    for (int i = 0; i < 100; i++) {
        for (int j = 0; j < 100; j++) {
            source_matrix[i][j] = (double)(i * j) / 100.0;
        }
    }
    
    strcpy(source_string, "Hello from regular memory!");
    
    /* Copy to mem_shared variables */
    printf("Copying to small_buffer (single core)...\n");
    memcpy(small_buffer, source_data, sizeof(small_buffer));
    // Compiler generates: single memcpy call
    
    printf("Copying to large_array (distributed)...\n");
    memcpy(large_array, source_data, sizeof(large_array));
    // Compiler generates: multiple memcpy calls
    // Example generated calls:
    // memcpy((void*)(0 << 21 | offset0), source_data + 0, chunk_size);
    // memcpy((void*)(1 << 21 | offset1), source_data + chunk_size, chunk_size);
    // memcpy((void*)(2 << 21 | offset2), source_data + 2*chunk_size, chunk_size);
    // memcpy((void*)(3 << 21 | offset3), source_data + 3*chunk_size, chunk_size);
    
    printf("Copying to matrix (distributed)...\n");
    memcpy(matrix, source_matrix, sizeof(matrix));
    // Compiler generates: 4 memcpy calls for distributed data
    
    printf("Copying string to message buffer...\n");
    memcpy(message, source_string, strlen(source_string) + 1);
    // Compiler generates: single memcpy call (small data)
}

void demonstrate_string_operations(void)
{
    printf("\n=== String Operations Demo ===\n");
    
    char source_text[] = "This is a test string for mem_shared variables";
    
    /* String copy operations */
    printf("Using strcpy...\n");
    strcpy(message, source_text);
    // For small data: single strcpy call
    // For distributed data: compiler would generate multiple operations
    
    printf("Using strncpy...\n");
    strncpy(message, "Short text", 10);
    // Similar handling based on data distribution
    
    /* Note: String operations on distributed data require special handling
       because string length is not known at compile time */
}

void demonstrate_performance_considerations(void)
{
    printf("\n=== Performance Considerations Demo ===\n");
    
    /* Operations that generate warnings */
    printf("Large memset operation (will generate multiple calls)...\n");
    memset(huge_array, 0, sizeof(huge_array));
    // Compiler may warn: "mem_shared intrinsic operation will generate 4 separate calls"
    
    /* Efficient operations */
    printf("Efficient single-core operation...\n");
    memset(medium_array, 0, sizeof(medium_array));
    // Single call, no warning
    
    /* Partial operations on distributed data */
    printf("Partial operation on distributed data...\n");
    memset(large_array, 1, 1000);  // Only affects first core
    // Compiler optimizes to single call on affected core
}

void demonstrate_compiler_optimizations(void)
{
    printf("\n=== Compiler Optimizations Demo ===\n");
    
    /* Constant size optimizations */
    memset(small_buffer, 0, 256);  // Constant size
    // Compiler can optimize chunk calculations
    
    /* Variable size operations */
    size_t dynamic_size = 1500;
    memset(large_array, 0, dynamic_size);
    // Compiler generates runtime chunk calculation
    
    /* Alignment optimizations */
    memset(matrix, 0, sizeof(matrix));  // Well-aligned operation
    // Compiler can optimize for aligned transfers
}

void verify_operations(void)
{
    printf("\n=== Verification ===\n");
    
    /* Verify memset operations */
    bool small_buffer_ok = true;
    for (int i = 0; i < 256; i++) {
        if (small_buffer[i] != 0) {
            small_buffer_ok = false;
            break;
        }
    }
    printf("Small buffer memset: %s\n", small_buffer_ok ? "OK" : "FAILED");
    
    /* Verify distributed operations */
    bool large_array_ok = true;
    for (int i = 0; i < 1000; i++) {  // Check first 1000 elements
        if (large_array[i] != 1) {  // From partial memset above
            large_array_ok = false;
            break;
        }
    }
    printf("Large array partial memset: %s\n", large_array_ok ? "OK" : "FAILED");
    
    /* Verify string operations */
    printf("Message content: \"%.50s\"\n", message);
}

void show_compiler_generated_code_info(void)
{
    printf("\n=== Compiler Generated Code Information ===\n");
    printf("This information is visible with -fdump-mem-shared:\n\n");
    
    printf("For 'memset(large_array, 42, sizeof(large_array))' on 4-core system:\n");
    printf("  - Variable: large_array (12KB, distributed)\n");
    printf("  - Generated calls: 4\n");
    printf("  - Core 0: memset(0x000000, 42, 3000)  // (0<<21)|0x0000\n");
    printf("  - Core 1: memset(0x200000, 42, 3000)  // (1<<21)|0x0000\n");
    printf("  - Core 2: memset(0x400000, 42, 3000)  // (2<<21)|0x0000\n");
    printf("  - Core 3: memset(0x600000, 42, 3000)  // (3<<21)|0x0000\n\n");
    
    printf("For 'memset(small_buffer, 0, sizeof(small_buffer))' on 4-core system:\n");
    printf("  - Variable: small_buffer (1KB, single core)\n");
    printf("  - Generated calls: 1\n");
    printf("  - Core 2: memset(0x400800, 0, 1024)   // (2<<21)|0x0800\n\n");
    
    printf("Performance impact:\n");
    printf("  - Single core operations: same as regular memset/memcpy\n");
    printf("  - Distributed operations: N calls for N cores\n");
    printf("  - Compiler warns when >4 calls generated\n");
}

int main(void)
{
    printf("GCC mem_shared Intrinsic Functions Demo\n");
    printf("======================================\n");
    
    /* Run all demonstrations */
    demonstrate_memset_operations();
    demonstrate_memcpy_operations();
    demonstrate_string_operations();
    demonstrate_performance_considerations();
    demonstrate_compiler_optimizations();
    verify_operations();
    show_compiler_generated_code_info();
    
    printf("\n=== Summary ===\n");
    printf("The compiler automatically replaced intrinsic calls with:\n");
    printf("- Single calls for data stored on one core\n");
    printf("- Multiple calls for data distributed across cores\n");
    printf("- Proper address encoding for each target core\n");
    printf("- Warnings for potentially expensive operations\n");
    
    return 0;
}

/* Expected compiler output with -fdump-mem-shared:
 *
 * [mem_shared] Processing function main for intrinsic replacement
 * [mem_shared] Replaced memset call with 1 chunk operations
 * [mem_shared] Generated memset for core 0, offset 0x0000, size 1024
 * [mem_shared] Replaced memset call with 4 chunk operations
 * [mem_shared] Generated memset for core 0, offset 0x0000, size 3000
 * [mem_shared] Generated memset for core 1, offset 0x0000, size 3000
 * [mem_shared] Generated memset for core 2, offset 0x0000, size 3000
 * [mem_shared] Generated memset for core 3, offset 0x0000, size 3000
 * ...
 */