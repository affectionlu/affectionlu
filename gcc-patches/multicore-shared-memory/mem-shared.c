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
#include "emit-rtl.h"
#include "explow.h"
#include "expr.h"
#include "diagnostic.h"
#include "flags.h"
#include "mem-shared.h"

/* Global state */
unsigned int mem_shared_num_cores = 1;
core_memory_t core_memories[MAX_CORES];
mem_shared_info_t *mem_shared_allocations = NULL;

/* Internal state */
static unsigned int next_core_hint = 0;
static bool mem_shared_initialized = false;

/* Hash table for quick lookup of allocation info */
static hash_map<tree, mem_shared_info_t *> *allocation_map = NULL;

/* Initialize mem_shared subsystem */
void
mem_shared_init (unsigned int num_cores)
{
  if (mem_shared_initialized)
    return;

  if (num_cores > MAX_CORES)
    {
      error ("too many cores specified for mem_shared: %u (max %u)", 
             num_cores, MAX_CORES);
      num_cores = MAX_CORES;
    }

  mem_shared_num_cores = num_cores;

  /* Initialize core memory tracking */
  for (unsigned int i = 0; i < mem_shared_num_cores; i++)
    {
      core_memories[i].used_size = 0;
      core_memories[i].available_size = CORE_MEMORY_SIZE;
      core_memories[i].num_allocations = 0;
      
      for (unsigned int j = 0; j < MAX_ALLOCATIONS; j++)
        core_memories[i].allocations[j] = NULL;
    }

  /* Initialize allocation tracking */
  allocation_map = new hash_map<tree, mem_shared_info_t *>;
  mem_shared_allocations = NULL;
  next_core_hint = 0;
  mem_shared_initialized = true;

  if (flag_debug_mem_shared)
    {
      fprintf (stderr, "mem_shared: initialized for %u cores\n", num_cores);
    }
}

/* Cleanup mem_shared subsystem */
void
mem_shared_cleanup (void)
{
  if (!mem_shared_initialized)
    return;

  /* Free all allocation info structures */
  mem_shared_info_t *current = mem_shared_allocations;
  while (current)
    {
      mem_shared_info_t *next = current->next;
      XDELETE (current);
      current = next;
    }

  /* Cleanup hash table */
  if (allocation_map)
    {
      delete allocation_map;
      allocation_map = NULL;
    }

  mem_shared_allocations = NULL;
  mem_shared_initialized = false;
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
    case FIXED_POINT_TYPE:
    case BOOLEAN_TYPE:
    case ENUMERAL_TYPE:
    case POINTER_TYPE:
      return true;
    default:
      return false;
    }
}

/* Get the size of data for a declaration */
unsigned int
mem_shared_get_data_size (tree decl)
{
  tree type = TREE_TYPE (decl);
  tree size_tree = TYPE_SIZE_UNIT (type);
  
  if (!size_tree || !tree_fits_uhwi_p (size_tree))
    {
      error ("cannot determine size of mem_shared variable %qD", decl);
      return 0;
    }

  return tree_to_uhwi (size_tree);
}

/* Find the best core for allocation */
unsigned int
mem_shared_get_best_core (unsigned int size)
{
  unsigned int best_core = 0;
  unsigned int min_waste = UINT_MAX;
  
  for (unsigned int i = 0; i < mem_shared_num_cores; i++)
    {
      if (core_memories[i].available_size >= size)
        {
          unsigned int waste = core_memories[i].available_size - size;
          if (waste < min_waste)
            {
              min_waste = waste;
              best_core = i;
            }
        }
    }
  
  return best_core;
}

/* Get next available core using round-robin */
unsigned int
mem_shared_get_next_core (void)
{
  unsigned int core = next_core_hint;
  next_core_hint = (next_core_hint + 1) % mem_shared_num_cores;
  return core;
}

/* Allocate space on a specific core */
bool
mem_shared_allocate_on_core (unsigned int core, unsigned int size)
{
  if (core >= mem_shared_num_cores)
    return false;

  if (core_memories[core].available_size < size)
    return false;

  core_memories[core].used_size += size;
  core_memories[core].available_size -= size;
  
  return true;
}

/* Distribute large data across cores */
void
mem_shared_distribute_data (mem_shared_info_t *info, unsigned int total_size)
{
  unsigned int chunk_size = (total_size + mem_shared_num_cores - 1) / mem_shared_num_cores;
  unsigned int remaining_size = total_size;
  
  info->chunk_size = chunk_size;
  info->num_chunks = 0;
  info->target_core = 0; /* Starting core */
  
  for (unsigned int i = 0; i < mem_shared_num_cores && remaining_size > 0; i++)
    {
      unsigned int current_chunk = MIN (chunk_size, remaining_size);
      
      if (mem_shared_allocate_on_core (i, current_chunk))
        {
          info->num_chunks++;
          remaining_size -= current_chunk;
        }
      else
        {
          mem_shared_error_core_overflow (i, 
                                        core_memories[i].used_size + current_chunk,
                                        core_memories[i].available_size);
        }
    }
}

