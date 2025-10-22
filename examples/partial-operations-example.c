/* partial-operations-example.c - Demonstrating Partial Operations with mem_shared
   
   This example shows how the compiler handles partial memcpy/memset operations
   where the destination or source is offset within a mem_shared variable.
   
   Compile with:
     gcc -fmem-shared -fdump-mem-shared partial-operations-example.c
*/

#include <stdio.h>
#include <string.h>

/* Test variables for different scenarios */
mem_shared int large_array[1024];        // 4KB - distributed across cores
mem_shared char buffer[2048];            // 2KB - fits in single core pool  
mem_shared short matrix[16][16];         // 512 bytes - single core
mem_shared double data_table[128];       // 1KB - single core

int regular_array[256];                  // Regular memory
char temp_buffer[1024];                  // Regular memory

void demonstrate_partial_memset(void)
{
    printf("=== Partial memset Operations ===\n");
    
    /* Case 1: memset(data + offset, value, size) where size < total_size */
    printf("1. memset(large_array + 256, 0, 256 * sizeof(int));\n");
    memset(large_array + 256, 0, 256 * sizeof(int));  // 1KB starting at offset 1KB
    printf("   -> Compiler detects: offset=1024 bytes, size=1024 bytes\n");
    printf("   -> Only affects cores that contain the specified range\n\n");
    
    /* Case 2: memset with array indexing */
    printf("2. memset(&buffer[512], 0xAA, 512);\n");
    memset(&buffer[512], 0xAA, 512);  // 512 bytes starting at offset 512
    printf("   -> Compiler detects: offset=512 bytes, size=512 bytes\n");
    printf("   -> Single core operation (buffer fits in one core)\n\n");
    
    /* Case 3: memset on matrix portion */
    printf("3. memset(&matrix[8][0], 0xFF, 8 * 16 * sizeof(short));\n");
    memset(&matrix[8][0], 0xFF, 8 * 16 * sizeof(short));  // Second half of matrix
    printf("   -> Compiler detects: offset=256 bytes, size=256 bytes\n");
    printf("   -> Single core operation within bounds\n\n");
}

void demonstrate_partial_memcpy_dest(void)
{
    printf("=== Partial memcpy with mem_shared Destination ===\n");
    
    /* Initialize regular array */
    for (int i = 0; i < 256; i++) {
        regular_array[i] = i + 1000;
    }
    
    /* Case 1: Copy to middle of distributed array */
    printf("1. memcpy(large_array + 256, regular_array, 256 * sizeof(int));\n");
    memcpy(large_array + 256, regular_array, 256 * sizeof(int));
    printf("   -> Source: regular memory at %p\n", regular_array);
    printf("   -> Target: mem_shared at offset 1024 bytes, size 1024 bytes\n");
    printf("   -> Affects multiple cores, generates chunk operations\n\n");
    
    /* Case 2: Copy to single core buffer */
    printf("2. memcpy(buffer + 1024, \"Test data\", 9);\n");
    memcpy(buffer + 1024, "Test data", 9);
    printf("   -> Source: string literal\n");
    printf("   -> Target: single core at offset 1024 bytes\n");
    printf("   -> Single memcpy operation\n\n");
    
    /* Case 3: Copy to partial matrix */
    printf("3. memcpy(&matrix[4][0], regular_array, 4 * 16 * sizeof(short));\n");
    memcpy(&matrix[4][0], regular_array, 4 * 16 * sizeof(short));
    printf("   -> Target: matrix rows 4-7, offset=128 bytes, size=128 bytes\n");
    printf("   -> Single core operation\n\n");
}

void demonstrate_partial_memcpy_source(void)
{
    printf("=== Partial memcpy with mem_shared Source ===\n");
    
    /* Case 1: Copy from middle of distributed array */
    printf("1. memcpy(temp_buffer, large_array + 512, 128 * sizeof(int));\n");
    memcpy(temp_buffer, large_array + 512, 128 * sizeof(int));
    printf("   -> Source: mem_shared at offset 2048 bytes, size 512 bytes\n");
    printf("   -> Target: regular memory at %p\n", temp_buffer);
    printf("   -> Reads from multiple cores based on chunk distribution\n\n");
    
    /* Case 2: Copy from single core buffer */
    printf("2. memcpy(temp_buffer, &buffer[256], 256);\n");
    memcpy(temp_buffer, &buffer[256], 256);
    printf("   -> Source: single core at offset 256 bytes\n");
    printf("   -> Target: regular memory\n");
    printf("   -> Single memcpy operation\n\n");
    
    /* Case 3: Copy partial data_table */
    printf("3. memcpy(temp_buffer, &data_table[32], 32 * sizeof(double));\n");
    memcpy(temp_buffer, &data_table[32], 32 * sizeof(double));
    printf("   -> Source: data_table[32-63], offset=256 bytes, size=256 bytes\n");
    printf("   -> Single core operation\n\n");
}

