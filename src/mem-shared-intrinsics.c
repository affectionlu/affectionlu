/* mem-shared-intrinsics.c - Implementation of intrinsic function handling
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
#include "gimple.h"
#include "tree-pass.h"
#include "tree-iterator.h"
#include "gimple-iterator.h"
#include "diagnostic.h"
#include "fold-const.h"
#include "stor-layout.h"
#include "calls.h"
#include "builtins.h"
#include "mem-shared.h"
#include "mem-shared-intrinsics.h"

/* Static function name to intrinsic type mapping */
static const struct {
  const char *name;
  mem_shared_intrinsic_type_t type;
} intrinsic_name_map[] = {
  { "memset",  MEM_SHARED_INTRINSIC_MEMSET },
  { "memcpy",  MEM_SHARED_INTRINSIC_MEMCPY },
  { "memmove", MEM_SHARED_INTRINSIC_MEMMOVE },
  { "memcmp",  MEM_SHARED_INTRINSIC_MEMCMP },
  { "bzero",   MEM_SHARED_INTRINSIC_BZERO },
  { "bcopy",   MEM_SHARED_INTRINSIC_BCOPY },
  { "strlen",  MEM_SHARED_INTRINSIC_STRLEN },
  { "strcpy",  MEM_SHARED_INTRINSIC_STRCPY },
  { "strncpy", MEM_SHARED_INTRINSIC_STRNCPY },
  { "strcmp",  MEM_SHARED_INTRINSIC_STRCMP },
  { "strncmp", MEM_SHARED_INTRINSIC_STRNCMP },
  { NULL,      MEM_SHARED_INTRINSIC_UNKNOWN }
};

/* Detect intrinsic function type from function declaration */
mem_shared_intrinsic_type_t
mem_shared_detect_intrinsic (tree fndecl)
{
  const char *name;
  int i;
  
  if (!fndecl || TREE_CODE (fndecl) != FUNCTION_DECL)
    return MEM_SHARED_INTRINSIC_UNKNOWN;
    
  name = IDENTIFIER_POINTER (DECL_NAME (fndecl));
  if (!name)
    return MEM_SHARED_INTRINSIC_UNKNOWN;
    
  for (i = 0; intrinsic_name_map[i].name; i++)
    {
      if (strcmp (name, intrinsic_name_map[i].name) == 0)
        return intrinsic_name_map[i].type;
    }
    
  return MEM_SHARED_INTRINSIC_UNKNOWN;
}

/* Extract mem_shared variable from an argument expression */
static tree
extract_mem_shared_var (tree arg)
{
  /* Handle address-of expressions */
  if (TREE_CODE (arg) == ADDR_EXPR)
    arg = TREE_OPERAND (arg, 0);
    
  /* Handle array references */
  while (TREE_CODE (arg) == ARRAY_REF)
    arg = TREE_OPERAND (arg, 0);
    
  if (TREE_CODE (arg) == VAR_DECL && mem_shared_decl_p (arg))
    return arg;
    
  return NULL_TREE;
}

/* Check if a call expression involves mem_shared data */
bool
mem_shared_is_intrinsic_call (tree call_expr)
{
  tree fndecl;
  mem_shared_intrinsic_type_t type;
  unsigned int i, nargs;
  
  if (TREE_CODE (call_expr) != CALL_EXPR)
    return false;
    
  fndecl = get_callee_fndecl (call_expr);
  if (!fndecl)
    return false;
    
  type = mem_shared_detect_intrinsic (fndecl);
  if (type == MEM_SHARED_INTRINSIC_UNKNOWN)
    return false;
    
  /* Check if any argument involves mem_shared data */
  nargs = call_expr_nargs (call_expr);
  for (i = 0; i < nargs; i++)
    {
      tree arg = CALL_EXPR_ARG (call_expr, i);
      if (extract_mem_shared_var (arg))
        return true;
    }
    
  return false;
}

