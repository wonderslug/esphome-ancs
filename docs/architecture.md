# Architecture

This document describes the internal design of the ESPHome ANCS component
for developers who want to understand, extend, or port it.

---

## Overview

The component is split into three layers with strict separation of concerns.
The key invariant is that **only Layer 3 ever touches ESPHome APIs** and
**only Layer 2 ever touches NimBLE APIs**. Layer 1 depends on nothing
but the C++ standard library.

```
┌─────────────────────────────────────────────────────────────────┐
│  Layer 3 — AncsComponent  (ESPHome main-loop task)              │
│  ancs_component.{h,cpp}  automation.h  __init__.py              │
│  binary_sensor.py  text_sensor.py                               │
│                                                                 │
│  • setup() / loop() / dump_config()                             │
│  • Drains event queue → fires triggers → publishes sensors      │
│  • Owns ESPHome CallbackManager / Trigger / Action objects      │
└───────────────────────────────┬─────────────────────────────────┘
                                │  BleEvent (POD, heap-allocated)
                                │  FreeRTOS queue (capacity 16)
┌───────────────────────────────▼─────────────────────────────────┐
│  Layer 2 — AncsBle  (NimBLE host task)                          │
│  ancs_ble.{h,cpp}                                               │
│                                                                 │
│  • Owns the ESP-IDF NimBLE host (nimble_port_init / run)        │
│  • Advertising with solicited ANCS UUID (AD type 0x15)          │
│  • SMP bonding, NVS-backed bond persistence                      │
│  • Encryption-timed GATT service discovery + CCCD subscribe     │
│  • NOTIFY_RX routing → calls Layer 1 parsers                   │
│  • Pushes results onto the event queue for Layer 3              │
└───────────────────────────────┬─────────────────────────────────┘
                                │  raw bytes + function calls
┌───────────────────────────────▼─────────────────────────────────┐
│  Layer 1 — ancs_protocol  (no dependencies)                     │
│  ancs_protocol.{h,cpp}                                          │
│                                                                 │
│  • parse_notification_source() — 8-byte NS record               │
│  • build_get_notification_attributes() — control-point command  │
│  • DataSourceAssembler — fragmentation reassembly               │
│  • Enums, category/event string mappers                         │
│  • 100% host-unit-tested (doctest, runs on any host)            │
└─────────────────────────────────────────────────────────────────┘
```

---

## File map

```
components/ancs/
  ancs_protocol.h/.cpp     Layer 1 — pure protocol, no deps
  ancs_ble.h/.cpp          Layer 2 — NimBLE host owner
  ancs_component.h/.cpp    Layer 3 — ESPHome Component lifecycle
  automation.h             Trigger<> and Action<> template classes
  __init__.py              ESPHome codegen: hub schema, triggers, actions, sdkconfig
  binary_sensor.py         ESPHome binary_sensor platform (connected, call_active)
  text_sensor.py           ESPHome text_sensor platform (last_title, etc.)

test/
  test_ancs_protocol.cpp   Host unit tests for Layer 1 (doctest)
  doctest.h                Vendored test framework (MIT, v2.4.11)
  Makefile                 `make test` — builds + runs on macOS/Linux

tests/
  test-ancs.yaml           ESPHome compile gate + on-device demo config

packages/
  ancs.yaml                Remote package — component only
  ancs-with-sensors.yaml   Remote package — component + sensors pre-wired
```

---

## Threading model

NimBLE runs its host stack in a dedicated FreeRTOS task (`nimble_host_task`).
ESPHome's lifecycle (`setup` / `loop`) runs on the Arduino/IDF main task.
These two tasks must not share mutable state without synchronisation.

