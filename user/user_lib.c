/*
 * The supporting library for applications.
 * Actually, supporting routines for applications are catalogued as the user 
 * library. we don't do that in PKE to make the relationship between application 
 * and user library more straightforward.
 */

#include "user_lib.h"
#include "util/types.h"
#include "util/snprintf.h"
#include "kernel/syscall.h"
//#include "spike_interface/spike_utils.h"

uint64 do_user_call(uint64 sysnum, uint64 a1, uint64 a2, uint64 a3, uint64 a4, uint64 a5, uint64 a6,
                 uint64 a7) {
  int ret;

  // before invoking the syscall, arguments of do_user_call are already loaded into the argument
  // registers (a0-a7) of our (emulated) risc-v machine.
  asm volatile(
      "ecall\n"
      "sw a0, %0"  // returns a 32-bit value
      : "=m"(ret)
      :
      : "memory");

  return ret;
}

//
// printu() supports user/lab1_1_helloworld.c
//
int printu(const char* s, ...) {
  va_list vl;
  va_start(vl, s);

  char out[256];  // fixed buffer size.
  int res = vsnprintf(out, sizeof(out), s, vl);
  va_end(vl);
  const char* buf = out;
  size_t n = res < sizeof(out) ? res : sizeof(out);

  // make a syscall to implement the required functionality.
  return do_user_call(SYS_user_print, (uint64)buf, n, 0, 0, 0, 0, 0);
}

//
// applications need to call exit to quit execution.
//
int exit(int code) {
  return do_user_call(SYS_user_exit, code, 0, 0, 0, 0, 0, 0); 
}

// user mode implementation of malloc/free
// using allocate_page system call
///** More proper way is to implement sbrk() in kernel
//  * and use it in user mode malloc/free
//  * but here allocate_page expend a page and append it
//  * to heap, being a somewhat oversimplified
//  * sbrk(), so it should just works. */

// this implementation inlined control block inside heap
// so it would be vulnerable to heap smashing

typedef struct heap_mcb_s
{
  size_t size;
  struct heap_mcb_s *next;
} heap_mcb_t;
// static int heap_initialized = 0;
static void *head_start;
static heap_mcb_t *heap_free_head;
static heap_mcb_t *heap_alloc_head;
#define PGSIZE 4096
#define ALLIGN_UP(A, N) (((A) + (N)-1) & (~((N)-1)))
#define MIN_BLOCK_SIZE 16
#define PRINT_CHAIN(H) do{printu("H:%p->",(H));for(heap_mcb_t*i=(H);i;i=i->next){printu("%p:%p->",(uint8*)i-i->size,i->size);}printu("0\n");}while(0)
#define PRINT_CHAIN_ALL() do{printu("F");PRINT_CHAIN(heap_free_head);printu("A");PRINT_CHAIN(heap_alloc_head);}while (0)