/* Extract the target and source mem_shared variables from intrinsic call */
tree
mem_shared_get_target_from_intrinsic (tree call_expr)
{
  tree fndecl;
  mem_shared_intrinsic_type_t type;
  
  if (TREE_CODE (call_expr) != CALL_EXPR)
    return NULL_TREE;
    
  fndecl = get_callee_fndecl (call_expr);
  if (!fndecl)
    return NULL_TREE;
    
  type = mem_shared_detect_intrinsic (fndecl);
  if (type == MEM_SHARED_INTRINSIC_UNKNOWN)
    return NULL_TREE;
    
  /* First argument is usually the target for most intrinsics */
  if (call_expr_nargs (call_expr) < 1)
    return NULL_TREE;
    
  return extract_mem_shared_var (CALL_EXPR_ARG (call_expr, 0));
}

/* Get source mem_shared variable from intrinsic call */
tree
mem_shared_get_source_from_intrinsic (tree call_expr)
{
  tree fndecl;
  mem_shared_intrinsic_type_t type;
  unsigned int src_arg_index;
  
  if (TREE_CODE (call_expr) != CALL_EXPR)
    return NULL_TREE;
    
  fndecl = get_callee_fndecl (call_expr);
  if (!fndecl)
    return NULL_TREE;
    
  type = mem_shared_detect_intrinsic (fndecl);
  
  /* Determine source argument index based on function type */
  switch (type)
    {
    case MEM_SHARED_INTRINSIC_MEMCPY:
    case MEM_SHARED_INTRINSIC_MEMMOVE:
    case MEM_SHARED_INTRINSIC_STRCPY:
    case MEM_SHARED_INTRINSIC_STRNCPY:
    case MEM_SHARED_INTRINSIC_STRCMP:
    case MEM_SHARED_INTRINSIC_STRNCMP:
    case MEM_SHARED_INTRINSIC_MEMCMP:
      src_arg_index = 1;  /* Second argument is source */
      break;
      
    case MEM_SHARED_INTRINSIC_BCOPY:
      src_arg_index = 0;  /* First argument is source for bcopy */
      break;
      
    case MEM_SHARED_INTRINSIC_STRLEN:
      src_arg_index = 0;  /* First argument is source for strlen */
      break;
      
    default:
      return NULL_TREE;  /* No source argument */
    }
    
  if (call_expr_nargs (call_expr) <= src_arg_index)
    return NULL_TREE;
    
  return extract_mem_shared_var (CALL_EXPR_ARG (call_expr, src_arg_index));
}

