/* mem-shared.c - Implementation of mem_shared memory management
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

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tree.h"
#include "rtl.h"
#include "expr.h"
#include "stor-layout.h"
#include "tree-iterator.h"
#include "gimplify.h"
#include "diagnostic.h"
#include "fold-const.h"
#include "langhooks.h"
#include "mem-shared.h"

/* Global state for mem_shared memory management */
unsigned int mem_shared_num_groups = MAX_CORE_GROUPS;  /* Default: all 54 groups */
unsigned int mem_shared_total_cores = MAX_TOTAL_CORES; /* Default: 108 cores */
unsigned int mem_shared_core_memory_size = DEFAULT_CORE_MEMORY_SIZE; /* Default: 2KB per core */

/* Core group tracking */
core_group_info_t mem_shared_group_info[MAX_CORE_GROUPS];
mem_shared_info_t *mem_shared_allocation_list = NULL;

/* Local memory pool management */
mem_shared_pool_t mem_shared_pools[MAX_TOTAL_CORES];
mem_shared_alloc_strategy_t mem_shared_alloc_strategy = MEM_SHARED_ALLOC_HYBRID;
bool mem_shared_pools_initialized = false;

/* Round-robin allocation state */
static unsigned int next_group_allocation = 0;

/* Initialize mem_shared subsystem */
void
mem_shared_init (unsigned int num_groups, unsigned int core_size)
{
  unsigned int i, j;
  
  /* Validate and set parameters */
  if (num_groups > MAX_CORE_GROUPS)
    {
      warning (0, "mem_shared: requested %u groups exceeds maximum %u, using %u",
              num_groups, MAX_CORE_GROUPS, MAX_CORE_GROUPS);
      num_groups = MAX_CORE_GROUPS;
    }
    
  if (core_size > MAX_CORE_MEMORY_SIZE)
    {
      warning (0, "mem_shared: requested core size %u exceeds maximum %u, using %u",
              core_size, MAX_CORE_MEMORY_SIZE, MAX_CORE_MEMORY_SIZE);
      core_size = MAX_CORE_MEMORY_SIZE;
    }
    
  mem_shared_num_groups = num_groups;
  mem_shared_total_cores = num_groups * CORES_PER_GROUP;
  mem_shared_core_memory_size = core_size;
  
  /* Initialize group information */
  for (i = 0; i < MAX_CORE_GROUPS; i++)
    {
      for (j = 0; j < CORES_PER_GROUP; j++)
        {
          mem_shared_group_info[i].used_memory[j] = 0;
          mem_shared_group_info[i].free_memory[j] = 
            (i < mem_shared_num_groups) ? mem_shared_core_memory_size : 0;
        }
      mem_shared_group_info[i].allocated_vars = 0;
    }
    
  /* Reset allocation state */
  mem_shared_allocation_list = NULL;
  next_group_allocation = 0;
  
  /* Initialize memory pools */
  mem_shared_pools_init ();
  
  if (flag_dump_mem_shared)
    fprintf (stderr, "[mem_shared] Initialized: %u groups (%u cores), %u bytes per core\n",
            mem_shared_num_groups, mem_shared_total_cores, mem_shared_core_memory_size);
}

/* Cleanup mem_shared subsystem */
void
mem_shared_cleanup (void)
{
  mem_shared_info_t *current, *next;
  
  /* Free allocation list */
  for (current = mem_shared_allocation_list; current; current = next)
    {
      next = current->next;
      free (current);
    }
    
  mem_shared_allocation_list = NULL;
  
  /* Cleanup memory pools */
  mem_shared_pools_cleanup ();
  
  if (flag_dump_mem_shared)
    fprintf (stderr, "[mem_shared] Cleanup completed\n");
}

/* Check if a type is a basic type */
bool
mem_shared_is_basic_type (tree type)
{
  if (!type)
    return false;
    
  switch (TREE_CODE (type))
    {
    case INTEGER_TYPE:
    case REAL_TYPE:
    case ENUMERAL_TYPE:
    case BOOLEAN_TYPE:
    case POINTER_TYPE:
      return true;
    default:
      return false;
    }
}

