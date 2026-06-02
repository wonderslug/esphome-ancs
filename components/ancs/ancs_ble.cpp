// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Brian Towles

#include "ancs_ble.h"
#ifdef USE_ESP_IDF
#include <cstring>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esphome/core/log.h"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "esphome/core/helpers.h"
#include "esp_system.h"

extern "C" void ble_store_config_init(void);

namespace esphome {
namespace ancs {
static const char *const TAG = "ancs.ble";
static QueueHandle_t s_queue = nullptr;

static BleConfig s_cfg;
static uint8_t s_own_addr_type = 0;

// ANCS solicited UUID, little-endian (AD type 0x15).
// This is what iOS watches for to identify ANCS notification consumers.
static const uint8_t k_ancs_uuid_le[16] = {0xD0, 0x00, 0x2D, 0x12, 0x1E, 0x4B, 0x0F, 0xA4,
                                           0x99, 0x4E, 0xCE, 0xB5, 0x31, 0xF4, 0x05, 0x79};

// ---------------------------------------------------------------------------
// ANCS 128-bit UUIDs as native ble_uuid128_t objects (little-endian byte arrays)
// ---------------------------------------------------------------------------
// ble_uuid128_t layout: { ble_uuid_t u { uint8_t type }, uint8_t value[16] }
// The macro-initializer works for both C and C++ aggregate initialization.
#define MK_UUID128(b0, b1, b2, b3, b4, b5, b6, b7, b8, b9, ba, bb, bc, bd, be, bf) \
  {                                                                                \
    {BLE_UUID_TYPE_128}, {                                                         \
      b0, b1, b2, b3, b4, b5, b6, b7, b8, b9, ba, bb, bc, bd, be, bf               \
    }                                                                              \
  }

// Service:           7905F431-B5CE-4E99-A40F-4B1E122D00D0
static ble_uuid128_t s_ancs_svc_uuid =
    MK_UUID128(0xD0, 0x00, 0x2D, 0x12, 0x1E, 0x4B, 0x0F, 0xA4, 0x99, 0x4E, 0xCE, 0xB5, 0x31, 0xF4, 0x05, 0x79);

// Notification Source: 9FBF120D-6301-42D9-8C58-25E699A21DBD
static ble_uuid128_t s_notif_src_uuid =
    MK_UUID128(0xBD, 0x1D, 0xA2, 0x99, 0xE6, 0x25, 0x58, 0x8C, 0xD9, 0x42, 0x01, 0x63, 0x0D, 0x12, 0xBF, 0x9F);

// Control Point:       69D1D8F3-45E1-49A8-9821-9BBDFDAAD9D9
static ble_uuid128_t s_ctrl_point_uuid =
    MK_UUID128(0xD9, 0xD9, 0xAA, 0xFD, 0xBD, 0x9B, 0x21, 0x98, 0xA8, 0x49, 0xE1, 0x45, 0xF3, 0xD8, 0xD1, 0x69);

// Data Source:         22EAC6E9-24D6-4BB5-BE44-B36ACE7C7BFB
static ble_uuid128_t s_data_src_uuid =
    MK_UUID128(0xFB, 0x7B, 0x7C, 0xCE, 0x6A, 0xB3, 0x44, 0xBE, 0xB5, 0x4B, 0xD6, 0x24, 0xE9, 0xC6, 0xEA, 0x22);

#undef MK_UUID128

// Helper: get const ble_uuid_t* from ble_uuid128_t (safe upcast via first member)
static inline const ble_uuid_t *u128p(ble_uuid128_t *u) {
  return reinterpret_cast<const ble_uuid_t *>(u);
}

// ---------------------------------------------------------------------------
// Connection state
// ---------------------------------------------------------------------------
static uint16_t s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static uint16_t s_ns_handle = 0;  // Notification Source value handle
static uint16_t s_cp_handle = 0;  // Control Point value handle
static uint16_t s_ds_handle = 0;  // Data Source value handle

static protocol::DataSourceAssembler s_assembler;
static uint32_t s_fetch_uid = 0;
static protocol::Category s_fetch_category = protocol::Category::OTHER;
static bool s_fetch_pending = false;  // true while waiting for Data Source to complete

// Set by start_advertising(), cleared by loop() once the deferred override
// is successfully applied from the main-loop task.
static volatile bool s_adv_override_pending = false;

// Push a BleEvent onto the queue (called from the NimBLE host task).
// Heap-allocates so the std::strings survive the queue; pop_event takes ownership.
static void push_event(const BleEvent &ev) {
  if (s_queue == nullptr)
    return;
  auto *heap_ev = new BleEvent(ev);
  if (xQueueSend(s_queue, &heap_ev, 0) != pdTRUE) {
    BleEvent *old = nullptr;
    if (xQueueReceive(s_queue, &old, 0) == pdTRUE)
      delete old;
    if (xQueueSend(s_queue, &heap_ev, 0) != pdTRUE) {
      delete heap_ev;
      ESP_LOGW(TAG, "event queue full; dropped event");
    }
  }
}

// ---------------------------------------------------------------------------
// DIS (Device Information Service) GATT table
// ---------------------------------------------------------------------------
#define MK_UUID16(val)        \
  ble_uuid16_t {              \
    {BLE_UUID_TYPE_16}, (val) \
  }

static ble_uuid16_t s_uuid_svc_dis = MK_UUID16(0x180A);
static ble_uuid16_t s_uuid_chr_mfr = MK_UUID16(0x2A29);
static ble_uuid16_t s_uuid_chr_model = MK_UUID16(0x2A24);
static ble_uuid16_t s_uuid_chr_serial = MK_UUID16(0x2A25);
static ble_uuid16_t s_uuid_chr_hw_rev = MK_UUID16(0x2A27);
static ble_uuid16_t s_uuid_chr_sw_rev = MK_UUID16(0x2A28);
static ble_uuid16_t s_uuid_chr_fw_rev = MK_UUID16(0x2A26);
// GAP Device Name (0x2A00) — read from the iPhone after ANCS is ready.
static ble_uuid16_t s_uuid_device_name = MK_UUID16(0x2A00);

#undef MK_UUID16

// Scratch buffer for the iPhone's device name while the GATT read is in flight.
static char s_peer_name_buf[64];

static inline const ble_uuid_t *u16p(ble_uuid16_t *u) {
  return reinterpret_cast<const ble_uuid_t *>(u);
}

static int dis_access_cb(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg) {
  (void)conn_handle;
  (void)attr_handle;

  const ble_uuid_t *uuid = ctxt->chr->uuid;
  const char *value = nullptr;

  if (ble_uuid_cmp(uuid, u16p(&s_uuid_chr_mfr)) == 0) {
    value = s_cfg.manufacturer.c_str();
  } else if (ble_uuid_cmp(uuid, u16p(&s_uuid_chr_model)) == 0) {
    value = s_cfg.model.c_str();
  } else if (ble_uuid_cmp(uuid, u16p(&s_uuid_chr_serial)) == 0) {
    value = "0";
  } else if (ble_uuid_cmp(uuid, u16p(&s_uuid_chr_hw_rev)) == 0) {
    value = "1";
  } else if (ble_uuid_cmp(uuid, u16p(&s_uuid_chr_sw_rev)) == 0) {
    value = "0.1.0";
  } else if (ble_uuid_cmp(uuid, u16p(&s_uuid_chr_fw_rev)) == 0) {
    value = "1.0.0";
  } else {
    return BLE_ATT_ERR_UNLIKELY;
  }

  int rc = os_mbuf_append(ctxt->om, value, (uint16_t)strlen(value));
  return (rc == 0) ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

static const ble_uuid_t *const s_p_svc_dis = reinterpret_cast<const ble_uuid_t *>(&s_uuid_svc_dis);
static const ble_uuid_t *const s_p_chr_mfr = reinterpret_cast<const ble_uuid_t *>(&s_uuid_chr_mfr);
static const ble_uuid_t *const s_p_chr_model = reinterpret_cast<const ble_uuid_t *>(&s_uuid_chr_model);
static const ble_uuid_t *const s_p_chr_serial = reinterpret_cast<const ble_uuid_t *>(&s_uuid_chr_serial);
static const ble_uuid_t *const s_p_chr_hw_rev = reinterpret_cast<const ble_uuid_t *>(&s_uuid_chr_hw_rev);
static const ble_uuid_t *const s_p_chr_sw_rev = reinterpret_cast<const ble_uuid_t *>(&s_uuid_chr_sw_rev);
static const ble_uuid_t *const s_p_chr_fw_rev = reinterpret_cast<const ble_uuid_t *>(&s_uuid_chr_fw_rev);

static const struct ble_gatt_chr_def s_dis_chars[] = {
    {
        .uuid = s_p_chr_mfr,
        .access_cb = dis_access_cb,
        .arg = nullptr,
        .descriptors = nullptr,
        .flags = BLE_GATT_CHR_F_READ,
        .min_key_size = 0,
        .val_handle = nullptr,
        .cpfd = nullptr,
    },
    {
        .uuid = s_p_chr_model,
        .access_cb = dis_access_cb,
        .arg = nullptr,
        .descriptors = nullptr,
        .flags = BLE_GATT_CHR_F_READ,
        .min_key_size = 0,
        .val_handle = nullptr,
        .cpfd = nullptr,
    },
    {
        .uuid = s_p_chr_serial,
        .access_cb = dis_access_cb,
        .arg = nullptr,
        .descriptors = nullptr,
        .flags = BLE_GATT_CHR_F_READ,
        .min_key_size = 0,
        .val_handle = nullptr,
        .cpfd = nullptr,
    },
    {
        .uuid = s_p_chr_hw_rev,
        .access_cb = dis_access_cb,
        .arg = nullptr,
        .descriptors = nullptr,
        .flags = BLE_GATT_CHR_F_READ,
        .min_key_size = 0,
        .val_handle = nullptr,
        .cpfd = nullptr,
    },
    {
        .uuid = s_p_chr_sw_rev,
        .access_cb = dis_access_cb,
        .arg = nullptr,
        .descriptors = nullptr,
        .flags = BLE_GATT_CHR_F_READ,
        .min_key_size = 0,
        .val_handle = nullptr,
        .cpfd = nullptr,
    },
    {
        .uuid = s_p_chr_fw_rev,
        .access_cb = dis_access_cb,
        .arg = nullptr,
        .descriptors = nullptr,
        .flags = BLE_GATT_CHR_F_READ,
        .min_key_size = 0,
        .val_handle = nullptr,
        .cpfd = nullptr,
    },
    {0},
};

static const struct ble_gatt_svc_def s_dis_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = s_p_svc_dis,
        .includes = nullptr,
        .characteristics = s_dis_chars,
    },
    {0},
};

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------
static void start_advertising();
static void handle_notification_source(const uint8_t *buf, uint16_t len);
static void handle_data_source(const uint8_t *buf, uint16_t len);
static int on_disc_svc(uint16_t conn_handle, const struct ble_gatt_error *error, const struct ble_gatt_svc *svc,
                       void *arg);
static int on_disc_chr(uint16_t conn_handle, const struct ble_gatt_error *error, const struct ble_gatt_chr *chr,
                       void *arg);

// ---------------------------------------------------------------------------
// read_device_name_cb — fires after reading the iPhone's GAP Device Name
// (0x2A00). Pushes the CONNECTED event so the name is available immediately.
// ---------------------------------------------------------------------------
static int read_device_name_cb(uint16_t conn_handle, const struct ble_gatt_error *error, struct ble_gatt_attr *attr,
                               void *arg) {
  (void)conn_handle;
  (void)arg;

  if (error->status == 0 && attr != nullptr && attr->om != nullptr) {
    uint16_t len = OS_MBUF_PKTLEN(attr->om);
    uint16_t copy = (len < sizeof(s_peer_name_buf) - 1) ? len : (uint16_t)(sizeof(s_peer_name_buf) - 1);
    os_mbuf_copydata(attr->om, 0, copy, s_peer_name_buf);
    s_peer_name_buf[copy] = '\0';
    return 0;  // wait for BLE_HS_EDONE to push the event
  }

  // BLE_HS_EDONE or error — emit CONNECTED with whatever name we have.
  BleEvent ev{};
  ev.type = BleEventType::CONNECTED;
  ev.device_name = (s_peer_name_buf[0] != '\0') ? s_peer_name_buf : "iPhone";
  push_event(ev);
  ESP_LOGI(TAG, "ANCS ready — connected to: %s", ev.device_name.c_str());
  return 0;
}

// ---------------------------------------------------------------------------
// CCCD enable helper — writes 0x0001 little-endian to val_handle+1
// ---------------------------------------------------------------------------
static void write_cccd_enable(uint16_t conn_handle, uint16_t val_handle) {
  static const uint8_t cccd_val[2] = {0x01, 0x00};
  ble_gattc_write_flat(conn_handle, val_handle + 1, cccd_val, sizeof(cccd_val), NULL, NULL);
}

// ---------------------------------------------------------------------------
// Build AttributeRequest list from BleConfig fetch_attributes
// ---------------------------------------------------------------------------
static size_t build_attr_requests(protocol::AttributeRequest *reqs, size_t max_reqs) {
  size_t n = 0;
  for (auto id : s_cfg.fetch_attributes) {
    if (n >= max_reqs)
      break;
    reqs[n].id = id;
    reqs[n].max_len = protocol::attribute_has_max_len(id) ? 64 : 0;
    n++;
  }
  return n;
}

// ---------------------------------------------------------------------------
// GAP event handler
// ---------------------------------------------------------------------------
static int gap_event_cb(struct ble_gap_event *event, void *arg) {
  (void)arg;

  switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
      if (event->connect.status == 0) {
        s_conn_handle = event->connect.conn_handle;
        ESP_LOGI(TAG, "connected handle=%u — initiating security", s_conn_handle);
        // Ask iOS to bond; discovery happens only after encryption (ENC_CHANGE)
        ble_gap_security_initiate(s_conn_handle);
      } else {
        ESP_LOGW(TAG, "connect failed status=%d", event->connect.status);
        start_advertising();
      }
      return 0;

    case BLE_GAP_EVENT_DISCONNECT:
      ESP_LOGI(TAG, "disconnected handle=%u reason=%d", event->disconnect.conn.conn_handle, event->disconnect.reason);
      s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
      s_ns_handle = 0;
      s_cp_handle = 0;
      s_ds_handle = 0;
      s_fetch_pending = false;
      push_event(BleEvent{BleEventType::DISCONNECTED});
      start_advertising();
      return 0;

    case BLE_GAP_EVENT_ENC_CHANGE: {
      uint16_t enc_handle = event->enc_change.conn_handle;

      // Discard stale ENC_CHANGE events queued for a connection that has
      // already been disconnected (and s_conn_handle reset to NONE or
      // a new handle). Calling disc_all_svcs with a dead handle returns
      // rc=7 (ENOTCONN) and can corrupt GATTC state for the new connection.
      if (enc_handle != s_conn_handle) {
        ESP_LOGW(TAG, "stale enc_change for handle=%u (active=%u) — ignoring", enc_handle, s_conn_handle);
        return 0;
      }

      if (event->enc_change.status == 0) {
        struct ble_gap_conn_desc desc;
        if (ble_gap_conn_find(enc_handle, &desc) == 0 && desc.sec_state.encrypted) {
          ESP_LOGI(TAG, "encrypted — starting ANCS service discovery");
          // Use enc_handle (the event's handle), NOT s_conn_handle — they are
          // the same at this point (guarded above) but being explicit avoids
          // a repeat of the stale-handle bug if the code is later refactored.
          int rc = ble_gattc_disc_all_svcs(enc_handle, on_disc_svc, NULL);
          if (rc != 0) {
            ESP_LOGE(TAG, "ble_gattc_disc_all_svcs failed rc=%d — terminating", rc);
            ble_gap_terminate(enc_handle, 0x13 /* remote user terminated */);
          }
        } else {
          ESP_LOGW(TAG, "enc_change ok but not encrypted — skipping discovery");
        }
      } else {
        // Encryption failed (e.g. 30-second SMP timeout, auth failure).
        // Terminate the connection so the disconnect handler fires, cleans
        // up state, and restarts advertising for a clean reconnect attempt.
        ESP_LOGW(TAG, "enc_change failed status=%d — terminating to recover", event->enc_change.status);
        ble_gap_terminate(enc_handle, 0x13 /* remote user terminated */);
      }
      return 0;
    }

    case BLE_GAP_EVENT_NOTIFY_RX: {
      uint16_t attr_handle = event->notify_rx.attr_handle;
      uint16_t pkt_len = OS_MBUF_PKTLEN(event->notify_rx.om);
      if (pkt_len == 0)
        return 0;

      uint8_t buf[protocol::ANCS_ATTR_BUF_SIZE];
      uint16_t copy_len = (pkt_len < protocol::ANCS_ATTR_BUF_SIZE) ? pkt_len : (uint16_t)protocol::ANCS_ATTR_BUF_SIZE;
      os_mbuf_copydata(event->notify_rx.om, 0, copy_len, buf);

      if (attr_handle == s_ns_handle) {
        handle_notification_source(buf, copy_len);
      } else if (attr_handle == s_ds_handle) {
        handle_data_source(buf, copy_len);
      }
      return 0;
    }

    case BLE_GAP_EVENT_REPEAT_PAIRING: {
      // iOS forgot the device but the ESP32 still has a stale bond. NimBLE
      // fires REPEAT_PAIRING when a peer that is already bonded initiates a
      // fresh pairing. Delete the stale bond entry and allow re-pairing so
      // that iOS shows the "Bluetooth Pairing Request" dialog again (including
      // when the connection was opened via nRF Connect on newer iOS versions
      // that no longer show homemade ANCS devices in Settings → Bluetooth).
      ESP_LOGI(TAG, "stale bond detected — deleting and retrying pairing");
      struct ble_gap_conn_desc desc;
      if (ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc) == 0) {
        ble_store_util_delete_peer(&desc.peer_id_addr);
      }
      return BLE_GAP_REPEAT_PAIRING_RETRY;
    }