/* Analyze intrinsic call and populate context */
bool
mem_shared_analyze_intrinsic_call (tree call_expr, mem_shared_intrinsic_context_t *ctx)
{
  tree fndecl, target, source;
  mem_shared_info_t *target_info = NULL, *source_info = NULL;
  
  memset (ctx, 0, sizeof (*ctx));
  
  fndecl = get_callee_fndecl (call_expr);
  if (!fndecl)
    return false;
    
  ctx->type = mem_shared_detect_intrinsic (fndecl);
  if (ctx->type == MEM_SHARED_INTRINSIC_UNKNOWN)
    return false;
    
  /* Get target and source variables */
  target = mem_shared_get_target_from_intrinsic (call_expr);
  source = mem_shared_get_source_from_intrinsic (call_expr);
  
  /* At least one must be mem_shared */
  if (!target && !source)
    return false;
    
  ctx->target_decl = target;
  ctx->source_decl = source;
  
  /* Get allocation info */
  if (target)
    {
      target_info = mem_shared_get_info (target);
      if (target_info)
        ctx->target_is_distributed = target_info->is_distributed;
    }
    
  if (source)
    {
      source_info = mem_shared_get_info (source);
      if (source_info)
        ctx->source_is_distributed = source_info->is_distributed;
    }
    
  /* Extract arguments based on intrinsic type */
  switch (ctx->type)
    {
    case MEM_SHARED_INTRINSIC_MEMSET:
    case MEM_SHARED_INTRINSIC_BZERO:
      if (call_expr_nargs (call_expr) >= 2)
        {
          ctx->value_arg = CALL_EXPR_ARG (call_expr, 1);
          if (call_expr_nargs (call_expr) >= 3)
            ctx->size_arg = CALL_EXPR_ARG (call_expr, 2);
        }
      break;
      
    case MEM_SHARED_INTRINSIC_MEMCPY:
    case MEM_SHARED_INTRINSIC_MEMMOVE:
      if (call_expr_nargs (call_expr) >= 2)
        {
          ctx->src_arg = CALL_EXPR_ARG (call_expr, 1);
          if (call_expr_nargs (call_expr) >= 3)
            ctx->size_arg = CALL_EXPR_ARG (call_expr, 2);
        }
      break;
      
    case MEM_SHARED_INTRINSIC_BCOPY:
      if (call_expr_nargs (call_expr) >= 3)
        {
          ctx->src_arg = CALL_EXPR_ARG (call_expr, 0);   /* src is first for bcopy */
          ctx->target_decl = extract_mem_shared_var (CALL_EXPR_ARG (call_expr, 1)); /* dst is second */
          ctx->size_arg = CALL_EXPR_ARG (call_expr, 2);
        }
      break;
      
    case MEM_SHARED_INTRINSIC_STRNCPY:
    case MEM_SHARED_INTRINSIC_STRNCMP:
      if (call_expr_nargs (call_expr) >= 2)
        {
          ctx->src_arg = CALL_EXPR_ARG (call_expr, 1);
          if (call_expr_nargs (call_expr) >= 3)
            ctx->size_arg = CALL_EXPR_ARG (call_expr, 2);
        }
      break;
      
    case MEM_SHARED_INTRINSIC_STRCPY:
    case MEM_SHARED_INTRINSIC_STRCMP:
      if (call_expr_nargs (call_expr) >= 2)
        ctx->src_arg = CALL_EXPR_ARG (call_expr, 1);
      break;
      
    case MEM_SHARED_INTRINSIC_STRLEN:
      /* Source is the only argument */
      ctx->src_arg = CALL_EXPR_ARG (call_expr, 0);
      break;
      
    default:
      return false;
    }
    
  return true;
}