/* Calculate data size for a declaration */
unsigned int
mem_shared_get_data_size (tree decl)
{
  tree type, size_tree;
  HOST_WIDE_INT size;
  
  if (!decl || TREE_CODE (decl) != VAR_DECL)
    return 0;
    
  type = TREE_TYPE (decl);
  size_tree = TYPE_SIZE_UNIT (type);
  
  if (!size_tree || !tree_fits_uhwi_p (size_tree))
    return 0;
    
  size = tree_to_uhwi (size_tree);
  return (unsigned int) size;
}

/* Find the best group for allocation using best-fit algorithm */
unsigned int
mem_shared_get_best_group (unsigned int size)
{
  unsigned int best_group = 0;
  unsigned int best_fit_size = UINT_MAX;
  unsigned int i, j;
  
  for (i = 0; i < mem_shared_num_groups; i++)
    {
      for (j = 0; j < CORES_PER_GROUP; j++)
        {
          if (mem_shared_group_info[i].free_memory[j] >= size &&
              mem_shared_group_info[i].free_memory[j] < best_fit_size)
            {
              best_group = i;
              best_fit_size = mem_shared_group_info[i].free_memory[j];
            }
        }
    }
    
  return best_group;
}

/* Get next group using round-robin allocation */
unsigned int
mem_shared_get_next_group (void)
{
  unsigned int group = next_group_allocation;
  next_group_allocation = (next_group_allocation + 1) % mem_shared_num_groups;
  return group;
}

/* Find the best core within a group for allocation */
unsigned int
mem_shared_get_best_core_in_group (unsigned int group_id, unsigned int size)
{
  unsigned int best_core = 0;
  unsigned int best_free = 0;
  unsigned int i;
  
  if (group_id >= mem_shared_num_groups)
    return 0;
    
  for (i = 0; i < CORES_PER_GROUP; i++)
    {
      if (mem_shared_group_info[group_id].free_memory[i] >= size &&
          mem_shared_group_info[group_id].free_memory[i] > best_free)
        {
          best_core = i;
          best_free = mem_shared_group_info[group_id].free_memory[i];
        }
    }
    
  return best_core;
}

/* Distribute large data across multiple cores */
void
mem_shared_distribute_data (mem_shared_info_t *info, unsigned int total_size)
{
  unsigned int chunk_size, i, core_id, group_id, intra_id;
  
  /* Calculate chunk size (distribute evenly across all participating cores) */
  chunk_size = (total_size + mem_shared_total_cores - 1) / mem_shared_total_cores;
  
  info->is_distributed = true;
  info->num_chunks = mem_shared_total_cores;  /* One chunk per core */
  info->chunk_size = chunk_size;
  info->target_group = 0;  /* Start from group 0 */
  info->intra_group_id = 0; /* Start from core 0 in group 0 */
  
  /* Update memory usage for all participating cores */
  for (i = 0; i < mem_shared_total_cores; i++)
    {
      unsigned int actual_chunk_size = chunk_size;
      
      /* Calculate which group and intra-group ID this core belongs to */
      group_id = i / CORES_PER_GROUP;     /* i / 2 */
      intra_id = i % CORES_PER_GROUP;     /* i % 2 */
      
      /* Adjust last chunk size */
      if (i == mem_shared_total_cores - 1)
        actual_chunk_size = total_size - (chunk_size * (mem_shared_total_cores - 1));
        
      /* Update memory usage for this specific core */
      if (group_id < mem_shared_num_groups && 
          mem_shared_group_info[group_id].free_memory[intra_id] >= actual_chunk_size)
        {
          mem_shared_group_info[group_id].used_memory[intra_id] += actual_chunk_size;
          mem_shared_group_info[group_id].free_memory[intra_id] -= actual_chunk_size;
        }
    }
    
  if (flag_dump_mem_shared)
    fprintf (stderr, "[mem_shared] Distributed %u bytes across %u cores (%u groups), %u bytes per chunk\n",
            total_size, mem_shared_total_cores, mem_shared_num_groups, chunk_size);
}

