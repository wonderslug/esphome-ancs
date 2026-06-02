// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Brian Towles

#pragma once
#include "esphome/core/automation.h"
#include "ancs_component.h"
#include "ancs_protocol.h"

namespace esphome {
namespace ancs {

class ConnectTrigger : public Trigger<> {
 public:
  explicit ConnectTrigger(AncsComponent *parent) { parent->add_on_connect_callback([this]() { this->trigger(); }); }
};

class DisconnectTrigger : public Trigger<> {
 public:
  explicit DisconnectTrigger(AncsComponent *parent) { parent->add_on_disconnect_callback([this]() { this->trigger(); }); }
};

// added: (uid, category, category_count, flags)
class NotificationAddedTrigger : public Trigger<uint32_t, std::string, uint8_t, uint8_t> {
 public:
  explicit NotificationAddedTrigger(AncsComponent *parent) {
    parent->add_on_added_callback([this](uint32_t uid, const std::string &cat, uint8_t count, uint8_t flags) {
      this->trigger(uid, cat, count, flags);
    });
  }
};

class NotificationModifiedTrigger : public Trigger<uint32_t, std::string, uint8_t> {
 public:
  explicit NotificationModifiedTrigger(AncsComponent *parent) {
    parent->add_on_modified_callback([this](uint32_t uid, const std::string &cat, uint8_t flags) {
      this->trigger(uid, cat, flags);
    });
  }
};

class NotificationRemovedTrigger : public Trigger<uint32_t, std::string> {
 public:
  explicit NotificationRemovedTrigger(AncsComponent *parent) {
    parent->add_on_removed_callback([this](uint32_t uid, const std::string &cat) { this->trigger(uid, cat); });
  }
};

// attributes: (uid, category, app_id, title, subtitle, message)
class NotificationAttributesTrigger
    : public Trigger<uint32_t, std::string, std::string, std::string, std::string, std::string> {
 public:
  explicit NotificationAttributesTrigger(AncsComponent *parent) {
    parent->add_on_attributes_callback(
        [this](uint32_t uid, const std::string &cat, const std::string &app, const std::string &title,
               const std::string &sub, const std::string &msg) { this->trigger(uid, cat, app, title, sub, msg); });
  }
};

template<typename... Ts> class ClearBondsAction : public Action<Ts...> {
 public:
  explicit ClearBondsAction(AncsComponent *p) : parent_(p) {}
  void play(Ts... x) override { parent_->clear_bonds(); }
 protected:
  AncsComponent *parent_;
};

template<typename... Ts> class DisconnectAction : public Action<Ts...> {
 public:
  explicit DisconnectAction(AncsComponent *p) : parent_(p) {}
  void play(Ts... x) override { parent_->disconnect(); }
 protected:
  AncsComponent *parent_;
};

template<typename... Ts> class RequestAttributesAction : public Action<Ts...> {
 public:
  explicit RequestAttributesAction(AncsComponent *p) : parent_(p) {}
  TEMPLATABLE_VALUE(uint32_t, uid)
  void play(Ts... x) override { parent_->request_attributes(this->uid_.value(x...)); }
 protected:
  AncsComponent *parent_;
};

}  // namespace ancs
}  // namespace esphome