```
 NimBLE host task                       ESPHome main-loop task
 ────────────────                       ──────────────────────
 BLE_GAP_EVENT_CONNECT
   └─ ble_gap_security_initiate()

 BLE_GAP_EVENT_ENC_CHANGE
   └─ ble_gattc_disc_all_svcs()         AncsComponent::loop()
       └─ on_disc_svc()                   └─ ble_.loop()          ← applies adv override
           └─ ble_gattc_disc_all_chrs()   └─ ble_.pop_event()     ← drains queue
               └─ subscribe CCCDs              └─ fire triggers
               └─ push CONNECTED event         └─ publish sensors

 BLE_GAP_EVENT_NOTIFY_RX
   └─ parse_notification_source()       (HCI-blocking calls must NOT run here
   └─ build_get_notification_attributes  — they block waiting for a controller
   └─ DataSourceAssembler::feed()         response and would deadlock the host
   └─ push NOTIF_ADDED / ATTRIBUTES       task; use loop() instead)
       │
       ▼
  ┌─────────────────────────┐
  │  FreeRTOS queue         │  capacity 16, sizeof(BleEvent*)
  │  xQueueSend / Receive   │  heap-allocated BleEvent, deleted by pop_event()
  └─────────────────────────┘
```

**Queue-full policy:** if the queue is full, the oldest event is dropped and a
`LOGW` is emitted. The NimBLE host task is never blocked; it always returns
immediately from `push_event()`.

**Deferred advertising override:** `ble_gap_adv_set_data()` sends an HCI
command and blocks for the controller's response. Calling it from a NimBLE
callback would deadlock the host task. Instead, `start_advertising()` sets a
`s_adv_override_pending` flag; `AncsBle::loop()` (main task) applies the real
advertisement bytes once `ble_gap_adv_active()` returns true.

---

## BLE connection lifecycle