    default:
      return 0;
  }
}

// ---------------------------------------------------------------------------
// GATT service discovery callback
// ---------------------------------------------------------------------------
static int on_disc_svc(uint16_t conn_handle, const struct ble_gatt_error *error, const struct ble_gatt_svc *svc,
                       void *arg) {
  (void)arg;

  if (error->status == BLE_HS_EDONE) {
    ESP_LOGD(TAG, "service discovery complete");
    return 0;
  }
  if (error->status != 0) {
    ESP_LOGW(TAG, "svc disc error status=%d", error->status);
    return 0;
  }

  if (ble_uuid_cmp(&svc->uuid.u, u128p(&s_ancs_svc_uuid)) == 0) {
    ESP_LOGI(TAG, "ANCS service found start=%u end=%u", svc->start_handle, svc->end_handle);
    // Reset handles — they are per-connection and must be re-discovered
    s_ns_handle = 0;
    s_cp_handle = 0;
    s_ds_handle = 0;

    int rc = ble_gattc_disc_all_chrs(conn_handle, svc->start_handle, svc->end_handle, on_disc_chr, NULL);
    if (rc != 0) {
      ESP_LOGE(TAG, "ble_gattc_disc_all_chrs failed rc=%d", rc);
    }
  }
  return 0;
}