/* Allocate memory for a mem_shared variable */
mem_shared_info_t *
mem_shared_allocate (tree decl)
{
  mem_shared_info_t *info;
  tree type;
  unsigned int size, group_id, intra_id, start_offset;
  
  if (!decl || TREE_CODE (decl) != VAR_DECL)
    return NULL;
    
  /* Check if already allocated */
  info = mem_shared_get_info (decl);
  if (info)
    return info;
    
  /* Calculate size and determine category */
  size = mem_shared_get_data_size (decl);
  if (size == 0)
    return NULL;
    
  /* Create allocation info */
  info = XNEW (mem_shared_info_t);
  info->decl = decl;
  info->size = size;
  info->is_distributed = false;
  info->num_chunks = 1;
  info->chunk_size = size;
  info->next = mem_shared_allocation_list;
  mem_shared_allocation_list = info;
  
  type = TREE_TYPE (decl);
  
  /* Determine allocation strategy */
  if (mem_shared_is_basic_type (type))
    {
      /* Basic types: round-robin allocation */
      info->category = MEM_SHARED_BASIC_TYPE;
      group_id = mem_shared_get_next_group ();
      intra_id = mem_shared_get_best_core_in_group (group_id, size);
    }
  else if (size < DISTRIBUTION_THRESHOLD)
    {
      /* Small data: best-fit allocation */
      info->category = MEM_SHARED_SMALL_DATA;
      group_id = mem_shared_get_best_group (size);
      intra_id = mem_shared_get_best_core_in_group (group_id, size);
    }
  else
    {
      /* Large data: distribute across groups */
      info->category = MEM_SHARED_LARGE_DATA;
      mem_shared_distribute_data (info, size);
      return info;
    }
    
  /* Single group allocation using pool system */
  info->target_group = group_id;
  info->intra_group_id = intra_id;
  
  /* Allocate from the core's memory pool */
  unsigned int core_id = mem_shared_core_id_from_group_intra (group_id, intra_id);
  uintptr_t pool_addr;
  
  if (mem_shared_pool_allocate (core_id, size, &pool_addr))
    {
      info->start_offset = (unsigned int)(pool_addr - mem_shared_get_pool_base (core_id));
      
      /* Update legacy group tracking for compatibility */
      mem_shared_group_info[group_id].used_memory[intra_id] += size;
      mem_shared_group_info[group_id].free_memory[intra_id] -= size;
      mem_shared_group_info[group_id].allocated_vars++;
      
      if (flag_dump_mem_shared)
        fprintf (stderr, "[mem_shared] Allocated %u bytes for '%s' in group %u core %u at pool offset 0x%x (addr 0x%lx)\n",
                size, IDENTIFIER_POINTER (DECL_NAME (decl)), group_id, intra_id, info->start_offset, pool_addr);
    }
  else
    {
      error ("failed to allocate %u bytes from core %u memory pool for %qD", size, core_id, decl);
      free (info);
      return NULL;
    }
            
  return info;
}

/* Get allocation info for a declaration */
mem_shared_info_t *
mem_shared_get_info (tree decl)
{
  mem_shared_info_t *info;
  
  if (!decl)
    return NULL;
    
  for (info = mem_shared_allocation_list; info; info = info->next)
    {
      if (info->decl == decl)
        return info;
    }
    
  return NULL;
}

/* Calculate target group for distributed data access */
unsigned int
mem_shared_calculate_target_group (tree decl, HOST_WIDE_INT offset)
{
  mem_shared_info_t *info;
  unsigned int chunk_index, target_core;
  
  info = mem_shared_get_info (decl);
  if (!info)
    return 0;
    
  if (!info->is_distributed)
    return info->target_group;
    
  /* Calculate which core this offset falls into */
  chunk_index = offset / info->chunk_size;
  target_core = chunk_index % mem_shared_total_cores;
  
  /* Convert core ID to group ID */
  return target_core / CORES_PER_GROUP;
}

