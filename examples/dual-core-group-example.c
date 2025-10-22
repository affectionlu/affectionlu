/* dual-core-group-example.c - Example demonstrating dual-core group architecture
   Copyright (C) 2024 Free Software Foundation, Inc.

This example demonstrates the new dual-core group architecture where:
- 54 core groups, each containing 2 cores
- Total of 108 cores
- Cross-core access encoding with addr[29]=1, addr[27]=0, addr[26:21]=group_id, addr[20]=intra_id
- Configurable core memory size (default 2KB, max 20KB)

Compile with:
  gcc -fmem-shared dual-core-group-example.c -o dual-core-group-example

With custom configuration:
  gcc -fmem-shared -fmem-shared-core-num=54 -fmem-shared-core-size=4096 dual-core-group-example.c

Debug output:
  gcc -fmem-shared -fdump-mem-shared dual-core-group-example.c
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Different types of mem_shared variables to demonstrate allocation strategies */

// Basic types - round-robin allocation across groups
mem_shared int counter = 0;                    // Group 0, intra-id determined by best fit
mem_shared float coefficient = 3.14f;          // Group 1, intra-id determined by best fit
mem_shared double precision = 2.718281828;     // Group 2, intra-id determined by best fit
mem_shared char flag = 'A';                    // Group 3, intra-id determined by best fit

// Small data (< 10KB) - best-fit allocation
mem_shared int small_buffer[512];              // 2KB - best fit to available group/core
mem_shared short lookup_table[1024];           // 2KB - best fit allocation  
mem_shared char message[3000];                 // 3KB - best fit allocation
mem_shared float matrix_2d[32][32];            // 4KB - best fit allocation

// Large data (>= 10KB) - distributed across all participating groups
mem_shared double large_matrix[64][64];        // 32KB - distributed across groups
mem_shared int huge_array[8192];               // 32KB - distributed across groups
mem_shared char big_buffer[50000];             // 50KB - distributed across groups

void demonstrate_basic_types(void)
{
    printf("=== Basic Types Allocation Demo ===\n");
    printf("Basic types use round-robin allocation across core groups.\n");
    printf("Each variable gets assigned to the next group in sequence.\n\n");
    
    /* Basic operations - each access generates cross-core address encoding */
    counter = 42;
    coefficient = 3.14159f;
    precision = 2.718281828459;
    flag = 'X';
    
    printf("counter = %d\n", counter);
    printf("coefficient = %.6f\n", coefficient);
    printf("precision = %.12f\n", precision);
    printf("flag = '%c'\n", flag);
    
    printf("\nAddress encoding for cross-core access:\n");
    printf("- addr[29] = 1 (cross-core access bit)\n");
    printf("- addr[27] = 0 (reserved bit)\n");
    printf("- addr[26:21] = group_id (0-53)\n");
    printf("- addr[20] = intra_group_id (0 or 1)\n");
    printf("- addr[19:0] = local_offset within core\n\n");
}

void demonstrate_small_data(void)
{
    printf("=== Small Data Allocation Demo ===\n");
    printf("Small data (< 10KB) uses best-fit allocation.\n");
    printf("Compiler finds the group/core with best available space.\n\n");
    
    /* Initialize arrays */
    for (int i = 0; i < 512; i++) {
        small_buffer[i] = i * 2;
    }
    
    for (int i = 0; i < 1024; i++) {
        lookup_table[i] = (short)(i % 256);
    }
    
    strcpy(message, "Hello from mem_shared small data allocation!");
    
    /* Initialize 2D matrix */
    for (int i = 0; i < 32; i++) {
        for (int j = 0; j < 32; j++) {
            matrix_2d[i][j] = (float)(i * j) / 10.0f;
        }
    }
    
    printf("small_buffer[100] = %d\n", small_buffer[100]);
    printf("lookup_table[500] = %d\n", lookup_table[500]);
    printf("message = \"%.40s...\"\n", message);
    printf("matrix_2d[10][15] = %.2f\n", matrix_2d[10][15]);
    
    printf("\nBest-fit algorithm selects group/core with:\n");
    printf("- Sufficient free space for the allocation\n");
    printf("- Minimal waste (closest fit to required size)\n");
    printf("- Balance between both cores in each group\n\n");
}

