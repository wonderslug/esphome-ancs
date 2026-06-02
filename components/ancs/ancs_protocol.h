// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Brian Towles

#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

namespace esphome {
namespace ancs {
namespace protocol {

// 128-bit ANCS UUID strings (big-endian text form)
static constexpr const char *SERVICE_UUID   = "7905F431-B5CE-4E99-A40F-4B1E122D00D0";
static constexpr const char *NOTIF_SRC_UUID = "9FBF120D-6301-42D9-8C58-25E699A21DBD";
static constexpr const char *CTRL_POINT_UUID = "69D1D8F3-45E1-49A8-9821-9BBDFDAAD9D9";
static constexpr const char *DATA_SRC_UUID  = "22EAC6E9-24D6-4BB5-BE44-B36ACE7C7BFB";

enum class EventId : uint8_t { ADDED = 0, MODIFIED = 1, REMOVED = 2 };

enum EventFlags : uint8_t {
  FLAG_SILENT = 0x01,
  FLAG_IMPORTANT = 0x02,
  FLAG_PRE_EXISTING = 0x04,
  FLAG_POSITIVE_ACTION = 0x08,
  FLAG_NEGATIVE_ACTION = 0x10,
};

enum class Category : uint8_t {
  OTHER = 0, INCOMING_CALL = 1, MISSED_CALL = 2, VOICEMAIL = 3, SOCIAL = 4,
  SCHEDULE = 5, EMAIL = 6, NEWS = 7, HEALTH_FITNESS = 8, BUSINESS_FINANCE = 9,
  LOCATION = 10, ENTERTAINMENT = 11,
};

enum class AttributeId : uint8_t {
  APP_IDENTIFIER = 0, TITLE = 1, SUBTITLE = 2, MESSAGE = 3, MESSAGE_SIZE = 4,
  DATE = 5, POSITIVE_ACTION_LABEL = 6, NEGATIVE_ACTION_LABEL = 7,
};

const char *category_to_string(Category c);
const char *event_id_to_string(EventId e);

struct NotificationSource {
  EventId event_id;
  uint8_t event_flags;
  Category category;     // clamped to known range; OTHER if unknown
  uint8_t raw_category;  // original byte, even if unknown
  uint8_t category_count;
  uint32_t uid;
};

// Parse the 8-byte Notification Source record. Returns false if len < 8.
bool parse_notification_source(const uint8_t *data, uint16_t len, NotificationSource &out);

inline bool is_pre_existing(const NotificationSource &n) {
  return (n.event_flags & FLAG_PRE_EXISTING) != 0;
}

struct AttributeRequest {
  AttributeId id;
  uint16_t max_len;  // 0 => length-less attribute (e.g. APP_IDENTIFIER)
};

// True for attributes that carry a 2-byte max length in the command.
inline bool attribute_has_max_len(AttributeId id) {
  return id == AttributeId::TITLE || id == AttributeId::SUBTITLE || id == AttributeId::MESSAGE;
}

// Build a GetNotificationAttributes (CommandID 0x00) request.
// Returns number of bytes written, or 0 if it would not fit in out_cap.
size_t build_get_notification_attributes(uint32_t uid, const AttributeRequest *reqs,
                                         size_t n_reqs, uint8_t *out, size_t out_cap);

static constexpr size_t ANCS_ATTR_BUF_SIZE = 512;

// Stateful reassembler for the GetNotificationAttributes response. Feed
// fragments in arrival order. COMPLETE once every requested attribute is seen.
class DataSourceAssembler {
 public:
  enum class Status { NEED_MORE, COMPLETE, STALE_UID, BUFFER_OVERFLOW, BAD_COMMAND };

  // Begin a new response for `uid`, expecting the listed attribute ids.
  void reset(uint32_t uid, std::vector<AttributeId> expected);

  // Append a fragment and attempt to parse. Idempotent on COMPLETE.
  Status feed(const uint8_t *data, uint16_t len);

  // Value of a parsed attribute (empty string if not present).
  const std::string &value(AttributeId id) const;
  uint32_t uid() const { return uid_; }

 private:
  Status parse_buffer_();

  uint32_t uid_{0};
  std::vector<AttributeId> expected_;
  std::vector<uint8_t> buf_;
  bool header_checked_{false};
  std::vector<std::pair<AttributeId, std::string>> values_;
  std::string empty_;
};

}  // namespace protocol
}  // namespace ancs
}  // namespace esphome