```
BOOT
 │
 ├─ nimble_port_init()
 ├─ ble_hs_cfg  ← security/bonding config, store_status_cb
 ├─ ble_store_config_init()  ← NVS-backed bond persistence
 ├─ DIS GATT service registered (makes iOS show the device in Bluetooth settings)
 └─ nimble_port_freertos_init(host_task)

on_sync() callback (NimBLE host ready)
 ├─ enumerate NVS bonds via ble_store_read_peer_sec()
 │   └─ store up to MAX_CONNECTIONS peer addresses in s_reconnect_peers[]
 └─ start_advertising_smart()

start_advertising_smart()  ← called at boot, after connect success/fail, after adv timeout
 ├─ if s_reconnect_idx < s_reconnect_count:
 │   └─ ble_gap_adv_start(BLE_GAP_CONN_MODE_DIR, peer, 3 s)  ← directed to next stored bond
 └─ else: start_advertising()  ← undirected solicited-UUID advertising

loop() (main task, every cycle)
 └─ if s_adv_override_pending && ble_gap_adv_active():
     ├─ ble_gap_adv_set_data()   ← primary: Flags + Solicited UUID (AD 0x15) + name
     └─ ble_gap_adv_rsp_set_data()  ← scan response: full name + mfr data

BLE_GAP_EVENT_CONNECT (status == 0 — iPhone connected)
 ├─ alloc_slot(conn_handle)  ← assigns a free ConnState slot
 ├─ ble_gap_security_initiate()  ← send SMP Security Request → iOS shows Pair dialog
 └─ if slots still available: start_advertising_smart()  ← keep advertising for more phones

BLE_GAP_EVENT_CONNECT (status != 0 — directed adv timed out or LL handshake failed)
 └─ start_advertising_smart()  ← advance to next stored bond or fall back to undirected

BLE_GAP_EVENT_ADV_COMPLETE (undirected advertising ended unexpectedly)
 └─ start_advertising_smart()

BLE_GAP_EVENT_REPEAT_PAIRING  (if iPhone forgot device but ESP32 has stale bond)
 └─ ble_store_util_delete_peer()  ← delete stale bond
 └─ return BLE_GAP_REPEAT_PAIRING_RETRY  ← fresh pairing proceeds

BLE_GAP_EVENT_ENC_CHANGE (encryption established — discovery MUST wait for this)
 ├─ find_slot(conn_handle)  ← locate the ConnState for this connection
 ├─ ble_gap_conn_find() confirms encrypted
 └─ ble_gattc_disc_all_svcs()
     └─ on_disc_svc(): match ANCS service UUID
         └─ ble_gattc_disc_all_chrs()
             └─ on_disc_chr(): store val_handles in slot (ns_handle / cp_handle / ds_handle)
                 └─ write CCCDs for NS + DS
                 └─ read Device Name characteristic (0x2A00) → slot.peer_name_buf
                     └─ slot.ancs_ready = true
                     └─ push CONNECTED event (with device_name)

ANCS RUNNING (one ConnState per connected phone, up to MAX_CONNECTIONS simultaneously)
 ├─ BLE_GAP_EVENT_NOTIFY_RX → find_slot() by conn_handle
 │   ├─ if attr_handle == slot.ns_handle → handle_notification_source()
 │   │   ├─ parse_notification_source() — Layer 1
 │   │   ├─ discard if PRE_EXISTING flag (avoids false alerts on reconnect)
 │   │   ├─ register uid→conn_handle in s_uid_routes[] ring buffer
 │   │   ├─ push NOTIF_ADDED/MODIFIED/REMOVED (with slot.peer_name_buf as device_name)
 │   │   └─ if ADDED + auto_fetch + !slot.fetch_pending:
 │   │       ├─ slot.assembler.reset(uid, fetch_attributes)
 │   │       ├─ build_get_notification_attributes() — Layer 1
 │   │       └─ ble_gattc_write_flat() → slot.cp_handle
 │   └─ if attr_handle == slot.ds_handle → handle_data_source()
 │       └─ slot.assembler.feed() — Layer 1
 │           └─ on COMPLETE: push ATTRIBUTES event (with device_name)
 │
 └─ UID routing (request_attributes action):
     └─ lookup_uid_route(uid, device_name?)  ← resolves uid to conn_handle
         ├─ searches s_uid_routes[] ring buffer most-recent-first
         └─ optional device_name filter resolves collisions when two phones
            have identical UIDs (iOS assigns UIDs sequentially from a low counter)

BLE_GAP_EVENT_DISCONNECT
 ├─ find_slot(conn_handle)
 ├─ if slot.ancs_ready: push DISCONNECTED event  ← guard prevents spurious events
 │   (connections that fail before ANCS discovery completes never fire CONNECTED,
 │    so they must not fire DISCONNECTED either)
 ├─ free_slot(conn_handle)
 └─ start_advertising()  ← undirected (boot-time directed cycle is one-shot)
```

---

## ANCS protocol

Apple Notification Center Service uses three GATT characteristics on the iPhone.
The ESP32 acts as a GATT **client** over the inbound peripheral link.

```
iPhone (GATT server)                       ESP32 (peripheral + GATT client)
────────────────────                       ─────────────────────────────────
Notification Source (notify)  ──notify──►  handle_notification_source()
                                           8 bytes: event_id | flags | category
                                           | category_count | uid (LE uint32)

                              ◄──write──   Control Point (write without response)
                                           GetNotificationAttributes command:
                                           [0x00][uid LE×4][attr_id...][max_len LE×2]

Data Source (notify)          ──notify──►  DataSourceAssembler::feed()
                                           Fragmented response reassembled across
                                           BLE MTU boundaries. Mid-header and
                                           mid-value splits handled by the
                                           compacting-buffer parser.
```

**Attribute IDs requested** (configurable via `fetch_attributes:`):

| ID | Name | Max-len field? |
|---|---|---|
| 0 | App Identifier | No |
| 1 | Title | Yes |
| 2 | Subtitle | Yes |
| 3 | Message | Yes |

**Why GATTC requires `CONFIG_BT_NIMBLE_ROLE_CENTRAL`:** ESP-IDF NimBLE only
compiles `ble_gattc_*` (service discovery, write, etc.) when the central role
is enabled. ANCS makes the ESP32 a GATT client over an *inbound* link — the
connection was initiated by the iPhone, not the ESP32 — but the host library's
build system still gates the GATTC code behind `ROLE_CENTRAL`. Both peripheral
and central roles are therefore enabled in the sdkconfig injected by `__init__.py`.

