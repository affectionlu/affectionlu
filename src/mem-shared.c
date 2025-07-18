/* mem-shared.c - Implementation of mem_shared functionality
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
#include "tree-iterator.h"
#include "diagnostic.h"
#include "fold-const.h"
#include "stor-layout.h"
#include "mem-shared.h"

/* Global state */
unsigned int mem_shared_num_cores = 1;
core_memory_t core_memories[MAX_CORES];
mem_shared_info_t *mem_shared_allocations = NULL;

/* Static variables */
static bool mem_shared_initialized = false;
static unsigned int next_core_allocation = 0;

/* Initialize mem_shared subsystem */
void
mem_shared_init (unsigned int num_cores)
{
  unsigned int i;
  
  if (mem_shared_initialized)
    return;
    
  if (num_cores == 0 || num_cores > MAX_CORES)
    {
      error ("invalid number of cores for mem_shared: %u", num_cores);
      return;
    }
    
  mem_shared_num_cores = num_cores;
  
  /* Initialize core memory tracking */
  for (i = 0; i < num_cores; i++)
    {
      core_memories[i].used_size = 0;
      core_memories[i].available_size = CORE_MEMORY_SIZE;
      core_memories[i].num_allocations = 0;
      memset (core_memories[i].allocations, 0, sizeof (core_memories[i].allocations));
    }
    
  mem_shared_initialized = true;
  
  if (flag_dump_mem_shared)
    fprintf (stderr, "[mem_shared] Initialized with %u cores\n", num_cores);
}

/* Cleanup mem_shared subsystem */
void
mem_shared_cleanup (void)
{
  mem_shared_info_t *info, *next;
  
  if (!mem_shared_initialized)
    return;
    
  /* Free all allocation records */
  for (info = mem_shared_allocations; info; info = next)
    {
      next = info->next;
      free (info);
    }
    
  mem_shared_allocations = NULL;
  mem_shared_initialized = false;
}

/* Check if a type is a basic type */
bool
mem_shared_is_basic_type (tree type)
{
  enum tree_code code = TREE_CODE (type);
  
  return (code == INTEGER_TYPE || code == REAL_TYPE || 
          code == FIXED_POINT_TYPE || code == BOOLEAN_TYPE ||
          code == ENUMERAL_TYPE || code == POINTER_TYPE);
}

/* Get data size for a declaration */
unsigned int
mem_shared_get_data_size (tree decl)
{
  tree type = TREE_TYPE (decl);
  
  if (!tree_fits_uhwi_p (TYPE_SIZE_UNIT (type)))
    {
      error ("mem_shared variable %qD has unknown size", decl);
      return 0;
    }
    
  return tree_to_uhwi (TYPE_SIZE_UNIT (type));
}

/* Find best core for allocation */
unsigned int
mem_shared_get_best_core (unsigned int size)
{
  unsigned int best_core = 0;
  unsigned int min_used = CORE_MEMORY_SIZE;
  unsigned int i;
  
  /* Find core with most available space */
  for (i = 0; i < mem_shared_num_cores; i++)
    {
      if (core_memories[i].available_size >= size && 
          core_memories[i].used_size < min_used)
        {
          min_used = core_memories[i].used_size;
          best_core = i;
        }
    }
    
  if (core_memories[best_core].available_size < size)
    {
      error ("no core has enough space for allocation of %u bytes", size);
      return 0;
    }
    
  return best_core;
}

/* Get next core for round-robin allocation */
unsigned int
mem_shared_get_next_core (void)
{
  unsigned int core = next_core_allocation;
  next_core_allocation = (next_core_allocation + 1) % mem_shared_num_cores;
  return core;
}

/* Distribute large data across cores */
void
mem_shared_distribute_data (mem_shared_info_t *info, unsigned int total_size)
{
  unsigned int chunk_size = total_size / mem_shared_num_cores;
  unsigned int remainder = total_size % mem_shared_num_cores;
  unsigned int i;
  
  info->num_chunks = mem_shared_num_cores;
  info->chunk_size = chunk_size;
  
  /* Allocate space on each core */
  for (i = 0; i < mem_shared_num_cores; i++)
    {
      unsigned int this_chunk_size = chunk_size;
      if (i < remainder)
        this_chunk_size++;
        
      if (core_memories[i].available_size < this_chunk_size)
        {
          error ("core %u has insufficient space for distributed allocation", i);
          return;
        }
        
      core_memories[i].used_size += this_chunk_size;
      core_memories[i].available_size -= this_chunk_size;
    }
}

