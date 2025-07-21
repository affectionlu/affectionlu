/* mem-shared-intrinsics.h - Intrinsic functions handling for mem_shared variables
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

#ifndef GCC_MEM_SHARED_INTRINSICS_H
#define GCC_MEM_SHARED_INTRINSICS_H

#include "mem-shared.h"

/* Intrinsic function types that need special handling */
typedef enum {
  MEM_SHARED_INTRINSIC_MEMSET,     /* memset() */
  MEM_SHARED_INTRINSIC_MEMCPY,     /* memcpy() */
  MEM_SHARED_INTRINSIC_MEMMOVE,    /* memmove() */
  MEM_SHARED_INTRINSIC_MEMCMP,     /* memcmp() */
  MEM_SHARED_INTRINSIC_BZERO,      /* bzero() */
  MEM_SHARED_INTRINSIC_BCOPY,      /* bcopy() */
  MEM_SHARED_INTRINSIC_STRLEN,     /* strlen() */
  MEM_SHARED_INTRINSIC_STRCPY,     /* strcpy() */
  MEM_SHARED_INTRINSIC_STRNCPY,    /* strncpy() */
  MEM_SHARED_INTRINSIC_STRCMP,     /* strcmp() */
  MEM_SHARED_INTRINSIC_STRNCMP,    /* strncmp() */
  MEM_SHARED_INTRINSIC_UNKNOWN
} mem_shared_intrinsic_type_t;

/* Chunk operation descriptor for distributed operations */
typedef struct mem_shared_chunk_op {
  unsigned int target_core;        /* Target core for this chunk */
  unsigned int source_core;        /* Source core for this chunk */
  unsigned int target_offset;      /* Target offset within core */
  unsigned int source_offset;      /* Source offset within core */
  unsigned int chunk_size;         /* Size of this chunk */
  void *src_ptr;                  /* Source pointer (for copy operations) */
  void *dst_ptr;                  /* Destination pointer */
  struct mem_shared_chunk_op *next; /* Next chunk operation */
} mem_shared_chunk_op_t;

/* Operation context for intrinsic replacement */
typedef struct {
  mem_shared_intrinsic_type_t type;
  tree target_decl;               /* Target mem_shared variable */
  tree source_decl;               /* Source mem_shared variable */
  tree size_arg;                  /* Size argument */
  tree value_arg;                 /* Value argument (for memset) */
  tree src_arg;                   /* Source argument (for copy ops) */
  mem_shared_chunk_op_t *chunk_ops; /* List of chunk operations */
  unsigned int num_chunks;        /* Number of chunk operations */
  bool target_is_distributed;     /* Whether target is distributed */
  bool source_is_distributed;     /* Whether source is distributed */
} mem_shared_intrinsic_context_t;

/* Function prototypes */

/* Detection and analysis */
extern mem_shared_intrinsic_type_t mem_shared_detect_intrinsic (tree fndecl);
extern bool mem_shared_is_intrinsic_call (tree call_expr);
extern tree mem_shared_get_target_from_intrinsic (tree call_expr);
extern tree mem_shared_get_source_from_intrinsic (tree call_expr);
extern bool mem_shared_analyze_intrinsic_call (tree call_expr, 
                                              mem_shared_intrinsic_context_t *ctx);

/* Chunk operation generation */
extern mem_shared_chunk_op_t *mem_shared_generate_chunk_operations (
    mem_shared_intrinsic_context_t *ctx);
extern void mem_shared_free_chunk_operations (mem_shared_chunk_op_t *ops);

/* Code generation and replacement */
extern tree mem_shared_expand_memset (mem_shared_intrinsic_context_t *ctx);
extern tree mem_shared_expand_memcpy (mem_shared_intrinsic_context_t *ctx);
extern tree mem_shared_expand_memmove (mem_shared_intrinsic_context_t *ctx);
extern tree mem_shared_expand_memcmp (mem_shared_intrinsic_context_t *ctx);
extern tree mem_shared_expand_string_op (mem_shared_intrinsic_context_t *ctx);

