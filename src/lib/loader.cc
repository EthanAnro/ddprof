// Unless explicitly stated otherwise all files in this repository are licensed
// under the Apache License Version 2.0. This product includes software
// developed at Datadog (https://www.datadoghq.com/). Copyright 2021-Present
// Datadog, Inc.

#include "constants.hpp"
#include "dd_profiling.h"
#include "ddres_helpers.hpp"
#include "defer.hpp"
#include "lib_embedded_data.hpp"
#include "loghandle.hpp"
#include "tempfile.hpp"

#include <dlfcn.h>
#include <fcntl.h>
#include <filesystem>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

extern "C" {
void *dlopen(const char *filename, int flags) noexcept __attribute__((weak));
void *dlsym(void *handle, const char *symbol) noexcept __attribute__((weak));
// NOLINTNEXTLINE cert-dcl51-cpp
void *__libc_dlopen_mode(const char *filename, int flag) noexcept
    __attribute__((weak));
// NOLINTNEXTLINE cert-dcl51-cpp
void *__libc_dlsym(void *handle, const char *symbol) __attribute__((weak));
int pthread_cancel(pthread_t thread) __attribute__((weak));
double log(double x) __attribute__((weak));
}

static void *s_libdl_handle = NULL;

static void *my_dlopen(const char *filename, int flags) {
  static decltype(dlopen) *dlopen_ptr = &dlopen;

  if (!dlopen_ptr) {
    dlopen_ptr = __libc_dlopen_mode;
  }
  if (dlopen_ptr) {
    return dlopen_ptr(filename, flags);
  }
  return NULL;
}

static void ensure_libdl_is_loaded() {
  if (!dlsym && !s_libdl_handle) {
    s_libdl_handle = my_dlopen("libdl.so.2", RTLD_GLOBAL | RTLD_NOW);
  }
}

static void ensure_libm_is_loaded() {
  if (!log) {
    my_dlopen("libm.so.6", RTLD_GLOBAL | RTLD_NOW);
  }
}

static void ensure_libpthread_is_loaded() {
  if (!pthread_cancel) {
    my_dlopen("libpthread.so.0", RTLD_GLOBAL | RTLD_NOW);
  }
}

static void *my_dlsym(void *handle, const char *symbol) {
  static decltype(dlsym) *dlsym_ptr = &dlsym;
  if (!dlsym_ptr) {
    if (!s_libdl_handle) {
      ensure_libdl_is_loaded();
    }

    if (s_libdl_handle) {
      dlsym_ptr = reinterpret_cast<decltype(dlsym) *>(
          __libc_dlsym(s_libdl_handle, "dlsym"));
    }
    if (!dlsym_ptr) {
      return NULL;
    }
  }

  return dlsym_ptr(handle, symbol);
}

typedef int (*FstatFunc)(int, struct stat *) noexcept;

// fstat is linked statically on glibc and  symbol is not present in libc.so.6
// Provide a replacement that calls __fxstat is present or fstat resolved with
// dlsym/RTLD_NEXT
extern "C" int __fxstat(int ver, int fd, struct stat *buf)
    __attribute__((weak));

extern int fstat(int fd, struct stat *buf)
    __attribute__((weak, alias("__fstat")));

// NOLINTNEXTLINE cert-dcl51-cpp
extern "C" __attribute__((unused)) int __fstat(int fd, struct stat *buf) {
  if (__fxstat) {
    return __fxstat(1, fd, buf);
  }
  static __typeof(fstat) *s_fstat = NULL;
  if (s_fstat == NULL) {
    s_fstat = (FstatFunc)my_dlsym(RTLD_NEXT, "fstat");
  }
  if (s_fstat) {
    return s_fstat(fd, buf);
  }
  return -1;
}

static std::string s_lib_profiling_path;
static std::string s_profiler_exe_path;
static void *s_profiling_lib_handle = nullptr;
decltype(ddprof_start_profiling) *s_start_profiling_func = nullptr;
decltype(ddprof_stop_profiling) *s_stop_profiling_func = nullptr;

static DDRes __attribute__((constructor)) loader() {
  LogHandle log_handle(LL_WARNING);

  const char *s = getenv(k_profiler_lib_env_variable);
  if (!s) {
    auto lib_data = ddprof::profiling_lib_data();
    auto exe_data = ddprof::profiler_exe_data();
    if (lib_data.empty() || exe_data.empty()) {
      // nothing to do
      return {};
    }
    DDRES_CHECK_FWD(create_temp_file(k_libdd_profiling_name, lib_data, 0644,
                                     s_lib_profiling_path));
    create_temp_file(k_profiler_exe_name, exe_data, 0755, s_profiler_exe_path);
    s = s_lib_profiling_path.c_str();
    setenv(k_profiler_ddprof_exe_env_variable, s_profiler_exe_path.c_str(), 1);
  }

  ensure_libdl_is_loaded();
  ensure_libm_is_loaded();
  ensure_libpthread_is_loaded();

  s_profiling_lib_handle = my_dlopen(s, RTLD_LOCAL | RTLD_NOW);
  if (s_profiling_lib_handle) {
    s_start_profiling_func = reinterpret_cast<decltype(s_start_profiling_func)>(
        my_dlsym(s_profiling_lib_handle, "ddprof_start_profiling"));
    s_stop_profiling_func = reinterpret_cast<decltype(s_stop_profiling_func)>(
        my_dlsym(s_profiling_lib_handle, "ddprof_stop_profiling"));
  }
  return {};
}

static void __attribute__((destructor)) unloader() {
  if (!s_lib_profiling_path.empty()) {
    unlink(s_lib_profiling_path.c_str());
  }
  if (!s_profiler_exe_path.empty()) {
    unlink(s_profiler_exe_path.c_str());
  }
}

int ddprof_start_profiling() {
  return s_start_profiling_func ? s_start_profiling_func() : -1;
}

void ddprof_stop_profiling(int timeout_ms) {
  if (s_stop_profiling_func) {
    s_stop_profiling_func(timeout_ms);
  }
}