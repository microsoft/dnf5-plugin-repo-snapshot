// SPDX-License-Identifier: MIT
#include <cstring>
#include <iostream>
#include <libdnf5/base/base.hpp>
#include <libdnf5/conf/option_string_list.hpp>
#include <libdnf5/conf/vars.hpp>
#include <libdnf5/plugin/iplugin.hpp>
#include <libdnf5/repo/repo_query.hpp>
#include <libdnf5/rpm/package_query.hpp>
#include <libdnf5/rpm/package_sack.hpp>
#include <libdnf5/rpm/package_set.hpp>
#include <set>
#include <stdexcept>

#include "snapshot.hpp"

namespace {

// The backend half checks repository inputs and adds exclusions to this
// invocation's sack.
class SnapshotHook final : public libdnf5::plugin::IPlugin {
 public:
  SnapshotHook(libdnf5::plugin::IPluginData& data, libdnf5::ConfigParser&)
      : IPlugin(data) {}
  libdnf5::PluginAPIVersion get_api_version() const noexcept override {
    return libdnf5::PLUGIN_API_VERSION;
  }
  const char* get_name() const noexcept override {
    return snapshot::PLUGIN_NAME;
  }
  libdnf5::plugin::Version get_version() const noexcept override {
    return {snapshot::VERSION_MAJOR, snapshot::VERSION_MINOR,
            snapshot::VERSION_PATCH};
  }
  const char* const* get_attributes() const noexcept override {
    return snapshot::ATTRIBUTES;
  }
  const char* get_attribute(const char* name) const noexcept override {
    return snapshot::attribute(
        name, "Filters available packages by repository publication time");
  }

  // Once repositories are loaded, resolve the requested exemptions,
  // then mark post-cutoff packages. Add to existing in-memory exclusions; do
  // not rewrite cached metadata or remove installed packages.
  void repos_loaded() override {
    auto& base = get_base();
    const auto vars = base.get_vars();
    const bool active = vars->contains(snapshot::TIME_VAR);
    const bool has_exemptions = vars->contains(snapshot::EXEMPT_REPOS_VAR);
    // The CLI writes these private keys at runtime. Reject ambient
    // environment/varsdir values so they cannot activate filtering or change
    // the requested exemptions.
    const auto runtime = libdnf5::Vars::Priority::RUNTIME;
    const bool invalid_cutoff_source =
        active && vars->get(snapshot::TIME_VAR).priority != runtime;
    const bool invalid_exemption_source =
        has_exemptions &&
        vars->get(snapshot::EXEMPT_REPOS_VAR).priority != runtime;
    if (invalid_cutoff_source || invalid_exemption_source) {
      throw std::runtime_error(
          "snapshot: variables must be set by the DNF5 CLI, not an environment "
          "or varsdir fallback");
    }
    if (!active) {
      // Also guard runtime handoffs that bypass the CLI's argument checks.
      if (has_exemptions) {
        throw std::runtime_error(
            "snapshot: repository exemptions require --snapshot-time");
      }
      return;
    }
    const auto cutoff =
        snapshot::parse_time(vars->get_value(snapshot::TIME_VAR));
    libdnf5::repo::RepoQuery repos(base);
    std::set<std::string> enabled;
    for (auto repo : repos) {
      if (repo->get_type() == libdnf5::repo::Repo::Type::AVAILABLE &&
          repo->is_enabled()) {
        enabled.insert(repo->get_id());
      }
    }
    std::set<std::string> exempt;
    libdnf5::OptionStringList patterns("");
    if (has_exemptions) {
      patterns.set(vars->get_value(snapshot::EXEMPT_REPOS_VAR));
    }
    for (const auto& pattern : patterns.get_value()) {
      libdnf5::repo::RepoQuery matches(base);
      matches.filter_type(libdnf5::repo::Repo::Type::AVAILABLE);
      matches.filter_enabled(true);
      matches.filter_id(pattern, libdnf5::sack::QueryCmp::GLOB);
      if (matches.empty()) {
        throw std::runtime_error(
            "snapshot: exempt repository not enabled or not found: " + pattern);
      }
      for (auto repo : matches) {
        exempt.insert(repo->get_id());
      }
    }
    for (const auto& id : exempt) {
      std::cerr
          << "snapshot: bypassing publication cutoff for entire repository "
          << id << '\n';
    }
    // Ignore all filters while marking: a command can ignore versionlock or
    // modular exclusions later. The added snapshot exclusions do not change
    // command filters.
    libdnf5::rpm::PackageQuery packages(
        base, libdnf5::sack::ExcludeFlags::IGNORE_EXCLUDES);
    packages.filter_available();
    libdnf5::rpm::PackageSet excluded(base);
    for (const auto& package : packages) {
      const auto id = package.get_repo_id();
      if (!enabled.contains(id) || exempt.contains(id)) {
        continue;
      }
      if (snapshot::filter_package(package.get_file_time(), cutoff, false,
                                   false, id, package.get_nevra())) {
        excluded.add(package);
      }
    }
    base.get_rpm_package_sack()->add_user_excludes(excluded);
  }

  // Reject local/URL RPM inputs before DNF5 adds them to the sack: they cannot
  // be checked against repository publication metadata when a cutoff is active.
  void pre_add_cmdline_packages(
      const std::vector<std::string>& paths) override {
    if (get_base().get_vars()->contains(snapshot::TIME_VAR)) {
      for (const auto& spec : paths) {
        if (spec.ends_with(".rpm") || spec.find("://") != std::string::npos) {
          throw std::runtime_error(
              "snapshot: local and URL RPM inputs are unsupported with "
              "--snapshot-time");
        }
      }
    }
  }
};

}  // namespace

// libdnf5 resolves these fixed, unmangled loader entry points into this C++
// backend.
extern "C" libdnf5::PluginAPIVersion libdnf_plugin_get_api_version() {
  return libdnf5::PLUGIN_API_VERSION;
}
extern "C" const char* libdnf_plugin_get_name() {
  return snapshot::PLUGIN_NAME;
}
extern "C" libdnf5::plugin::Version libdnf_plugin_get_version() {
  return {snapshot::VERSION_MAJOR, snapshot::VERSION_MINOR,
          snapshot::VERSION_PATCH};
}
extern "C" libdnf5::plugin::IPlugin* libdnf_plugin_new_instance(
    libdnf5::LibraryVersion, libdnf5::plugin::IPluginData& data,
    libdnf5::ConfigParser& parser) {
  return new SnapshotHook(data, parser);
}
extern "C" void libdnf_plugin_delete_instance(
    libdnf5::plugin::IPlugin* instance) {
  delete instance;
}