/* Main replacement function */
extern tree mem_shared_replace_intrinsic_call (tree call_expr);

/* Optimization support */
extern bool mem_shared_can_optimize_intrinsic (mem_shared_intrinsic_context_t *ctx);
extern tree mem_shared_optimize_single_core_intrinsic (mem_shared_intrinsic_context_t *ctx);
extern tree mem_shared_optimize_distributed_intrinsic (mem_shared_intrinsic_context_t *ctx);

/* Helper functions for address calculation */
extern tree mem_shared_build_chunk_address (tree base_addr, unsigned int core,
                                           unsigned int offset);
extern tree mem_shared_build_chunk_size_expr (mem_shared_chunk_op_t *op);
extern tree mem_shared_build_intrinsic_call (const char *func_name, 
                                            tree dst, tree src, tree size);

/* Error handling and diagnostics */
extern void mem_shared_warn_intrinsic_performance (tree call_expr, 
                                                  mem_shared_intrinsic_context_t *ctx);
extern void mem_shared_error_unsupported_intrinsic (tree call_expr);

/* Built-in function registration */
extern void mem_shared_register_intrinsic_builtins (void);

/* Inline helper functions */

static inline bool
mem_shared_is_memory_intrinsic (mem_shared_intrinsic_type_t type)
{
  return (type == MEM_SHARED_INTRINSIC_MEMSET ||
          type == MEM_SHARED_INTRINSIC_MEMCPY ||
          type == MEM_SHARED_INTRINSIC_MEMMOVE ||
          type == MEM_SHARED_INTRINSIC_BZERO ||
          type == MEM_SHARED_INTRINSIC_BCOPY);
}

static inline bool
mem_shared_is_string_intrinsic (mem_shared_intrinsic_type_t type)
{
  return (type == MEM_SHARED_INTRINSIC_STRLEN ||
          type == MEM_SHARED_INTRINSIC_STRCPY ||
          type == MEM_SHARED_INTRINSIC_STRNCPY ||
          type == MEM_SHARED_INTRINSIC_STRCMP ||
          type == MEM_SHARED_INTRINSIC_STRNCMP);
}

static inline bool
mem_shared_is_copy_intrinsic (mem_shared_intrinsic_type_t type)
{
  return (type == MEM_SHARED_INTRINSIC_MEMCPY ||
          type == MEM_SHARED_INTRINSIC_MEMMOVE ||
          type == MEM_SHARED_INTRINSIC_BCOPY ||
          type == MEM_SHARED_INTRINSIC_STRCPY ||
          type == MEM_SHARED_INTRINSIC_STRNCPY);
}

static inline bool
mem_shared_needs_size_calculation (mem_shared_intrinsic_type_t type)
{
  return (type == MEM_SHARED_INTRINSIC_STRLEN ||
          type == MEM_SHARED_INTRINSIC_STRCPY ||
          type == MEM_SHARED_INTRINSIC_STRCMP);
}

static inline bool
mem_shared_has_source_operand (mem_shared_intrinsic_type_t type)
{
  return (type == MEM_SHARED_INTRINSIC_MEMCPY ||
          type == MEM_SHARED_INTRINSIC_MEMMOVE ||
          type == MEM_SHARED_INTRINSIC_BCOPY ||
          type == MEM_SHARED_INTRINSIC_STRCPY ||
          type == MEM_SHARED_INTRINSIC_STRNCPY ||
          type == MEM_SHARED_INTRINSIC_STRCMP ||
          type == MEM_SHARED_INTRINSIC_STRNCMP ||
          type == MEM_SHARED_INTRINSIC_MEMCMP ||
          type == MEM_SHARED_INTRINSIC_STRLEN);
}

#endif /* GCC_MEM_SHARED_INTRINSICS_H */