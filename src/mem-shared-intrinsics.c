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

/* Check if a call expression is an intrinsic call targeting mem_shared data */
bool
mem_shared_is_intrinsic_call (tree call_expr)
{
  tree fndecl, target;
  mem_shared_intrinsic_type_t type;
  
  if (TREE_CODE (call_expr) != CALL_EXPR)
    return false;
    
  fndecl = get_callee_fndecl (call_expr);
  if (!fndecl)
    return false;
    
  type = mem_shared_detect_intrinsic (fndecl);
  if (type == MEM_SHARED_INTRINSIC_UNKNOWN)
    return false;
    
  /* Check if any argument points to mem_shared data */
  target = mem_shared_get_target_from_intrinsic (call_expr);
  return target && mem_shared_decl_p (target);
}

/* Extract the target mem_shared variable from intrinsic call */
tree
mem_shared_get_target_from_intrinsic (tree call_expr)
{
  tree fndecl, arg0;
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
    
  arg0 = CALL_EXPR_ARG (call_expr, 0);
  
  /* Handle address-of expressions */
  if (TREE_CODE (arg0) == ADDR_EXPR)
    arg0 = TREE_OPERAND (arg0, 0);
    
  /* Handle array references */
  while (TREE_CODE (arg0) == ARRAY_REF)
    arg0 = TREE_OPERAND (arg0, 0);
    
  if (TREE_CODE (arg0) == VAR_DECL && mem_shared_decl_p (arg0))
    return arg0;
    
  return NULL_TREE;
}

/* Analyze intrinsic call and populate context */
bool
mem_shared_analyze_intrinsic_call (tree call_expr, mem_shared_intrinsic_context_t *ctx)
{
  tree fndecl, target;
  mem_shared_info_t *info;
  
  memset (ctx, 0, sizeof (*ctx));
  
  fndecl = get_callee_fndecl (call_expr);
  if (!fndecl)
    return false;
    
  ctx->type = mem_shared_detect_intrinsic (fndecl);
  if (ctx->type == MEM_SHARED_INTRINSIC_UNKNOWN)
    return false;
    
  target = mem_shared_get_target_from_intrinsic (call_expr);
  if (!target)
    return false;
    
  ctx->target_decl = target;
  info = mem_shared_get_info (target);
  if (!info)
    return false;
    
  ctx->is_distributed = info->is_distributed;
  
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
    case MEM_SHARED_INTRINSIC_BCOPY:
      if (call_expr_nargs (call_expr) >= 2)
        {
          ctx->src_arg = CALL_EXPR_ARG (call_expr, 1);
          if (call_expr_nargs (call_expr) >= 3)
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
      /* No additional arguments needed */
      break;
      
    default:
      return false;
    }
    
  return true;
}

