/*
 * contains the implementation of all syscalls.
 */

#include <stdint.h>
#include <errno.h>

#include "util/types.h"
#include "syscall.h"
#include "string.h"
#include "process.h"
#include "util/functions.h"
#include "elf.h"

#include "spike_interface/spike_utils.h"

//
// implement the SYS_user_print syscall
//
ssize_t sys_user_print(const char* buf, size_t n) {
  sprint(buf);
  return 0;
}

//
// implement the SYS_user_exit syscall
//
ssize_t sys_user_exit(uint64 code) {
  sprint("User exit with code:%d.\n", code);
  // in lab1, PKE considers only one app (one process). 
  // therefore, shutdown the system when the app calls exit()
  shutdown(code);
}


//
// implement the SYS_user_backtrace syscall
//
ssize_t sys_user_backtrace(uint64 depth)
{
  size_t cur_depth;
  // Direct trace stack frame base ptr
  // as we know -fno-omit-frame-pointer is always on
  // trapframe of do_user_call
  uint64 *ra = (uint64 *)(current->trapframe->regs.ra);
  uint64 *fp = (uint64 *)(current->trapframe->regs.s0) + 2;
  uint64 *sp = (uint64 *)(current->trapframe->regs.sp);
  const char *sym_name;
  // dereference the stack frame pointer of print_backtrace
  sp = (uint64 *)fp;
  ra = (uint64 *)*(fp - 1);
  fp = (uint64 *)*(fp - 2);
  sym_name = elf_get_sym_name(&elfloader, ra);
  // loop through depth
  for (cur_depth = 0;
       cur_depth < depth;
       cur_depth++,
      (sp = (uint64 *)fp),
      (ra = (uint64 *)*(fp - 1)),
      (fp = (uint64 *)*(fp - 2)),
      (sym_name = elf_get_sym_name(&elfloader, ra)))
  {
    // sprint("[%d]fp: 0x%lx, ra: 0x%lx\n", cur_depth,
    //        fp,
    //        ra);
    sprint("%s\n", sym_name);
    if(!strcmp(sym_name, "main") && cur_depth < depth)
      return EL_ERR;
    // for(uint64 * i = fp + 32; i >= sp - 32; i--)
    // {
    //   sprint("%s0x%lx: 0x%lx\n",i==fp?"fp -> ":(i==sp?"sp -> ":"      "), i, *i);
    // }
  }

  return 0;
}

//
// [a0]: the syscall number; [a1] ... [a7]: arguments to the syscalls.
// returns the code of success, (e.g., 0 means success, fail for otherwise)
//
long do_syscall(long a0, long a1, long a2, long a3, long a4, long a5, long a6, long a7) {
  switch (a0) {
    case SYS_user_print:
      return sys_user_print((const char*)a1, a2);
    case SYS_user_exit:
      return sys_user_exit(a1);
    case SYS_user_backtrace:
      return sys_user_backtrace(a1);
    default:
      panic("Unknown syscall %ld \n", a0);
  }
}