// ---------------------------------------------------------------------------
// GATT characteristic discovery callback
// ---------------------------------------------------------------------------
static int on_disc_chr(uint16_t conn_handle, const struct ble_gatt_error *error, const struct ble_gatt_chr *chr,
                       void *arg) {
  (void)arg;

  if (error->status == BLE_HS_EDONE) {
    // All ANCS characteristics found — subscribe to NS + DS notifications.
    ESP_LOGI(TAG, "ANCS chars done: ns=%u cp=%u ds=%u", s_ns_handle, s_cp_handle, s_ds_handle);
    if (s_ns_handle)
      write_cccd_enable(conn_handle, s_ns_handle);
    if (s_ds_handle)
      write_cccd_enable(conn_handle, s_ds_handle);

    // Read the iPhone's display name from the GAP Device Name characteristic
    // (0x2A00 in the Generic Access Service). The CONNECTED event is pushed
    // inside read_device_name_cb once the read completes (~100 ms).
    s_peer_name_buf[0] = '\0';
    int rc = ble_gattc_read_by_uuid(conn_handle, 1, 0xFFFF, u16p(&s_uuid_device_name), read_device_name_cb, nullptr);
    if (rc != 0) {
      // Read could not be initiated — push CONNECTED immediately with no name.
      ESP_LOGW(TAG, "device name read failed rc=%d", rc);
      BleEvent ev{};
      ev.type = BleEventType::CONNECTED;
      ev.device_name = "iPhone";
      push_event(ev);
      ESP_LOGI(TAG, "ANCS ready — connected to: iPhone");
    }
    return 0;
  }
  if (error->status != 0) {
    ESP_LOGW(TAG, "chr disc error status=%d", error->status);
    return 0;
  }

  // Match characteristic UUIDs and store value handles
  if (ble_uuid_cmp(&chr->uuid.u, u128p(&s_notif_src_uuid)) == 0) {
    s_ns_handle = chr->val_handle;
    ESP_LOGD(TAG, "NotifSrc val_handle=%u", s_ns_handle);
  } else if (ble_uuid_cmp(&chr->uuid.u, u128p(&s_ctrl_point_uuid)) == 0) {
    s_cp_handle = chr->val_handle;
    ESP_LOGD(TAG, "CtrlPoint val_handle=%u", s_cp_handle);
  } else if (ble_uuid_cmp(&chr->uuid.u, u128p(&s_data_src_uuid)) == 0) {
    s_ds_handle = chr->val_handle;
    ESP_LOGD(TAG, "DataSrc val_handle=%u", s_ds_handle);
  }
  return 0;
}