---

## Layer 1: `ancs_protocol`

Pure C++17, depends only on `<cstdint>`, `<string>`, `<vector>`. Compiles
on any host with no ESP-IDF or NimBLE headers present.

### `parse_notification_source(data, len, out)`

Deserialises the 8-byte Notification Source record into a `NotificationSource`
struct. Returns `false` if `len < 8`. Raw category byte is preserved in
`raw_category`; the `category` field is clamped to the known enum range.

### `build_get_notification_attributes(uid, reqs, n_reqs, out, out_cap)`

Serialises a GetNotificationAttributes command (CommandID 0x00) for the
Control Point write. Returns bytes written, or 0 if the output buffer is too
small. Attributes with a 2-byte max-length field (TITLE, SUBTITLE, MESSAGE)
are distinguished from those without (APP_IDENTIFIER) via `attribute_has_max_len()`.

### `DataSourceAssembler`

Stateful reassembler. Call `reset(uid, expected_attrs)` for each new fetch,
then `feed(data, len)` for each arriving fragment. Returns:

| Status | Meaning |
|---|---|
| `NEED_MORE` | Incomplete — wait for the next fragment |
| `COMPLETE` | All requested attributes received; call `value(attr_id)` |
| `STALE_UID` | Response is for a different UID — discard |
| `BUFFER_OVERFLOW` | Accumulated data exceeded `ANCS_ATTR_BUF_SIZE` (512 B) |
| `BAD_COMMAND` | First byte is not 0x00 |

**Compacting-buffer design:** after each `feed()`, fully-parsed attribute
triples are erased from the front of the buffer (`buf_.erase(begin, begin+pos)`).
If a value is incomplete, the position is rewound and the partial triple waits
at the front for the next fragment. This correctly handles splits at any byte
boundary including mid-header (between AttrID and the 2-byte length) and
mid-value.

---

## Layer 2: `ancs_ble`

Owns the NimBLE host. Runs entirely on the NimBLE host FreeRTOS task, with the
single exception of `AncsBle::loop()` and the action methods, which run on the
ESPHome main-loop task.

**State visible to both tasks** is strictly limited to the FreeRTOS queue
handle (`s_queue`, written once at init) and the volatile advertising-pending
flag (`s_adv_override_pending`). All other state is touched only from the NimBLE
host task.

**`ConnState` slot array:** replaces the former single-connection globals. Each
slot holds `conn_handle`, `peer_name_buf`, GATT val-handles (`ns_handle`,
`cp_handle`, `ds_handle`), a `DataSourceAssembler`, a `fetch_pending` flag, and
an `ancs_ready` flag. Up to `CONFIG_BT_NIMBLE_MAX_CONNECTIONS` slots are active
simultaneously.

**`fetch_pending` guard (per-slot):** prevents concurrent `ble_gattc_write_flat()`
calls to a slot's Control Point when iOS delivers several notifications in rapid
succession. A second ADDED event while a fetch is in flight skips the fetch;
`fetch_pending` is cleared when the assembler returns `COMPLETE`, when a write
fails, or on disconnect.

**UID routing ring buffer (`s_uid_routes[]`):** when a notification arrives on
any slot, a `{uid, conn_handle}` pair is appended (ring, 32 entries). The
`request_attributes` action resolves a uid to a conn_handle by searching
most-recent-first with an optional `device_name` filter to break ties when two
phones have overlapping UIDs (iOS assigns UIDs from a small sequential counter).

**Directed reconnect cycle:** `start_advertising_smart()` is called after
`on_sync()`, after each connect success/failure, and after advertising completes.
It works through `s_reconnect_peers[]` (populated from NVS bonds at boot) using
3-second directed advertising windows before falling back to undirected. This
ensures all previously bonded phones are proactively targeted after an ESP32 reset.

---

## Layer 3: `AncsComponent`

