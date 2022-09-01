// Unless explicitly stated otherwise all files in this repository are licensed
// under the Apache License Version 2.0. This product includes software
// developed at Datadog (https://www.datadoghq.com/). Copyright 2021-Present
// Datadog, Inc.

// External API: This file should stay in C

#pragma once

#include <stdint.h>

#include "ddprof_defs.hpp"
#include "string_view.hpp"

typedef struct FunLoc {
  uint64_t ip; // Relative to file, not VMA
  SymbolIdx_t _symbol_idx;
  MapInfoIdx_t _map_info_idx;
} FunLoc;

typedef struct UnwindOutput {
  FunLoc locs[DD_MAX_STACK_DEPTH];
  uint64_t nb_locs;
  int pid;
  int tid;
  bool is_incomplete;
} UnwindOutput;

void uw_output_clear(UnwindOutput *);