// ---------------------------------------------------------------------------
// Notification Source handler
// ---------------------------------------------------------------------------
static void handle_notification_source(const uint8_t *buf, uint16_t len) {
  protocol::NotificationSource ns;
  if (!protocol::parse_notification_source(buf, len, ns)) {
    ESP_LOGW(TAG, "notification_source: parse failed (len=%u)", len);
    return;
  }

  // Discard pre-existing notifications — avoid false alerts on reconnect
  if (protocol::is_pre_existing(ns)) {
    ESP_LOGV(TAG, "discard pre-existing uid=%u", ns.uid);
    return;
  }

  switch (ns.event_id) {
    case protocol::EventId::ADDED: {
      BleEvent ev{};
      ev.type = BleEventType::NOTIF_ADDED;
      ev.uid = ns.uid;
      ev.category = ns.category;
      ev.category_count = ns.category_count;
      ev.flags = ns.event_flags;
      push_event(ev);

      // Auto-fetch attributes if configured, control-point is available,
      // and no fetch is already in flight. Skipping while a fetch is pending
      // prevents mbuf exhaustion (BLE_HS_ENOMEM) when iOS delivers several
      // notifications in rapid succession and keeps the assembler coherent.
      if (s_cfg.auto_fetch && s_cp_handle != 0 && !s_cfg.fetch_attributes.empty() && !s_fetch_pending) {
        s_assembler.reset(ns.uid, s_cfg.fetch_attributes);
        s_fetch_uid = ns.uid;
        s_fetch_category = ns.category;
        s_fetch_pending = true;

        protocol::AttributeRequest reqs[8];
        size_t n_reqs = build_attr_requests(reqs, 8);

        uint8_t cmd[protocol::ANCS_ATTR_BUF_SIZE];
        size_t cmd_len = protocol::build_get_notification_attributes(ns.uid, reqs, n_reqs, cmd, sizeof(cmd));

        if (cmd_len > 0) {
          int rc = ble_gattc_write_flat(s_conn_handle, s_cp_handle, cmd, cmd_len, NULL, NULL);
          if (rc != 0) {
            ESP_LOGW(TAG, "write ctrl_point failed rc=%d; will retry on next notification", rc);
            s_fetch_pending = false;  // allow retry on the next ADDED event
          }
        }
      }
      break;
    }

    case protocol::EventId::MODIFIED: {
      BleEvent ev{};
      ev.type = BleEventType::NOTIF_MODIFIED;
      ev.uid = ns.uid;
      ev.category = ns.category;
      ev.flags = ns.event_flags;
      push_event(ev);
      break;
    }

    case protocol::EventId::REMOVED: {
      BleEvent ev{};
      ev.type = BleEventType::NOTIF_REMOVED;
      ev.uid = ns.uid;
      ev.category = ns.category;
      push_event(ev);
      break;
    }

    default:
      ESP_LOGW(TAG, "unknown event_id=%u", (uint8_t)ns.event_id);
      break;
  }
}