/* Generate chunk operations for distributed intrinsic operations */
mem_shared_chunk_op_t *
mem_shared_generate_chunk_operations (mem_shared_intrinsic_context_t *ctx)
{
  mem_shared_chunk_op_t *ops = NULL, *current_op;
  mem_shared_info_t *target_info = NULL, *source_info = NULL;
  unsigned int num_chunks, i;
  bool target_is_distributed = false, source_is_distributed = false;
  
  /* Get allocation info for target and source */
  if (ctx->target_decl)
    {
      target_info = mem_shared_get_info (ctx->target_decl);
      target_is_distributed = target_info ? target_info->is_distributed : false;
    }
    
  if (ctx->source_decl)
    {
      source_info = mem_shared_get_info (ctx->source_decl);
      source_is_distributed = source_info ? source_info->is_distributed : false;
    }
    
  /* Determine number of chunks based on distribution */
  if (target_is_distributed || source_is_distributed)
    {
      /* Use total cores for distributed data */
      num_chunks = mem_shared_total_cores;
    }
  else
    {
      /* Single chunk for non-distributed data */
      num_chunks = 1;
    }
    
  ctx->num_chunks = num_chunks;
  ctx->target_is_distributed = target_is_distributed;
  ctx->source_is_distributed = source_is_distributed;
  
  /* Generate chunk operations */
  for (i = 0; i < num_chunks; i++)
    {
      current_op = XNEW (mem_shared_chunk_op_t);
      
      /* Calculate target core information */
      if (target_is_distributed)
        {
          unsigned int target_core = i;
          current_op->target_group = target_core / CORES_PER_GROUP;
          current_op->target_intra_id = target_core % CORES_PER_GROUP;
          current_op->target_offset = target_info->start_offset + 
                                    (i * target_info->chunk_size);
        }
      else if (target_info)
        {
          current_op->target_group = target_info->target_group;
          current_op->target_intra_id = target_info->intra_group_id;
          current_op->target_offset = target_info->start_offset;
        }
      else
        {
          /* Regular memory target */
          current_op->target_group = 0;
          current_op->target_intra_id = 0;
          current_op->target_offset = 0;
        }
        
      /* Calculate source core information */
      if (source_is_distributed)
        {
          unsigned int source_core = i;
          current_op->source_group = source_core / CORES_PER_GROUP;
          current_op->source_intra_id = source_core % CORES_PER_GROUP;
          current_op->source_offset = source_info->start_offset + 
                                    (i * source_info->chunk_size);
        }
      else if (source_info)
        {
          current_op->source_group = source_info->target_group;
          current_op->source_intra_id = source_info->intra_group_id;
          current_op->source_offset = source_info->start_offset;
        }
      else
        {
          /* Regular memory source */
          current_op->source_group = 0;
          current_op->source_intra_id = 0;
          current_op->source_offset = i * (target_info ? target_info->chunk_size : 1024);
        }
        
      /* Calculate chunk size */
      if (target_is_distributed && target_info)
        {
          current_op->chunk_size = target_info->chunk_size;
          /* Adjust last chunk size */
          if (i == num_chunks - 1)
            {
              unsigned int remaining = target_info->size - (target_info->chunk_size * (num_chunks - 1));
              if (remaining < target_info->chunk_size)
                current_op->chunk_size = remaining;
            }
        }
      else if (source_is_distributed && source_info)
        {
          current_op->chunk_size = source_info->chunk_size;
          /* Adjust last chunk size */
          if (i == num_chunks - 1)
            {
              unsigned int remaining = source_info->size - (source_info->chunk_size * (num_chunks - 1));
              if (remaining < source_info->chunk_size)
                current_op->chunk_size = remaining;
            }
        }
      else
        {
          /* Default chunk size for non-distributed operations */
          current_op->chunk_size = (target_info ? target_info->size : 1024);
        }
        
      /* Link to list */
      current_op->next = ops;
      ops = current_op;
    }
    
  if (flag_dump_mem_shared)
    fprintf (stderr, "[mem_shared] Generated %u chunk operations for intrinsic (target:%s, source:%s)\n",
            num_chunks, 
            target_is_distributed ? "distributed" : "single",
            source_is_distributed ? "distributed" : "single");
            
  return ops;
}

/* Free chunk operations list */
void
mem_shared_free_chunk_operations (mem_shared_chunk_op_t *ops)
{
  mem_shared_chunk_op_t *current, *next;
  
  for (current = ops; current; current = next)
    {
      next = current->next;
      free (current);
    }
}

/* Build address for a chunk in dual-core group architecture */
tree
mem_shared_build_chunk_address (tree base_addr, unsigned int group_id,
                               unsigned int intra_id, unsigned int offset)
{
  tree encoded_addr;
  HOST_WIDE_INT addr_value = offset;
  
  /* Set cross-core access bit (bit 29) */
  addr_value = mem_shared_set_cross_core_bit (addr_value);
  
  /* Clear reserved bit (bit 27) */
  addr_value = mem_shared_clear_reserved_bit (addr_value);
  
  /* Set group ID in bits 26:21 */
  addr_value = mem_shared_set_group_bits (addr_value, group_id);
  
  /* Set intra-group ID in bit 20 */
  addr_value = mem_shared_set_intra_id_bit (addr_value, intra_id);
  
  /* Create the encoded address expression */
  encoded_addr = build_int_cst (sizetype, addr_value);
  
  /* Convert to pointer type and return */
  return fold_convert (TREE_TYPE (base_addr), encoded_addr);
}