/* Calculate intra-group ID for access */
unsigned int
mem_shared_calculate_intra_group_id (tree decl, HOST_WIDE_INT offset)
{
  mem_shared_info_t *info;
  unsigned int chunk_index, target_core;
  
  info = mem_shared_get_info (decl);
  if (!info)
    return 0;
    
  if (!info->is_distributed)
    return info->intra_group_id;
    
  /* Calculate which core this offset falls into */
  chunk_index = offset / info->chunk_size;
  target_core = chunk_index % mem_shared_total_cores;
  
  /* Convert core ID to intra-group ID */
  return target_core % CORES_PER_GROUP;
}

/* Calculate local offset within target core */
unsigned int
mem_shared_calculate_local_offset (tree decl, HOST_WIDE_INT offset)
{
  mem_shared_info_t *info;
  unsigned int local_offset;
  
  info = mem_shared_get_info (decl);
  if (!info)
    return 0;
    
  if (!info->is_distributed)
    return info->start_offset + offset;
    
  /* For distributed data, calculate offset within chunk */
  local_offset = info->start_offset + (offset % info->chunk_size);
  return local_offset;
}

/* Encode cross-core access address */
rtx
mem_shared_encode_cross_core_address (unsigned int group_id, unsigned int intra_id,
                                     unsigned int local_offset)
{
  HOST_WIDE_INT encoded_addr = local_offset;
  
  /* Set cross-core access bit (bit 29) */
  encoded_addr = mem_shared_set_cross_core_bit (encoded_addr);
  
  /* Clear reserved bit (bit 27) */
  encoded_addr = mem_shared_clear_reserved_bit (encoded_addr);
  
  /* Set group ID in bits 26:21 */
  encoded_addr = mem_shared_set_group_bits (encoded_addr, group_id);
  
  /* Set intra-group ID in bit 20 */
  encoded_addr = mem_shared_set_intra_id_bit (encoded_addr, intra_id);
  
  return gen_int_mode (encoded_addr, Pmode);
}

/* Generate address for mem_shared variable access */
rtx
mem_shared_generate_address (tree decl, HOST_WIDE_INT offset)
{
  unsigned int target_group, intra_id, local_offset, core_id;
  uintptr_t pool_base, actual_addr;
  
  /* Calculate address components */
  target_group = mem_shared_calculate_target_group (decl, offset);
  intra_id = mem_shared_calculate_intra_group_id (decl, offset);
  local_offset = mem_shared_calculate_local_offset (decl, offset);
  
  /* Get actual pool base address for this core */
  core_id = mem_shared_core_id_from_group_intra (target_group, intra_id);
  pool_base = mem_shared_get_pool_base (core_id);
  
  if (pool_base != 0)
    {
      /* Use actual pool address */
      actual_addr = pool_base + local_offset;
      return gen_int_mode (actual_addr, Pmode);
    }
  else
    {
      /* Fallback to encoded cross-core address */
      return mem_shared_encode_cross_core_address (target_group, intra_id, local_offset);
    }
}

/* Expand load operation for mem_shared variable */
rtx
mem_shared_expand_load (tree decl, HOST_WIDE_INT offset, machine_mode mode)
{
  rtx addr, mem;
  
  addr = mem_shared_generate_address (decl, offset);
  mem = gen_rtx_MEM (mode, addr);
  
  /* Mark as mem_shared access for optimization */
  MEM_VOLATILE_P (mem) = 0; /* Allow optimization */
  
  if (flag_dump_mem_shared)
    {
      mem_shared_info_t *info = mem_shared_get_info (decl);
      if (info)
        fprintf (stderr, "[mem_shared] Load from '%s' group %u intra %u offset 0x%x\n",
                IDENTIFIER_POINTER (DECL_NAME (decl)),
                mem_shared_calculate_target_group (decl, offset),
                mem_shared_calculate_intra_group_id (decl, offset),
                mem_shared_calculate_local_offset (decl, offset));
    }
    
  return mem;
}

