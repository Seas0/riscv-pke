#include "kernel/riscv.h"
#include "kernel/process.h"
#include "spike_interface/spike_utils.h"
#include "util/string.h"

static void print_error_line()
{
  ssize_t i = 0;
  uint64 pc = read_csr(mepc),
         line = 0,
         path_idx = 0,
         file_idx = 0;
  const char *path = NULL, *file = NULL;
  static char rel_path[512];

  // find the meta data by iterating over all possible addr
  for (i = 0; i < current->line_ind; i++)
  {
    if (pc == current->line[i].addr)
    {
      line = current->line[i].line;
      file_idx = current->line[i].file;
      path_idx = current->file[file_idx].dir;
      file = current->file[file_idx].file;
      path = current->dir[path_idx];
      break;
    }
  }

  // get full relative path
  ssize_t path_len = strlen(path),
          file_len = strlen(file);
  strcpy(rel_path, path);
  rel_path[path_len] = '/';
  strcpy(rel_path + path_len + 1, file);
  
  sprint("Runtime error at %s:%ld\n", rel_path, line);

  // open src file to read line
  spike_file_t *f = spike_file_open(rel_path, O_RDONLY, 0);
  char c = 0;
  i = 1;
  while (spike_file_read(f, &c, 1) != 0)
  {
    if (c == '\n')
      i++;
    if (i == line)
      break;
  }
  while (spike_file_read(f, &c, 1) != 0)
  {
    sprint("%c", c);
    if (c == '\n')
      break;
  }
  spike_file_close(f);
}

static void handle_instruction_access_fault() { panic("Instruction access fault!"); }

static void handle_load_access_fault() { panic("Load access fault!"); }

static void handle_store_access_fault() { panic("Store/AMO access fault!"); }

static void handle_illegal_instruction() { panic("Illegal instruction!"); }

static void handle_misaligned_load() { panic("Misaligned Load!"); }

static void handle_misaligned_store() { panic("Misaligned AMO!"); }

// added @lab1_3
static void handle_timer() {
  int cpuid = 0;
  // setup the timer fired at next time (TIMER_INTERVAL from now)
  *(uint64*)CLINT_MTIMECMP(cpuid) = *(uint64*)CLINT_MTIMECMP(cpuid) + TIMER_INTERVAL;

  // setup a soft interrupt in sip (S-mode Interrupt Pending) to be handled in S-mode
  write_csr(sip, SIP_SSIP);
}

//
// handle_mtrap calls a handling function according to the type of a machine mode interrupt (trap).
//
void handle_mtrap() {
  print_error_line();
  uint64 mcause = read_csr(mcause);
  switch (mcause) {
    case CAUSE_MTIMER:
      handle_timer();
      break;
    case CAUSE_FETCH_ACCESS:
      handle_instruction_access_fault();
      break;
    case CAUSE_LOAD_ACCESS:
      handle_load_access_fault();
    case CAUSE_STORE_ACCESS:
      handle_store_access_fault();
      break;
    case CAUSE_ILLEGAL_INSTRUCTION:
      // TODO (lab1_2): call handle_illegal_instruction to implement illegal instruction
      // interception, and finish lab1_2.
      // panic( "call handle_illegal_instruction to accomplish illegal instruction interception for lab1_2.\n" );
      handle_illegal_instruction();

      break;
    case CAUSE_MISALIGNED_LOAD:
      handle_misaligned_load();
      break;
    case CAUSE_MISALIGNED_STORE:
      handle_misaligned_store();
      break;

    default:
      sprint("machine trap(): unexpected mscause %p\n", mcause);
      sprint("            mepc=%p mtval=%p\n", read_csr(mepc), read_csr(mtval));
      panic( "unexpected exception happened in M-mode.\n" );
      break;
  }
}
