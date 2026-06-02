// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Brian Towles

#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include "ancs_protocol.h"

namespace esphome {
namespace ancs {

// POD events marshaled from the NimBLE host task to the ESPHome loop.
enum class BleEventType { CONNECTED, DISCONNECTED, NOTIF_ADDED, NOTIF_MODIFIED, NOTIF_REMOVED, ATTRIBUTES };

struct BleEvent {
  BleEventType type;
  uint32_t uid;
  protocol::Category category;
  uint8_t category_count;
  uint8_t flags;
  std::string app_id;
  std::string title;
  std::string subtitle;
  std::string message;
  // Populated only on CONNECTED: the iPhone's own name from the GAP
  // Device Name characteristic (0x2A00), e.g. "Brian's iPhone".
  std::string device_name;
};

// Configuration passed from the component into the BLE layer.
struct BleConfig {
  std::string device_name;
  std::string manufacturer;
  std::string model;
  bool auto_fetch{true};
  std::vector<protocol::AttributeId> fetch_attributes;
};

class AncsBle {
 public:
  // Called once from AncsComponent::setup() (main task).
  void init(const BleConfig &cfg);
  // Called every AncsComponent::loop() — applies deferred adv override.
  void loop();

  // Non-blocking dequeue. Returns false if empty. Called from the ESPHome loop.
  bool pop_event(BleEvent &out);

  // Actions (safe to call from the ESPHome loop task).
  void clear_bonds_and_restart();
  void disconnect();
  void request_attributes(uint32_t uid);
};

}  // namespace ancs
}  // namespace esphome
