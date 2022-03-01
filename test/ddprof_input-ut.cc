// Unless explicitly stated otherwise all files in this repository are licensed
// under the Apache License Version 2.0. This product includes software
// developed at Datadog (https://www.datadoghq.com/). Copyright 2021-Present
// Datadog, Inc.

extern "C" {
#include "ddprof_input.h"
#include "perf_option.h"
#include "string_view.h"
}

#include "arraysize.h"
#include "defer.hpp"
#include "loghandle.hpp"

#include <gtest/gtest.h>
#include <string_view>

using namespace std::literals;

class InputTest : public ::testing::Test {
protected:
  void SetUp() override {}

  LogHandle _handle;
};

bool s_version_called = false;
void print_version() { s_version_called = true; }
string_view str_version() { return STRING_VIEW_LITERAL("1.2.3"); }

TEST_F(InputTest, default_values) {
  DDProfInput input;
  DDRes res = ddprof_input_default(&input);
  EXPECT_TRUE(IsDDResOK(res));
  const char *env_serv = getenv("DD_SERVICE");
  if (env_serv != NULL) {
    EXPECT_EQ(strcmp(input.exp_input.service, env_serv), 0);
  } else {
    EXPECT_EQ(strcmp(input.exp_input.service, "myservice"), 0);
  }
  EXPECT_EQ(strcmp(input.log_mode, "stdout"), 0);
  ddprof_input_free(&input);
}

TEST_F(InputTest, version_called) {
  DDProfInput input;
  bool contine_exec = true;
  const char *input_values[] = {MYNAME, "-v", "my_program"};
  DDRes res =
      ddprof_input_parse(3, (char **)input_values, &input, &contine_exec);
  EXPECT_TRUE(IsDDResOK(res));
  EXPECT_TRUE(s_version_called);
  EXPECT_FALSE(contine_exec);
  EXPECT_EQ(input.nb_parsed_params, 2);
  ddprof_input_free(&input);
}

TEST_F(InputTest, single_param) {
  DDProfInput input;
  bool contine_exec;
  int argc = 4;
  const char *input_values[] = {MYNAME, "-m", "yes", "my_program"};
  //   const char *input_values[] = {MYNAME, "--core_dumps", "yes",
  //   "my_program"};

  DDRes res =
      ddprof_input_parse(argc, (char **)input_values, &input, &contine_exec);
  EXPECT_TRUE(IsDDResOK(res));
  EXPECT_TRUE(contine_exec);
  EXPECT_EQ(strcmp(input.core_dumps, "yes"), 0);
  EXPECT_EQ(input.nb_parsed_params, 3);
  ddprof_print_params(&input);
  ddprof_input_free(&input);
}

TEST_F(InputTest, no_params) {
  DDProfInput input;
  bool contine_exec = false;
  int argc = 2;
  const char *input_values[] = {MYNAME, "my_program"};
  DDRes res =
      ddprof_input_parse(argc, (char **)input_values, &input, &contine_exec);
  EXPECT_TRUE(IsDDResOK(res));
  EXPECT_TRUE(contine_exec);
  EXPECT_EQ(input.nb_parsed_params, 1);
  ddprof_input_free(&input);
}

TEST_F(InputTest, dump_fixed) {
  DDProfInput input;
  bool contine_exec = true;
  int argc = 3;
  const char *input_values[] = {MYNAME, "--V", "my_program"};
  DDRes res =
      ddprof_input_parse(argc, (char **)input_values, &input, &contine_exec);
  EXPECT_FALSE(IsDDResOK(res));
  EXPECT_FALSE(contine_exec);
  EXPECT_EQ(input.nb_parsed_params, 2);
  ddprof_input_free(&input);
}

TEST_F(InputTest, event_from_env) {
  defer { unsetenv("DD_PROFILING_NATIVE_EVENTS"); };
  {
    DDProfInput input;
    bool contine_exec = true;
    const char *input_values[] = {MYNAME, "my_program"};
    setenv("DD_PROFILING_NATIVE_EVENTS", "sCPU,1000", 1);
    DDRes res = ddprof_input_parse(
        ARRAY_SIZE(input_values), (char **)input_values, &input, &contine_exec);

    EXPECT_TRUE(IsDDResOK(res));
    EXPECT_TRUE(contine_exec);
    EXPECT_EQ(input.nb_parsed_params, 1);
    EXPECT_EQ(input.num_watchers, 1);
    // cppcheck-suppress literalWithCharPtrCompare
    EXPECT_EQ(perfoptions_lookup_idx(input.watchers[0]), "sCPU"sv);
    EXPECT_EQ(input.sampling_value[0], 1000);
    ddprof_input_free(&input);
  }
  {
    DDProfInput input;
    bool contine_exec = true;
    const char *input_values[] = {MYNAME, "my_program"};
    setenv("DD_PROFILING_NATIVE_EVENTS", ";", 1);
    DDRes res = ddprof_input_parse(
        ARRAY_SIZE(input_values), (char **)input_values, &input, &contine_exec);

    EXPECT_TRUE(IsDDResOK(res));
    EXPECT_TRUE(contine_exec);
    EXPECT_EQ(input.nb_parsed_params, 1);
    EXPECT_EQ(input.num_watchers, 0);
    ddprof_input_free(&input);
  }
  {
    DDProfInput input;
    bool contine_exec = true;
    const char *input_values[] = {MYNAME, "my_program"};
    setenv("DD_PROFILING_NATIVE_EVENTS", ";sCPU,1000;", 1);
    DDRes res = ddprof_input_parse(
        ARRAY_SIZE(input_values), (char **)input_values, &input, &contine_exec);

    EXPECT_TRUE(IsDDResOK(res));
    EXPECT_TRUE(contine_exec);
    EXPECT_EQ(input.nb_parsed_params, 1);
    EXPECT_EQ(input.num_watchers, 1);

    // string_view literal confuses cppcheck
    // cppcheck-suppress literalWithCharPtrCompare
    EXPECT_EQ(perfoptions_lookup_idx(input.watchers[0]), "sCPU"sv);
    EXPECT_EQ(input.sampling_value[0], 1000);
    ddprof_input_free(&input);
  }
  {
    DDProfInput input;
    bool contine_exec = true;
    const char *input_values[] = {MYNAME, "-e", "hINSTR,456", "my_program"};
    setenv("DD_PROFILING_NATIVE_EVENTS", "sCPU,1000;hCPU,123", 1);
    DDRes res = ddprof_input_parse(
        ARRAY_SIZE(input_values), (char **)input_values, &input, &contine_exec);

    EXPECT_TRUE(IsDDResOK(res));
    EXPECT_TRUE(contine_exec);
    EXPECT_EQ(input.nb_parsed_params, 3);
    EXPECT_EQ(input.num_watchers, 3);
    // cppcheck-suppress literalWithCharPtrCompare
    EXPECT_EQ(perfoptions_lookup_idx(input.watchers[0]), "sCPU"sv);
    // cppcheck-suppress literalWithCharPtrCompare
    EXPECT_EQ(perfoptions_lookup_idx(input.watchers[1]), "hCPU"sv);
    // cppcheck-suppress literalWithCharPtrCompare
    EXPECT_EQ(perfoptions_lookup_idx(input.watchers[2]), "hINSTR"sv);

    EXPECT_EQ(input.sampling_value[0], 1000);
    EXPECT_EQ(input.sampling_value[1], 123);
    EXPECT_EQ(input.sampling_value[2], 456);

    ddprof_input_free(&input);
  }
}