/* Expand store operation for mem_shared variable */
void
mem_shared_expand_store (tree decl, rtx value, HOST_WIDE_INT offset)
{
  rtx addr, mem;
  
  addr = mem_shared_generate_address (decl, offset);
  mem = gen_rtx_MEM (GET_MODE (value), addr);
  
  /* Mark as mem_shared access */
  MEM_VOLATILE_P (mem) = 0; /* Allow optimization */
  
  /* Emit store instruction */
  emit_move_insn (mem, value);
  
  if (flag_dump_mem_shared)
    {
      mem_shared_info_t *info = mem_shared_get_info (decl);
      if (info)
        fprintf (stderr, "[mem_shared] Store to '%s' group %u intra %u offset 0x%x\n",
                IDENTIFIER_POINTER (DECL_NAME (decl)),
                mem_shared_calculate_target_group (decl, offset),
                mem_shared_calculate_intra_group_id (decl, offset),
                mem_shared_calculate_local_offset (decl, offset));
    }
}

/* Process a mem_shared declaration */
void
mem_shared_process_declaration (tree decl)
{
  mem_shared_info_t *info;
  
  if (!decl || TREE_CODE (decl) != VAR_DECL)
    return;
    
  if (!mem_shared_decl_p (decl))
    return;
    
  /* Ensure static pools are generated */
  if (!mem_shared_pools_initialized)
    {
      mem_shared_pools_init ();
      mem_shared_emit_static_pools ();
    }
    
  /* Allocate memory for this declaration */
  info = mem_shared_allocate (decl);
  if (!info)
    {
      error ("failed to allocate mem_shared memory for %qD", decl);
      return;
    }
    
  if (flag_dump_mem_shared)
    fprintf (stderr, "[mem_shared] Processed declaration '%s'\n",
            IDENTIFIER_POINTER (DECL_NAME (decl)));
}

/* Check if declaration is mem_shared */
bool
mem_shared_decl_p (tree decl)
{
  return (TREE_CODE (decl) == VAR_DECL && 
          lookup_attribute ("mem_shared", DECL_ATTRIBUTES (decl)) != NULL);
}

/* Dump allocation information */
void
mem_shared_dump_allocation_info (FILE *file)
{
  mem_shared_info_t *info;
  const char *category_names[] = {"BASIC", "SMALL", "LARGE"};
  
  fprintf (file, "\n=== mem_shared Allocation Information ===\n");
  fprintf (file, "Configuration: %u groups, %u total cores, %u bytes per core\n",
          mem_shared_num_groups, mem_shared_total_cores, mem_shared_core_memory_size);
          
  fprintf (file, "\nAllocated Variables:\n");
  for (info = mem_shared_allocation_list; info; info = info->next)
    {
      fprintf (file, "  %-20s: %6u bytes, %s, group %u intra %u",
              IDENTIFIER_POINTER (DECL_NAME (info->decl)),
              info->size,
              category_names[info->category],
              info->target_group,
              info->intra_group_id);
              
      if (info->is_distributed)
        fprintf (file, " (distributed, %u chunks)", info->num_chunks);
        
      fprintf (file, "\n");
    }
}

/* Dump group usage information */
void
mem_shared_dump_group_usage (FILE *file)
{
  unsigned int i, j;
  unsigned int total_used = 0, total_available = 0;
  
  fprintf (file, "\n=== mem_shared Group Usage ===\n");
  fprintf (file, "Group  Core0-Used  Core0-Free  Core1-Used  Core1-Free  Variables\n");
  
  for (i = 0; i < mem_shared_num_groups; i++)
    {
      fprintf (file, "%5u  %10u  %10u  %10u  %10u  %9u\n",
              i,
              mem_shared_group_info[i].used_memory[0],
              mem_shared_group_info[i].free_memory[0],
              mem_shared_group_info[i].used_memory[1],
              mem_shared_group_info[i].free_memory[1],
              mem_shared_group_info[i].allocated_vars);
              
      for (j = 0; j < CORES_PER_GROUP; j++)
        {
          total_used += mem_shared_group_info[i].used_memory[j];
          total_available += mem_shared_group_info[i].free_memory[j];
        }
    }
    
  fprintf (file, "\nTotal: %u bytes used, %u bytes available\n",
          total_used, total_available);
  fprintf (file, "Usage: %.1f%% of total capacity\n",
          (double)total_used / (total_used + total_available) * 100.0);
}

