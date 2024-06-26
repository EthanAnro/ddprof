// Unless explicitly stated otherwise all files in this repository are licensed
// under the Apache License Version 2.0. This product includes software
// developed at Datadog (https://www.datadoghq.com/). Copyright 2021-Present
// Datadog, Inc.

#pragma once

#include "ddprof_context.hpp"
#include "ddres_def.hpp"
#include "pevent.hpp"

#include <span>

namespace ddprof {

/// Sets initial state for every pevent in the pevent_hdr
void pevent_init(PEventHdr *pevent_hdr);

/// Setup perf event according to requested watchers.
DDRes pevent_open(DDProfContext *ctx, std::span<pid_t> pids, int num_cpu,
                  PEventHdr *pevent_hdr);

/// Setup mmap buffers according to content of peventhdr
DDRes pevent_mmap(PEventHdr *pevent_hdr, bool use_override);

/// Compute minimum size for a given ring buffer
/// This is adjusted using the number of samples we can fit in a buffer
int pevent_compute_min_mmap_order(int min_buffer_size_order,
                                  uint32_t stack_sample_size,
                                  unsigned min_number_samples);

/// Setup watchers = setup mmap + setup perfevent
DDRes pevent_setup(DDProfContext &ctx, std::span<pid_t> pids, int num_cpu,
                   PEventHdr *pevent_hdr);

/// Call ioctl PERF_EVENT_IOC_ENABLE on available file descriptors
DDRes pevent_enable(PEventHdr *pevent_hdr);

/// Clean the buffers allocated by mmap
DDRes pevent_munmap(PEventHdr *pevent_hdr);

/// Clean the file descriptors
DDRes pevent_close(PEventHdr *pevent_hdr);

/// cleanup watchers = cleanup perfevent + cleanup mmap (clean everything)
DDRes pevent_cleanup(PEventHdr *pevent_hdr);

/// true if one perf_event_attr we used included kernel events
bool pevent_include_kernel_events(const PEventHdr *pevent_hdr);

DDRes pevent_mmap_event(PEvent *pevent);

DDRes pevent_munmap_event(PEvent *pevent);

DDRes pevent_close_event(PEvent *pevent);

} // namespace ddprof