/* Generate chunk operations for distributed intrinsic */
mem_shared_chunk_op_t *
mem_shared_generate_chunk_operations (mem_shared_intrinsic_context_t *ctx)
{
  mem_shared_info_t *info;
  mem_shared_chunk_op_t *ops = NULL, *current_op;
  unsigned int i, total_size, processed_size = 0;
  
  info = mem_shared_get_info (ctx->target_decl);
  if (!info)
    return NULL;
    
  /* Calculate total operation size */
  if (ctx->size_arg && tree_fits_uhwi_p (ctx->size_arg))
    total_size = tree_to_uhwi (ctx->size_arg);
  else if (mem_shared_needs_size_calculation (ctx->type))
    {
      /* For string operations, we'll need runtime size calculation */
      total_size = info->size; /* Use full variable size as fallback */
    }
  else
    total_size = info->size;
    
  if (!ctx->is_distributed)
    {
      /* Single core operation */
      ops = XNEW (mem_shared_chunk_op_t);
      ops->target_core = info->target_core;
      ops->local_offset = info->start_offset;
      ops->chunk_size = total_size;
      ops->next = NULL;
      ctx->num_chunks = 1;
      return ops;
    }
    
  /* Generate operations for each core */
  for (i = 0; i < mem_shared_num_cores && processed_size < total_size; i++)
    {
      unsigned int chunk_size = info->chunk_size;
      
      /* Adjust chunk size for last chunk */
      if (processed_size + chunk_size > total_size)
        chunk_size = total_size - processed_size;
        
      if (chunk_size == 0)
        break;
        
      /* Create chunk operation */
      current_op = XNEW (mem_shared_chunk_op_t);
      current_op->target_core = (info->target_core + i) % mem_shared_num_cores;
      current_op->local_offset = processed_size % info->chunk_size;
      current_op->chunk_size = chunk_size;
      current_op->next = ops;
      ops = current_op;
      
      processed_size += chunk_size;
      ctx->num_chunks++;
    }
    
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

/* Build address expression for a chunk operation */
tree
mem_shared_build_chunk_address (tree base_addr, unsigned int core, unsigned int offset)
{
  tree core_expr, offset_expr, shift_expr, addr_expr;
  
  /* Create (core << 21) | offset expression */
  core_expr = build_int_cst (sizetype, core);
  shift_expr = build_int_cst (sizetype, 21);
  offset_expr = build_int_cst (sizetype, offset);
  
  /* (core << 21) */
  core_expr = fold_build2 (LSHIFT_EXPR, sizetype, core_expr, shift_expr);
  
  /* (core << 21) | offset */
  addr_expr = fold_build2 (BIT_IOR_EXPR, sizetype, core_expr, offset_expr);
  
  /* Convert to pointer */
  return fold_convert (TREE_TYPE (base_addr), addr_expr);
}

/* Build intrinsic function call expression */
tree
mem_shared_build_intrinsic_call (const char *func_name, tree dst, tree src, tree size)
{
  tree fndecl, call_expr;
  vec<tree, va_gc> *args = NULL;
  
  /* Find the intrinsic function declaration */
  fndecl = builtin_decl_explicit (BUILT_IN_MEMSET);
  if (!fndecl)
    {
      /* Create function declaration if not found */
      tree void_type = void_type_node;
      tree ptr_type = ptr_type_node;
      tree int_type = integer_type_node;
      tree size_type = sizetype;
      tree fntype;
      
      if (strcmp (func_name, "memset") == 0)
        fntype = build_function_type_list (ptr_type, ptr_type, int_type, size_type, NULL_TREE);
      else if (strcmp (func_name, "memcpy") == 0 || strcmp (func_name, "memmove") == 0)
        fntype = build_function_type_list (ptr_type, ptr_type, ptr_type, size_type, NULL_TREE);
      else
        return NULL_TREE;
        
      fndecl = build_decl (UNKNOWN_LOCATION, FUNCTION_DECL,
                          get_identifier (func_name), fntype);
      DECL_EXTERNAL (fndecl) = 1;
      TREE_PUBLIC (fndecl) = 1;
    }
    
  /* Build argument list */
  vec_alloc (args, 3);
  if (dst)
    vec_safe_push (args, dst);
  if (src)
    vec_safe_push (args, src);
  if (size)
    vec_safe_push (args, size);
    
  /* Create call expression */
  call_expr = build_call_expr_loc_vec (UNKNOWN_LOCATION, fndecl, args);
  
  return call_expr;
}

/* Expand memset operation for mem_shared data */
tree
mem_shared_expand_memset (mem_shared_intrinsic_context_t *ctx)
{
  mem_shared_chunk_op_t *ops, *current_op;
  tree stmt_list = NULL_TREE;
  tree base_addr, target_addr;
  
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
                                                  current_op->target_core,
                                                  current_op->local_offset);
      
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
                current_op->target_core, current_op->local_offset, current_op->chunk_size);
    }
    
  mem_shared_free_chunk_operations (ops);
  return stmt_list;
}

/* Expand memcpy operation for mem_shared data */
tree
mem_shared_expand_memcpy (mem_shared_intrinsic_context_t *ctx)
{
  mem_shared_chunk_op_t *ops, *current_op;
  tree stmt_list = NULL_TREE;
  tree base_addr, src_base;
  unsigned int src_offset = 0;
  
  ops = mem_shared_generate_chunk_operations (ctx);
  if (!ops)
    return NULL_TREE;
    
  /* Get base addresses */
  base_addr = build_fold_addr_expr (ctx->target_decl);
  src_base = ctx->src_arg;
  
  /* Generate memcpy call for each chunk */
  for (current_op = ops; current_op; current_op = current_op->next)
    {
      tree chunk_addr, src_addr, chunk_size, memcpy_call;
      
      /* Calculate destination chunk address */
      chunk_addr = mem_shared_build_chunk_address (base_addr,
                                                  current_op->target_core,
                                                  current_op->local_offset);
      
      /* Calculate source address (assume source is regular memory) */
      if (src_offset > 0)
        {
          tree offset_expr = build_int_cst (sizetype, src_offset);
          src_addr = fold_build_pointer_plus (src_base, offset_expr);
        }
      else
        src_addr = src_base;
        
      /* Build chunk size expression */
      chunk_size = build_int_cst (sizetype, current_op->chunk_size);
      
      /* Create memcpy call for this chunk */
      memcpy_call = mem_shared_build_intrinsic_call ("memcpy", chunk_addr,
                                                     src_addr, chunk_size);
      
      /* Add to statement list */
      if (stmt_list)
        append_to_statement_list (memcpy_call, &stmt_list);
      else
        stmt_list = memcpy_call;
        
      src_offset += current_op->chunk_size;
      
      if (flag_dump_mem_shared)
        fprintf (stderr, "[mem_shared] Generated memcpy for core %u, offset 0x%x, size %u\n",
                current_op->target_core, current_op->local_offset, current_op->chunk_size);
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
    fprintf (stderr, "[mem_shared] Replaced %s call with %u chunk operations\n",
            IDENTIFIER_POINTER (DECL_NAME (get_callee_fndecl (call_expr))),
            ctx.num_chunks);
            
  return replacement;
}

/* Warning for potentially slow intrinsic operations */
void
mem_shared_warn_intrinsic_performance (tree call_expr, mem_shared_intrinsic_context_t *ctx)
{
  if (ctx->is_distributed && ctx->num_chunks > 4)
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