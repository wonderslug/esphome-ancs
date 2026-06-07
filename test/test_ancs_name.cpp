// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Brian Towles

#include "doctest.h"
#include "ancs_name_resolver.h"

using esphome::ancs::resolve_ble_name;

TEST_CASE("no suffix, configured ancs name -> ancs name") {
  CHECK(resolve_ble_name("MyDevice", true, "node-a1b2c3", "a1b2c3", "") == "MyDevice");
}

TEST_CASE("no suffix, no ancs name -> node name kept as-is (mac retained)") {
  CHECK(resolve_ble_name("", false, "ancservice-a1b2c3", "a1b2c3", "") == "ancservice-a1b2c3");
}

TEST_CASE("no suffix, no ancs name, no mac suffix -> node name") {
  CHECK(resolve_ble_name("", false, "ancservice", "a1b2c3", "") == "ancservice");
}

TEST_CASE("suffix + configured ancs name -> appended") {
  CHECK(resolve_ble_name("MyDevice", true, "node-a1b2c3", "a1b2c3", "Kitchen") == "MyDevice-Kitchen");
}

TEST_CASE("suffix + node name with mac -> mac segment replaced") {
  CHECK(resolve_ble_name("", false, "ancservice-a1b2c3", "a1b2c3", "Kitchen") == "ancservice-Kitchen");
}

TEST_CASE("suffix + node name without mac -> appended") {
  CHECK(resolve_ble_name("", false, "ancservice", "a1b2c3", "Kitchen") == "ancservice-Kitchen");
}

TEST_CASE("mac suffix match is case-insensitive") {
  CHECK(resolve_ble_name("", false, "ancservice-A1B2C3", "a1b2c3", "Kitchen") == "ancservice-Kitchen");
}

TEST_CASE("whitespace-only suffix is treated as cleared") {
  CHECK(resolve_ble_name("", false, "ancservice-a1b2c3", "a1b2c3", "   ") == "ancservice-a1b2c3");
}

TEST_CASE("suffix is trimmed") {
  CHECK(resolve_ble_name("", false, "ancservice", "a1b2c3", "  Kitchen  ") == "ancservice-Kitchen");
}

TEST_CASE("result clamped to 29, base trimmed to keep suffix") {
  std::string r = resolve_ble_name("verylongprefixname", true, "n", "a1b2c3", "LivingRoomDownstairs");
  CHECK(r.size() == 29);
  CHECK(r.substr(r.size() - 20) == "LivingRoomDownstairs");
  CHECK(r == "verylong-LivingRoomDownstairs");
}

TEST_CASE("no-suffix name longer than 29 is clamped") {
  std::string r = resolve_ble_name("", false, "this-node-name-is-way-too-long-for-ble", "a1b2c3", "");
  CHECK(r.size() == 29);
  CHECK(r == "this-node-name-is-way-too-lon");
}
