// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Brian Towles

#include "ancs_component.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ancs {

static const char *const TAG = "ancs";

void AncsComponent::setup() {
  BleConfig cfg;
  cfg.device_name = device_name_;
  cfg.manufacturer = manufacturer_;
  cfg.model = model_;
  cfg.auto_fetch = auto_fetch_;
  cfg.fetch_attributes = fetch_attrs_;
  ble_.init(cfg);
#ifdef USE_BINARY_SENSOR
  if (connected_bs_)
    connected_bs_->publish_initial_state(false);
  if (call_active_bs_)
    call_active_bs_->publish_initial_state(false);
#endif
}

void AncsComponent::loop() {
  ble_.loop();
  BleEvent ev;
  while (ble_.pop_event(ev)) {
    const std::string cat = protocol::category_to_string(ev.category);
    switch (ev.type) {
      case BleEventType::CONNECTED:
        ESP_LOGI(TAG, "iPhone connected: %s", ev.device_name.c_str());
        connected_count_++;
        connected_device_name_ = ev.device_name;
#ifdef USE_BINARY_SENSOR
        if (connected_bs_)
          connected_bs_->publish_state(true);
#endif
#ifdef USE_TEXT_SENSOR
        if (connected_device_ts_)
          connected_device_ts_->publish_state(ev.device_name);
#endif
        on_connect_.call(ev.device_name);
        break;

      case BleEventType::DISCONNECTED:
        ESP_LOGI(TAG, "iPhone disconnected: %s", ev.device_name.c_str());
        if (connected_count_ > 0)
          connected_count_--;
        call_active_count_ = 0;
#ifdef USE_BINARY_SENSOR
        if (call_active_bs_)
          call_active_bs_->publish_state(false);
#endif
        if (connected_count_ == 0) {
          connected_device_name_ = "";
#ifdef USE_TEXT_SENSOR
          if (connected_device_ts_)
            connected_device_ts_->publish_state("");
#endif
        }
#ifdef USE_BINARY_SENSOR
        if (connected_bs_)
          connected_bs_->publish_state(connected_count_ > 0);
#endif
        on_disconnect_.call(ev.device_name);
        break;

      case BleEventType::NOTIF_ADDED:
        if (ev.category == protocol::Category::INCOMING_CALL) {
          call_active_count_++;
#ifdef USE_BINARY_SENSOR
          if (call_active_bs_)
            call_active_bs_->publish_state(true);
#endif
        }
        on_added_.call(ev.uid, cat, ev.category_count, ev.flags, ev.device_name);
        break;

      case BleEventType::NOTIF_MODIFIED:
        on_modified_.call(ev.uid, cat, ev.flags, ev.device_name);
        break;

      case BleEventType::NOTIF_REMOVED:
        if (ev.category == protocol::Category::INCOMING_CALL && call_active_count_ > 0) {
          call_active_count_--;
#ifdef USE_BINARY_SENSOR
          if (call_active_bs_ && call_active_count_ == 0)
            call_active_bs_->publish_state(false);
#endif
        }
        on_removed_.call(ev.uid, cat, ev.device_name);
        break;

      case BleEventType::ATTRIBUTES:
        if (ev.date.empty()) {
          ESP_LOGI(TAG, "Attributes uid=%u app=%s title=%s", ev.uid, ev.app_id.c_str(), ev.title.c_str());
        } else {
          ESP_LOGI(TAG, "Attributes uid=%u app=%s title=%s date=%s", ev.uid, ev.app_id.c_str(), ev.title.c_str(),
                   ev.date.c_str());
        }
#ifdef USE_TEXT_SENSOR
        if (last_title_ts_)
          last_title_ts_->publish_state(ev.title);
        if (last_message_ts_)
          last_message_ts_->publish_state(ev.message);
        if (last_app_id_ts_)
          last_app_id_ts_->publish_state(ev.app_id);
        if (last_caller_ts_ && ev.category == protocol::Category::INCOMING_CALL)
          last_caller_ts_->publish_state(ev.title);
#endif
        on_attributes_.call(ev.uid, cat, ev.app_id, ev.title, ev.subtitle, ev.message, ev.device_name, ev.date);
        break;
    }
  }
}
void AncsComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "ANCS:");
  ESP_LOGCONFIG(TAG, "  Device name: %s", device_name_.c_str());
  ESP_LOGCONFIG(TAG, "  Auto fetch attributes: %s", YESNO(auto_fetch_));
  ESP_LOGCONFIG(TAG, "  Max connections: %d", CONFIG_BT_NIMBLE_MAX_CONNECTIONS);
}

}  // namespace ancs
}  // namespace esphome
