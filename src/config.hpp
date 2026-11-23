// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace libdnf5 {
struct ConfigParser;
struct Vars;
}  // namespace libdnf5

namespace snapshot {

struct Settings {
  std::optional<std::uint64_t> cutoff;
  std::vector<std::string> exempt_repos;
};

Settings read_settings(const libdnf5::ConfigParser& config,
                       const libdnf5::Vars& vars);

}  // namespace snapshot
