/* mem-shared.h - Memory management for mem_shared variables
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

/* Maximum number of cores supported */
#define MAX_CORES 256

/* Maximum allocations per core */
#define MAX_ALLOCATIONS 1024

/* Core memory size (512KB) */
#define CORE_MEMORY_SIZE 524288

/* Data size threshold for distribution (10KB) */
#define DISTRIBUTION_THRESHOLD 10240

/* mem_shared data classification */
typedef enum {
  MEM_SHARED_BASIC_TYPE,    /* Basic data types */
  MEM_SHARED_SMALL_DATA,    /* Data < 10KB */
  MEM_SHARED_LARGE_DATA     /* Data >= 10KB */
} mem_shared_category_t;

/* Memory allocation information for mem_shared variables */
typedef struct mem_shared_info {
  tree decl;                        /* Declaration node */
  mem_shared_category_t category;   /* Data category */
  unsigned int target_core;         /* Target core number */
  unsigned int start_offset;        /* Starting offset in core memory */
  unsigned int size;               /* Total size in bytes */
  bool is_distributed;             /* Whether data is distributed */
  unsigned int num_chunks;         /* Number of distribution chunks */
  unsigned int chunk_size;         /* Size of each chunk */
  struct mem_shared_info *next;    /* Next allocation in list */
} mem_shared_info_t;

/* Per-core memory tracking */
typedef struct {
  unsigned int used_size;          /* Used memory in bytes */
  unsigned int available_size;     /* Available memory in bytes */
  mem_shared_info_t *allocations[MAX_ALLOCATIONS];
  unsigned int num_allocations;    /* Number of allocations */
} core_memory_t;

/* Global memory management state */
extern unsigned int mem_shared_num_cores;
extern core_memory_t core_memories[MAX_CORES];
extern mem_shared_info_t *mem_shared_allocations;

/* Function prototypes */

/* Initialization and cleanup */
extern void mem_shared_init (unsigned int num_cores);
extern void mem_shared_cleanup (void);

/* Memory allocation */
extern mem_shared_info_t *mem_shared_allocate (tree decl);
extern bool mem_shared_deallocate (tree decl);

/* Core management */
extern unsigned int mem_shared_get_best_core (unsigned int size);
extern unsigned int mem_shared_get_next_core (void);
extern bool mem_shared_allocate_on_core (unsigned int core, unsigned int size);

/* Distribution management */
extern void mem_shared_distribute_data (mem_shared_info_t *info, unsigned int total_size);
extern unsigned int mem_shared_calculate_target_core (tree decl, HOST_WIDE_INT offset);
extern unsigned int mem_shared_calculate_local_offset (tree decl, HOST_WIDE_INT offset);

/* Query functions */
extern mem_shared_info_t *mem_shared_get_info (tree decl);
extern bool mem_shared_is_basic_type (tree type);
extern bool mem_shared_is_distributed (tree decl);
extern unsigned int mem_shared_get_data_size (tree decl);

/* Address generation */
extern rtx mem_shared_generate_address (tree decl, HOST_WIDE_INT offset);
extern rtx mem_shared_expand_load (tree decl, HOST_WIDE_INT offset, machine_mode mode);
extern void mem_shared_expand_store (tree decl, rtx value, HOST_WIDE_INT offset);

/* Debugging and diagnostics */
extern void mem_shared_dump_allocation_info (FILE *file);
extern void mem_shared_dump_core_usage (FILE *file);
extern void mem_shared_check_conflicts (void);

/* Compiler hooks */
extern void mem_shared_process_declaration (tree decl);
extern void mem_shared_finalize_allocations (void);

/* Target-specific hooks */
extern bool mem_shared_target_supports_multicore (void);
extern unsigned int mem_shared_target_get_core_count (void);
extern rtx mem_shared_target_encode_address (unsigned int core, unsigned int offset);

/* Optimization support */
extern bool mem_shared_can_optimize_access (tree decl);
extern tree mem_shared_fold_address_expression (tree expr);

/* Error handling */
extern void mem_shared_error_core_overflow (unsigned int core, 
                                          unsigned int used, 
                                          unsigned int available);
extern void mem_shared_error_invalid_declaration (tree decl, const char *reason);

/* Inline helper functions */

static inline bool
mem_shared_decl_p (tree decl)
{
  return (TREE_CODE (decl) == VAR_DECL && DECL_MEM_SHARED_P (decl));
}

static inline unsigned int
mem_shared_align_size (unsigned int size)
{
  /* Align to 4-byte boundary */
  return (size + 3) & ~3;
}

static inline bool
mem_shared_core_has_space (unsigned int core, unsigned int size)
{
  return (core < mem_shared_num_cores && 
          core_memories[core].available_size >= size);
}

static inline unsigned int
mem_shared_get_core_used_size (unsigned int core)
{
  return (core < mem_shared_num_cores) ? core_memories[core].used_size : 0;
}

static inline unsigned int
mem_shared_get_core_available_size (unsigned int core)
{
  return (core < mem_shared_num_cores) ? core_memories[core].available_size : 0;
}

#endif /* GCC_MEM_SHARED_H */