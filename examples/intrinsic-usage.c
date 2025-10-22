/* intrinsic-usage.c - Complete example of mem_shared intrinsic function usage
   Copyright (C) 2024 Free Software Foundation, Inc.

This example demonstrates how the compiler automatically replaces
intrinsic functions like memset/memcpy when used with mem_shared variables
as both source and destination operands.

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
mem_shared int small_buffer_a[256];      // 1KB - single core
mem_shared int small_buffer_b[256];      // 1KB - single core
mem_shared char message_a[1024];         // 1KB - single core  
mem_shared char message_b[1024];         // 1KB - single core
mem_shared float coefficients[500];      // 2KB - single core

// Large arrays - distributed storage
mem_shared int large_array_a[3000];      // 12KB - distributed across cores
mem_shared int large_array_b[3000];      // 12KB - distributed across cores
mem_shared double matrix_a[100][100];    // 80KB - distributed across cores
mem_shared double matrix_b[100][100];    // 80KB - distributed across cores
mem_shared char big_buffer_a[20000];     // 20KB - distributed across cores
mem_shared char big_buffer_b[20000];     // 20KB - distributed across cores

// Mixed size data
mem_shared short medium_array[2000];     // 4KB - single core
mem_shared long huge_array[5000];        // 40KB - distributed

void demonstrate_dest_only_operations(void)
{
    printf("=== Destination-Only Operations (Original Support) ===\n");
    
    /* Small data memset - single core operation */
    printf("Setting small_buffer_a to 0...\n");
    memset(small_buffer_a, 0, sizeof(small_buffer_a));
    // Compiler generates: single memset call with encoded address
    
    /* Large data memset - distributed operation */
    printf("Setting large_array_a to 42...\n");
    memset(large_array_a, 42, sizeof(large_array_a));
    // Compiler generates: 4 memset calls, one per core
    
    printf("Setting matrix_a to 0...\n");
    memset(matrix_a, 0, sizeof(matrix_a));
    // Compiler generates: 4 memset calls for 4-core system
}

void demonstrate_src_only_operations(void)
{
    printf("\n=== Source-Only Operations (New Support) ===\n");
    
    /* Regular memory destinations */
    int regular_buffer[3000];
    double regular_matrix[100][100];
    char regular_message[1024];
    
    /* Copy from small mem_shared to regular memory */
    printf("Copying from small_buffer_a to regular memory...\n");
    memcpy(regular_buffer, small_buffer_a, sizeof(small_buffer_a));
    // Compiler generates: single memcpy call with encoded source address
    
    /* Copy from large mem_shared to regular memory */
    printf("Copying from large_array_a to regular memory...\n");
    memcpy(regular_buffer, large_array_a, sizeof(large_array_a));
    // Compiler generates: 4 memcpy calls, each reading from different core
    // memcpy(regular_buffer + 0, (void*)((0 << 21) | offset0), chunk_size);
    // memcpy(regular_buffer + chunk_size, (void*)((1 << 21) | offset1), chunk_size);
    // memcpy(regular_buffer + 2*chunk_size, (void*)((2 << 21) | offset2), chunk_size);
    // memcpy(regular_buffer + 3*chunk_size, (void*)((3 << 21) | offset3), chunk_size);
    
    /* Copy from distributed matrix */
    printf("Copying from matrix_a to regular memory...\n");
    memcpy(regular_matrix, matrix_a, sizeof(matrix_a));
    // Compiler generates: 4 memcpy calls for distributed source
    
    printf("Copying from message_a to regular memory...\n");
    memcpy(regular_message, message_a, sizeof(message_a));
    // Compiler generates: single memcpy call (small data)
}

