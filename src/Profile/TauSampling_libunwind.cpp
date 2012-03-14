#ifdef TAU_USE_LIBUNWIND

#include "Profile/TauSampling_unwind.h"
#include <ucontext.h>

#define UNW_LOCAL_ONLY
#include <libunwind.h>

#define TAU_SAMP_NUM_PARENTS 0

void show_backtrace_unwind(void *pc) {
  unw_cursor_t cursor;
  unw_context_t uc;
  unw_word_t ip, sp;
  int found = 0;

  unw_getcontext(&uc);
  unw_init_local(&cursor, &uc);
  while (unw_step(&cursor) > 0) {
    unw_get_reg(&cursor, UNW_REG_IP, &ip);
    // unw_get_reg(&cursor, UNW_REG_SP, &sp);
    if (ip == (unw_word_t)pc) {
      found = 1;
    }
    //    if (found) {
    printf("ip = %lx, sp = %lx\n", (long)ip, (long)sp);
    //    }
  }
}

void printStack(vector<unsigned long> *pcStack) {
  printf("PC Stack: ");
  vector<unsigned long>::iterator it;
  for (it = pcStack->begin(); it != pcStack->end(); it++) {
    printf("%p ", (void *)(*it));
  }
  printf("end\n");
}

// *CWL* - *TODO*. The unwind used for trace ought to be made
//         common to the unwind for profiles. The way
//         to do this is to merge into a common function that
//         simply processes a PC stack generated from an
//         unwind directly.
//
//         Something to try to work in after Supercomputing 11
void Tau_sampling_outputTraceCallstack(int tid, void *pc, 
				       void *context) {
  unw_cursor_t cursor;
  unw_context_t uc;
  unw_word_t ip, sp;
  int found = 0;

  fprintf(ebsTrace[tid], " |");

  unw_getcontext(&uc);
  unw_init_local(&cursor, &uc);
  while (unw_step(&cursor) > 0) {
    unw_get_reg(&cursor, UNW_REG_IP, &ip);
    // unw_get_reg(&cursor, UNW_REG_SP, &sp);
    if (found) {
      fprintf(ebsTrace[tid], " %p", ip);
    }
    if (ip == (unw_word_t)pc) {
      found = 1;
    }
  }
}

bool Tau_unwind_unwindTauContext(int tid, unsigned long *addresses) {
  ucontext_t context;
  int ret = getcontext(&context);
  
  if (ret != 0) {
    fprintf(stderr, "TAU: Error getting context\n");
    return false;
  }

  unw_cursor_t cursor;
  unw_word_t ip;
  unw_init_local(&cursor, &context);

  int count = 0;
  int idx = 1;  // we want to fill the first entry with the length later.
  unw_word_t last_address = 0;
  while (unw_step(&cursor) > 0 && idx < TAU_SAMP_NUM_ADDRESSES) {
    unw_get_reg(&cursor, UNW_REG_IP, &ip);
    // In the case of Unwinding from TAU context, we ignore recursion as
    //   un-helpful for the purposes of determining callsite information.
    if (ip == last_address) {
      continue;
    }
    addresses[idx++] = (unsigned long)ip;
    last_address = ip;
    count++;
  }
  if (count > 0) {
    addresses[0] = count;
    return true;
  } else {
    return false;
  }
}

void Tau_sampling_unwindTauContext(int tid, void **addresses) {
  ucontext_t context;
  int ret = getcontext(&context);
  
  if (ret != 0) {
    fprintf(stderr, "TAU: Error getting context\n");
    return;
  }

  unw_cursor_t cursor;
  unw_word_t ip;
  unw_init_local(&cursor, &context);

  int idx = 0;
  while (unw_step(&cursor) > 0 && idx < TAU_SAMP_NUM_ADDRESSES) {
    unw_get_reg(&cursor, UNW_REG_IP, &ip);
    addresses[idx++] = (void *)ip;
  }
}

extern "C" FunctionInfo *findTopContext(Profiler *currentProfiler, void *address);
vector<unsigned long> *Tau_sampling_unwind(int tid, Profiler *profiler,
					   void *pc, void *context) {
  unw_cursor_t cursor;
  unw_context_t uc;
  unw_word_t unwind_ip, sp;
  unw_word_t curr_ip;

  vector<unsigned long> *pcStack = new vector<unsigned long>();
  int unwindDepth = 0;
  int depthCutoff = TauEnv_get_ebs_unwind_depth();

  pcStack->push_back((unsigned long)pc);

  // Commence the unwind

  // We should use the original sample context rather than the current
  // TAU EBS context for the unwind.
  //  unw_getcontext(&uc);
  uc = *(unw_context_t *)context;
  unw_init_local(&cursor, &uc);
  while (unw_step(&cursor) > 0) {
    unw_get_reg(&cursor, UNW_REG_IP, &unwind_ip);
    if ((unwindDepth >= depthCutoff) ||
	(unwind_cutoff(profiler->address, (void *)unwind_ip))) {
      pcStack->push_back((unsigned long)unwind_ip);
      unwindDepth++;  // for accounting only
      break; // always break when limit or cutoff is reached.
    } // Cut-off or limit check conditional
    pcStack->push_back((unsigned long)unwind_ip);
    unwindDepth++;
  }
  return pcStack;
}

#endif /* TAU_USE_LIBUNWIND */

