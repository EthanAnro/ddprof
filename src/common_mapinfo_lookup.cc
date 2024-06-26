// Unless explicitly stated otherwise all files in this repository are licensed
// under the Apache License Version 2.0. This product includes software
// developed at Datadog (https://www.datadoghq.com/). Copyright 2021-Present
// Datadog, Inc.

#include "common_mapinfo_lookup.hpp"

namespace ddprof {

MapInfo mapinfo_from_common(CommonMapInfoLookup::MappingErrors lookup_case) {
  switch (lookup_case) {
  case CommonMapInfoLookup::MappingErrors::empty:
    return {};
  default:
    break;
  }
  return {};
}

MapInfoIdx_t CommonMapInfoLookup::get_or_insert(
    CommonMapInfoLookup::MappingErrors lookup_case,
    MapInfoTable &mapinfo_table) {
  auto const it = _map.find(lookup_case);
  MapInfoIdx_t res;
  if (it != _map.end()) {
    res = it->second;
  } else { // insert things
    res = mapinfo_table.size();
    mapinfo_table.push_back(mapinfo_from_common(lookup_case));
    _map.insert({lookup_case, res});
  }
  return res;
}

} // namespace ddprof