void demonstrate_both_mem_shared_operations(void)
{
    printf("\n=== Both Source and Destination mem_shared (New Support) ===\n");
    
    /* Case 1: Both single core */
    printf("Copying between small buffers (both single core)...\n");
    memcpy(small_buffer_b, small_buffer_a, sizeof(small_buffer_a));
    // Compiler generates: single memcpy call
    // memcpy((void*)((core_b << 21) | offset_b), (void*)((core_a << 21) | offset_a), size);
    
    /* Case 2: Single to distributed */
    printf("Copying from small buffer to large array...\n");
    memcpy(large_array_b, small_buffer_a, sizeof(small_buffer_a));
    // Compiler generates: single memcpy to first chunk of distributed array
    // memcpy((void*)((0 << 21) | offset_b), (void*)((core_a << 21) | offset_a), size);
    
    /* Case 3: Distributed to single */
    printf("Copying from large array to small buffer (partial)...\n");
    memcpy(small_buffer_b, large_array_a, sizeof(small_buffer_b));
    // Compiler generates: single memcpy from first chunk of distributed array
    // memcpy((void*)((core_b << 21) | offset_b), (void*)((0 << 21) | offset_a), size);
    
    /* Case 4: Both distributed */
    printf("Copying between large arrays (both distributed)...\n");
    memcpy(large_array_b, large_array_a, sizeof(large_array_a));
    // Compiler generates: 4 memcpy calls, core-to-core
    // memcpy((void*)((0 << 21) | offset_b0), (void*)((0 << 21) | offset_a0), chunk_size);
    // memcpy((void*)((1 << 21) | offset_b1), (void*)((1 << 21) | offset_a1), chunk_size);
    // memcpy((void*)((2 << 21) | offset_b2), (void*)((2 << 21) | offset_a2), chunk_size);
    // memcpy((void*)((3 << 21) | offset_b3), (void*)((3 << 21) | offset_a3), chunk_size);
    
    /* Case 5: Matrix operations */
    printf("Copying between matrices (both distributed)...\n");
    memcpy(matrix_b, matrix_a, sizeof(matrix_a));
    // Compiler generates: 4 memcpy calls for matrix-to-matrix copy
}

void demonstrate_mixed_scenarios(void)
{
    printf("\n=== Mixed Source/Destination Scenarios ===\n");
    
    /* Initialize some test data */
    int regular_data[3000];
    for (int i = 0; i < 3000; i++) {
        regular_data[i] = i * 3;
    }
    
    /* Scenario 1: Regular -> Small mem_shared */
    printf("Regular memory to small mem_shared...\n");
    memcpy(small_buffer_a, regular_data, sizeof(small_buffer_a));
    // Single memcpy call to encoded destination
    
    /* Scenario 2: Regular -> Large mem_shared */
    printf("Regular memory to large mem_shared...\n");
    memcpy(large_array_a, regular_data, sizeof(large_array_a));
    // 4 memcpy calls, distributing regular data across cores
    
    /* Scenario 3: Small mem_shared -> Regular */
    printf("Small mem_shared to regular memory...\n");
    memcpy(regular_data, small_buffer_a, sizeof(small_buffer_a));
    // Single memcpy call from encoded source
    
    /* Scenario 4: Large mem_shared -> Regular */
    printf("Large mem_shared to regular memory...\n");
    memcpy(regular_data, large_array_a, sizeof(large_array_a));
    // 4 memcpy calls, gathering from distributed cores
    
    /* Scenario 5: Partial operations */
    printf("Partial copy operations...\n");
    memcpy(small_buffer_a, large_array_a, 512);  // Only 512 bytes
    // Compiler optimizes: likely single memcpy from first core
    
    memcpy(large_array_b, small_buffer_a, 512);  // Only 512 bytes
    // Compiler optimizes: single memcpy to first core chunk
}

