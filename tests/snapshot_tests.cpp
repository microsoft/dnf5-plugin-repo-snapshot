// SPDX-License-Identifier: MIT
#include "snapshot.hpp"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <functional>
#include <limits>
#include <stdexcept>

static void rejects(const std::function<void()>& action) {
  bool rejected = false;
  try {
    action();
  } catch (const std::exception&) {
    rejected = true;
  }
  assert(rejected);
}

int main() {
  constexpr auto max_time = std::numeric_limits<std::uint64_t>::max();
  assert(snapshot::VERSION_MAJOR == 0);
  assert(snapshot::VERSION_MINOR == 1);
  assert(snapshot::VERSION_PATCH == 0);
  assert(snapshot::compatible_backend(2, 2));
  assert(snapshot::compatible_backend(2, 3));
  assert(!snapshot::compatible_backend(2, 1));
  assert(!snapshot::compatible_backend(3, 2));
  assert(std::string(snapshot::attribute("author.name", "backend")) ==
         "Microsoft");
  assert(std::string(snapshot::attribute("author.email", "backend")) ==
         "azlinux@microsoft.com");
  assert(std::string(snapshot::attribute("description", "backend")) ==
         "backend");
  assert(snapshot::attribute("other", "backend") == nullptr);
  assert(snapshot::parse_time("0") == 0);
  assert(snapshot::parse_time("1761696000") == 1761696000);
  assert(snapshot::parse_time("9223372036854775807") == 9223372036854775807ULL);
  assert(snapshot::parse_time("9223372036854775808") == 9223372036854775808ULL);
  assert(snapshot::parse_time("18446744073709551615") == max_time);
  for (const auto& bad : {"", "-1", "+1", " 1", "1 ", "1\n", "1761696000x",
                          "18446744073709551616", "99999999999999999999999"}) {
    rejects([&] { (void)snapshot::parse_time(bad); });
  }
  const auto filter = [](std::optional<unsigned long long> time,
                         std::optional<std::uint64_t> cutoff,
                         bool installed = false, bool exempt = false) {
    return snapshot::filter_package(time, cutoff, installed, exempt, "fixture",
                                    "alpha-1-1.x86_64");
  };
  assert(!filter(1761695999, 1761696000));
  assert(!filter(1761696000, 1761696000));
  assert(filter(1761696001, 1761696000));
  assert(!filter(0, 0));
  assert(filter(1, 0));
  assert(!filter(max_time, max_time));
  assert(filter(max_time, max_time - 1));
  assert(!filter(9223372036854775808ULL, max_time));
  assert(!filter(1761696001, std::nullopt));
  assert(!filter(1761696001, 1761696000, true));
  assert(!filter(1761696001, 1761696000, false, true));
  assert(!filter(std::nullopt, std::nullopt));
  assert(!filter(std::nullopt, 1761696000, true));
  assert(!filter(std::nullopt, 1761696000, false, true));
  rejects([&] { (void)filter(std::nullopt, 1761696000); });
  rejects([&] { (void)filter(std::nullopt, max_time); });
}
