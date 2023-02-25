#ifndef _PROC_H_
#define _PROC_H_

#include "riscv.h"

typedef struct trapframe_t {
  // space to store context (all common registers)
  /* offset:0   */ riscv_regs regs;

  // process's "user kernel" stack
  /* offset:248 */ uint64 kernel_sp;
  // pointer to smode_trap_handler
  /* offset:256 */ uint64 kernel_trap;
  // saved user process counter
  /* offset:264 */ uint64 epc;

  // kernel page table. added @lab2_1
  /* offset:272 */ uint64 kernel_satp;
}trapframe;

// riscv-pke kernel supports at most 32 processes
#define NPROC 32

// support at most 32 semaphore
// added @ lab3_challenge2
#define NSEM 32

// possible status of a process
enum proc_status {
  FREE,            // unused state
  READY,           // ready state
  RUNNING,         // currently running
  BLOCKED,         // waiting for something
  ZOMBIE,          // terminated but not reclaimed yet
};

// types of a segment
enum segment_type {
  CODE_SEGMENT,    // ELF segment
  DATA_SEGMENT,    // ELF segment
  STACK_SEGMENT,   // runtime segment
  CONTEXT_SEGMENT, // trapframe segment
  SYSTEM_SEGMENT,  // system segment
};

// the VM regions mapped to a user process
typedef struct mapped_region {
  uint64 va;       // mapped virtual address
  uint32 npages;   // mapping_info is unused if npages == 0
  uint32 seg_type; // segment type, one of the segment_types
} mapped_region;

// the extremely simple definition of process, used for begining labs of PKE
typedef struct process_t {
  // pointing to the stack used in trap handling.
  uint64 kstack;
  // user page table
  pagetable_t pagetable;
  // trapframe storing the context of a (User mode) process.
  trapframe* trapframe;

  // points to a page that contains mapped_regions. below are added @lab3_1
  mapped_region *mapped_info;
  // next free mapped region in mapped_info
  int total_mapped_region;

  // process id
  uint64 pid;
  // process status
  int status;
  // parent process
  struct process_t *parent;
  // next queue element
  struct process_t *queue_next;

  // accounting. added @lab3_3
  int tick_count;
}process;

// semaphore structure
// added @ lab3_challenge2
// a semaphore is a tuple (i, q), where i is
// the value of itself, representing free resources
// when equal or higher than 0, and the length of
// q when equal or lower than 0, while q is the waiting
// queue of blocked processes
typedef struct semaphore_s
{
  int valid;
  int64 i;
  process *wait_queue_head;
} semaphore_t;

// switch to run user app
void switch_to(process*);

// initialize process pool (the procs[] array)
void init_proc_pool();
// allocate an empty process, init its vm space. returns its pid
process* alloc_process();
// set process state to zombie.
int free_process( process* proc );
// actually delete process
int delete_process( process* proc );
// fork a child from parent
int do_fork(process* parent);
// initialize process pool (the procs[] array)
void init_proc_pool();

// semaphore operations
// added @ lab3_challenge2
// get new semaphore
size_t do_sem_new(uint64 init_value);
// semaphore P operation
void do_sem_p(size_t id);
// semaphore V operation
void do_sem_v(size_t id);

// current running process
extern process* current;

// address of the first free page in our simple heap. added @lab2_2
extern uint64 g_ufree_page;

// process pool. added @lab3_1
extern process procs[NPROC];

#endif
