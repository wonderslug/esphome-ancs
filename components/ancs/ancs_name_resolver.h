// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Brian Towles

#pragma once
#include <cstddef>
#include <string>

namespace esphome {
namespace ancs {

// Maximum advertised / GAP device-name length (BLE adv-name cap).
static constexpr size_t MAX_ADV_NAME_LEN = 29;

// Pure resolution of the BLE advertised name. No ESPHome-core dependencies so it
// can be unit-tested host-side.
//
//   configured_name  the `ancs: name:` value (only used when name_configured)
//   name_configured  true if the user set `ancs: name:`
//   node_name        App.get_name() at runtime (may be "node" or "node-<mac6>")
//   mac6             lowercase last-6-hex of the device MAC (for suffix detection)
//   suffix           the raw name_suffix value (may be empty / whitespace)
//
// Returns a name clamped to MAX_ADV_NAME_LEN. When a suffix is present it is the
// human-meaningful part: it is preserved and the base/prefix is trimmed to fit.
std::string resolve_ble_name(const std::string &configured_name, bool name_configured, const std::string &node_name,
                             const std::string &mac6, const std::string &suffix);

}  // namespace ancs
}  // namespace esphome
