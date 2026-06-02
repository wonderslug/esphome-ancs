// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Brian Towles

#include "ancs_protocol.h"

namespace esphome {
namespace ancs {
namespace protocol {

const char *category_to_string(Category c) {
  switch (c) {
    case Category::OTHER:
      return "other";
    case Category::INCOMING_CALL:
      return "incoming_call";
    case Category::MISSED_CALL:
      return "missed_call";
    case Category::VOICEMAIL:
      return "voicemail";
    case Category::SOCIAL:
      return "social";
    case Category::SCHEDULE:
      return "schedule";
    case Category::EMAIL:
      return "email";
    case Category::NEWS:
      return "news";
    case Category::HEALTH_FITNESS:
      return "health_fitness";
    case Category::BUSINESS_FINANCE:
      return "business_finance";
    case Category::LOCATION:
      return "location";
    case Category::ENTERTAINMENT:
      return "entertainment";
  }
  return "unknown";
}

const char *event_id_to_string(EventId e) {
  switch (e) {
    case EventId::ADDED:
      return "added";
    case EventId::MODIFIED:
      return "modified";
    case EventId::REMOVED:
      return "removed";
  }
  return "unknown";
}

bool parse_notification_source(const uint8_t *data, uint16_t len, NotificationSource &out) {
  if (len < 8)
    return false;
  out.event_id = static_cast<EventId>(data[0]);
  out.event_flags = data[1];
  out.raw_category = data[2];
  out.category =
      (data[2] <= static_cast<uint8_t>(Category::ENTERTAINMENT)) ? static_cast<Category>(data[2]) : Category::OTHER;
  out.category_count = data[3];
  out.uid = static_cast<uint32_t>(data[4]) | (static_cast<uint32_t>(data[5]) << 8) |
            (static_cast<uint32_t>(data[6]) << 16) | (static_cast<uint32_t>(data[7]) << 24);
  return true;
}

size_t build_get_notification_attributes(uint32_t uid, const AttributeRequest *reqs, size_t n_reqs, uint8_t *out,
                                         size_t out_cap) {
  // worst case: 1 + 4 + n*(1 + 2)
  size_t needed = 5;
  for (size_t i = 0; i < n_reqs; i++)
    needed += 1 + (attribute_has_max_len(reqs[i].id) ? 2 : 0);
  if (needed > out_cap)
    return 0;

  size_t pos = 0;
  out[pos++] = 0x00;  // CommandID: GetNotificationAttributes
  out[pos++] = static_cast<uint8_t>(uid & 0xFF);
  out[pos++] = static_cast<uint8_t>((uid >> 8) & 0xFF);
  out[pos++] = static_cast<uint8_t>((uid >> 16) & 0xFF);
  out[pos++] = static_cast<uint8_t>((uid >> 24) & 0xFF);
  for (size_t i = 0; i < n_reqs; i++) {
    out[pos++] = static_cast<uint8_t>(reqs[i].id);
    if (attribute_has_max_len(reqs[i].id)) {
      out[pos++] = static_cast<uint8_t>(reqs[i].max_len & 0xFF);
      out[pos++] = static_cast<uint8_t>((reqs[i].max_len >> 8) & 0xFF);
    }
  }
  return pos;
}

void DataSourceAssembler::reset(uint32_t uid, std::vector<AttributeId> expected) {
  uid_ = uid;
  expected_ = std::move(expected);
  buf_.clear();
  header_checked_ = false;
  values_.clear();
}

const std::string &DataSourceAssembler::value(AttributeId id) const {
  for (auto &v : values_)
    if (v.first == id)
      return v.second;
  return empty_;
}

DataSourceAssembler::Status DataSourceAssembler::feed(const uint8_t *data, uint16_t len) {
  if (buf_.size() + len > ANCS_ATTR_BUF_SIZE) {
    buf_.clear();
    header_checked_ = false;
    values_.clear();
    return Status::BUFFER_OVERFLOW;
  }
  buf_.insert(buf_.end(), data, data + len);
  return parse_buffer_();
}

DataSourceAssembler::Status DataSourceAssembler::parse_buffer_() {
  size_t pos = 0;

  if (!header_checked_) {
    if (buf_.size() < 5)
      return Status::NEED_MORE;

    if (buf_[0] != 0x00) {  // CommandID GetNotificationAttributes
      buf_.clear();
      return Status::BAD_COMMAND;
    }
    uint32_t uid = static_cast<uint32_t>(buf_[1]) | (static_cast<uint32_t>(buf_[2]) << 8) |
                   (static_cast<uint32_t>(buf_[3]) << 16) | (static_cast<uint32_t>(buf_[4]) << 24);
    if (uid != uid_) {
      buf_.clear();
      return Status::STALE_UID;
    }
    pos = 5;
    header_checked_ = true;
  }

  while (true) {
    if (buf_.size() - pos < 3)
      break;  // need id + len
    AttributeId aid = static_cast<AttributeId>(buf_[pos]);
    uint16_t alen = static_cast<uint16_t>(buf_[pos + 1]) | (static_cast<uint16_t>(buf_[pos + 2]) << 8);
    if (buf_.size() - pos - 3 < alen)
      break;  // value incomplete — wait for more
    pos += 3;
    std::string val(reinterpret_cast<const char *>(buf_.data() + pos), alen);
    bool found = false;
    for (auto &v : values_)
      if (v.first == aid) {
        v.second = val;
        found = true;
        break;
      }
    if (!found)
      values_.emplace_back(aid, val);
    pos += alen;
  }

  // Compact consumed bytes to the front.
  if (pos > 0)
    buf_.erase(buf_.begin(), buf_.begin() + pos);

  // COMPLETE once all expected attributes have been collected.
  for (AttributeId want : expected_) {
    bool have = false;
    for (auto &v : values_)
      if (v.first == want) {
        have = true;
        break;
      }
    if (!have)
      return Status::NEED_MORE;
  }
  return Status::COMPLETE;
}

}  // namespace protocol
}  // namespace ancs
}  // namespace esphome
