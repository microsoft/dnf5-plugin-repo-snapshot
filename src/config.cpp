// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
// SPDX-License-Identifier: MIT

#include "config.hpp"

#include <libdnf5/conf/config_parser.hpp>
#include <libdnf5/conf/option_errors.hpp>
#include <libdnf5/conf/option_string_list.hpp>
#include <libdnf5/conf/vars.hpp>
#include <regex>
#include <stdexcept>

#include "snapshot.hpp"

namespace snapshot {
namespace {

std::string resolve(const libdnf5::ConfigParser& config,
                    const libdnf5::Vars& vars, const char* field) {
  if (!config.has_option("main", field)) {
    return {};
  }
  auto value = vars.substitute(config.get_value("main", field));
  if (value.find('$') != std::string::npos) {
    throw std::runtime_error("snapshot: invalid " + std::string(field) +
                             ": unresolved or malformed variable reference");
  }
  return value;
}

}  // namespace

Settings read_settings(const libdnf5::ConfigParser& config,
                       const libdnf5::Vars& vars) {
  Settings settings;
  const auto time = resolve(config, vars, "snapshot-time");
  const auto exemptions = resolve(config, vars, "snapshot-exclude-repos");
  if (!time.empty()) {
    try {
      settings.cutoff = parse_time(time);
    } catch (const std::invalid_argument& error) {
      throw std::runtime_error("snapshot: invalid snapshot-time: " +
                               std::string(error.what()));
    }
  }

  if (!exemptions.empty()) {
    // OptionStringList drops empty tokens after commas; reject those instead
    // of silently broadening the requested exemptions.
    static const std::regex empty_entry(R"((^\s*,|,\s*(,|$)))");
    if (std::regex_search(exemptions, empty_entry)) {
      throw std::runtime_error(
          "snapshot: invalid snapshot-exclude-repos: empty repository pattern");
    }
    try {
      libdnf5::OptionStringList patterns("", ".+", false);
      patterns.set(exemptions);
      settings.exempt_repos = patterns.get_value();
    } catch (const libdnf5::OptionError& error) {
      throw std::runtime_error("snapshot: invalid snapshot-exclude-repos: " +
                               std::string(error.what()));
    }
    if (settings.exempt_repos.empty()) {
      throw std::runtime_error(
          "snapshot: invalid snapshot-exclude-repos: empty repository pattern");
    }
    if (!settings.cutoff) {
      throw std::runtime_error(
          "snapshot: snapshot-exclude-repos requires snapshot-time");
    }
  }
  return settings;
}

}  // namespace snapshot