// ---------------------------------------------------------------------------
// Data Source handler
// ---------------------------------------------------------------------------
static void handle_data_source(const uint8_t *buf, uint16_t len) {
  auto status = s_assembler.feed(buf, len);

  switch (status) {
    case protocol::DataSourceAssembler::Status::NEED_MORE:
      // Partial — wait for more fragments
      break;

    case protocol::DataSourceAssembler::Status::COMPLETE: {
      s_fetch_pending = false;
      BleEvent ev{};
      ev.type = BleEventType::ATTRIBUTES;
      ev.uid = s_fetch_uid;
      ev.category = s_fetch_category;
      ev.app_id = s_assembler.value(protocol::AttributeId::APP_IDENTIFIER);
      ev.title = s_assembler.value(protocol::AttributeId::TITLE);
      ev.subtitle = s_assembler.value(protocol::AttributeId::SUBTITLE);
      ev.message = s_assembler.value(protocol::AttributeId::MESSAGE);
      push_event(ev);
      break;
    }

    case protocol::DataSourceAssembler::Status::STALE_UID:
      ESP_LOGW(TAG, "data_source: stale UID — discarding fragment");
      break;

    case protocol::DataSourceAssembler::Status::BUFFER_OVERFLOW:
      ESP_LOGW(TAG, "data_source: buffer overflow");
      break;

    case protocol::DataSourceAssembler::Status::BAD_COMMAND:
      ESP_LOGW(TAG, "data_source: bad command byte");
      break;

    default:
      break;
  }
}

