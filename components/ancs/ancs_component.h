// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Brian Towles

#pragma once
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#include "ancs_ble.h"
#include "ancs_protocol.h"
#include <functional>
#include <string>
#include <vector>

namespace esphome {
namespace ancs {

class AncsComponent : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  void set_device_name(const std::string &n) { device_name_ = n; }
  void set_auto_fetch(bool b) { auto_fetch_ = b; }
  void set_max_bonds(uint8_t n) { max_bonds_ = n; }
  void set_manufacturer(const std::string &m) { manufacturer_ = m; }
  void set_model(const std::string &m) { model_ = m; }
  void add_fetch_attribute(protocol::AttributeId a) { fetch_attrs_.push_back(a); }

#ifdef USE_BINARY_SENSOR
  void set_connected_binary_sensor(binary_sensor::BinarySensor *s) { connected_bs_ = s; }
  void set_call_active_binary_sensor(binary_sensor::BinarySensor *s) { call_active_bs_ = s; }
#endif
#ifdef USE_TEXT_SENSOR
  void set_connected_device_text_sensor(text_sensor::TextSensor *s) { connected_device_ts_ = s; }
  void set_last_title_text_sensor(text_sensor::TextSensor *s) { last_title_ts_ = s; }
  void set_last_message_text_sensor(text_sensor::TextSensor *s) { last_message_ts_ = s; }
  void set_last_app_id_text_sensor(text_sensor::TextSensor *s) { last_app_id_ts_ = s; }
  void set_last_caller_text_sensor(text_sensor::TextSensor *s) { last_caller_ts_ = s; }
#endif

  void add_on_connect_callback(std::function<void(const std::string &)> cb) { on_connect_.add(std::move(cb)); }
  void add_on_disconnect_callback(std::function<void(const std::string &)> cb) { on_disconnect_.add(std::move(cb)); }
  void add_on_added_callback(
      std::function<void(uint32_t, const std::string &, uint8_t, uint8_t, const std::string &)> cb) {
    on_added_.add(std::move(cb));
  }
  void add_on_modified_callback(
      std::function<void(uint32_t, const std::string &, uint8_t, const std::string &)> cb) {
    on_modified_.add(std::move(cb));
  }
  void add_on_removed_callback(
      std::function<void(uint32_t, const std::string &, const std::string &)> cb) {
    on_removed_.add(std::move(cb));
  }
  void add_on_attributes_callback(
      std::function<void(uint32_t, const std::string &, const std::string &, const std::string &,
                         const std::string &, const std::string &, const std::string &)>
          cb) {
    on_attributes_.add(std::move(cb));
  }

  void clear_bonds() { ble_.clear_bonds_and_restart(); }
  void disconnect() { ble_.disconnect(); }
  void request_attributes(uint32_t uid, const std::string &device_name = "") {
    ble_.request_attributes(uid, device_name);
  }
  const std::string &get_connected_device_name() const { return connected_device_name_; }

 protected:
  AncsBle ble_;
  std::string device_name_{"ESPHome-ANCS"};
  std::string manufacturer_{"ESPHome"};
  std::string model_{"ANCS Node"};
  bool auto_fetch_{true};
  uint8_t max_bonds_{3};
  std::vector<protocol::AttributeId> fetch_attrs_;
  uint8_t call_active_count_{0};
  uint8_t connected_count_{0};
  std::string connected_device_name_;

#ifdef USE_BINARY_SENSOR
  binary_sensor::BinarySensor *connected_bs_{nullptr};
  binary_sensor::BinarySensor *call_active_bs_{nullptr};
#endif
#ifdef USE_TEXT_SENSOR
  text_sensor::TextSensor *connected_device_ts_{nullptr};
  text_sensor::TextSensor *last_title_ts_{nullptr};
  text_sensor::TextSensor *last_message_ts_{nullptr};
  text_sensor::TextSensor *last_app_id_ts_{nullptr};
  text_sensor::TextSensor *last_caller_ts_{nullptr};
#endif

  CallbackManager<void(const std::string &)> on_connect_;
  CallbackManager<void(const std::string &)> on_disconnect_;
  CallbackManager<void(uint32_t, const std::string &, uint8_t, uint8_t, const std::string &)> on_added_;
  CallbackManager<void(uint32_t, const std::string &, uint8_t, const std::string &)> on_modified_;
  CallbackManager<void(uint32_t, const std::string &, const std::string &)> on_removed_;
  CallbackManager<void(uint32_t, const std::string &, const std::string &, const std::string &,
                       const std::string &, const std::string &, const std::string &)>
      on_attributes_;
};

}  // namespace ancs
}  // namespace esphome
