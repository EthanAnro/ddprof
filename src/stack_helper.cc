// Unless explicitly stated otherwise all files in this repository are licensed
// under the Apache License Version 2.0. This product includes software
// developed at Datadog (https://www.datadoghq.com/). Copyright 2024-Present
// Datadog, Inc.
#include "stack_helper.hpp"
#include "unwind_state.hpp"

namespace ddprof {
// read a word from the given stack
bool memory_read(ProcessAddress_t addr, ElfWord_t *result, int regno,
                 void *arg) {
  *result = 0;
  auto *us = static_cast<UnwindState *>(arg);

  constexpr uint64_t k_zero_page_limit = 4096;
  if (addr < k_zero_page_limit) {
    LG_DBG("[MEMREAD] Skipping 0 page");
    return false;
  }

  constexpr uint64_t k_expected_address_alignment = 8;
  if ((addr & (k_expected_address_alignment - 1)) != 0) {
    // The address is not 8 bytes aligned here
    LG_DBG("Addr is not aligned 0x%lx", addr);
    return false;
  }

#ifdef SKIP_UNALIGNED_REGS
  // for sanitizer
  if ((us->initial_regs.sp & 0x7) != 0) {
    // The address is not 8-bit aligned here
    LG_DBG("Addr is not aligned 0x%lx", addr);
    return false;
  }
#endif

  // Check for overflow, which won't be captured by the checks below.  Sometimes
  // addr is un-physically high and we don't know why yet.
  if (addr > addr + sizeof(ElfWord_t)) {
    LG_DBG("Overflow in addr 0x%lx", addr);
    return false;
  }

  // stack grows down, so end of stack is start
  // us->initial_regs.sp does not have to be aligned
  uint64_t const sp_start = us->initial_regs.regs[REGNAME(SP)];
  uint64_t const sp_end = sp_start + us->stack_sz;

  uint64_t constexpr k_page_size = 4096;
  if (addr < sp_start && addr > sp_start - k_page_size) {
    // Sometime DWARF emits CFI instructions that require reads below SP
    // (in the 128-byte red zone beyond SP)
    // This often occurs in function epilogues, for example in libc-2.31.so
    // on ubuntu 20.04 x86_64 in realloc function:
    // $ readelf -wF /usr/lib/x86_64-linux-gnu/libc-2.31.so | grep -F
    // '009ae80..'
    //    LOC           CFA      rbx   rbp   r12   r13   r14   r15   ra
    // 000000000009ae80 rsp+8    u     u     u     u     u     u     c-8
    // 000000000009ae86 rsp+16   u     u     u     u     u     c-16  c-8
    // 000000000009ae88 rsp+24   u     u     u     u     c-24  c-16  c-8
    // 000000000009ae8a rsp+32   u     u     u     c-32  c-24  c-16  c-8
    // 000000000009ae8c rsp+40   u     u     c-40  c-32  c-24  c-16  c-8
    // 000000000009ae90 rsp+48   u     c-48  c-40  c-32  c-24  c-16  c-8
    // 000000000009ae94 rsp+56   c-56  c-48  c-40  c-32  c-24  c-16  c-8
    // 000000000009ae98 rsp+80   c-56  c-48  c-40  c-32  c-24  c-16  c-8
    // 000000000009aebd rsp+56   c-56  c-48  c-40  c-32  c-24  c-16  c-8
    // 000000000009aec1 rsp+48   c-56  c-48  c-40  c-32  c-24  c-16  c-8
    // 000000000009aec2 rsp+40   c-56  c-48  c-40  c-32  c-24  c-16  c-8
    // 000000000009aec4 rsp+32   c-56  c-48  c-40  c-32  c-24  c-16  c-8
    // 000000000009aec6 rsp+24   c-56  c-48  c-40  c-32  c-24  c-16  c-8
    // 000000000009aec8 rsp+16   c-56  c-48  c-40  c-32  c-24  c-16  c-8
    // 000000000009aeca rsp+8    c-56  c-48  c-40  c-32  c-24  c-16  c-8
    // ...
    // Dump of assembler code for function __GI___libc_realloc:
    //  0x000000000009ae80 <+0>:  endbr64
    //  0x000000000009ae84 <+4>:  push   %r15
    //  0x000000000009ae86 <+6>:  push   %r14
    //  0x000000000009ae88 <+8>:  push   %r13
    //  0x000000000009ae8a <+10>:	push   %r12
    //  0x000000000009ae8c <+12>:	mov    %rsi,%r12
    //  0x000000000009ae8f <+15>:	push   %rbp
    //  0x000000000009ae90 <+16>:	mov    %rdi,%rbp
    //  0x000000000009ae93 <+19>:	push   %rbx
    //  0x000000000009ae94 <+20>:	sub    $0x18,%rsp
    //  0x000000000009ae98 <+24>:	mov    0x151141(%rip),%rax
    //  0x000000000009ae9f <+31>:	mov    (%rax),%rax
    //  0x000000000009aea2 <+34>:	test   %rax,%rax
    //  0x000000000009aea5 <+37>:	jne    0x9b0e0 <__GI___libc_realloc+608>
    //  0x000000000009aeab <+43>:	test   %rsi,%rsi
    //  0x000000000009aeae <+46>:	jne    0x9aed0 <__GI___libc_realloc+80>
    //  0x000000000009aeb0 <+48>:	test   %rdi,%rdi
    //  0x000000000009aeb3 <+51>:	jne    0x9b0f8 <__GI___libc_realloc+632>
    //  0x000000000009aeb9 <+57>:	add    $0x18,%rsp
    //  0x000000000009aebd <+61>:	mov    %r12,%rdi
    //  0x000000000009aec0 <+64>:	pop    %rbx
    //  0x000000000009aec1 <+65>:	pop    %rbp
    //  0x000000000009aec2 <+66>:	pop    %r12
    //  0x000000000009aec4 <+68>:	pop    %r13
    //  0x000000000009aec6 <+70>:	pop    %r14
    //  0x000000000009aec8 <+72>:	pop    %r15
    //  0x000000000009aeca <+74>:	jmp    0x9a0e0 <__GI___libc_malloc>
    //  0x000000000009aecf <+79>:	nop
    //  ...
    // As a size optimization, DW_CFA_restore instruction are not emitted for
    // stack pops during function epilogue and compiler relies on the
    // guarantee by System V AMD64 ABI that 128-byte red zone
    // beyond SP cannot being changed by anything (eg. by signal handlers),
    // and therefore stack values are preserved after pop instructions.
    // In remote unwinding this part of stack is not available since perf
    // captures the stack starting from SP.
    // We observe though that these CFI instructions follow pop assembly
    // instructions that already restore the register value, and this issue
    // only occurs in leaf function, therefore if a read before SP is
    // requested when unwinding the leaf function for a register, we simply
    // return the initial register value.
    constexpr uint64_t k_red_zone_size = 128;
    if (us->output.locs.size() <= 1 && addr >= sp_start - k_red_zone_size &&
        regno >= 0 &&
        regno < static_cast<int>(std::size(us->initial_regs.regs))) {
      *result = us->initial_regs.regs[regno];
      return true;
    }
#ifdef DEBUG
    // libdwfl might try to read values which are before our snapshot of the
    // stack.  Because the stack has the growsdown property and has a max size,
    // the pages before the current top of the stack are safe (no DSOs will
    // ever be mapped there on Linux, even if they actually did fit in the
    // single page before the top of the stack).  Avoiding these reads allows
    // us to prevent unnecessary backpopulate calls.
    LG_DBG("Invalid stack access:%lu before SP", sp_start - addr);
#endif
    return false;
  }
  if (addr < sp_start || addr + sizeof(ElfWord_t) > sp_end) {
    // We used to look within the binaries when then matched mapped binaries.
    // Though looking at the cases when this occured, it was not useful.
    // Dwarf should not need to look inside binaries to define how to unwind.
    // It was usually a result of frame pointer unwinding (where the frame
    // pointer was used for something else).
    LG_DBG("Attempting to read outside of stack 0x%lx from %d, (0x%lx, "
           "0x%lx)[%p, %p]",
           addr, us->pid, sp_start, sp_end, us->stack,
           us->stack + us->stack_sz);
    return false;
  }
  // If we're here, we're going to read from the stack.  Just the same, we need
  // to protect stack reads carefully, so split the indexing into a
  // precomputation followed by a bounds check
  uint64_t const stack_idx = addr - sp_start;
  if (stack_idx > addr) {
    LG_WRN("Stack miscalculation: %lx - %lx != %lx", addr, sp_start, stack_idx);
    return false;
  }
  *result = *reinterpret_cast<const ElfWord_t *>(us->stack + stack_idx);
  return true;
}

} // namespace ddprof