// ---------------------------------------------------------------------------
// start_advertising
// ---------------------------------------------------------------------------
static void start_advertising() {
  struct ble_gap_adv_params adv_params = {0};
  adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
  adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

  int rc = ble_gap_adv_start(s_own_addr_type, NULL, BLE_HS_FOREVER, &adv_params, gap_event_cb, NULL);
  if (rc != 0) {
    ESP_LOGE(TAG, "ble_gap_adv_start failed rc=%d", rc);
    return;
  }

  // Defer the solicited-UUID override to the main-loop task.
  s_adv_override_pending = true;
  ESP_LOGI(TAG, "advertising started — solicited-UUID override pending");
}

// ---------------------------------------------------------------------------
// NimBLE host callbacks
// ---------------------------------------------------------------------------
static void on_reset(int reason) {
  ESP_LOGW(TAG, "NimBLE reset; reason=%d", reason);
}

static void on_sync(void) {
  ble_hs_util_ensure_addr(0);
  int rc = ble_hs_id_infer_auto(0, &s_own_addr_type);
  if (rc != 0) {
    ESP_LOGE(TAG, "infer addr failed rc=%d", rc);
    return;
  }
  ESP_LOGI(TAG, "NimBLE synced; own_addr_type=%u", s_own_addr_type);
  start_advertising();
}