void demonstrate_large_data_distribution(void)
{
    printf("=== Large Data Distribution Demo ===\n");
    printf("Large data (>= 10KB) is distributed across all participating cores.\n");
    printf("Each core gets approximately equal chunks of the data.\n\n");
    
    /* Initialize distributed data */
    
    // Initialize large matrix (32KB distributed across 108 cores)
    for (int i = 0; i < 64; i++) {
        for (int j = 0; j < 64; j++) {
            large_matrix[i][j] = (double)(i + j) / 100.0;
        }
    }
    
    // Initialize huge array (32KB distributed across 108 cores)
    for (int i = 0; i < 8192; i++) {
        huge_array[i] = i * i;
    }
    
    // Initialize big buffer (50KB distributed across 108 cores)
    memset(big_buffer, 0xAB, sizeof(big_buffer));
    
    printf("large_matrix[30][40] = %.6f\n", large_matrix[30][40]);
    printf("huge_array[1000] = %d\n", huge_array[1000]);
    printf("big_buffer[25000] = 0x%02X\n", (unsigned char)big_buffer[25000]);
    
    printf("\nDistribution characteristics:\n");
    printf("- Data split into chunks across all %u participating cores\n", 108);
    printf("- Each core stores approximately: size / num_cores bytes\n");
    printf("- Access to element calculates: target_core = (offset / chunk_size) %% num_cores\n");
    printf("- Target group = target_core / 2, intra_id = target_core %% 2\n\n");
}

void demonstrate_intrinsic_operations(void)
{
    printf("=== Intrinsic Function Operations Demo ===\n");
    printf("memset/memcpy operations are automatically chunked for distributed data.\n\n");
    
    int regular_buffer[8192];
    
    /* memset on distributed data */
    printf("Executing: memset(huge_array, 0, sizeof(huge_array));\n");
    memset(huge_array, 0, sizeof(huge_array));
    printf("Compiler generates ~108 memset calls (one per participating core)\n\n");
    
    /* memcpy from regular to distributed */
    for (int i = 0; i < 8192; i++) {
        regular_buffer[i] = i + 1000;
    }
    
    printf("Executing: memcpy(huge_array, regular_buffer, sizeof(huge_array));\n");
    memcpy(huge_array, regular_buffer, sizeof(huge_array));
    printf("Compiler generates ~108 memcpy calls to distribute regular_buffer\n\n");
    
    /* memcpy from distributed to regular */
    printf("Executing: memcpy(regular_buffer, huge_array, sizeof(regular_buffer));\n");
    memcpy(regular_buffer, huge_array, sizeof(regular_buffer));
    printf("Compiler generates ~108 memcpy calls to gather from distributed data\n\n");
    
    /* Verify operation */
    printf("Verification: huge_array[100] = %d (should be 1100)\n", huge_array[100]);
    printf("Verification: regular_buffer[100] = %d (should be 1100)\n", regular_buffer[100]);
}

void demonstrate_address_encoding(void)
{
    printf("=== Address Encoding Demonstration ===\n");
    printf("Cross-core access addresses are encoded as follows:\n\n");
    
    printf("Bit Layout:\n");
    printf("31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0\n");
    printf(" X  X  C  X  R  G  G  G  G  G  G  I  O  O  O  O  O  O  O  O  O  O  O  O  O  O  O  O  O  O  O  O\n");
    printf("       |     |  |<--- 6 bits --->|  |<---------- 20 bits local offset ----------->|\n");
    printf("       |     |       Group ID     |                   Local Offset\n");
    printf("       |     |                    |\n");
    printf("       |     |                    +-- Intra-group ID (0 or 1)\n");
    printf("       |     +-- Reserved (always 0)\n");
    printf("       +-- Cross-core access (1 for cross-core)\n\n");
    
    printf("Examples:\n");
    printf("Access to group 0, core 0, offset 0x100:\n");
    printf("  Encoded address: 0x%08X\n", (1 << 29) | (0 << 21) | (0 << 20) | 0x100);
    printf("Access to group 25, core 1, offset 0x800:\n");
    printf("  Encoded address: 0x%08X\n", (1 << 29) | (25 << 21) | (1 << 20) | 0x800);
    printf("Access to group 53, core 0, offset 0x1FF:\n");
    printf("  Encoded address: 0x%08X\n", (1 << 29) | (53 << 21) | (0 << 20) | 0x1FF);
    
    printf("\nGroup ID encoding allows for 64 possible groups (0-63)\n");
    printf("Current system uses 54 groups (0-53)\n");
    printf("Intra-group ID allows for 2 cores per group (0-1)\n\n");
}

void demonstrate_memory_configuration(void)
{
    printf("=== Memory Configuration Demo ===\n");
    printf("Default configuration:\n");
    printf("- 54 core groups (108 total cores)\n");
    printf("- 2KB memory per core\n");
    printf("- Total shared memory: 216KB\n\n");
    
    printf("Alternative configurations via compiler options:\n");
    printf("-fmem-shared-core-num=N     : Use N cores (max 108)\n");
    printf("-fmem-shared-core-size=S    : Use S bytes per core (max 20KB)\n");
    printf("-fdump-mem-shared           : Enable allocation debug output\n\n");
    
    printf("Examples:\n");
    printf("gcc -fmem-shared -fmem-shared-core-num=72 program.c\n");
    printf("  Uses 36 groups (72 cores) with default 2KB per core\n\n");
    printf("gcc -fmem-shared -fmem-shared-core-size=8192 program.c\n");
    printf("  Uses all 54 groups with 8KB per core (864KB total)\n\n");
    printf("gcc -fmem-shared -fmem-shared-core-num=54 -fmem-shared-core-size=4096 program.c\n");
    printf("  Uses 27 groups (54 cores) with 4KB per core (216KB total)\n\n");
}