void demonstrate_mixed_partial_memcpy(void)
{
    printf("=== Mixed Partial memcpy (both mem_shared) ===\n");
    
    /* Case 1: Copy between different mem_shared arrays */
    printf("1. memcpy(buffer + 512, &large_array[128], 256 * sizeof(int));\n");
    memcpy(buffer + 512, &large_array[128], 256 * sizeof(int));
    printf("   -> Source: large_array at offset 512 bytes (distributed)\n");
    printf("   -> Target: buffer at offset 512 bytes (single core)\n");
    printf("   -> Multiple source chunks to single target\n\n");
    
    /* Case 2: Copy between single core arrays */
    printf("2. memcpy(&matrix[0][8], &data_table[16], 8 * sizeof(double));\n");
    memcpy(&matrix[0][8], &data_table[16], 8 * sizeof(double));
    printf("   -> Source: data_table[16-23], offset=128 bytes\n");
    printf("   -> Target: matrix first row second half, offset=16 bytes\n");
    printf("   -> Single core to single core operation\n\n");
}

void demonstrate_bounds_checking(void)
{
    printf("=== Bounds Checking and Validation ===\n");
    
    printf("Compiler performs compile-time bounds checking when possible:\n\n");
    
    /* Valid operations */
    printf("Valid operations:\n");
    printf("- memset(large_array + 512, 0, 512 * sizeof(int)); // Within bounds\n");
    printf("- memcpy(buffer + 1000, temp_buffer, 1048); // Exactly fits\n");
    printf("- memset(&matrix[15][15], 0, sizeof(short)); // Last element\n\n");
    
    /* Operations that would trigger warnings */
    printf("Operations that trigger warnings:\n");
    printf("- memset(large_array + 1000, 0, 100 * sizeof(int)); // Exceeds bounds\n");
    printf("- memcpy(buffer + 2000, temp_buffer, 100); // Offset too large\n");
    printf("- memcpy(&matrix[16][0], temp_buffer, 32); // Index out of bounds\n\n");
    
    printf("Compiler output examples:\n");
    printf("warning: mem_shared operation exceeds target variable bounds:\n");
    printf("         offset 4000 + size 400 > variable size 4096\n\n");
}

void demonstrate_chunk_optimization(void)
{
    printf("=== Chunk Operation Optimization ===\n");
    
    printf("For distributed arrays, compiler optimizes chunk operations:\n\n");
    
    /* Large distributed operation */
    printf("Large operation analysis:\n");
    printf("memset(large_array + 256, 0, 512 * sizeof(int));\n");
    printf("-> Operation range: offset=1024, size=2048 bytes\n");
    printf("-> Affects cores: ~8-16 out of 108 total cores\n");
    printf("-> Skips chunks outside operation range\n");
    printf("-> Generates only necessary chunk operations\n\n");
    
    /* Small targeted operation */
    printf("Small operation analysis:\n");
    printf("memset(&large_array[100], 0xCC, 4 * sizeof(int));\n");
    printf("-> Operation range: offset=400, size=16 bytes\n");
    printf("-> Affects only 1-2 cores\n");
    printf("-> Minimal chunk operations generated\n\n");
    
    printf("Debug output with -fdump-mem-shared:\n");
    printf("[mem_shared] Partial operation detected: target offset 1024 size 2048\n");
    printf("[mem_shared] Generated 12 chunk operations (skipped 96 unnecessary)\n");
    printf("[mem_shared] Chunk optimization: reduced operations by 88.9%%\n\n");
}

int main(void)
{
    printf("Partial Operations with mem_shared Variables\n");
    printf("============================================\n\n");
    
    printf("This example demonstrates how the compiler handles:\n");
    printf("- memcpy(mem_shared_var + offset, src, size)\n");
    printf("- memcpy(dst, mem_shared_var + offset, size)\n");
    printf("- memset(mem_shared_var + offset, value, size)\n");
    printf("- Operations with array indexing: &array[index]\n");
    printf("- Bounds checking and validation\n");
    printf("- Chunk operation optimization\n\n");
    
    demonstrate_partial_memset();
    demonstrate_partial_memcpy_dest();
    demonstrate_partial_memcpy_source();
    demonstrate_mixed_partial_memcpy();
    demonstrate_bounds_checking();
    demonstrate_chunk_optimization();
    
    printf("=== Key Improvements ===\n");
    printf("✓ Supports arbitrary offsets within mem_shared variables\n");
    printf("✓ Handles operations smaller than variable size\n");
    printf("✓ Optimizes chunk operations for partial ranges\n");
    printf("✓ Provides compile-time bounds checking\n");
    printf("✓ Supports both array indexing and pointer arithmetic\n");
    printf("✓ Works with mixed distributed/non-distributed scenarios\n");
    printf("✓ Generates minimal necessary operations\n");
    
    return 0;
}

/*
Expected debug output with -fdump-mem-shared:

[mem_shared] Partial operation detected: target offset 1024 size 1024
[mem_shared] Generated memset chunk: target group 8 intra 0 offset 0x100, size 512
[mem_shared] Generated memset chunk: target group 8 intra 1 offset 0x100, size 512
[mem_shared] Skipped 104 chunks outside operation range

[mem_shared] Partial operation detected: source offset 2048 size 512
[mem_shared] Generated memcpy chunk: source group 16 intra 0 offset 0x200, size 256
[mem_shared] Generated memcpy chunk: source group 17 intra 1 offset 0x000, size 256
[mem_shared] Mixed operation: mem_shared source to regular target

[mem_shared] Bounds validation: offset 1024 + size 1024 <= variable size 4096 ✓
[mem_shared] Operation optimization: reduced from 108 to 12 chunk operations
*/