static void host_task(void *param) {
  (void)param;
  nimble_port_run();  // blocks until nimble_port_stop()
  nimble_port_freertos_deinit();
}

// ---------------------------------------------------------------------------
// AncsBle::init
// ---------------------------------------------------------------------------
void AncsBle::init(const BleConfig &cfg) {
  if (s_queue == nullptr)
    s_queue = xQueueCreate(16, sizeof(BleEvent *));
  s_cfg = cfg;

  esp_err_t err = nimble_port_init();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "nimble_port_init failed: %d", err);
    return;
  }

  // Security / bonding config
  ble_hs_cfg.reset_cb = on_reset;
  ble_hs_cfg.sync_cb = on_sync;
  ble_hs_cfg.sm_io_cap = BLE_HS_IO_NO_INPUT_OUTPUT;
  ble_hs_cfg.sm_bonding = 1;
  ble_hs_cfg.sm_sc = 1;
  ble_hs_cfg.sm_mitm = 0;
  ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
  ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
  ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

  ble_svc_gap_init();
  ble_svc_gatt_init();

  // Register Device Information Service
  int rc = ble_gatts_count_cfg(s_dis_svcs);
  if (rc != 0) {
    ESP_LOGE(TAG, "ble_gatts_count_cfg failed: %d", rc);
    return;
  }
  rc = ble_gatts_add_svcs(s_dis_svcs);
  if (rc != 0) {
    ESP_LOGE(TAG, "ble_gatts_add_svcs failed: %d", rc);
    return;
  }

  // Set GAP device name
  rc = ble_svc_gap_device_name_set(s_cfg.device_name.c_str());
  if (rc != 0) {
    ESP_LOGE(TAG, "ble_svc_gap_device_name_set failed: %d", rc);
  }

  // Set preferred MTU (matches bluebell's 185)
  rc = ble_att_set_preferred_mtu(185);
  if (rc != 0) {
    ESP_LOGE(TAG, "ble_att_set_preferred_mtu failed: %d", rc);
  }

  ble_store_config_init();  // NVS-backed bond persistence

  nimble_port_freertos_init(host_task);
  ESP_LOGCONFIG(TAG, "NimBLE host started for ANCS");
}