/* Allocate memory for a mem_shared declaration */
mem_shared_info_t *
mem_shared_allocate (tree decl)
{
  if (!mem_shared_initialized)
    mem_shared_init (1);

  if (!mem_shared_decl_p (decl))
    {
      mem_shared_error_invalid_declaration (decl, "not a mem_shared variable");
      return NULL;
    }

  /* Check if already allocated */
  mem_shared_info_t **slot = allocation_map->get (decl);
  if (slot && *slot)
    return *slot;

  /* Create new allocation info */
  mem_shared_info_t *info = XNEW (mem_shared_info_t);
  info->decl = decl;
  info->size = mem_shared_align_size (mem_shared_get_data_size (decl));
  info->is_distributed = false;
  info->num_chunks = 1;
  info->chunk_size = info->size;
  info->next = mem_shared_allocations;

  /* Determine allocation strategy */
  if (mem_shared_is_basic_type (TREE_TYPE (decl)))
    {
      info->category = MEM_SHARED_BASIC_TYPE;
      info->target_core = mem_shared_get_next_core ();
      
      if (!mem_shared_allocate_on_core (info->target_core, info->size))
        {
          info->target_core = mem_shared_get_best_core (info->size);
          if (!mem_shared_allocate_on_core (info->target_core, info->size))
            {
              mem_shared_error_core_overflow (info->target_core,
                                            core_memories[info->target_core].used_size + info->size,
                                            core_memories[info->target_core].available_size);
              XDELETE (info);
              return NULL;
            }
        }
    }
  else if (info->size < DISTRIBUTION_THRESHOLD)
    {
      info->category = MEM_SHARED_SMALL_DATA;
      info->target_core = mem_shared_get_best_core (info->size);
      
      if (!mem_shared_allocate_on_core (info->target_core, info->size))
        {
          mem_shared_error_core_overflow (info->target_core,
                                        core_memories[info->target_core].used_size + info->size,
                                        core_memories[info->target_core].available_size);
          XDELETE (info);
          return NULL;
        }
    }
  else
    {
      info->category = MEM_SHARED_LARGE_DATA;
      info->is_distributed = true;
      mem_shared_distribute_data (info, info->size);
    }

  /* Calculate start offset based on current usage */
  info->start_offset = CORE_MEMORY_SIZE - core_memories[info->target_core].available_size - info->size;

  /* Register allocation */
  allocation_map->put (decl, info);
  mem_shared_allocations = info;

  /* Mark the declaration */
  DECL_MEM_SHARED_P (decl) = 1;

  if (flag_debug_mem_shared)
    {
      const char *name = IDENTIFIER_POINTER (DECL_NAME (decl));
      if (!info->is_distributed)
        {
          fprintf (stderr, "mem_shared %s: core=%u, offset=0x%x, size=%u\n",
                   name, info->target_core, info->start_offset, info->size);
        }
      else
        {
          fprintf (stderr, "mem_shared %s: distributed across %u cores, size=%u\n",
                   name, mem_shared_num_cores, info->size);
        }
    }

  return info;
}

/* Get allocation info for a declaration */
mem_shared_info_t *
mem_shared_get_info (tree decl)
{
  if (!allocation_map)
    return NULL;
    
  mem_shared_info_t **slot = allocation_map->get (decl);
  return slot ? *slot : NULL;
}

/* Calculate target core for distributed data */
unsigned int
mem_shared_calculate_target_core (tree decl, HOST_WIDE_INT offset)
{
  mem_shared_info_t *info = mem_shared_get_info (decl);
  
  if (!info || !info->is_distributed)
    return info ? info->target_core : 0;

  unsigned int chunk_index = offset / info->chunk_size;
  return (info->target_core + chunk_index) % mem_shared_num_cores;
}

/* Calculate local offset for distributed data */
unsigned int
mem_shared_calculate_local_offset (tree decl, HOST_WIDE_INT offset)
{
  mem_shared_info_t *info = mem_shared_get_info (decl);
  
  if (!info || !info->is_distributed)
    return info ? info->start_offset + offset : offset;

  unsigned int local_offset = offset % info->chunk_size;
  return info->start_offset + local_offset;
}

/* Generate address for mem_shared access */
rtx
mem_shared_generate_address (tree decl, HOST_WIDE_INT offset)
{
  mem_shared_info_t *info = mem_shared_get_info (decl);
  
  if (!info)
    {
      error ("mem_shared variable %qD not allocated", decl);
      return NULL_RTX;
    }

  unsigned int target_core = mem_shared_calculate_target_core (decl, offset);
  unsigned int local_offset = mem_shared_calculate_local_offset (decl, offset);

  /* Generate address with core number in bit 21 */
  rtx core_id = GEN_INT (target_core);
  rtx base_addr = GEN_INT (local_offset);
  
  /* Create (core_id << 21) | offset */
  rtx shifted_core = gen_rtx_ASHIFT (Pmode, core_id, GEN_INT (21));
  rtx addr = gen_rtx_IOR (Pmode, shifted_core, base_addr);
  
  return addr;
}