void demonstrate_performance_analysis(void)
{
    printf("\n=== Performance Analysis of Different Scenarios ===\n");
    
    /* Performance impact analysis */
    printf("Performance comparison:\n");
    
    /* Best case: single core to single core */
    memcpy(small_buffer_b, small_buffer_a, sizeof(small_buffer_a));
    printf("  Single->Single: 1 call (best performance)\n");
    
    /* Good case: single to regular or regular to single */
    int temp_buffer[256];
    memcpy(temp_buffer, small_buffer_a, sizeof(small_buffer_a));
    printf("  Single->Regular: 1 call (good performance)\n");
    
    /* Moderate case: single to distributed or distributed to single */
    memcpy(large_array_a, small_buffer_a, sizeof(small_buffer_a));
    printf("  Single->Distributed: 1 call (good performance, partial fill)\n");
    
    /* Higher cost: distributed to regular or regular to distributed */
    memcpy(temp_buffer, large_array_a, 1024);  // Partial copy
    printf("  Distributed->Regular: 1-4 calls (moderate performance)\n");
    
    /* Highest cost: distributed to distributed */
    memcpy(large_array_b, large_array_a, sizeof(large_array_a));
    printf("  Distributed->Distributed: 4 calls (higher cost, but parallel)\n");
}

void demonstrate_compiler_optimizations(void)
{
    printf("\n=== Compiler Optimizations Demo ===\n");
    
    /* Optimization 1: Partial distributed access */
    printf("Optimizing partial distributed access...\n");
    memcpy(small_buffer_a, large_array_a, 100);  // Only 100 bytes
    // Compiler generates: single memcpy from first core only
    
    /* Optimization 2: Aligned transfers */
    printf("Optimizing aligned transfers...\n");
    memcpy(large_array_b, large_array_a, 12000);  // Full size, aligned
    // Compiler generates: optimal 4-way parallel transfer
    
    /* Optimization 3: Size-based decisions */
    size_t dynamic_size = 2048;  // 2KB
    memcpy(large_array_a, small_buffer_a, dynamic_size);
    // Compiler generates: runtime check, likely 1-2 calls
    
    /* Optimization 4: Zero-copy potential */
    // Note: Future optimization could detect when source and dest
    // are on same core and optimize accordingly
}

void verify_operations(void)
{
    printf("\n=== Verification of Operations ===\n");
    
    /* Verify small to small copy */
    small_buffer_a[0] = 12345;
    small_buffer_a[255] = 67890;
    memcpy(small_buffer_b, small_buffer_a, sizeof(small_buffer_a));
    
    bool small_copy_ok = (small_buffer_b[0] == 12345 && small_buffer_b[255] == 67890);
    printf("Small-to-small copy: %s\n", small_copy_ok ? "OK" : "FAILED");
    
    /* Verify distributed operations */
    large_array_a[0] = 111;
    large_array_a[2999] = 999;
    memcpy(large_array_b, large_array_a, sizeof(large_array_a));
    
    bool dist_copy_ok = (large_array_b[0] == 111 && large_array_b[2999] == 999);
    printf("Distributed-to-distributed copy: %s\n", dist_copy_ok ? "OK" : "FAILED");
    
    /* Verify mixed operations */
    int regular_test[256];
    memcpy(regular_test, small_buffer_a, sizeof(small_buffer_a));
    memcpy(small_buffer_b, regular_test, sizeof(regular_test));
    
    bool mixed_ok = (small_buffer_b[0] == small_buffer_a[0] && 
                     small_buffer_b[255] == small_buffer_a[255]);
    printf("Mixed operations: %s\n", mixed_ok ? "OK" : "FAILED");
}

