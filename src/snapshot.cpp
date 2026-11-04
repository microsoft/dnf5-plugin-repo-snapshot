// SPDX-License-Identifier: MIT
#include "snapshot.hpp"

#include <charconv>
#include <stdexcept>

namespace snapshot {

std::uint64_t parse_time(const std::string& value) {
  std::uint64_t result = 0;
  const auto [end, error] =
      std::from_chars(value.data(), value.data() + value.size(), result);
  if (error != std::errc{} || end != value.data() + value.size()) {
    throw std::invalid_argument(
        "snapshot time must be a nonnegative decimal POSIX second in unsigned "
        "64-bit range");
  }
  return result;
}

bool filter_package(std::optional<unsigned long long> file_time,
                    std::optional<std::uint64_t> cutoff, bool installed,
                    bool exempt, const std::string& repo_id,
                    const std::string& nevra) {
  if (!cutoff || installed || exempt) {
    return false;
  }
  if (!file_time) {
    throw std::runtime_error("snapshot: missing file time in " + repo_id +
                             ": " + nevra);
  }
  return *file_time > *cutoff;
}

}  // namespace snapshot
