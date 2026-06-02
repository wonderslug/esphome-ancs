// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Brian Towles

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "ancs_protocol.h"

using namespace esphome::ancs::protocol;

TEST_CASE("category enum values match ANCS spec") {
  CHECK(static_cast<uint8_t>(Category::OTHER) == 0);
  CHECK(static_cast<uint8_t>(Category::INCOMING_CALL) == 1);
  CHECK(static_cast<uint8_t>(Category::MISSED_CALL) == 2);
  CHECK(static_cast<uint8_t>(Category::ENTERTAINMENT) == 11);
}

TEST_CASE("event id enum values match ANCS spec") {
  CHECK(static_cast<uint8_t>(EventId::ADDED) == 0);
  CHECK(static_cast<uint8_t>(EventId::MODIFIED) == 1);
  CHECK(static_cast<uint8_t>(EventId::REMOVED) == 2);
}

TEST_CASE("attribute id enum values match ANCS spec") {
  CHECK(static_cast<uint8_t>(AttributeId::APP_IDENTIFIER) == 0);
  CHECK(static_cast<uint8_t>(AttributeId::TITLE) == 1);
  CHECK(static_cast<uint8_t>(AttributeId::MESSAGE) == 3);
}

TEST_CASE("category_to_string maps known categories") {
  CHECK(std::string(category_to_string(Category::INCOMING_CALL)) == "incoming_call");
  CHECK(std::string(category_to_string(Category::OTHER)) == "other");
}

TEST_CASE("parse_notification_source parses a valid 8-byte record") {
  // event_id=0(ADDED) flags=0x02(IMPORTANT) category=1(CALL) count=3 uid=0x04030201
  const uint8_t data[8] = {0x00, 0x02, 0x01, 0x03, 0x01, 0x02, 0x03, 0x04};
  NotificationSource ns;
  REQUIRE(parse_notification_source(data, 8, ns));
  CHECK(ns.event_id == EventId::ADDED);
  CHECK(ns.event_flags == 0x02);
  CHECK(ns.category == Category::INCOMING_CALL);
  CHECK(ns.category_count == 3);
  CHECK(ns.uid == 0x04030201u);
}

TEST_CASE("parse_notification_source rejects short buffers") {
  const uint8_t data[7] = {0};
  NotificationSource ns;
  CHECK_FALSE(parse_notification_source(data, 7, ns));
}

TEST_CASE("parse_notification_source detects PRE_EXISTING flag") {
  const uint8_t data[8] = {0x00, 0x04, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00};
  NotificationSource ns;
  REQUIRE(parse_notification_source(data, 8, ns));
  CHECK(is_pre_existing(ns));
}

TEST_CASE("parse_notification_source unknown category preserved as raw") {
  const uint8_t data[8] = {0x02, 0x00, 0x63, 0x00, 0x09, 0x00, 0x00, 0x00};
  NotificationSource ns;
  REQUIRE(parse_notification_source(data, 8, ns));
  CHECK(ns.event_id == EventId::REMOVED);
  CHECK(ns.raw_category == 0x63);
}

TEST_CASE("build_get_notification_attributes emits correct bytes") {
  // Request APP_IDENTIFIER (no max_len) + TITLE (max_len 32) for uid 0x04030201
  AttributeRequest reqs[] = {
      {AttributeId::APP_IDENTIFIER, 0},
      {AttributeId::TITLE, 32},
  };
  uint8_t buf[32];
  size_t n = build_get_notification_attributes(0x04030201u, reqs, 2, buf, sizeof(buf));
  // [0x00][uid LE x4][0x00][0x01][32,0]
  const uint8_t expected[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x00, 0x01, 0x20, 0x00};
  REQUIRE(n == sizeof(expected));
  for (size_t i = 0; i < n; i++)
    CHECK(buf[i] == expected[i]);
}

TEST_CASE("build_get_notification_attributes returns 0 on insufficient buffer") {
  AttributeRequest reqs[] = {{AttributeId::TITLE, 32}};
  uint8_t buf[3];
  CHECK(build_get_notification_attributes(1, reqs, 1, buf, sizeof(buf)) == 0);
}