Standard ESPHome `Component`. `setup()` builds a `BleConfig` from the YAML
configuration and calls `ble_.init()`. `loop()` calls `ble_.loop()` (for the
adv override) then drains the event queue:

```
while (ble_.pop_event(ev)):
    switch ev.type:
        CONNECTED     → connected_count_++; publish connected_bs_, connected_device_ts_
                        fire on_connect_(device_name)
        DISCONNECTED  → connected_count_--; publish connected_bs_, call_active_bs_
                        fire on_disconnect_(device_name)
        NOTIF_ADDED   → if incoming_call: call_active_count_++, publish call_active_bs_
                        fire on_added_(uid, category_string, count, flags, device_name)
        NOTIF_REMOVED → if incoming_call: call_active_count_--, publish call_active_bs_
                        fire on_removed_(uid, category_string, device_name)
        NOTIF_MODIFIED → fire on_modified_(uid, category_string, flags, device_name)
        ATTRIBUTES    → publish last_title_ts_, last_message_ts_, etc.
                        fire on_attributes_(uid, cat, app_id, title, subtitle, message, device_name, date)
```

All triggers receive `device_name` (std::string) as their last variable, identifying
which phone the event originated from. This allows automations to filter or branch
on a specific phone when multiple iPhones are simultaneously connected.

**`automation.h`** defines the six `Trigger<>` subclasses and three `Action<>`
subclasses (`ClearBondsAction`, `DisconnectAction`, `RequestAttributesAction`).
Each trigger registers a callback with `AncsComponent` via `add_on_*_callback()`.
`__init__.py` instantiates these via `cg.new_Pvariable` and wires them with
`automation.build_automation()`. `RequestAttributesAction` accepts an optional
`device_name` template value to disambiguate the UID lookup when multiple phones
are connected.

---

## ESPHome codegen (`__init__.py`)

Key responsibilities:

1. **Schema validation** — defines and validates all YAML keys; raises a config
   error if Bluedroid BLE components are also present (`FINAL_VALIDATE_SCHEMA`).
2. **sdkconfig injection** — calls `add_idf_sdkconfig_option()` for every
   required NimBLE option (enabled, peripheral+central roles, SM bonding/SC,
   NVS persist). `max_connections` drives `CONFIG_BT_NIMBLE_MAX_CONNECTIONS`,
   `CONFIG_BT_NIMBLE_MAX_BONDS` (set equal to connections), `CONFIG_BTDM_CTRL_BLE_MAX_CONN`,
   `CONFIG_BT_CTRL_BLE_MAX_ACT`, and `CONFIG_BT_NIMBLE_MAX_CCCDS` (= 2 × connections + 2).
3. **Trigger codegen** — for each `on_*` block, instantiates the correct
   `Trigger<>` class and calls `automation.build_automation()` with the typed
   lambda variable list.
4. **Action registration** — `@automation.register_action` decorators expose
   `ancs.clear_bonds`, `ancs.disconnect`, and `ancs.request_attributes`.
5. **Fetch attribute mapping** — maps YAML strings (`"title"`, `"app_id"`, …)
   to `protocol::AttributeId` enum values via `cg.add(var.add_fetch_attribute(...))`.

---

## Testing strategy

| Layer | Test mechanism | Runner |
|---|---|---|
| Layer 1 (`ancs_protocol`) | doctest unit tests — 16 cases, 48 assertions | Any host (macOS, Linux, Windows); `make test CXX=g++` |
| Layer 2 (`ancs_ble`) | `esphome compile` — confirms C++ compiles and all NimBLE symbols link | CI (ubuntu-latest) + local |
| Layer 3 + codegen | `esphome config` + `esphome compile` — schema, codegen, triggers, sensors | CI + local |
| End-to-end | Manual on-device checklist in README — pair, call, missed call, reconnect | Real ESP32 + real iPhone |

CI runs both automated gates (host tests + compile) on every PR via
`.github/workflows/test.yml` and `.github/workflows/compile.yml`.
