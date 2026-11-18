// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
// SPDX-License-Identifier: MIT

#include <libdnf5/base/base.hpp>
#include <libdnf5/conf/config_parser.hpp>
#include <libdnf5/conf/vars.hpp>

#include "config.hpp"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <functional>
#include <limits>
#include <stdexcept>

static void rejects(const char* field, const std::function<void()>& action) {
  try {
    action();
  } catch (const std::runtime_error& error) {
    assert(std::string(error.what()).find(field) != std::string::npos);
    return;
  }
  assert(false);
}

int main() {
  libdnf5::Base base;
  auto vars = base.get_vars();
  libdnf5::ConfigParser config;
  config.add_section("main");
  assert(!snapshot::read_settings(config, *vars).cutoff);

  config.set_value("main", "snapshot-time", "0");
  vars->set("repo_snapshot_time", "123");
  assert(snapshot::read_settings(config, *vars).cutoff == 0);
  config.set_value("main", "snapshot-time", "${repo_snapshot_time:-}");
  assert(snapshot::read_settings(config, *vars).cutoff == 123);
  vars->unset("repo_snapshot_time");
  assert(!snapshot::read_settings(config, *vars).cutoff);
  config.set_value("main", "snapshot-time", "18446744073709551615");
  assert(snapshot::read_settings(config, *vars).cutoff ==
         std::numeric_limits<std::uint64_t>::max());

  config.set_value("main", "snapshot-exclude-repos", "hotfix*, extra");
  const auto settings = snapshot::read_settings(config, *vars);
  assert(
      (settings.exempt_repos == std::vector<std::string>{"hotfix*", "extra"}));
  config.set_value("main", "snapshot-time", "");
  rejects("snapshot-time",
          [&] { (void)snapshot::read_settings(config, *vars); });
  config.set_value("main", "snapshot-time", "18446744073709551616");
  rejects("snapshot-time",
          [&] { (void)snapshot::read_settings(config, *vars); });
  config.set_value("main", "snapshot-time", "${missing}");
  rejects("snapshot-time",
          [&] { (void)snapshot::read_settings(config, *vars); });
  config.set_value("main", "snapshot-time", "${broken");
  rejects("snapshot-time",
          [&] { (void)snapshot::read_settings(config, *vars); });
  config.set_value("main", "snapshot-time", "0");
  for (const auto* bad : {"hotfix,,extra", "hotfix, ", " ,hotfix", "  "}) {
    config.set_value("main", "snapshot-exclude-repos", bad);
    rejects("snapshot-exclude-repos",
            [&] { (void)snapshot::read_settings(config, *vars); });
  }
  config.set_value("main", "snapshot-exclude-repos", "${missing}");
  rejects("snapshot-exclude-repos",
          [&] { (void)snapshot::read_settings(config, *vars); });
  config.set_value("main", "snapshot-exclude-repos",
                   "${repo_snapshot_exclude_repos:-}");
  assert(snapshot::read_settings(config, *vars).exempt_repos.empty());
  vars->set("repo_snapshot_exclude_repos", "hotfix*,extra");
  assert(snapshot::read_settings(config, *vars).exempt_repos.size() == 2);
}
