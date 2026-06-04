# Pairing with an iPhone

This page explains why pairing requires nRF Connect, how iOS Bluetooth bonding
works under the hood, and how to diagnose and recover from pairing problems.

---

## Why Settings → Bluetooth no longer works

### The historical situation

When Apple first published the ANCS specification (iOS 7, 2013), any BLE
peripheral that advertised the ANCS solicited service UUID in its advertisement
(AD type `0x15`) would appear in **Settings → Bluetooth** and the user could
tap it to pair. DIY ANCS projects, including the reference this component is
based on, relied on this behaviour and documented it as the standard pairing
flow.

### What changed

Starting around iOS 15–16 and becoming consistent by iOS 17–18, Apple quietly
changed the filter for which devices appear in Settings → Bluetooth.
The list now shows only:

- **MFi-certified accessories** — devices that have licensed Apple's Made for
  iPhone programme and carry a certified Bluetooth chip identity.
- **Devices using recognised standard profiles** — HID (keyboards, mice),
  A2DP (headphones), HFP (headsets), and a handful of others that iOS maps to
  known accessory categories.
- **Devices the user already has a prior bond with** — previously paired devices
  can reappear when they are in range.

A custom ESP32 ANCS node is none of these things. It broadcasts the ANCS
solicited UUID correctly and exposes a Device Information Service (both
required for ANCS), but iOS applies the filter *before* the device can be
tapped. From the user's perspective, the device simply does not exist in the
Settings → Bluetooth list even though it is actively advertising.

This is **not a bug in the firmware** — it is an iOS policy decision. The
device is advertising correctly. The radio is working. nRF Connect can see it
just fine.

### Why nRF Connect works

nRF Connect for Mobile is a general-purpose BLE scanner and GATT browser from
Nordic Semiconductor. It bypasses the Settings → Bluetooth device list entirely
by using Core Bluetooth's `CBCentralManager` API to connect to any visible
peripheral directly. When nRF Connect connects to the ESP32:

1. The ESP32 immediately sends an **SMP Security Request** (`ble_gap_security_initiate()`),
   signalling to iOS that it wants to establish an encrypted bonded link.
2. iOS interprets the Security Request and surfaces the standard **system-level
   pairing dialog** — the same "X Would Like to Pair With Your iPhone" prompt
   you would see from Settings → Bluetooth. This dialog is rendered by iOS
   itself, not by nRF Connect.
3. When the user taps **Pair**, iOS generates shared encryption keys (LTK),
   stores a full **system-level bond**, and completes the pairing handshake.

The critical point is that **the bond is stored at the iOS system level**, not
inside nRF Connect. Once paired, iOS knows about the device globally. When the
ESP32 advertises again later (after disconnect, reboot, etc.), the iOS Bluetooth
daemon recognises the device's IRK (Identity Resolving Key), resolves its
resolvable private address, and connects automatically — no nRF Connect, no
Settings → Bluetooth, no user interaction.

---

## What a bond actually is

BLE pairing produces two outputs:

- **LTK (Long-Term Key)** — a symmetric encryption key used to re-establish
  encryption on reconnect without repeating the full pairing ceremony.
- **IRK (Identity Resolving Key)** — used to resolve iOS's rotating MAC
  addresses. iOS changes its Bluetooth MAC address periodically to prevent
  tracking. Without the IRK, the ESP32 would think a returning iPhone is a
  new stranger. With the IRK stored in NVS, NimBLE resolves the address
  transparently.

Both keys are stored in NVS on the ESP32 (`ble_store_config_init()` handles
this) and in the iOS Bluetooth system store. The bond survives power cycles
on both sides as long as neither side deliberately deletes it.

---

## Step-by-step: first-time pairing

**Prerequisites:**
- ESP32 flashed and powered on
- nRF Connect for Mobile installed (free on the App Store, from Nordic Semiconductor)
- Bluetooth enabled on the iPhone

**Steps:**

1. **Open nRF Connect** → tap the **SCANNER** tab at the bottom.

2. Tap **SCAN** (top right). The scanner starts listing nearby BLE peripherals.

3. **Find your device** in the list. It will appear by the `name:` you set in
   your ESPHome YAML (e.g. `ESPHome-ANCS`, `Call Blinker`, etc.). If you don't
   see it, check the serial log — it should show `advertising started` and
   `solicited-UUID advertisement active`.

4. Tap **CONNECT** next to your device name.

5. **The iOS pairing dialog appears** — a system prompt reading something like:
   *"ESPHome-ANCS" Would Like to Pair With Your iPhone*. Tap **Pair**.

6. **A second iOS dialog appears** — *Allow "ESPHome-ANCS" to Receive Your iPhone
   Notifications?* Tap **Allow**. This grants the ANCS permission; without it the
   component connects but receives no notifications.

7. nRF Connect will display the GATT service discovery results. After a few
   seconds you will see the ANCS service (`7905F431-B5CE-4E99-A40F-4B1E122D00D0`)
   and its three characteristics appear in the list. This confirms the bond was
   accepted and ANCS discovery succeeded.

8. **Close nRF Connect.** You do not need to keep it open. The bond is stored
   at the iOS system level and the ANCS connection persists (or will
   auto-reconnect if nRF Connect dropped it when closed).

9. Check the ESP32 serial log. You should see:
   ```
   [I][ancs.ble]: encrypted — starting ANCS service discovery
   [I][ancs.ble]: ANCS chars done: ns=XX cp=XX ds=XX
   [I][ancs.ble]: iPhone connected
   ```
   The `connected` binary sensor (if configured) turns `ON` in Home Assistant.

