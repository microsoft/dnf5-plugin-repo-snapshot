// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
// SPDX-License-Identifier: MIT

#include <libdnf5/utils/bgettext/bgettext-mark.h>

#include <cstring>
#include <dnf5/context.hpp>
#include <dnf5/iplugin.hpp>
#include <libdnf5-cli/argument_parser.hpp>
#include <libdnf5-cli/argument_parser_errors.hpp>
#include <libdnf5/base/base.hpp>
#include <libdnf5/conf/option_errors.hpp>
#include <libdnf5/conf/option_string_list.hpp>
#include <libdnf5/conf/vars.hpp>
#include <libdnf5/plugin/plugin_info.hpp>
#include <memory>
#include <stdexcept>
#include <vector>

#include "snapshot.hpp"

namespace {

// The CLI half registers options on existing DNF5 commands; the backend applies
// the filter.
class SnapshotCli final : public dnf5::IPlugin {
 public:
  using IPlugin::IPlugin;

  dnf5::PluginAPIVersion get_api_version() const noexcept override {
    return dnf5::PLUGIN_API_VERSION;
  }
  const char* get_name() const noexcept override {
    return snapshot::PLUGIN_NAME;
  }
  dnf5::PluginVersion get_version() const noexcept override {
    return {snapshot::VERSION_MAJOR, snapshot::VERSION_MINOR,
            snapshot::VERSION_PATCH};
  }
  const char* const* get_attributes() const noexcept override {
    return snapshot::ATTRIBUTES;
  }
  const char* get_attribute(const char* name) const noexcept override {
    return snapshot::attribute(
        name,
        "DNF5 command-line options for repository publication-time filtering");
  }
  std::vector<std::unique_ptr<dnf5::Command>> create_commands() override {
    return {};
  }
  void finish() noexcept override {}

  // After Base::setup(), verify the actual loaded backend.
  // This runs before repository work: accepting the flags alone does not ensure
  // filtering.
  void post_base_setup() {
    auto& context = get_context();
    auto& base = context.get_base();
    if (!base.get_vars()->contains(snapshot::TIME_VAR)) {
      return;
    }
    bool backend_ready = false;
    for (const auto& info : base.get_plugins_info()) {
      if (!info.is_loaded() || !info.get_real_name() ||
          std::strcmp(info.get_real_name(), snapshot::PLUGIN_NAME) != 0) {
        continue;
      }
      const auto api = info.get_api_version();
      backend_ready = snapshot::compatible_backend(api.major, api.minor);
      if (backend_ready) {
        break;
      }
    }
    if (!backend_ready) {
      throw std::runtime_error(
          "snapshot: --snapshot-time requires a loaded repo_snapshot backend "
          "with a compatible API");
    }
  }

  // Register the snapshot flags and parse-time checks, then pass the validated
  // request through private Base variables for the backend's repository hooks
  // to consume.
  void init() override {
    auto& context = get_context();
    auto& parser = context.get_argument_parser();
    auto& root = *parser.get_root_command();
    auto& group = root.get_group("global_options");
    auto* time = parser.add_new_named_arg("snapshot-time");
    time->set_long_name("snapshot-time");
    time->set_has_value(true);
    time->set_arg_value_help("POSIX_SECONDS");
    time->set_description(
        "Exclude repository RPMs published after this POSIX second");
    time->set_parse_hook_func(
        [&context](libdnf5::cli::ArgumentParser::NamedArg*, const char*,
                   const char* value) {
          std::uint64_t validated;
          try {
            validated = snapshot::parse_time(value);
          } catch (const std::invalid_argument& error) {
            throw libdnf5::cli::ArgumentParserInvalidValueError(
                M_("--snapshot-time: {}"), std::string(error.what()));
          }
          context.get_base().get_vars()->set(snapshot::TIME_VAR,
                                             std::to_string(validated));
          return true;
        });
    root.register_named_arg(time);
    group.register_argument(time);

    auto* exempt = parser.add_new_named_arg("snapshot-exclude-repos");
    exempt->set_long_name("snapshot-exclude-repos");
    exempt->set_has_value(true);
    exempt->set_arg_value_help("REPO_PATTERN,...");
    exempt->set_description(
        "Bypass the snapshot cutoff for repositories matching these ID globs");
    // Let libdnf5 reject empty entries before appending each incoming batch.
    auto exemption_list =
        std::make_shared<libdnf5::OptionStringList>("", ".+", false);
    exempt->set_parse_hook_func([&context, exemption_list](
                                    libdnf5::cli::ArgumentParser::NamedArg*,
                                    const char*, const char* value) {
      const auto patterns = exemption_list->from_string(value);
      if (patterns.empty()) {
        throw libdnf5::cli::ArgumentParserInvalidValueError(
            M_("--snapshot-exclude-repos requires a repository ID pattern"));
      }
      try {
        exemption_list->add(libdnf5::Option::Priority::RUNTIME, patterns);
      } catch (const libdnf5::OptionError& error) {
        throw libdnf5::cli::ArgumentParserInvalidValueError(
            M_("--snapshot-exclude-repos: {}"), std::string(error.what()));
      }
      context.get_base().get_vars()->set(snapshot::EXEMPT_REPOS_VAR,
                                         exemption_list->get_value_string());
      return true;
    });
    root.register_named_arg(exempt);
    group.register_argument(exempt);

    // Keep DNF5's existing parse checks and require a cutoff for exemptions.
    const auto previous = root.get_parse_hook_func();
    root.set_parse_hook_func(
        [&context, previous](libdnf5::cli::ArgumentParser::Command* command,
                             const char* option, int argc,
                             const char* const argv[]) {
          if (previous && !previous(command, option, argc, argv)) {
            return false;
          }
          auto vars = context.get_base().get_vars();
          if (vars->contains(snapshot::EXEMPT_REPOS_VAR) &&
              !vars->contains(snapshot::TIME_VAR)) {
            throw libdnf5::cli::ArgumentParserInvalidValueError(
                M_("--snapshot-exclude-repos requires --snapshot-time"));
          }
          return true;
        });
  }
};

}  // namespace

// DNF5 resolves these fixed, unmangled loader entry points; they bridge into
// the C++ plugin.
extern "C" dnf5::PluginAPIVersion dnf5_plugin_get_api_version() {
  return dnf5::PLUGIN_API_VERSION;
}
extern "C" const char* dnf5_plugin_get_name() { return snapshot::PLUGIN_NAME; }
extern "C" dnf5::PluginVersion dnf5_plugin_get_version() {
  return {snapshot::VERSION_MAJOR, snapshot::VERSION_MINOR,
          snapshot::VERSION_PATCH};
}
extern "C" dnf5::IPlugin* dnf5_plugin_new_instance(dnf5::ApplicationVersion,
                                                   dnf5::Context& context) {
  return new SnapshotCli(context);
}
extern "C" void dnf5_plugin_delete_instance(dnf5::IPlugin* instance) {
  delete instance;
}
extern "C" void dnf5_plugin_post_base_setup(dnf5::IPlugin* instance) {
  static_cast<SnapshotCli*>(instance)->post_base_setup();
}
