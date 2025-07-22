/* mem-shared.h - Header file for mem_shared memory management subsystem
   Copyright (C) 2024 Free Software Foundation, Inc.

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 3, or (at your option) any later
version.

GCC is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING3.  If not see
<http://www.gnu.org/licenses/>.  */

#ifndef GCC_MEM_SHARED_H
#define GCC_MEM_SHARED_H

#include "config.h"
#include "system.h"
#include "tree.h"
#include "rtl.h"

/* Maximum number of core groups supported (54 groups) */
#define MAX_CORE_GROUPS 54

/* Cores per group (always 2) */
#define CORES_PER_GROUP 2

/* Maximum total cores (54 groups * 2 cores) */
#define MAX_TOTAL_CORES (MAX_CORE_GROUPS * CORES_PER_GROUP)

/* Default core memory size (2KB) */
#define DEFAULT_CORE_MEMORY_SIZE 2048

/* Maximum core memory size (20KB) */
#define MAX_CORE_MEMORY_SIZE 20480

/* Data size threshold for distribution (10KB) */
#define DISTRIBUTION_THRESHOLD 10240

/* Local memory pool allocation strategies */
typedef enum {
  MEM_SHARED_ALLOC_STATIC_ONLY,    /* Use only static pools */
  MEM_SHARED_ALLOC_DYNAMIC_ONLY,   /* Use only llc_malloc() */
  MEM_SHARED_ALLOC_HYBRID          /* Static pools with dynamic fallback */
} mem_shared_alloc_strategy_t;

/* Per-core memory pool descriptor */
typedef struct mem_shared_pool {
  unsigned int core_id;             /* Core ID (0-107) */
  unsigned int group_id;            /* Core group (0-53) */
  unsigned int intra_id;            /* Intra-group ID (0-1) */
  
  /* Static pool management */
  tree static_pool_decl;            /* Static pool variable declaration */
  uintptr_t static_pool_base;       /* Base address of static pool */
  size_t static_pool_size;          /* Total size of static pool */
  size_t static_used_bytes;         /* Used bytes in static pool */
  uintptr_t static_current_ptr;     /* Current allocation pointer */
  
  /* Dynamic allocation tracking */
  bool dynamic_enabled;             /* Whether dynamic allocation is enabled */
  size_t dynamic_used_bytes;        /* Total dynamic allocations */
  
  /* Pool state */
  bool initialized;                 /* Whether pool is initialized */
} mem_shared_pool_t;

/* Address encoding bit positions */
#define CROSS_CORE_ACCESS_BIT 29     /* addr[29] = 1 for cross-core access */
#define CORE_GROUP_START_BIT 21      /* addr[26:21] = core group number */
#define CORE_GROUP_END_BIT 26
#define INTRA_GROUP_ID_BIT 20        /* addr[20] = intra-group ID (0 or 1) */
#define RESERVED_BIT 27              /* addr[27] = 0 (reserved) */

/* Memory allocation categories */
typedef enum {
  MEM_SHARED_BASIC_TYPE,    /* Basic data types (int, float, double) */
  MEM_SHARED_SMALL_DATA,    /* Data < DISTRIBUTION_THRESHOLD */
  MEM_SHARED_LARGE_DATA     /* Data >= DISTRIBUTION_THRESHOLD */
} mem_shared_category_t;

/* Memory allocation information for each mem_shared variable */
typedef struct mem_shared_info {
  tree decl;                      /* Variable declaration */
  mem_shared_category_t category; /* Allocation category */
  unsigned int target_group;      /* Target core group (0-53) */
  unsigned int intra_group_id;    /* Intra-group ID (0 or 1) */
  unsigned int start_offset;      /* Start offset within core memory */
  unsigned int size;              /* Total size of the variable */
  bool is_distributed;            /* Whether data is distributed across groups */
  unsigned int num_chunks;        /* Number of chunks (for distributed data) */
  unsigned int chunk_size;        /* Size of each chunk */
  struct mem_shared_info *next;   /* Next in linked list */
} mem_shared_info_t;

/* Core group memory usage tracking */
typedef struct {
  unsigned int used_memory[CORES_PER_GROUP]; /* Memory used by each core in group */
  unsigned int free_memory[CORES_PER_GROUP]; /* Free memory in each core */
  unsigned int allocated_vars;               /* Number of allocated variables in group */
} core_group_info_t;

/* Global configuration */
extern unsigned int mem_shared_num_groups;      /* Number of participating groups */
extern unsigned int mem_shared_total_cores;     /* Total participating cores */
extern unsigned int mem_shared_core_memory_size; /* Memory size per core */
extern bool flag_mem_shared;                    /* Enable mem_shared support */
extern bool flag_dump_mem_shared;               /* Enable debug output */

/* Core group tracking */
extern core_group_info_t mem_shared_group_info[MAX_CORE_GROUPS];
extern mem_shared_info_t *mem_shared_allocation_list;