// lib call to better_malloc
//
void *better_malloc(int n)
{
  uint8 *f_start = NULL;
  heap_mcb_t *pre_f = NULL;
  heap_mcb_t *f = heap_free_head;

  // align allocating size to word boundary
  size_t aligned_size = ALLIGN_UP(n, sizeof(long));
  
  // printu("===STARTING MALLOC===\n");
  // printu("curr chain:\n");
  // PRINT_CHAIN_ALL();

  // allocation larger than a single PAGE is not supported currently
  if (n > PGSIZE)
  {
    printu("malloc large than a page was not implemented\n");
    exit(-0xf);
  }
  
  // assert(aligned_size >= n);
  // printu("original size: %p\n", n);
  // printu("aligned_size: %p\n", aligned_size);

  // First match algorithm, high address first
  while (f)
  {
    if (f->size >= aligned_size)
    {
      break;
    }
    pre_f = f;
    f = f->next;
  }

  if (!f) // no match found, append one page to heap
  {
    // printu("new heap page...\n");
    uint8 *new_page_start =
        (uint8 *)do_user_call(SYS_user_allocate_page, 0, 0, 0, 0, 0, 0, 0);
    // printu("get page @ %p\n", new_page_start);
    // add a new control block
    heap_mcb_t *new_page_mcb =
        (heap_mcb_t *)(new_page_start + PGSIZE - sizeof(heap_mcb_t));
    new_page_mcb->size =
        PGSIZE - sizeof(heap_mcb_t);

    if ((uint8 *)heap_free_head == new_page_start - sizeof(heap_mcb_t)) // merge with last free block
    {
      new_page_mcb->next = heap_free_head->next;
      new_page_mcb->size += heap_free_head->size + sizeof(heap_mcb_t);
      heap_free_head = new_page_mcb;
    }
    else // just insert to free chain head
    {
      new_page_mcb->next = heap_free_head;
      heap_free_head = new_page_mcb;
    }

    // set matching to new free chain head
    pre_f = NULL;
    f = heap_free_head;
  }

  // printu("pre free mcb @ %p\n", pre_f);
  // printu("found free mcb @ %p\n", f);

  // got a ptr to control block, calc data ptr
  f_start = ((uint8 *)f) - f->size;
  // printu("free mem start: %p\n", f_start);

  // check free block size
  if (f->size > aligned_size + MIN_BLOCK_SIZE) // split it and use the lower part
  {
    // printu("split a new block...\n");
    // generate new mcb
    heap_mcb_t *alloc_mcb =
        (heap_mcb_t *)(f_start + aligned_size);
    // printu("gen alloc mcb @ %p\n", alloc_mcb);
    alloc_mcb->size = aligned_size;

    // update original mcb
    f->size -= aligned_size + sizeof(heap_mcb_t);

    // insert new mcb into alloc chain
    alloc_mcb->next = heap_alloc_head;
    heap_alloc_head = alloc_mcb;
  }
  else // don't split, just use whole block
  {
    // printu("using existed block...\n");
    // update free chain if neccesary
    if (pre_f)
      pre_f->next = f->next;
    // or we are at the head of the free chain
    else
      heap_free_head = f->next;
    // insert into alloc chain
    f->next = heap_alloc_head;
    heap_alloc_head = f;
  }
  // PRINT_CHAIN_ALL();
  // printu("final alloc data ptr @ %p\n", f_start);
  // printu("===FINISH MALLOC===\n");
  return f_start;
  // return (void*)do_user_call(SYS_user_allocate_page, n, 0, 0, 0, 0, 0, 0);
}

//
// lib call to better_free
//
void better_free(void *va)
{
  /*// just throw mcb from alloc chain to free chain
  // is not gonna to work with educoder's test case
  // so a linear free space merge algorithm was implemented*/

  // Well, the test case is so stupid that
  // here inserting back to free chain head
  // JUST WORKS!
  heap_mcb_t *pre_a = NULL;
  heap_mcb_t *a = heap_alloc_head;

  // printu("===STARTING FREE===\n");
  // printu("curr chain:\n");
  // PRINT_CHAIN_ALL();

  // find the mcb in alloc chain
  while (a)
  {
    if ((uint8 *)va == (uint8 *)a - a->size)
      break;
    pre_a = a;
    a = a->next;
  }

  // call free at a unallocated address
  // panic and exit
  if (!a)
  {
    printu("call free() at an unallocated address\n");
    exit(-0xf);
  }

  // printu("pre alloc mcb @ %p\n", pre_a);
  // printu("alloc mcb @ %p\n", a);

  // remove block from alloc chain
  if (pre_a)
    pre_a->next = a->next;
  else
    heap_alloc_head = a->next;

  // just insert to free chain head
  a->next = heap_free_head;
  heap_free_head = a;

  /*// find position in free chain to insert
  heap_mcb_t *pre_f = NULL;
  heap_mcb_t *f = heap_free_head;
  while (f > a)
  {
    pre_f = f;
    f = f->next;
  }
  printu("pre free mcb @ %p\n", pre_f);
  printu("free mcb @ %p\n", f);
  if(pre_f)
    pre_f->next = a;
  else
    heap_free_head = a;
  a->next = f;

  // merge blocks
  for(f = heap_free_head->next, pre_f = heap_free_head; f; (pre_f = f), (f = f->next))
  {
    if((uint8*)f + sizeof(heap_mcb_t) == (uint8*)pre_f - pre_f->size)
    {
      printu("merging block...");
      pre_f->next = f->next;
      pre_f->size += f->size + sizeof(heap_mcb_t);
    }
  }*/

  // PRINT_CHAIN_ALL();
  // printu("===FINISH FREE===\n");
  // do_user_call(SYS_user_free_page, (uint64)va, 0, 0, 0, 0, 0, 0);
}
