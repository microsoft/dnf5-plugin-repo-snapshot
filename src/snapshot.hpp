// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <cstring>
#include <optional>
#include <string>

namespace snapshot {

// Private CLI-to-backend request handoff, not persistent configuration or
// repository IDs.
constexpr auto TIME_VAR = "azl_snapshot_time";
constexpr auto EXEMPT_REPOS_VAR = "azl_snapshot_exempt_repos";
constexpr auto PLUGIN_NAME = "repo_snapshot";
constexpr auto AUTHOR_NAME = "Microsoft";
constexpr auto AUTHOR_EMAIL = "azlinux@microsoft.com";
constexpr auto VERSION_MAJOR = SNAPSHOT_VERSION_MAJOR;
constexpr auto VERSION_MINOR = SNAPSHOT_VERSION_MINOR;
constexpr auto VERSION_PATCH = SNAPSHOT_VERSION_PATCH;
constexpr auto REQUIRED_BACKEND_API_MAJOR = 2;
constexpr auto REQUIRED_BACKEND_API_MINOR = 2;
constexpr const char* ATTRIBUTES[]{"author.name", "author.email", "description",
                                   nullptr};

inline const char* attribute(const char* name,
                             const char* description) noexcept {
  if (std::strcmp(name, "author.name") == 0) {
    return AUTHOR_NAME;
  }
  if (std::strcmp(name, "author.email") == 0) {
    return AUTHOR_EMAIL;
  }
  return std::strcmp(name, "description") == 0 ? description : nullptr;
}

constexpr bool compatible_backend(int api_major, int api_minor) noexcept {
  return api_major == REQUIRED_BACKEND_API_MAJOR &&
         api_minor >= REQUIRED_BACKEND_API_MINOR;
}

// Parse --snapshot-time as nonnegative POSIX seconds within unsigned 64-bit
// range. Reject malformed or out-of-range CLI input rather than accepting a
// numeric prefix.
std::uint64_t parse_time(const std::string& value);

// Return whether a package was published after the cutoff; equality remains
// eligible. No cutoff, installed packages, and exempt repositories never
// exclude a package. Missing loaded timestamps fail only for packages subject
// to the cutoff.
bool filter_package(std::optional<unsigned long long> file_time,
                    std::optional<std::uint64_t> cutoff, bool installed,
                    bool exempt, const std::string& repo_id,
                    const std::string& nevra);

}  // namespace snapshot
