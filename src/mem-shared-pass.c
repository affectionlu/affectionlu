/* mem-shared-pass.c - GCC pass for mem_shared intrinsic replacement
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
#include "backend.h"
#include "tree.h"
#include "gimple.h"
#include "tree-pass.h"
#include "ssa.h"
#include "gimple-pretty-print.h"
#include "diagnostic-core.h"
#include "fold-const.h"
#include "gimple-fold.h"
#include "tree-eh.h"
#include "gimple-iterator.h"
#include "tree-cfg.h"
#include "tree-into-ssa.h"
#include "tree-dfa.h"
#include "tree-ssa.h"
#include "stringpool.h"
#include "attribs.h"
#include "mem-shared.h"
#include "mem-shared-intrinsics.h"

namespace {

const pass_data pass_data_mem_shared_intrinsics =
{
  GIMPLE_PASS, /* type */
  "mem_shared_intrinsics", /* name */
  OPTGROUP_NONE, /* optinfo_flags */
  TV_NONE, /* tv_id */
  PROP_cfg | PROP_ssa, /* properties_required */
  0, /* properties_provided */
  0, /* properties_destroyed */
  0, /* todo_flags_start */
  TODO_update_ssa | TODO_cleanup_cfg | TODO_verify_ssa /* todo_flags_finish */
};

class pass_mem_shared_intrinsics : public gimple_opt_pass
{
public:
  pass_mem_shared_intrinsics (gcc::context *ctxt)
    : gimple_opt_pass (pass_data_mem_shared_intrinsics, ctxt)
  {}

  /* opt_pass methods: */
  virtual bool gate (function *) { return flag_mem_shared; }
  virtual unsigned int execute (function *);

private:
  bool process_function (function *fun);
  bool process_basic_block (basic_block bb);
  bool replace_intrinsic_call (gimple_stmt_iterator *gsi, gcall *call);
  tree convert_tree_to_gimple (tree expr, gimple_stmt_iterator *gsi);
  void split_statement_list (tree stmt_list, gimple_stmt_iterator *gsi);
}; // class pass_mem_shared_intrinsics

/* Main execute function for the pass */
unsigned int
pass_mem_shared_intrinsics::execute (function *fun)
{
  bool changed = false;
  
  if (!flag_mem_shared)
    return 0;
    
  if (flag_dump_mem_shared)
    fprintf (stderr, "[mem_shared] Processing function %s for intrinsic replacement\n",
            function_name (fun));
            
  changed = process_function (fun);
  
  if (changed && flag_dump_mem_shared)
    fprintf (stderr, "[mem_shared] Function %s: intrinsic replacements made\n",
            function_name (fun));
            
  return changed ? TODO_update_ssa | TODO_cleanup_cfg : 0;
}

/* Process entire function */
bool
pass_mem_shared_intrinsics::process_function (function *fun)
{
  bool changed = false;
  basic_block bb;
  
  FOR_EACH_BB_FN (bb, fun)
    {
      if (process_basic_block (bb))
        changed = true;
    }
    
  return changed;
}

/* Process a single basic block */
bool
pass_mem_shared_intrinsics::process_basic_block (basic_block bb)
{
  gimple_stmt_iterator gsi;
  bool changed = false;
  
  for (gsi = gsi_start_bb (bb); !gsi_end_p (gsi);)
    {
      gimple *stmt = gsi_stmt (gsi);
      
      if (is_gimple_call (stmt))
        {
          gcall *call = as_a <gcall *> (stmt);
          tree call_expr = gimple_call_fn (call);
          
          /* Convert gimple call to tree call_expr for analysis */
          if (gimple_call_fndecl (call))
            {
              tree fndecl = gimple_call_fndecl (call);
              vec<tree, va_gc> *args = NULL;
              unsigned int i;
              
              /* Build argument list */
              vec_alloc (args, gimple_call_num_args (call));
              for (i = 0; i < gimple_call_num_args (call); i++)
                vec_safe_push (args, gimple_call_arg (call, i));
                
              /* Create call expression */
              tree call_tree = build_call_expr_loc_vec (gimple_location (stmt),
                                                       fndecl, args);
              
              /* Check if this is a mem_shared intrinsic call */
              if (mem_shared_is_intrinsic_call (call_tree))
                {
                  if (replace_intrinsic_call (&gsi, call))
                    {
                      changed = true;
                      continue; /* Don't advance iterator, it was moved by replacement */
                    }
                }
            }
        }
        
      gsi_next (&gsi);
    }
    
  return changed;
}

/* Replace an intrinsic call with mem_shared-aware implementation */
bool
pass_mem_shared_intrinsics::replace_intrinsic_call (gimple_stmt_iterator *gsi, 
                                                   gcall *call)
{
  tree fndecl = gimple_call_fndecl (call);
  tree replacement_tree;
  vec<tree, va_gc> *args = NULL;
  unsigned int i;
  
  if (!fndecl)
    return false;
    
  /* Build tree call expression from gimple call */
  vec_alloc (args, gimple_call_num_args (call));
  for (i = 0; i < gimple_call_num_args (call); i++)
    vec_safe_push (args, gimple_call_arg (call, i));
    
  tree call_tree = build_call_expr_loc_vec (gimple_location (call), fndecl, args);
  
  /* Get replacement from intrinsic handler */
  replacement_tree = mem_shared_replace_intrinsic_call (call_tree);
  if (!replacement_tree)
    return false;
    
  if (flag_dump_mem_shared)
    {
      fprintf (stderr, "[mem_shared] Replacing call to %s\n",
              IDENTIFIER_POINTER (DECL_NAME (fndecl)));
      fprintf (stderr, "[mem_shared] Original: ");
      print_gimple_stmt (stderr, call, 0, TDF_SLIM);
    }
    
  /* Convert tree replacement to gimple and insert */
  split_statement_list (replacement_tree, gsi);
  
  /* Remove original call */
  unlink_stmt_vdef (call);
  gsi_remove (gsi, true);
  
  return true;
}

/* Convert tree expression to gimple statements */
tree
pass_mem_shared_intrinsics::convert_tree_to_gimple (tree expr, 
                                                   gimple_stmt_iterator *gsi)
{
  gimple_seq seq = NULL;
  tree result;
  
  /* Use gimplification to convert tree to gimple */
  result = force_gimple_operand (expr, &seq, true, NULL_TREE);
  
  /* Insert generated statements before current position */
  if (seq)
    gsi_insert_seq_before (gsi, seq, GSI_SAME_STMT);
    
  return result;
}

/* Split a statement list into individual gimple statements */
void
pass_mem_shared_intrinsics::split_statement_list (tree stmt_list, 
                                                 gimple_stmt_iterator *gsi)
{
  tree_stmt_iterator tsi;
  
  if (TREE_CODE (stmt_list) == STATEMENT_LIST)
    {
      /* Handle statement list */
      for (tsi = tsi_start (stmt_list); !tsi_end_p (tsi); tsi_next (&tsi))
        {
          tree stmt = tsi_stmt (tsi);
          convert_tree_to_gimple (stmt, gsi);
        }
    }
  else
    {
      /* Handle single statement */
      convert_tree_to_gimple (stmt_list, gsi);
    }
}

} // anon namespace

/* Create pass instance */
gimple_opt_pass *
make_pass_mem_shared_intrinsics (gcc::context *ctxt)
{
  return new pass_mem_shared_intrinsics (ctxt);
}