/* Build intrinsic function call */
tree
mem_shared_build_intrinsic_call (const char *func_name, tree dst, tree src, tree size)
{
  tree fndecl, call_expr;
  vec<tree, va_gc> *args = NULL;
  
  /* Look up the function declaration */
  fndecl = builtin_decl_explicit (BUILT_IN_MEMCPY);  /* Use memcpy as template */
  if (!fndecl)
    return NULL_TREE;
    
  /* Build argument list */
  vec_alloc (args, 3);
  args->quick_push (dst);
  if (src)
    args->quick_push (src);
  if (size)
    args->quick_push (size);
    
  /* Build function call */
  call_expr = build_call_expr_loc_vec (UNKNOWN_LOCATION, fndecl, args);
  
  return call_expr;
}

/* Expand memset operation for mem_shared data */
tree
mem_shared_expand_memset (mem_shared_intrinsic_context_t *ctx)
{
  mem_shared_chunk_op_t *ops, *current_op;
  tree stmt_list = NULL_TREE;
  tree base_addr;
  
  ops = mem_shared_generate_chunk_operations (ctx);
  if (!ops)
    return NULL_TREE;
    
  /* Get base address of target */
  base_addr = build_fold_addr_expr (ctx->target_decl);
  
  /* Generate memset call for each chunk */
  for (current_op = ops; current_op; current_op = current_op->next)
    {
      tree chunk_addr, chunk_size, memset_call;
      
      /* Calculate chunk address */
      chunk_addr = mem_shared_build_chunk_address (base_addr, 
                                                  current_op->target_group,
                                                  current_op->target_intra_id,
                                                  current_op->target_offset);
      
      /* Build chunk size expression */
      chunk_size = build_int_cst (sizetype, current_op->chunk_size);
      
      /* Create memset call for this chunk */
      memset_call = mem_shared_build_intrinsic_call ("memset", chunk_addr, 
                                                     ctx->value_arg, chunk_size);
      
      /* Add to statement list */
      if (stmt_list)
        append_to_statement_list (memset_call, &stmt_list);
      else
        stmt_list = memset_call;
        
      if (flag_dump_mem_shared)
        fprintf (stderr, "[mem_shared] Generated memset for core %u, offset 0x%x, size %u\n",
                current_op->target_core, current_op->target_offset, current_op->chunk_size);
    }
    
  mem_shared_free_chunk_operations (ops);
  return stmt_list;
}

/* Expand memcpy operation for mem_shared variables */
tree
mem_shared_expand_memcpy (mem_shared_intrinsic_context_t *ctx)
{
  mem_shared_chunk_op_t *ops, *current_op;
  tree stmt_list = NULL_TREE;
  tree target_base = NULL, source_base = NULL;

  ops = mem_shared_generate_chunk_operations (ctx);
  if (!ops)
    return NULL_TREE;

  /* Get base addresses */
  if (ctx->target_decl)
    target_base = build_fold_addr_expr (ctx->target_decl);
  if (ctx->source_decl)
    source_base = build_fold_addr_expr (ctx->source_decl);
  else
    source_base = ctx->src_arg;  /* Regular memory source */

  /* Generate memcpy call for each chunk */
  for (current_op = ops; current_op; current_op = current_op->next)
    {
      tree target_addr, source_addr, chunk_size, memcpy_call;

      /* Calculate target address */
      if (ctx->target_decl && mem_shared_decl_p (ctx->target_decl))
        {
          target_addr = mem_shared_build_chunk_address (target_base,
                                                      current_op->target_group,
                                                      current_op->target_intra_id,
                                                      current_op->target_offset);
        }
      else
        {
          /* Regular memory target */
          tree offset_expr = build_int_cst (sizetype, current_op->target_offset);
          target_addr = fold_build_pointer_plus (target_base, offset_expr);
        }

      /* Calculate source address */
      if (ctx->source_decl && mem_shared_decl_p (ctx->source_decl))
        {
          source_addr = mem_shared_build_chunk_address (source_base,
                                                      current_op->source_group,
                                                      current_op->source_intra_id,
                                                      current_op->source_offset);
        }
      else
        {
          /* Regular memory source */
          tree offset_expr = build_int_cst (sizetype, current_op->source_offset);
          source_addr = fold_build_pointer_plus (source_base, offset_expr);
        }

      /* Build chunk size expression */
      chunk_size = build_int_cst (sizetype, current_op->chunk_size);

      /* Create memcpy call for this chunk */
      memcpy_call = mem_shared_build_intrinsic_call ("memcpy", target_addr,
                                                     source_addr, chunk_size);

      /* Add to statement list */
      if (stmt_list)
        append_to_statement_list (memcpy_call, &stmt_list);
      else
        stmt_list = memcpy_call;

      if (flag_dump_mem_shared)
        {
          fprintf (stderr, "[mem_shared] Generated memcpy chunk: ");
          if (ctx->target_decl && mem_shared_decl_p (ctx->target_decl))
            fprintf (stderr, "target group %u intra %u offset 0x%x, ",
                    current_op->target_group, current_op->target_intra_id, 
                    current_op->target_offset);
          if (ctx->source_decl && mem_shared_decl_p (ctx->source_decl))
            fprintf (stderr, "source group %u intra %u offset 0x%x, ",
                    current_op->source_group, current_op->source_intra_id,
                    current_op->source_offset);
          fprintf (stderr, "size %u\n", current_op->chunk_size);
        }
    }

  mem_shared_free_chunk_operations (ops);
  return stmt_list;
}

