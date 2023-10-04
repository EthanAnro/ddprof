// Unless explicitly stated otherwise all files in this repository are licensed
// under the Apache License Version 2.0. This product includes software
// developed at Datadog (https://www.datadoghq.com/). Copyright 2021-Present
// Datadog, Inc.

#include "dso.hpp"

#include "constants.hpp"
#include "ddprof_defs.hpp"
#include "logger.hpp"
#include "string_format.hpp"

#include <algorithm>
#include <string_view>

namespace ddprof {

constexpr std::string_view s_vdso_str = "[vdso]";
constexpr std::string_view s_vsyscall_str = "[vsyscall]";
constexpr std::string_view s_stack_str = "[stack]";
constexpr std::string_view s_heap_str = "[heap]";
// anon and empty are the same (one comes from perf, the other from proc maps)
constexpr std::string_view s_anon_str = "//anon";
constexpr std::string_view s_anon_2_str = "[anon";
constexpr std::string_view s_jsa_str = ".jsa";
constexpr std::string_view s_mem_fd_str = "/memfd";
// Example of these include : anon_inode:[perf_event]
constexpr std::string_view s_anon_inode_str = "anon_inode";
// dll should not be considered as elf files
constexpr std::string_view s_dll_str = ".dll";
// Example socket:[123456]
constexpr std::string_view s_socket_str = "socket";
// null elements
constexpr std::string_view s_dev_zero_str = "/dev/zero";
constexpr std::string_view s_dev_null_str = "/dev/null";
constexpr std::string_view s_dd_profiling_str = k_libdd_profiling_name;

Dso::Dso(pid_t pid, ProcessAddress_t start, ProcessAddress_t end,
         Offset_t offset, std::string &&filename, inode_t inode, uint32_t prot,
         DsoOrigin origin)
    : _start(start), _end(end), _offset(offset), _filename(std::move(filename)),
      _inode(inode), _pid(pid), _prot(prot), _id(k_file_info_undef),
      _type(DsoType::kStandard), _origin(origin) {
  // note that substr manages the case where len str < len vdso_str
  if (_filename.substr(0, s_vdso_str.length()) == s_vdso_str) {
    _type = DsoType::kVdso;
  } else if (_filename.substr(0, s_vsyscall_str.length()) == s_vsyscall_str) {
    _type = DsoType::kVsysCall;
  } else if (_filename.substr(0, s_stack_str.length()) == s_stack_str) {
    _type = DsoType::kStack;
  } else if (_filename.substr(0, s_heap_str.length()) == s_heap_str) {
    _type = DsoType::kHeap;
    // Safeguard against other types of files we would not handle
  } else if (_filename.empty() || _filename.starts_with(s_anon_str) ||
             _filename.starts_with(s_anon_inode_str) ||
             _filename.starts_with(s_anon_2_str) ||
             _filename.starts_with(s_dev_zero_str) ||
             _filename.starts_with(s_dev_null_str) ||
             _filename.starts_with(s_mem_fd_str)) {
    _type = DsoType::kAnon;
  } else if ( // ends with .jsa
      _filename.ends_with(s_jsa_str) || _filename.ends_with(s_dll_str)) {
    _type = DsoType::kRuntime;
  } else if (_filename.substr(0, s_socket_str.length()) == s_socket_str) {
    _type = DsoType::kSocket;
  } else if (_filename[0] == '[') {
    _type = DsoType::kUndef;
  } else if (is_jit_dump_str(_filename)) {
    _type = DsoType::kJITDump;
  } else { // check if this standard dso matches our internal dd_profiling lib
    std::size_t const pos = _filename.rfind('/');
    if (pos != std::string::npos &&
        _filename.substr(pos + 1, s_dd_profiling_str.length()) ==
            s_dd_profiling_str) {
      _type = DsoType::kDDProfiling;
    }
  }
}

// The string should end with: "jit-[0-9]+\\.dump"
// and the number should be the pid, however, in whole host mode
// we don't have visibility on the namespace's PID value.
bool Dso::is_jit_dump_str(std::string_view file_path) {
  const std::string_view prefix = "jit-";
  const std::string_view ext = ".dump";
  if (!file_path.ends_with(ext)) {
    return false;
  }
  file_path = file_path.substr(0, file_path.size() - ext.size());
  auto pos = file_path.rfind('/');
  if (pos != std::string_view::npos) {
    file_path = file_path.substr(pos + 1);
  }
  if (!file_path.starts_with(prefix)) {
    return false;
  }
  file_path = file_path.substr(prefix.size());
  return std::all_of(file_path.begin(), file_path.end(), [](char c) {
    return std::isdigit(static_cast<unsigned char>(c));
  });
}

std::string Dso::to_string() const {
  return string_format("PID[%d] %lx-%lx %lx (%s)(T-%s)(%c%c%c)(ID#%d)", _pid,
                       _start, _end, _offset, _filename.c_str(),
                       dso_type_str(_type), _prot & PROT_READ ? 'r' : '-',
                       _prot & PROT_WRITE ? 'w' : '-',
                       _prot & PROT_EXEC ? 'x' : '-', _id);
}

std::string Dso::format_filename() const {
  if (has_relevant_path(_type)) {
    return _filename;
  }
  return dso_type_str(_type);
}

std::ostream &operator<<(std::ostream &os, const Dso &dso) {
  os << dso.to_string() << '\n';
  return os;
}

// perf does not return the same sizes as proc maps
// Example :
// PID<1> 7f763019e000-7f76304ddfff (//anon)
// PID<1> 7f763019e000-7f76304de000 ()
bool Dso::adjust_same(const Dso &o) {
  if (is_same_or_smaller(o)) {
    _end = o._end;
     _origin = o._origin;
    return true;
  }
  return false;
}

bool Dso::is_same_or_smaller(const Dso &o) const {
  if (_start != o._start) {
    return false;
  }
  if (_offset != o._offset) {
    return false;
  }
  if (_type != o._type) {
    return false;
  }
  // only compare filename if we are backed by real files
  if (_type == DsoType::kStandard &&
      (_filename != o._filename || _inode != o._inode)) {
    return false;
  }
  if (_prot != o._prot) {
    return false;
  }
  return o._end <= _end;
}

bool Dso::intersects(const Dso &o) const {
  // Check order of points
  // Test that we have lowest-start <-> lowest-end  ... highest-start
  if (_start < o._start) {
    // this Dso comes first check then it ends before the other
    if (_end < o._start) {
      return false;
    }
  } else if (o._end < _start) {
    // dso comes after, check that other ends before our start
    return false;
  }
  return true;
}

bool Dso::is_within(ElfAddress_t addr) const {
  return (addr >= _start) && (addr <= _end);
}

bool Dso::is_executable() const { return _prot & PROT_EXEC; }

} // namespace ddprof
