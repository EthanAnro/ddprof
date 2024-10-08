// Unless explicitly stated otherwise all files in this repository are licensed
// under the Apache License Version 2.0. This product includes software
// developed at Datadog (https://www.datadoghq.com/). Copyright 2021-Present
// Datadog, Inc.

#include "perf_ringbuffer.hpp"
#include "perf_watcher.hpp" // for default sample type used in ddprof

#include <gtest/gtest.h>
#include <stdio.h>

namespace ddprof {

const char cmp_fmt[] = "Mismatch in %s, 0x%lx != 0x%lx";
#define SAMPLE_COMPARE(field)                                                  \
  if (s1->field != s2->field) {                                                \
    printf(cmp_fmt, #field, (uint64_t)s1->field, (uint64_t)s2->field);         \
    return false;                                                              \
  }

// TODO comparison should be done relative to a mask
bool sample_eq(struct perf_event_sample *s1, struct perf_event_sample *s2) {
  SAMPLE_COMPARE(sample_id);
  SAMPLE_COMPARE(ip);
  SAMPLE_COMPARE(pid);
  SAMPLE_COMPARE(tid);
  SAMPLE_COMPARE(time);
  SAMPLE_COMPARE(addr);
  SAMPLE_COMPARE(id);
  SAMPLE_COMPARE(stream_id);
  SAMPLE_COMPARE(cpu);
  SAMPLE_COMPARE(res);
  SAMPLE_COMPARE(period);
  SAMPLE_COMPARE(nr);
  SAMPLE_COMPARE(size_raw);
  SAMPLE_COMPARE(bnr);
  SAMPLE_COMPARE(abi);
  SAMPLE_COMPARE(size_stack);
  SAMPLE_COMPARE(dyn_size_stack);
  SAMPLE_COMPARE(weight);
  SAMPLE_COMPARE(data_src);
  SAMPLE_COMPARE(transaction);
  SAMPLE_COMPARE(abi_intr);

  if (s1->size_stack &&
      memcmp(s1->data_stack, s2->data_stack, s1->size_stack)) {
    printf("Stack mismatch\n");
    return false;
  }

  if (s1->size_raw && memcmp(s1->data_raw, s2->data_raw, s1->size_raw)) {
    printf("Raw mismatch\n");
    return false;
  }

  // TODO hardcoded register number, should get from ABI
  if (s1->abi && memcmp(s1->regs, s2->regs, 3)) {
    printf("Register mismatch\n");
    return false;
  }

  return true;
}

TEST(PerfRingbufferTest, SampleSymmetryx86) {
  // Setup the reference sample
  uint64_t mask = perf_event_default_sample_type();
  mask |= PERF_SAMPLE_IDENTIFIER | PERF_SAMPLE_IP | PERF_SAMPLE_ADDR |
      PERF_SAMPLE_RAW;
  char default_stack[4096] = {0};
  for (uint64_t i = 0; i < std::size(default_stack); i++)
    default_stack[i] = i & 255;
  char default_raw[16] = {0};
  for (uint64_t i = 0; i < sizeof(default_raw) / sizeof(*default_raw); i++)
    default_raw[i] = (i + 11) & 255;
  uint64_t default_regs[k_perf_register_count] = {};
  for (size_t i = 0; i < k_perf_register_count; ++i) {
    default_regs[i] = 1ull << i;
  }
  struct perf_event_sample sample = {};
  uint64_t default_val = 0x11111111;
  sample.header.type = PERF_RECORD_SAMPLE;
  sample.sample_id = default_val;
  sample.ip = 0x2 * default_val;
  sample.pid = 0x3 * default_val;
  sample.tid = 0x4 * default_val;
  sample.time = 0x5 * default_val;
  sample.addr = 0x6 * default_val;
  sample.period = 0x7 * default_val;
  // s_id, v, nr, ips, bnr, lbr -- untested because unused!
  sample.size_raw = sizeof(default_raw) / sizeof(*default_raw);
  sample.data_raw = default_raw;
  sample.abi = PERF_SAMPLE_REGS_ABI_64;
  sample.regs = default_regs;
  sample.size_stack = 4096;
  sample.data_stack = default_stack;
  sample.dyn_size_stack = 4096;
  // weight, data_src, transaction, abi_intr, regs_intr -- untested again!

  // Convert the sample to a header
  char hdr_placeholder[2 * 4096] = {0}; // too big, but that's OK for now
  struct perf_event_header *hdr = (struct perf_event_header *)hdr_placeholder;
  ASSERT_TRUE(samp2hdr(hdr, &sample, sizeof(hdr_placeholder), mask));

  // Convert the header back into a sample
  struct perf_event_sample *sample_new;
  sample_new = hdr2samp(hdr, mask);

  // Compare
  ASSERT_TRUE(sample_eq(&sample, sample_new));
}

} // namespace ddprof