/* Main function to replace intrinsic calls */
tree
mem_shared_replace_intrinsic_call (tree call_expr)
{
  mem_shared_intrinsic_context_t ctx;
  tree replacement = NULL_TREE;
  
  if (!mem_shared_analyze_intrinsic_call (call_expr, &ctx))
    return NULL_TREE;
    
  /* Generate replacement based on intrinsic type */
  switch (ctx.type)
    {
    case MEM_SHARED_INTRINSIC_MEMSET:
    case MEM_SHARED_INTRINSIC_BZERO:
      replacement = mem_shared_expand_memset (&ctx);
      break;
      
    case MEM_SHARED_INTRINSIC_MEMCPY:
    case MEM_SHARED_INTRINSIC_MEMMOVE:
      replacement = mem_shared_expand_memcpy (&ctx);
      break;
      
    case MEM_SHARED_INTRINSIC_MEMCMP:
      /* TODO: Implement memcmp expansion */
      mem_shared_warn_intrinsic_performance (call_expr, &ctx);
      break;
      
    default:
      mem_shared_error_unsupported_intrinsic (call_expr);
      break;
    }
    
  if (replacement && flag_dump_mem_shared)
    {
      const char *operation_desc = "unknown";
      if (ctx.target_decl && ctx.source_decl)
        operation_desc = "mem_shared to mem_shared";
      else if (ctx.target_decl)
        operation_desc = "regular to mem_shared";
      else if (ctx.source_decl)
        operation_desc = "mem_shared to regular";
        
      fprintf (stderr, "[mem_shared] Replaced %s call (%s) with %u chunk operations\n",
              IDENTIFIER_POINTER (DECL_NAME (get_callee_fndecl (call_expr))),
              operation_desc, ctx.num_chunks);
    }
            
  return replacement;
}

/* Warning for potentially slow intrinsic operations */
void
mem_shared_warn_intrinsic_performance (tree call_expr, mem_shared_intrinsic_context_t *ctx)
{
  if (ctx->num_chunks > 4)
    {
      warning_at (EXPR_LOCATION (call_expr), 0,
                 "mem_shared intrinsic operation will generate %u separate calls "
                 "which may impact performance", ctx->num_chunks);
    }
}

/* Error for unsupported intrinsic operations */
void
mem_shared_error_unsupported_intrinsic (tree call_expr)
{
  tree fndecl = get_callee_fndecl (call_expr);
  error_at (EXPR_LOCATION (call_expr),
           "intrinsic function %qD not yet supported for mem_shared variables",
           fndecl);
}