/* Allocate memory for mem_shared variable */
mem_shared_info_t *
mem_shared_allocate (tree decl)
{
  mem_shared_info_t *info;
  unsigned int data_size;
  unsigned int aligned_size;
  
  if (!mem_shared_initialized)
    {
      error ("mem_shared not initialized");
      return NULL;
    }
    
  data_size = mem_shared_get_data_size (decl);
  if (data_size == 0)
    return NULL;
    
  aligned_size = mem_shared_align_size (data_size);
  
  /* Create allocation record */
  info = XNEW (mem_shared_info_t);
  info->decl = decl;
  info->size = aligned_size;
  info->is_distributed = false;
  info->num_chunks = 1;
  info->chunk_size = aligned_size;
  info->next = mem_shared_allocations;
  mem_shared_allocations = info;
  
  /* Determine allocation strategy */
  if (mem_shared_is_basic_type (TREE_TYPE (decl)))
    {
      info->category = MEM_SHARED_BASIC_TYPE;
      info->target_core = mem_shared_get_next_core ();
    }
  else if (aligned_size < DISTRIBUTION_THRESHOLD)
    {
      info->category = MEM_SHARED_SMALL_DATA;
      info->target_core = mem_shared_get_best_core (aligned_size);
    }
  else
    {
      info->category = MEM_SHARED_LARGE_DATA;
      info->target_core = 0; /* Start from core 0 */
      info->is_distributed = true;
      mem_shared_distribute_data (info, aligned_size);
      
      if (flag_dump_mem_shared)
        fprintf (stderr, "[mem_shared] %s: distributed %u bytes across %u cores\n",
                IDENTIFIER_POINTER (DECL_NAME (decl)), aligned_size, 
                mem_shared_num_cores);
      return info;
    }
    
  /* Single core allocation */
  if (core_memories[info->target_core].available_size < aligned_size)
    {
      error ("core %u has insufficient space for %qD (%u bytes)",
             info->target_core, decl, aligned_size);
      return NULL;
    }
    
  info->start_offset = core_memories[info->target_core].used_size;
  core_memories[info->target_core].used_size += aligned_size;
  core_memories[info->target_core].available_size -= aligned_size;
  
  if (flag_dump_mem_shared)
    fprintf (stderr, "[mem_shared] %s: allocated %u bytes on core %u at offset 0x%x\n",
            IDENTIFIER_POINTER (DECL_NAME (decl)), aligned_size, 
            info->target_core, info->start_offset);
            
  return info;
}

/* Get allocation info for a declaration */
mem_shared_info_t *
mem_shared_get_info (tree decl)
{
  mem_shared_info_t *info;
  
  for (info = mem_shared_allocations; info; info = info->next)
    {
      if (info->decl == decl)
        return info;
    }
    
  return NULL;
}

/* Calculate target core for distributed data access */
unsigned int
mem_shared_calculate_target_core (tree decl, HOST_WIDE_INT offset)
{
  mem_shared_info_t *info = mem_shared_get_info (decl);
  
  if (!info || !info->is_distributed)
    return info ? info->target_core : 0;
    
  /* Calculate which core contains this offset */
  unsigned int chunk_index = offset / info->chunk_size;
  return (info->target_core + chunk_index) % mem_shared_num_cores;
}

/* Calculate local offset within target core */
unsigned int
mem_shared_calculate_local_offset (tree decl, HOST_WIDE_INT offset)
{
  mem_shared_info_t *info = mem_shared_get_info (decl);
  
  if (!info || !info->is_distributed)
    return info ? (info->start_offset + offset) : offset;
    
  /* Calculate offset within the target core */
  unsigned int local_offset = offset % info->chunk_size;
  return local_offset;
}

/* Generate address for mem_shared variable */
rtx
mem_shared_generate_address (tree decl, HOST_WIDE_INT offset)
{
  unsigned int target_core;
  unsigned int local_offset;
  rtx core_id, base_addr, addr;
  
  target_core = mem_shared_calculate_target_core (decl, offset);
  local_offset = mem_shared_calculate_local_offset (decl, offset);
  
  /* Create RTL for (core_id << 21) | local_offset */
  core_id = GEN_INT (target_core);
  base_addr = GEN_INT (local_offset);
  
  addr = gen_rtx_IOR (Pmode,
                     gen_rtx_ASHIFT (Pmode, core_id, GEN_INT (21)),
                     base_addr);
                     
  return addr;
}

