// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Brian Towles

#include "ancs_name_resolver.h"
#include <algorithm>
#include <cctype>

namespace esphome {
namespace ancs {

namespace {

std::string trim(const std::string &s) {
  size_t start = 0;
  size_t end = s.size();
  while (start < end && std::isspace(static_cast<unsigned char>(s[start])))
    start++;
  while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1])))
    end--;
  return s.substr(start, end - start);
}

std::string to_lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return s;
}

// True if `name` ends with "-<mac6>" (case-insensitive).
bool ends_with_mac_suffix(const std::string &name, const std::string &mac6) {
  if (mac6.empty())
    return false;
  const std::string tail = "-" + mac6;
  if (name.size() < tail.size())
    return false;
  return to_lower(name.substr(name.size() - tail.size())) == to_lower(tail);
}

std::string clamp(const std::string &s, size_t max_len) {
  return s.size() > max_len ? s.substr(0, max_len) : s;
}

}  // namespace

std::string resolve_ble_name(const std::string &configured_name, bool name_configured, const std::string &node_name,
                             const std::string &mac6, const std::string &suffix) {
  const std::string s = trim(suffix);

  if (s.empty()) {
    // No suffix: use the configured ancs name, or the node name as-is (the node
    // name keeps any auto MAC suffix for uniqueness).
    return clamp(name_configured ? configured_name : node_name, MAX_ADV_NAME_LEN);
  }

  // Suffix present: build "<base>-<suffix>".
  std::string base;
  if (name_configured) {
    base = configured_name;
  } else {
    base = node_name;
    if (ends_with_mac_suffix(base, mac6))
      base = base.substr(0, base.size() - (mac6.size() + 1));
  }

  // Preserve the suffix; trim the base to fit the 29-char cap.
  if (s.size() >= MAX_ADV_NAME_LEN)
    return s.substr(0, MAX_ADV_NAME_LEN);

  const size_t base_budget = MAX_ADV_NAME_LEN - 1 - s.size();  // room for '-' + suffix
  if (base.size() > base_budget)
    base = base.substr(0, base_budget);
  if (base.empty())
    return s;  // no room for base/dash — return the suffix alone
  return base + "-" + s;
}

}  // namespace ancs
}  // namespace esphome