/* Expand mem_shared load operation */
rtx
mem_shared_expand_load (tree decl, HOST_WIDE_INT offset, machine_mode mode)
{
  rtx addr = mem_shared_generate_address (decl, offset);
  if (!addr)
    return NULL_RTX;

  rtx mem = gen_rtx_MEM (mode, addr);
  
  /* Set memory attributes */
  set_mem_alias_set (mem, new_alias_set ());
  MEM_VOLATILE_P (mem) = 1; /* Prevent unwanted optimizations */
  
  return mem;
}

/* Expand mem_shared store operation */
void
mem_shared_expand_store (tree decl, rtx value, HOST_WIDE_INT offset)
{
  machine_mode mode = GET_MODE (value);
  rtx addr = mem_shared_generate_address (decl, offset);
  
  if (!addr)
    return;

  rtx mem = gen_rtx_MEM (mode, addr);
  
  /* Set memory attributes */
  set_mem_alias_set (mem, new_alias_set ());
  MEM_VOLATILE_P (mem) = 1;
  
  emit_move_insn (mem, value);
}

/* Check if data is distributed */
bool
mem_shared_is_distributed (tree decl)
{
  mem_shared_info_t *info = mem_shared_get_info (decl);
  return info ? info->is_distributed : false;
}

/* Process a mem_shared declaration */
void
mem_shared_process_declaration (tree decl)
{
  if (!mem_shared_decl_p (decl))
    return;

  /* Allocate memory for the declaration */
  mem_shared_allocate (decl);
}

/* Check for memory allocation conflicts */
void
mem_shared_check_conflicts (void)
{
  for (unsigned int i = 0; i < mem_shared_num_cores; i++)
    {
      if (core_memories[i].used_size > CORE_MEMORY_SIZE)
        {
          mem_shared_error_core_overflow (i, 
                                        core_memories[i].used_size,
                                        CORE_MEMORY_SIZE);
        }
    }
}

/* Dump allocation information */
void
mem_shared_dump_allocation_info (FILE *file)
{
  if (!file)
    file = stderr;

  fprintf (file, "\n=== mem_shared Allocation Summary ===\n");
  fprintf (file, "Number of cores: %u\n", mem_shared_num_cores);
  
  mem_shared_info_t *current = mem_shared_allocations;
  unsigned int total_allocations = 0;
  
  while (current)
    {
      const char *name = IDENTIFIER_POINTER (DECL_NAME (current->decl));
      
      fprintf (file, "Variable: %s\n", name);
      fprintf (file, "  Category: %s\n", 
               current->category == MEM_SHARED_BASIC_TYPE ? "basic" :
               current->category == MEM_SHARED_SMALL_DATA ? "small" : "large");
      fprintf (file, "  Size: %u bytes\n", current->size);
      fprintf (file, "  Distributed: %s\n", current->is_distributed ? "yes" : "no");
      
      if (!current->is_distributed)
        {
          fprintf (file, "  Core: %u\n", current->target_core);
          fprintf (file, "  Offset: 0x%x\n", current->start_offset);
        }
      else
        {
          fprintf (file, "  Chunks: %u\n", current->num_chunks);
          fprintf (file, "  Chunk size: %u bytes\n", current->chunk_size);
        }
      
      total_allocations++;
      current = current->next;
    }
  
  fprintf (file, "\nTotal allocations: %u\n", total_allocations);
}

/* Dump core usage information */
void
mem_shared_dump_core_usage (FILE *file)
{
  if (!file)
    file = stderr;

  fprintf (file, "\n=== Core Memory Usage ===\n");
  
  for (unsigned int i = 0; i < mem_shared_num_cores; i++)
    {
      fprintf (file, "Core %u:\n", i);
      fprintf (file, "  Used: %u bytes (%.1f%%)\n", 
               core_memories[i].used_size,
               (core_memories[i].used_size * 100.0) / CORE_MEMORY_SIZE);
      fprintf (file, "  Available: %u bytes\n", core_memories[i].available_size);
      fprintf (file, "  Allocations: %u\n", core_memories[i].num_allocations);
    }
}

/* Error handlers */
void
mem_shared_error_core_overflow (unsigned int core, 
                               unsigned int used, 
                               unsigned int available)
{
  error ("core %u memory overflow: need %u bytes, only %u available", 
         core, used, available);
}

void
mem_shared_error_invalid_declaration (tree decl, const char *reason)
{
  error ("invalid mem_shared declaration %qD: %s", decl, reason);
}

/* Target-specific functions (can be overridden by target) */
bool
mem_shared_target_supports_multicore (void)
{
  return true; /* Default implementation */
}

unsigned int
mem_shared_target_get_core_count (void)
{
  return mem_shared_num_cores; /* Default implementation */
}

rtx
mem_shared_target_encode_address (unsigned int core, unsigned int offset)
{
  /* Default implementation using bit 21 for core number */
  rtx core_id = GEN_INT (core);
  rtx base_addr = GEN_INT (offset);
  rtx shifted_core = gen_rtx_ASHIFT (Pmode, core_id, GEN_INT (21));
  return gen_rtx_IOR (Pmode, shifted_core, base_addr);
}