void show_compiler_generated_info(void)
{
    printf("\n=== Compiler Generated Code Information ===\n");
    printf("With -fdump-mem-shared, you'll see output like:\n\n");
    
    printf("For mem_shared source operations:\n");
    printf("memcpy(regular_buffer, large_array_a, 12000):\n");
    printf("  [mem_shared] Replaced memcpy call (mem_shared to regular) with 4 chunk operations\n");
    printf("  [mem_shared] Generated memcpy source: core 0, offset 0x0000, size 3000\n");
    printf("  [mem_shared] Generated memcpy source: core 1, offset 0x0000, size 3000\n");
    printf("  [mem_shared] Generated memcpy source: core 2, offset 0x0000, size 3000\n");
    printf("  [mem_shared] Generated memcpy source: core 3, offset 0x0000, size 3000\n\n");
    
    printf("For both mem_shared operations:\n");
    printf("memcpy(large_array_b, large_array_a, 12000):\n");
    printf("  [mem_shared] Replaced memcpy call (mem_shared to mem_shared) with 4 chunk operations\n");
    printf("  [mem_shared] Generated memcpy target: core 0, offset 0x0000, source: core 0, offset 0x0000, size 3000\n");
    printf("  [mem_shared] Generated memcpy target: core 1, offset 0x0000, source: core 1, offset 0x0000, size 3000\n");
    printf("  [mem_shared] Generated memcpy target: core 2, offset 0x0000, source: core 2, offset 0x0000, size 3000\n");
    printf("  [mem_shared] Generated memcpy target: core 3, offset 0x0000, source: core 3, offset 0x0000, size 3000\n\n");
    
    printf("Generated assembly patterns:\n");
    printf("Single core source: ld instructions with (core_id << 21) | offset encoding\n");
    printf("Distributed source: Multiple ld sequences from different cores\n");
    printf("Both mem_shared: Core-to-core transfer optimization\n");
}

int main(void)
{
    printf("GCC mem_shared Complete Intrinsic Functions Demo\n");
    printf("===============================================\n");
    printf("Demonstrating source and destination support\n\n");
    
    /* Initialize test data */
    for (int i = 0; i < 256; i++) {
        small_buffer_a[i] = i;
    }
    for (int i = 0; i < 3000; i++) {
        large_array_a[i] = i * 2;
    }
    
    /* Run all demonstrations */
    demonstrate_dest_only_operations();
    demonstrate_src_only_operations();
    demonstrate_both_mem_shared_operations();
    demonstrate_mixed_scenarios();
    demonstrate_performance_analysis();
    demonstrate_compiler_optimizations();
    verify_operations();
    show_compiler_generated_info();
    
    printf("\n=== Complete Summary ===\n");
    printf("The compiler now automatically handles:\n");
    printf("✓ mem_shared variables as destinations (original)\n");
    printf("✓ mem_shared variables as sources (new)\n");
    printf("✓ Both source and destination as mem_shared (new)\n");
    printf("✓ Mixed regular and mem_shared operations\n");
    printf("✓ Optimizations for partial and aligned access\n");
    printf("✓ Performance warnings for expensive operations\n");
    printf("✓ Proper address encoding for all core combinations\n");
    
    return 0;
}

/* Expected compiler output with enhanced support:
 *
 * [mem_shared] Processing function main for intrinsic replacement
 * [mem_shared] Replaced memcpy call (regular to mem_shared) with 1 chunk operations
 * [mem_shared] Generated memcpy target: core 2, offset 0x0800, size 1024
 * [mem_shared] Replaced memcpy call (mem_shared to regular) with 1 chunk operations  
 * [mem_shared] Generated memcpy source: core 2, offset 0x0800, size 1024
 * [mem_shared] Replaced memcpy call (mem_shared to mem_shared) with 1 chunk operations
 * [mem_shared] Generated memcpy target: core 1, offset 0x1000, source: core 2, offset 0x0800, size 1024
 * [mem_shared] Replaced memcpy call (mem_shared to mem_shared) with 4 chunk operations
 * [mem_shared] Generated memcpy target: core 0, offset 0x0000, source: core 0, offset 0x0000, size 3000
 * [mem_shared] Generated memcpy target: core 1, offset 0x0000, source: core 1, offset 0x0000, size 3000
 * [mem_shared] Generated memcpy target: core 2, offset 0x0000, source: core 2, offset 0x0000, size 3000
 * [mem_shared] Generated memcpy target: core 3, offset 0x0000, source: core 3, offset 0x0000, size 3000
 * ...
 */