void demonstrate_performance_characteristics(void)
{
    printf("=== Performance Characteristics ===\n");
    printf("Access patterns and their performance implications:\n\n");
    
    printf("1. Local core access (same core):\n");
    printf("   - No address encoding needed\n");
    printf("   - Direct memory access\n");
    printf("   - Best performance\n\n");
    
    printf("2. Cross-core access (different core):\n");
    printf("   - Address encoding with group_id and intra_id\n");
    printf("   - Hardware handles routing\n");
    printf("   - Slightly higher latency\n\n");
    
    printf("3. Distributed data access:\n");
    printf("   - Multiple chunks across different groups\n");
    printf("   - Intrinsic functions generate multiple operations\n");
    printf("   - Can benefit from parallel execution\n\n");
    
    printf("4. Intrinsic function performance:\n");
    printf("   - Single group data: 1 function call\n");
    printf("   - Distributed data: ~54 function calls\n");
    printf("   - Compiler warns when >4 calls generated\n\n");
}

int main(void)
{
    printf("Dual-Core Group Architecture Demonstration\n");
    printf("==========================================\n");
    printf("System: 54 groups × 2 cores = 108 total cores\n");
    printf("Memory: 2KB per core (configurable up to 20KB)\n");
    printf("Address encoding: 30-bit with group and intra-group fields\n\n");
    
    demonstrate_basic_types();
    demonstrate_small_data();
    demonstrate_large_data_distribution();
    demonstrate_intrinsic_operations();
    demonstrate_address_encoding();
    demonstrate_memory_configuration();
    demonstrate_performance_characteristics();
    
    printf("=== Summary ===\n");
    printf("The dual-core group architecture provides:\n");
    printf("✓ Scalable design with 54 configurable groups\n");
    printf("✓ Efficient address encoding for cross-core access\n");
    printf("✓ Intelligent allocation strategies for different data types\n");
    printf("✓ Automatic intrinsic function chunking for distributed data\n");
    printf("✓ Configurable memory sizes per core\n");
    printf("✓ Transparent cross-core memory access\n");
    printf("✓ Optimal performance for local and distributed access patterns\n\n");
    
    printf("Use -fdump-mem-shared to see detailed allocation information!\n");
    
    return 0;
}

/*
Expected compiler output with -fdump-mem-shared:

[mem_shared] Initialized: 54 groups (108 cores), 2048 bytes per core
[mem_shared] Allocated 4 bytes for 'counter' in group 0 core 0 at offset 0x0
[mem_shared] Allocated 4 bytes for 'coefficient' in group 1 core 0 at offset 0x0  
[mem_shared] Allocated 8 bytes for 'precision' in group 2 core 0 at offset 0x0
[mem_shared] Allocated 1 bytes for 'flag' in group 3 core 0 at offset 0x0
[mem_shared] Allocated 2048 bytes for 'small_buffer' in group 5 core 0 at offset 0x11
[mem_shared] Allocated 2048 bytes for 'lookup_table' in group 6 core 1 at offset 0x0
[mem_shared] Allocated 3000 bytes for 'message' in group 7 core 0 at offset 0x829
[mem_shared] Allocated 4096 bytes for 'matrix_2d' in group 8 core 1 at offset 0x800
[mem_shared] Distributed 32768 bytes across 108 cores (54 groups), 303 bytes per chunk
[mem_shared] Distributed 32768 bytes across 108 cores (54 groups), 303 bytes per chunk  
[mem_shared] Distributed 50000 bytes across 108 cores (54 groups), 463 bytes per chunk

Load/Store operations:
[mem_shared] Load from 'counter' group 0 intra 0 offset 0x0
[mem_shared] Store to 'coefficient' group 1 intra 0 offset 0x0
[mem_shared] Load from 'large_matrix' group 15 intra 1 offset 0x12F
[mem_shared] Store to 'huge_array' group 20 intra 0 offset 0x12F

Intrinsic operations:
[mem_shared] Replaced memset call (regular to mem_shared) with 108 chunk operations
[mem_shared] Generated memset chunk: target group 0 intra 0 offset 0x0, size 303
[mem_shared] Generated memset chunk: target group 0 intra 1 offset 0x0, size 303
[mem_shared] Generated memset chunk: target group 1 intra 0 offset 0x0, size 303
[mem_shared] Generated memset chunk: target group 1 intra 1 offset 0x0, size 303
...
[mem_shared] Generated memset chunk: target group 53 intra 0 offset 0x0, size 303
[mem_shared] Generated memset chunk: target group 53 intra 1 offset 0x0, size 303
*/