// Helper: build a GetNotificationAttributes RESPONSE stream.
// [0x00][uid LE x4]({id}{len LE x2}{value})...
static std::vector<uint8_t> make_response(uint32_t uid, const std::vector<std::pair<AttributeId, std::string>> &attrs) {
  std::vector<uint8_t> v;
  v.push_back(0x00);
  v.push_back(uid & 0xFF);
  v.push_back((uid >> 8) & 0xFF);
  v.push_back((uid >> 16) & 0xFF);
  v.push_back((uid >> 24) & 0xFF);
  for (auto &a : attrs) {
    v.push_back(static_cast<uint8_t>(a.first));
    v.push_back(a.second.size() & 0xFF);
    v.push_back((a.second.size() >> 8) & 0xFF);
    for (char c : a.second)
      v.push_back(static_cast<uint8_t>(c));
  }
  return v;
}

TEST_CASE("DataSourceAssembler parses a single-packet response") {
  DataSourceAssembler asm_;
  asm_.reset(0x04030201u, {AttributeId::APP_IDENTIFIER, AttributeId::TITLE});
  auto stream =
      make_response(0x04030201u, {{AttributeId::APP_IDENTIFIER, "com.apple.mobilephone"}, {AttributeId::TITLE, "Mom"}});
  CHECK(asm_.feed(stream.data(), stream.size()) == DataSourceAssembler::Status::COMPLETE);
  CHECK(asm_.value(AttributeId::TITLE) == "Mom");
  CHECK(asm_.value(AttributeId::APP_IDENTIFIER) == "com.apple.mobilephone");
}

TEST_CASE("DataSourceAssembler reassembles across a mid-value split") {
  DataSourceAssembler asm_;
  asm_.reset(0x04030201u, {AttributeId::TITLE});
  auto s = make_response(0x04030201u, {{AttributeId::TITLE, "Alexander"}});
  // split in the middle of the value
  size_t cut = 9;  // header(5)+id(1)+len(2)+1 value byte
  CHECK(asm_.feed(s.data(), cut) == DataSourceAssembler::Status::NEED_MORE);
  CHECK(asm_.feed(s.data() + cut, s.size() - cut) == DataSourceAssembler::Status::COMPLETE);
  CHECK(asm_.value(AttributeId::TITLE) == "Alexander");
}

TEST_CASE("DataSourceAssembler reassembles across a mid-header split") {
  DataSourceAssembler asm_;
  asm_.reset(0x04030201u, {AttributeId::APP_IDENTIFIER, AttributeId::TITLE});
  auto s = make_response(0x04030201u, {{AttributeId::APP_IDENTIFIER, "a"}, {AttributeId::TITLE, "Bob"}});
  // cut inside the TITLE header (after app_id value, mid length field)
  size_t cut = 5 + 3 + 1 + 1;  // header + appid header + appid value + 1 byte into title header
  CHECK(asm_.feed(s.data(), cut) == DataSourceAssembler::Status::NEED_MORE);
  CHECK(asm_.feed(s.data() + cut, s.size() - cut) == DataSourceAssembler::Status::COMPLETE);
  CHECK(asm_.value(AttributeId::TITLE) == "Bob");
}

TEST_CASE("DataSourceAssembler byte-at-a-time reassembly") {
  DataSourceAssembler asm_;
  asm_.reset(0x04030201u, {AttributeId::APP_IDENTIFIER, AttributeId::TITLE});
  auto s = make_response(0x04030201u, {{AttributeId::APP_IDENTIFIER, "x"}, {AttributeId::TITLE, "Carol"}});
  DataSourceAssembler::Status st = DataSourceAssembler::Status::NEED_MORE;
  for (size_t i = 0; i < s.size(); i++)
    st = asm_.feed(&s[i], 1);
  CHECK(st == DataSourceAssembler::Status::COMPLETE);
  CHECK(asm_.value(AttributeId::TITLE) == "Carol");
}

TEST_CASE("DataSourceAssembler discards a response for a stale UID") {
  DataSourceAssembler asm_;
  asm_.reset(0x04030201u, {AttributeId::TITLE});
  auto s = make_response(0x09090909u, {{AttributeId::TITLE, "Nope"}});
  CHECK(asm_.feed(s.data(), s.size()) == DataSourceAssembler::Status::STALE_UID);
}

TEST_CASE("DataSourceAssembler reports overflow and resets") {
  DataSourceAssembler asm_;
  asm_.reset(0x04030201u, {AttributeId::TITLE});
  std::vector<uint8_t> big(ANCS_ATTR_BUF_SIZE + 10, 0x00);
  CHECK(asm_.feed(big.data(), big.size()) == DataSourceAssembler::Status::BUFFER_OVERFLOW);
}
