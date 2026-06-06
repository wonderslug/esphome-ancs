// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Brian Towles

#pragma once
#include "esphome/components/text/text.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "ancs_component.h"
#include <string>

namespace esphome {
namespace ancs {

// A friendly name suffix, settable from any ESPHome frontend (HA / MQTT /
// web_server) and persisted to NVS. On change it pushes the value into the
// AncsComponent, which re-resolves the advertised name and re-advertises.
class AncsNameSuffixText : public text::Text, public Component {
 public:
  void set_parent(AncsComponent *parent) { parent_ = parent; }
  void set_restore_value(bool restore) { restore_value_ = restore; }

  void setup() override;
  void dump_config() override;
  // Run before the ANCS component (AFTER_WIFI) so the restored suffix is pushed
  // into the component before BLE init builds the first advertisement.
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  void control(const std::string &value) override;

  AncsComponent *parent_{nullptr};
  bool restore_value_{true};
  ESPPreferenceObject pref_;
};

}  // namespace ancs
}  // namespace esphome
