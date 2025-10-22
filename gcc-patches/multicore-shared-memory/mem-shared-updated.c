/* Updated mem-shared.c - Proper integration with GCC instruction selection */

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
#include "optabs.h"
#include "recog.h"

/* ... existing global state and functions ... */

/* Modified to properly emit RTL that will be recognized by instruction selection */
rtx
mem_shared_expand_load (tree decl, HOST_WIDE_INT offset, machine_mode mode)
{
  rtx addr = mem_shared_generate_address (decl, offset);
  if (!addr)
    return NULL_RTX;

  /* Create a memory reference with the shared address */
  rtx mem = gen_rtx_MEM (mode, addr);
  
  /* Set memory attributes */
  set_mem_alias_set (mem, new_alias_set ());
  MEM_VOLATILE_P (mem) = 1; /* Prevent unwanted optimizations */
  
  /* Try to use the mem_shared_load pattern if available */
  if (targetm.have_mem_shared_load && targetm.have_mem_shared_load ())
    {
      /* Emit using the specialized mem_shared_load pattern */
      rtx target = gen_reg_rtx (mode);
      rtx_insn *insn = targetm.gen_mem_shared_load (target, addr);
      if (insn)
        {
          emit_insn (insn);
          return target;
        }
    }
  
  /* Fall back to regular memory access if no specialized pattern */
  return mem;
}

/* Modified to properly emit RTL for stores */
void
mem_shared_expand_store (tree decl, rtx value, HOST_WIDE_INT offset)
{
  machine_mode mode = GET_MODE (value);
  rtx addr = mem_shared_generate_address (decl, offset);
  
  if (!addr)
    return;

  /* Try to use the mem_shared_store pattern if available */
  if (targetm.have_mem_shared_store && targetm.have_mem_shared_store ())
    {
      rtx_insn *insn = targetm.gen_mem_shared_store (addr, value);
      if (insn)
        {
          emit_insn (insn);
          return;
        }
    }

  /* Fall back to regular memory store */
  rtx mem = gen_rtx_MEM (mode, addr);
  set_mem_alias_set (mem, new_alias_set ());
  MEM_VOLATILE_P (mem) = 1;
  
  emit_move_insn (mem, value);
}

/* Function to create RTL patterns that will match the instruction patterns */
rtx
mem_shared_create_load_pattern (rtx target, rtx address, machine_mode mode)
{
  /* Create the RTL pattern that matches our define_insn */
  return gen_rtx_SET (target, gen_rtx_MEM (mode, address));
}

rtx
mem_shared_create_store_pattern (rtx address, rtx source, machine_mode mode)
{
  /* Create the RTL pattern that matches our define_insn */
  return gen_rtx_SET (gen_rtx_MEM (mode, address), source);
}

/* Hook into the target-specific code generation */
bool
mem_shared_recognize_pattern (rtx pattern)
{
  /* Check if this is a mem_shared pattern by examining the address */
  if (GET_CODE (pattern) == SET)
    {
      rtx dest = SET_DEST (pattern);
      rtx src = SET_SRC (pattern);
      
      /* Check for load pattern: (set reg (mem shared_addr)) */
      if (REG_P (dest) && MEM_P (src))
        {
          rtx addr = XEXP (src, 0);
          return mem_shared_address_operand (addr, GET_MODE (addr));
        }
      
      /* Check for store pattern: (set (mem shared_addr) reg) */
      if (MEM_P (dest) && REG_P (src))
        {
          rtx addr = XEXP (dest, 0);
          return mem_shared_address_operand (addr, GET_MODE (addr));
        }
    }
  
  return false;
}

/* ... rest of existing functions ... */