void AncsBle::loop() {
  // Deferred advertisement override — must run from the main-loop task (not from
  // a NimBLE callback) because ble_gap_adv_set_data() blocks on an HCI response.
  if (!s_adv_override_pending)
    return;
  if (!ble_gap_adv_active())
    return;  // not started yet — retry next loop

  // --- Primary advertisement packet (max 31 bytes) ---
  const char *dev_name = s_cfg.device_name.c_str();
  size_t adv_name_len = strlen(dev_name);
  bool shortened = adv_name_len > 8;
  if (adv_name_len > 8)
    adv_name_len = 8;

  uint8_t adv[31];
  int adv_len = 0;
  adv[adv_len++] = 0x02;
  adv[adv_len++] = 0x01;
  adv[adv_len++] = 0x06;
  adv[adv_len++] = 0x11;
  adv[adv_len++] = 0x15;
  memcpy(&adv[adv_len], k_ancs_uuid_le, 16);
  adv_len += 16;
  adv[adv_len++] = (uint8_t)(adv_name_len + 1);
  adv[adv_len++] = shortened ? 0x08 : 0x09;
  memcpy(&adv[adv_len], dev_name, adv_name_len);
  adv_len += (int)adv_name_len;

  // --- Scan-response packet (max 31 bytes) ---
  const char *model = s_cfg.model.c_str();
  size_t model_len = strlen(model);
  if (model_len > 20)
    model_len = 20;  // clamp to prevent size_t underflow below
  size_t mfr_ad_bytes = 4 + model_len;
  size_t scan_name_len = strlen(dev_name);
  size_t scan_name_max;
  if (mfr_ad_bytes + 2 >= 31) {
    scan_name_max = 0;
  } else {
    scan_name_max = 31 - 2 - mfr_ad_bytes;
  }
  if (scan_name_len > scan_name_max)
    scan_name_len = scan_name_max;

  uint8_t scan[31];
  int scan_len = 0;
  scan[scan_len++] = (uint8_t)(scan_name_len + 1);
  scan[scan_len++] = 0x09;
  memcpy(&scan[scan_len], dev_name, scan_name_len);
  scan_len += (int)scan_name_len;
  scan[scan_len++] = (uint8_t)(1 + 2 + model_len);  // length byte uses clamped model_len
  scan[scan_len++] = 0xFF;
  scan[scan_len++] = 0xFF;
  scan[scan_len++] = 0xFF;
  memcpy(&scan[scan_len], model, model_len);
  scan_len += (int)model_len;  // clamped model_len

  int rc1 = ble_gap_adv_set_data(adv, adv_len);
  int rc2 = ble_gap_adv_rsp_set_data(scan, scan_len);

  ESP_LOGD(TAG, "adv override: adv(%d B) rc=%d  scan(%d B) rc=%d", adv_len, rc1, scan_len, rc2);

  if (rc1 == 0 && rc2 == 0) {
    s_adv_override_pending = false;
    ESP_LOGI(TAG, "solicited-UUID advertisement active");
  }
}

bool AncsBle::pop_event(BleEvent &out) {
  BleEvent *ev = nullptr;
  if (s_queue == nullptr || xQueueReceive(s_queue, &ev, 0) != pdTRUE)
    return false;
  out = *ev;
  delete ev;
  return true;
}

// ---------------------------------------------------------------------------
// Action methods
// ---------------------------------------------------------------------------

void AncsBle::request_attributes(uint32_t uid) {
  if (s_cp_handle == 0 || s_cfg.fetch_attributes.empty())
    return;

  s_assembler.reset(uid, s_cfg.fetch_attributes);
  s_fetch_uid = uid;
  s_fetch_category = protocol::Category::OTHER;  // no known category for manual fetch

  protocol::AttributeRequest reqs[8];
  size_t n_reqs = build_attr_requests(reqs, 8);

  uint8_t cmd[protocol::ANCS_ATTR_BUF_SIZE];
  size_t cmd_len = protocol::build_get_notification_attributes(uid, reqs, n_reqs, cmd, sizeof(cmd));

  if (cmd_len > 0) {
    int rc = ble_gattc_write_flat(s_conn_handle, s_cp_handle, cmd, cmd_len, NULL, NULL);
    if (rc != 0) {
      ESP_LOGW(TAG, "request_attributes: write ctrl_point failed rc=%d", rc);
    }
  }
}

void AncsBle::disconnect() {
  if (s_conn_handle == BLE_HS_CONN_HANDLE_NONE)
    return;
  int rc = ble_gap_terminate(s_conn_handle, 0x13 /* BLE_ERR_REM_USER_CONN_TERM */);
  if (rc != 0) {
    ESP_LOGW(TAG, "ble_gap_terminate failed rc=%d", rc);
  }
}

void AncsBle::clear_bonds_and_restart() {
  ESP_LOGI(TAG, "clearing all bonds and restarting");
  ble_store_clear();
  esp_restart();
}

}  // namespace ancs
}  // namespace esphome
#endif  // USE_ESP_IDF