/* ===================================================================
   LOCAL MEMORY POOL MANAGEMENT IMPLEMENTATION
   =================================================================== */

/* Initialize all memory pools */
void
mem_shared_pools_init (void)
{
  unsigned int i, group_id, intra_id;
  
  if (mem_shared_pools_initialized)
    return;
    
  /* Initialize each core's pool */
  for (i = 0; i < mem_shared_total_cores; i++)
    {
      group_id = i / CORES_PER_GROUP;
      intra_id = i % CORES_PER_GROUP;
      
      mem_shared_pools[i].core_id = i;
      mem_shared_pools[i].group_id = group_id;
      mem_shared_pools[i].intra_id = intra_id;
      
      /* Static pool initialization */
      mem_shared_pools[i].static_pool_decl = NULL_TREE;
      mem_shared_pools[i].static_pool_base = 0;
      mem_shared_pools[i].static_pool_size = mem_shared_core_memory_size;
      mem_shared_pools[i].static_used_bytes = 0;
      mem_shared_pools[i].static_current_ptr = 0;
      
      /* Dynamic allocation settings */
      mem_shared_pools[i].dynamic_enabled = 
        (mem_shared_alloc_strategy == MEM_SHARED_ALLOC_DYNAMIC_ONLY ||
         mem_shared_alloc_strategy == MEM_SHARED_ALLOC_HYBRID);
      mem_shared_pools[i].dynamic_used_bytes = 0;
      
      mem_shared_pools[i].initialized = false;
    }
    
  mem_shared_pools_initialized = true;
  
  if (flag_dump_mem_shared)
    fprintf (stderr, "[mem_shared] Pools initialized for %u cores with %s strategy\n",
            mem_shared_total_cores,
            (mem_shared_alloc_strategy == MEM_SHARED_ALLOC_STATIC_ONLY) ? "static-only" :
            (mem_shared_alloc_strategy == MEM_SHARED_ALLOC_DYNAMIC_ONLY) ? "dynamic-only" : "hybrid");
}

/* Cleanup all memory pools */
void
mem_shared_pools_cleanup (void)
{
  unsigned int i;
  
  if (!mem_shared_pools_initialized)
    return;
    
  /* Reset all pools */
  for (i = 0; i < MAX_TOTAL_CORES; i++)
    {
      mem_shared_pools[i].initialized = false;
      mem_shared_pools[i].static_pool_decl = NULL_TREE;
      mem_shared_pools[i].static_pool_base = 0;
      mem_shared_pools[i].static_used_bytes = 0;
      mem_shared_pools[i].dynamic_used_bytes = 0;
    }
    
  mem_shared_pools_initialized = false;
  
  if (flag_dump_mem_shared)
    fprintf (stderr, "[mem_shared] Pools cleanup completed\n");
}

/* Allocate memory from a specific core's pool */
bool
mem_shared_pool_allocate (unsigned int core_id, size_t size, uintptr_t *addr_out)
{
  mem_shared_pool_t *pool;
  size_t aligned_size;
  
  if (core_id >= mem_shared_total_cores || !addr_out)
    return false;
    
  if (!mem_shared_pools_initialized)
    mem_shared_pools_init ();
    
  pool = &mem_shared_pools[core_id];
  aligned_size = mem_shared_align_size (size);
  
  /* Try static pool first (if available and strategy allows) */
  if (mem_shared_alloc_strategy != MEM_SHARED_ALLOC_DYNAMIC_ONLY)
    {
      if (pool->static_pool_base != 0 && 
          pool->static_used_bytes + aligned_size <= pool->static_pool_size)
        {
          *addr_out = pool->static_current_ptr;
          pool->static_current_ptr += aligned_size;
          pool->static_used_bytes += aligned_size;
          
          if (flag_dump_mem_shared)
            fprintf (stderr, "[mem_shared] Static pool alloc: core %u, size %zu, addr 0x%lx\n",
                    core_id, aligned_size, *addr_out);
          return true;
        }
    }
    
  /* Fallback to dynamic allocation (if enabled) */
  if (pool->dynamic_enabled)
    {
      /* Note: llc_malloc() would be called at runtime, not at compile time
         For now, we simulate with a placeholder address */
      *addr_out = 0xDEADBEEF + core_id * 0x1000; /* Placeholder */
      pool->dynamic_used_bytes += aligned_size;
      
      if (flag_dump_mem_shared)
        fprintf (stderr, "[mem_shared] Dynamic alloc: core %u, size %zu, addr 0x%lx (simulated)\n",
                core_id, aligned_size, *addr_out);
      return true;
    }
    
  /* Allocation failed */
  if (flag_dump_mem_shared)
    fprintf (stderr, "[mem_shared] Pool allocation failed: core %u, size %zu\n", core_id, size);
  return false;
}