/* Local memory pool management */
extern mem_shared_pool_t mem_shared_pools[MAX_TOTAL_CORES];
extern mem_shared_alloc_strategy_t mem_shared_alloc_strategy;
extern bool mem_shared_pools_initialized;

/* Function prototypes */

/* Initialization and cleanup */
extern void mem_shared_init (unsigned int num_groups, unsigned int core_size);
extern void mem_shared_cleanup (void);

/* Memory allocation and management */
extern bool mem_shared_is_basic_type (tree type);
extern unsigned int mem_shared_get_data_size (tree decl);
extern unsigned int mem_shared_get_best_group (unsigned int size);
extern unsigned int mem_shared_get_next_group (void);
extern unsigned int mem_shared_get_best_core_in_group (unsigned int group_id, unsigned int size);
extern void mem_shared_distribute_data (mem_shared_info_t *info, unsigned int total_size);
extern mem_shared_info_t *mem_shared_allocate (tree decl);
extern mem_shared_info_t *mem_shared_get_info (tree decl);

/* Address calculation and encoding */
extern unsigned int mem_shared_calculate_target_group (tree decl, HOST_WIDE_INT offset);
extern unsigned int mem_shared_calculate_intra_group_id (tree decl, HOST_WIDE_INT offset);
extern unsigned int mem_shared_calculate_local_offset (tree decl, HOST_WIDE_INT offset);
extern rtx mem_shared_generate_address (tree decl, HOST_WIDE_INT offset);
extern rtx mem_shared_encode_cross_core_address (unsigned int group_id, 
                                                unsigned int intra_id,
                                                unsigned int local_offset);

/* RTL expansion */
extern rtx mem_shared_expand_load (tree decl, HOST_WIDE_INT offset, machine_mode mode);
extern void mem_shared_expand_store (tree decl, rtx value, HOST_WIDE_INT offset);

/* Declaration processing */
extern void mem_shared_process_declaration (tree decl);

/* Utility functions */
extern bool mem_shared_decl_p (tree decl);
extern void mem_shared_dump_allocation_info (FILE *file);
extern void mem_shared_dump_group_usage (FILE *file);

/* Local memory pool management functions */
extern void mem_shared_pools_init (void);
extern void mem_shared_pools_cleanup (void);
extern bool mem_shared_pool_allocate (unsigned int core_id, size_t size, uintptr_t *addr_out);
extern tree mem_shared_generate_static_pool (unsigned int group_id, unsigned int intra_id);
extern uintptr_t mem_shared_get_pool_base (unsigned int core_id);
extern void mem_shared_emit_static_pools (void);

/* Address encoding helper functions */
static inline bool
mem_shared_needs_cross_core_access (unsigned int source_group, unsigned int source_id,
                                   unsigned int target_group, unsigned int target_id)
{
  return (source_group != target_group || source_id != target_id);
}

static inline unsigned int
mem_shared_core_to_group (unsigned int core_id)
{
  return core_id / CORES_PER_GROUP;
}

static inline unsigned int
mem_shared_core_to_intra_id (unsigned int core_id)
{
  return core_id % CORES_PER_GROUP;
}

static inline unsigned int
mem_shared_group_and_id_to_core (unsigned int group_id, unsigned int intra_id)
{
  return group_id * CORES_PER_GROUP + intra_id;
}

/* Address bit manipulation helpers */
static inline HOST_WIDE_INT
mem_shared_set_cross_core_bit (HOST_WIDE_INT addr)
{
  return addr | (1LL << CROSS_CORE_ACCESS_BIT);
}

static inline HOST_WIDE_INT
mem_shared_clear_reserved_bit (HOST_WIDE_INT addr)
{
  return addr & ~(1LL << RESERVED_BIT);
}

static inline HOST_WIDE_INT
mem_shared_set_group_bits (HOST_WIDE_INT addr, unsigned int group_id)
{
  /* Clear existing group bits and set new ones */
  addr &= ~(((1LL << (CORE_GROUP_END_BIT - CORE_GROUP_START_BIT + 1)) - 1) << CORE_GROUP_START_BIT);
  return addr | ((HOST_WIDE_INT)group_id << CORE_GROUP_START_BIT);
}

static inline HOST_WIDE_INT
mem_shared_set_intra_id_bit (HOST_WIDE_INT addr, unsigned int intra_id)
{
  if (intra_id)
    return addr | (1LL << INTRA_GROUP_ID_BIT);
  else
    return addr & ~(1LL << INTRA_GROUP_ID_BIT);
}

/* Pool utility inline functions */
static inline unsigned int
mem_shared_core_id_from_group_intra (unsigned int group_id, unsigned int intra_id)
{
  return group_id * CORES_PER_GROUP + intra_id;
}

static inline size_t
mem_shared_align_size (size_t size)
{
  return (size + 7) & ~7;  /* 8-byte alignment */
}

static inline bool
mem_shared_core_has_pool (unsigned int core_id)
{
  return (core_id < MAX_TOTAL_CORES && 
          mem_shared_pools_initialized &&
          mem_shared_pools[core_id].initialized);
}

#endif /* GCC_MEM_SHARED_H */