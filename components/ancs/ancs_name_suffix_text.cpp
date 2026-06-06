// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Brian Towles

#include "ancs_name_suffix_text.h"
#include "esphome/core/log.h"
#include <cstring>

namespace esphome {
namespace ancs {

static const char *const TAG = "ancs.text";

// Stored-suffix cap. The final advertised name is capped at 29 chars, so a
// suffix beyond this is never useful; keep the NVS record small and fixed-size.
static const size_t SUFFIX_STORE_MAX = 31;

void AncsNameSuffixText::setup() {
  std::string restored;
  if (this->restore_value_) {
    this->pref_ = global_preferences->make_preference<char[SUFFIX_STORE_MAX + 1]>(this->get_object_id_hash());
    char buf[SUFFIX_STORE_MAX + 1] = {};
    if (this->pref_.load(&buf)) {
      buf[SUFFIX_STORE_MAX] = '\0';
      restored = buf;
    }
  }
  this->publish_state(restored);
  if (this->parent_ != nullptr)
    this->parent_->set_name_suffix(restored);
}

void AncsNameSuffixText::control(const std::string &value) {
  this->publish_state(value);
  if (this->restore_value_) {
    char buf[SUFFIX_STORE_MAX + 1] = {};
    std::strncpy(buf, value.c_str(), SUFFIX_STORE_MAX);
    this->pref_.save(&buf);
  }
  if (this->parent_ != nullptr)
    this->parent_->set_name_suffix(value);
}

void AncsNameSuffixText::dump_config() {
  LOG_TEXT("", "ANCS Name Suffix", this);
  ESP_LOGCONFIG(TAG, "  Restore value: %s", YESNO(this->restore_value_));
}

}  // namespace ancs
}  // namespace esphome