/* Generate a static pool declaration for a core */
tree
mem_shared_generate_static_pool (unsigned int group_id, unsigned int intra_id)
{
  unsigned int core_id;
  tree pool_type, pool_decl, pool_name;
  char name_buffer[64];
  
  core_id = mem_shared_core_id_from_group_intra (group_id, intra_id);
  if (core_id >= mem_shared_total_cores)
    return NULL_TREE;
    
  /* Create pool array type */
  pool_type = build_array_type (char_type_node,
                               build_index_type (size_int (mem_shared_core_memory_size - 1)));
                               
  /* Generate unique pool name */
  snprintf (name_buffer, sizeof(name_buffer), 
           "__mem_shared_pool_core_%u_group_%u_intra_%u", 
           core_id, group_id, intra_id);
  pool_name = get_identifier (name_buffer);
  
  /* Create pool declaration */
  pool_decl = build_decl (UNKNOWN_LOCATION, VAR_DECL, pool_name, pool_type);
  
  /* Set attributes for local memory section */
  TREE_STATIC (pool_decl) = 1;
  TREE_PUBLIC (pool_decl) = 0;
  DECL_ARTIFICIAL (pool_decl) = 1;
  
  /* Add section attribute for local memory */
  tree section_attr = tree_cons (get_identifier ("section"),
                                tree_cons (NULL_TREE, 
                                          build_string (5, ".llc"), NULL_TREE),
                                NULL_TREE);
  DECL_ATTRIBUTES (pool_decl) = section_attr;
  
  /* Register the pool */
  mem_shared_pools[core_id].static_pool_decl = pool_decl;
  mem_shared_pools[core_id].static_pool_base = (uintptr_t)pool_decl; /* Will be resolved by linker */
  mem_shared_pools[core_id].static_current_ptr = mem_shared_pools[core_id].static_pool_base;
  mem_shared_pools[core_id].initialized = true;
  
  if (flag_dump_mem_shared)
    fprintf (stderr, "[mem_shared] Generated static pool: %s, size %u bytes\n",
            name_buffer, mem_shared_core_memory_size);
            
  return pool_decl;
}

/* Get the base address of a core's memory pool */
uintptr_t
mem_shared_get_pool_base (unsigned int core_id)
{
  if (core_id >= mem_shared_total_cores || !mem_shared_pools_initialized)
    return 0;
    
  return mem_shared_pools[core_id].static_pool_base;
}

/* Emit static pool declarations for all cores */
void
mem_shared_emit_static_pools (void)
{
  unsigned int i, group_id, intra_id;
  tree pool_decl;
  
  if (!mem_shared_pools_initialized)
    return;
    
  if (mem_shared_alloc_strategy == MEM_SHARED_ALLOC_DYNAMIC_ONLY)
    return; /* No static pools needed */
    
  for (i = 0; i < mem_shared_total_cores; i++)
    {
      group_id = i / CORES_PER_GROUP;
      intra_id = i % CORES_PER_GROUP;
      
      if (group_id < mem_shared_num_groups)
        {
          pool_decl = mem_shared_generate_static_pool (group_id, intra_id);
          if (pool_decl)
            {
              /* Add to global scope for compilation */
              pushdecl (pool_decl);
              rest_of_decl_compilation (pool_decl, 1, 0);
            }
        }
    }
    
  if (flag_dump_mem_shared)
    fprintf (stderr, "[mem_shared] Emitted %u static pool declarations\n", 
            mem_shared_total_cores);
}