**If the pairing dialog did not appear** — see the [Troubleshooting](#troubleshooting) section.

---

## Re-pairing after "Forget This Device"

When you tap **Forget This Device** in Settings → Bluetooth or Settings → General →
Transfer or Reset iPhone, iOS deletes its bond. The ESP32 still has a copy of
the old bond in NVS. This creates a **bond mismatch** that would normally
prevent re-pairing.

The component handles this automatically. When the iPhone connects (via nRF
Connect) and attempts a fresh pairing while the ESP32 still holds an old bond,
NimBLE fires a `BLE_GAP_EVENT_REPEAT_PAIRING` event. The component's handler:

1. Looks up the peer's address from the connection handle.
2. Calls `ble_store_util_delete_peer()` to delete the stale bond from NVS.
3. Returns `BLE_GAP_REPEAT_PAIRING_RETRY` — NimBLE starts a fresh pairing
   sequence and the iOS pairing dialog appears in nRF Connect.

**You do not need to manually clear bonds on the ESP32 for a normal re-pair.**
Simply forget the device on iOS and connect via nRF Connect — the dialog will
appear automatically.

The manual `ancs.clear_bonds` action (which wipes ALL bonds and restarts) is
only needed if something has gone badly wrong — for example, if the automatic
mismatch recovery fails repeatedly or if you want to completely factory-reset
the Bluetooth state.

---

## Auto-reconnect after pairing

Once paired, the ESP32 and iPhone reconnect automatically:

| Scenario | Behaviour |
|---|---|
| iPhone walks back into range | iOS Bluetooth daemon sees the ESP32's advertisement, resolves the IRK, connects, re-encrypts using the LTK, ANCS is ready within ~1–2 s |
| ESP32 reboots | Resumes advertising from NVS-loaded bond; iPhone reconnects as above |
| iPhone Airplane mode off | iPhone resumes Bluetooth, auto-connects |
| nRF Connect disconnects | ESP32 detects disconnect, restarts advertising, iOS reconnects within seconds |
| Network outage / HA restart | BLE is independent of Wi-Fi; ANCS continues working |

You **never need nRF Connect again** after the initial pairing unless you
deliberately forget the device on iOS.

---

## Pairing multiple ESP32 nodes

Multiple nodes can be paired to the same iPhone simultaneously (iOS supports
several parallel peripheral connections, practical limit ~5). Each node must
be paired individually:

1. Give each node a unique `name:` in its ESPHome YAML.
2. Flash and pair each node in turn using nRF Connect — connect to one, pair,
   close nRF Connect, then repeat for the next.
3. Once all nodes are paired, iOS reconnects to all of them automatically.

Each node has its own `max_bonds` NVS store (default 3). This means each
node can remember up to 3 different iPhones.

---

## Troubleshooting

### Pairing dialog does not appear when connecting in nRF Connect

**Likely cause:** the ESP32 has a stale bond for this iPhone but the iPhone
does not (or vice versa) and the `REPEAT_PAIRING` event did not resolve it.

**Fix:**
1. In Home Assistant (or via a button automation), trigger `ancs.clear_bonds`. The ESP32 restarts.
2. On iOS, go to **Settings → Bluetooth**, find the device if it appears, and tap **Forget This Device** (or skip if it doesn't appear).
3. Re-pair via nRF Connect from step 1 of the pairing guide.

---

### Device appears in nRF Connect scanner but CONNECT times out

**Likely cause:** the device is advertising but the iOS Bluetooth stack is
in a bad state, or the device is already connected to another app.

**Fix:** Force-close nRF Connect, disable and re-enable Bluetooth on the iPhone
(toggle in Control Centre), then try again.

---

### After pairing, ANCS service not visible in nRF Connect

**Likely cause:** ANCS discovery only starts *after* the link is encrypted, and
nRF Connect's GATT browser may cache a pre-encryption discovery. This is
expected behaviour.

**Confirm it worked:** check the ESP32 serial log for `ANCS chars done`. If
those lines appear, ANCS is working regardless of what nRF Connect shows.

---

### `enc_change failed status=13` in the log

Status 13 is `BLE_HS_ETIMEOUT` — the 30-second SMP (security pairing) timer
expired. iOS did not respond to the Security Request. The component
automatically terminates the connection when this happens, restarts advertising,
and retries on the next connection.

**Likely cause:** the pairing dialog appeared on iOS but was ignored or
dismissed, or a stale ENC_CHANGE event from a previous connection confused the
state machine. The auto-recovery should handle subsequent connection attempts
without manual intervention.

---

### Notifications stop working after disconnect/reconnect

**Most likely cause:** a stale ENC_CHANGE event fired after disconnect and
attempted ANCS discovery with an invalid connection handle, corrupting the GATTC
state for the new connection. This manifests as:
```
[W][ancs.ble]: stale enc_change for handle=0 (active=65535) — ignoring
```
followed by the correct ENC_CHANGE and a successful discovery on the next
line. If you see this pattern and notifications resume, the automatic recovery
is working. If notifications do not resume, power-cycle the ESP32 — it will
advertise fresh and iOS will reconnect cleanly.

---

## Reference

| Spec | Reference |
|---|---|
| Apple ANCS specification | [developer.apple.com/library/archive/documentation/CoreBluetooth/Reference/AppleNotificationCenterServiceSpecification](https://developer.apple.com/library/archive/documentation/CoreBluetooth/Reference/AppleNotificationCenterServiceSpecification/) |
| nRF Connect for Mobile | [nordicsemi.com/Products/Development-tools/nRF-Connect-for-mobile](https://www.nordicsemi.com/Products/Development-tools/nRF-Connect-for-mobile) |
| NimBLE SMP pairing events | [mynewt.apache.org/latest/network/ble_sec](https://mynewt.apache.org/latest/network/ble_sec.html) |