/* Expand load from mem_shared variable */
rtx
mem_shared_expand_load (tree decl, HOST_WIDE_INT offset, machine_mode mode)
{
  rtx addr, mem;
  
  addr = mem_shared_generate_address (decl, offset);
  mem = gen_rtx_MEM (mode, addr);
  
  /* Set memory attributes */
  set_mem_alias_set (mem, 0); /* May alias anything */
  MEM_VOLATILE_P (mem) = 1;   /* Prevent unwanted optimizations */
  
  return mem;
}

/* Expand store to mem_shared variable */
void
mem_shared_expand_store (tree decl, rtx value, HOST_WIDE_INT offset)
{
  machine_mode mode = TYPE_MODE (TREE_TYPE (decl));
  rtx addr, mem;
  
  addr = mem_shared_generate_address (decl, offset);
  mem = gen_rtx_MEM (mode, addr);
  
  set_mem_alias_set (mem, 0);
  MEM_VOLATILE_P (mem) = 1;
  
  emit_move_insn (mem, value);
}

/* Process mem_shared declaration */
void
mem_shared_process_declaration (tree decl)
{
  mem_shared_info_t *info;
  
  if (!DECL_MEM_SHARED_P (decl))
    return;
    
  if (TREE_CODE (decl) != VAR_DECL)
    {
      error ("mem_shared can only be applied to variables");
      return;
    }
    
  if (!mem_shared_initialized)
    mem_shared_init (mem_shared_num_cores);
    
  info = mem_shared_allocate (decl);
  if (!info)
    {
      error ("failed to allocate mem_shared storage for %qD", decl);
      return;
    }
}

/* Dump allocation information */
void
mem_shared_dump_allocation_info (FILE *file)
{
  mem_shared_info_t *info;
  
  fprintf (file, "\n=== mem_shared Allocation Information ===\n");
  fprintf (file, "Number of cores: %u\n", mem_shared_num_cores);
  fprintf (file, "Core memory size: %u bytes\n", CORE_MEMORY_SIZE);
  fprintf (file, "Distribution threshold: %u bytes\n", DISTRIBUTION_THRESHOLD);
  
  for (info = mem_shared_allocations; info; info = info->next)
    {
      const char *name = IDENTIFIER_POINTER (DECL_NAME (info->decl));
      const char *category_name;
      
      switch (info->category)
        {
        case MEM_SHARED_BASIC_TYPE:
          category_name = "basic type";
          break;
        case MEM_SHARED_SMALL_DATA:
          category_name = "small data";
          break;
        case MEM_SHARED_LARGE_DATA:
          category_name = "large data";
          break;
        default:
          category_name = "unknown";
          break;
        }
        
      fprintf (file, "Variable: %s\n", name);
      fprintf (file, "  Category: %s\n", category_name);
      fprintf (file, "  Size: %u bytes\n", info->size);
      fprintf (file, "  Target core: %u\n", info->target_core);
      fprintf (file, "  Start offset: 0x%x\n", info->start_offset);
      fprintf (file, "  Distributed: %s\n", info->is_distributed ? "yes" : "no");
      if (info->is_distributed)
        {
          fprintf (file, "  Chunks: %u\n", info->num_chunks);
          fprintf (file, "  Chunk size: %u bytes\n", info->chunk_size);
        }
      fprintf (file, "\n");
    }
}

/* Dump core usage statistics */
void
mem_shared_dump_core_usage (FILE *file)
{
  unsigned int i;
  
  fprintf (file, "\n=== Core Memory Usage ===\n");
  
  for (i = 0; i < mem_shared_num_cores; i++)
    {
      fprintf (file, "Core %u:\n", i);
      fprintf (file, "  Used: %u bytes (%.1f%%)\n", 
              core_memories[i].used_size,
              (float)core_memories[i].used_size * 100.0f / CORE_MEMORY_SIZE);
      fprintf (file, "  Available: %u bytes (%.1f%%)\n",
              core_memories[i].available_size,
              (float)core_memories[i].available_size * 100.0f / CORE_MEMORY_SIZE);
      fprintf (file, "  Allocations: %u\n", core_memories[i].num_allocations);
      fprintf (file, "\n");
    }
}