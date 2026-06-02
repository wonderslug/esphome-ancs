# ESPHome ANCS Component

An ESPHome external component that turns an ESP32 into an Apple Notification Center Service (ANCS) consumer. When a bonded iPhone receives a notification — an incoming call, iMessage, app alert, and so on — the ANCS events drive ESPHome automations and sensor entities directly on the device. The component runs on the native NimBLE stack over ESP-IDF; because the ESP32's Bluetooth radio is dedicated to ANCS, it cannot coexist with Bluedroid-based features such as `bluetooth_proxy`, `esp32_ble_tracker`, or `ble_client`.

---

## Requirements

- **Hardware**: any ESP32 module (tested with `esp32dev`)
- **Framework**: ESP-IDF (the component forces this; Arduino framework is not supported)
- **Mobile**: an iPhone running iOS 7 or later (ANCS is an Apple protocol)
- **[nRF Connect for Mobile](https://www.nordicsemi.com/Products/Development-tools/nRF-Connect-for-mobile)** — required on iOS to complete the initial pairing (see below)

---

## ⚠️ Pairing requires nRF Connect — not Settings → Bluetooth

> **This is the single most important thing to know before you flash.**

Newer versions of iOS no longer show arbitrary third-party BLE devices in
**Settings → Bluetooth**. Apple restricts that list to MFi-certified accessories
and devices using recognised Bluetooth profiles. A DIY ANCS node does not
qualify, so it simply does not appear there — even though the hardware and
firmware are working perfectly.

**The workaround is to use [nRF Connect for Mobile](https://www.nordicsemi.com/Products/Development-tools/nRF-Connect-for-mobile)** (free, from Nordic Semiconductor) to initiate the
initial connection. nRF Connect acts as a Bluetooth central and connects
directly to your ESP32. The ESP32 immediately sends an SMP Security Request,
which causes iOS to display its standard **"Bluetooth Pairing Request"** system
dialog — the same dialog you would see from Settings → Bluetooth. Once you tap
**Pair**, iOS stores a full system-level bond and the ANCS service becomes
available. After that first pairing, iOS reconnects automatically whenever the
device is in range with no further use of nRF Connect required.

### First-time pairing — step by step

1. Install **nRF Connect for Mobile** from the App Store.
2. Flash the ESP32 and power it on. Watch the serial log for `NimBLE synced` and `advertising started`.
3. Open nRF Connect → **SCANNER** tab → tap **SCAN**.
4. Find your device by the BLE name you set (e.g. `ESPHome-ANCS`) and tap **CONNECT**.
5. The ESP32 receives the connection and immediately requests pairing. iOS shows a system dialog: **"ESPHome-ANCS" Would Like to Pair With Your iPhone** → tap **Pair**.
6. nRF Connect shows the GATT services being discovered. You will see the ANCS service appear (`7905F431-…`).
7. Close nRF Connect. The bond is now stored at the iOS system level.
8. The ESP32 logs show `ANCS chars done` and `iPhone connected`. ESPHome sensors reflect the connected state.
9. From this point on iOS reconnects automatically — nRF Connect is no longer needed unless you need to re-pair.

### Re-pairing after "Forget This Device"

If you tap **Forget This Device** on iOS, the iPhone's bond is deleted but the
ESP32 still holds its copy in NVS. The next connection attempt detects the
mismatch and automatically deletes the stale bond (via `BLE_GAP_EVENT_REPEAT_PAIRING`),
so the pairing dialog will appear again in nRF Connect without any manual
intervention on the ESP32 side.

If for any reason the dialog does not appear, trigger the `ancs.clear_bonds`
action (e.g. from Home Assistant or a button), then re-pair from step 3 above.

> 📖 **Full pairing guide** — see [docs/pairing.md](docs/pairing.md) for a
> detailed explanation of why this is necessary, how the bond persists, and
> troubleshooting for common pairing problems.

---

<a name="rtfm"></a>

> **📢 A word before you file that issue...**
>
> If your next action is to open a GitHub issue, Discord message, or forum post that reads *"my ESPHome device isn't showing up in Settings → Bluetooth!"* — congratulations. You have successfully scrolled past hundreds of words of bolded, emoji-decorated, all-caps warnings without absorbing a single one. This is, in its own way, an achievement.
>
> The author reserves the right to respond to such reports with nothing but a link back to this paragraph, a slow clap, and the quiet dignity of someone who already explained it **twice**. The device is fine. iOS just doesn't list DIY BLE gadgets there. **Use nRF Connect.** It's free. It works. It's described above in numbered steps with links and everything.  There is even a [whole doc](docs/pairing.md) just for this that goes into a lot of detail.
>
> We believe in you. You can do this. 🫵

---

## Installation

### Via ESPHome packages (recommended)

The simplest way to use the component in any project. Add one line to your YAML — ESPHome fetches the component automatically at compile time.

**Minimal (component code only — you configure everything):**

```yaml
packages:
  ancs_component: github://wonderslug/esphome-ancs/packages/ancs.yaml@master
```

**With sensors pre-wired (binary + text sensors appear in Home Assistant automatically):**

```yaml
substitutions:
  friendly_name: "Living Room"

packages:
  ancs_component: github://wonderslug/esphome-ancs/packages/ancs-with-sensors.yaml@master
```

The `ancs-with-sensors` package uses `${friendly_name}` for sensor names — define it as a substitution and all sensors are prefixed automatically.

**Pin to a release tag for production (recommended once the repo has releases):**

```yaml
packages:
  ancs_component: github://wonderslug/esphome-ancs/packages/ancs.yaml@v1.0.0
```

---

### Local (development / monorepo)

```yaml
external_components:
  - source:
      type: local
      path: ../components   # adjust to your relative path
```

### Direct git reference

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/wonderslug/esphome-ancs
      ref: master
    components: [ancs]
```

---

## Configuration Reference

### Hub: `ancs:`

The hub block configures the BLE ANCS peripheral.  Add one block at the top level of your YAML.

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `id` | ID | _(required)_ | ESPHome component ID; used to reference the hub from sensors and actions. |
| `name` | string | `"ESPHome-ANCS"` | BLE advertised name shown when pairing on iOS. |
| `auto_fetch_attributes` | bool | `true` | Automatically issue an ANCS Get Notification Attributes request after each `on_notification_added` event. |
| `fetch_attributes` | list | `[app_id, title, message]` | Which attributes to retrieve. Subset of `[app_id, title, subtitle, message]`. |
| `manufacturer` | string | _(optional)_ | BLE Device Information Service manufacturer string. |
| `model` | string | _(optional)_ | BLE Device Information Service model string. |
| `max_bonds` | int (1–9) | `3` | Maximum number of bonded iPhones stored in NVS. Known devices auto-reconnect without re-pairing. |

### Triggers

All triggers are defined inside the `ancs:` hub block.

| Trigger | Variables | Notes |
|---------|-----------|-------|
| `on_connect` | _(none)_ | Fires when an iPhone establishes an encrypted ANCS connection. |
| `on_disconnect` | _(none)_ | Fires when the connection drops. |
| `on_notification_added` | `uid` (uint32), `category` (std::string), `category_count` (uint8), `flags` (uint8) | Fires immediately when a notification appears — before attribute text is fetched. Use this to react fast (e.g., start blinking on `category == "incoming_call"`). |
| `on_notification_attributes` | `uid`, `category`, `app_id`, `title`, `subtitle`, `message` (all std::string) | Fires after a GATT round-trip once the requested attribute text has arrived. Use this to display caller name, message body, etc. |
| `on_notification_removed` | `uid` (uint32), `category` (std::string) | Fires when the notification is dismissed or acknowledged on the iPhone. |
| `on_notification_modified` | `uid` (uint32), `category` (std::string), `flags` (uint8) | Fires when an existing notification is updated (e.g., call accepted on another device). |

**Added vs. Attributes split**: `on_notification_added` carries only the notification header and fires the moment the notification arrives — ideal for low-latency reactions like starting a light flash. `on_notification_attributes` carries caller/title/message text but requires one GATT round-trip, so it arrives a fraction of a second later.

### `binary_sensor` platform `ancs`

```yaml
binary_sensor:
  - platform: ancs
    ancs_id: my_ancs
    connected:
      name: "iPhone Connected"
    call_active:
      name: "Call Active"
```

| Sensor key | Description |
|------------|-------------|
| `connected` | `ON` while an iPhone has an active ANCS connection. |
| `call_active` | `ON` while an `incoming_call` notification is present. |

### `text_sensor` platform `ancs`

```yaml
text_sensor:
  - platform: ancs
    ancs_id: my_ancs
    last_title:
      name: "Last Notification"
    last_message:
      name: "Last Message"
    last_app_id:
      name: "Last App ID"
    last_caller:
      name: "Last Caller"
```

| Sensor key | Description |
|------------|-------------|
| `last_title` | Title of the most recently received notification. |
| `last_message` | Message body of the most recently received notification. |
| `last_app_id` | Bundle ID of the source app (e.g., `com.apple.mobilephone`). |
| `last_caller` | Caller name from the most recent incoming-call notification. |

### Actions

| Action | Argument | Description |
|--------|----------|-------------|
| `ancs.clear_bonds` | hub ID | Wipes all stored BLE bonds from NVS and restarts the ESP32. Use this to force re-pairing after the user taps "Forget This Device" on iOS. |
| `ancs.disconnect` | hub ID | Drops the active BLE connection without wiping bonds. The device will re-advertise and the iPhone will reconnect automatically. |

---

## Full Example Configuration

```yaml
esphome:
  name: ancs-test

esp32:
  board: esp32dev
  framework:
    type: esp-idf

logger:

external_components:
  - source:
      type: local
      path: ../components

ancs:
  id: my_ancs
  name: "ESPHome-ANCS"
  auto_fetch_attributes: true
  fetch_attributes: [app_id, title, message]
  max_bonds: 3
  on_notification_added:
    - logger.log:
        format: "ADDED uid=%u cat=%s count=%u"
        args: [uid, category.c_str(), category_count]
    - if:
        condition:
          lambda: 'return category == "incoming_call";'
        then:
          - light.turn_on:
              id: alert_light
              flash_length: 60s
  on_notification_attributes:
    - logger.log:
        format: "ATTRS uid=%u title=%s msg=%s"
        args: [uid, title.c_str(), message.c_str()]
  on_notification_removed:
    - logger.log:
        format: "REMOVED uid=%u cat=%s"
        args: [uid, category.c_str()]
    - if:
        condition:
          lambda: 'return category == "incoming_call";'
        then:
          - light.turn_off: alert_light
  on_connect:
    - logger.log: "iPhone connected"
  on_disconnect:
    - logger.log: "iPhone disconnected"

output:
  - platform: gpio
    pin: GPIO2
    id: led_output

light:
  - platform: binary
    name: "Alert Light"
    output: led_output
    id: alert_light

binary_sensor:
  - platform: ancs
    ancs_id: my_ancs
    connected:
      name: "iPhone Connected"
    call_active:
      name: "Call Active"

text_sensor:
  - platform: ancs
    ancs_id: my_ancs
    last_title:
      name: "Last Notification"
    last_message:
      name: "Last Message"
    last_caller:
      name: "Last Caller"

interval:
  - interval: 3600s
    then:
      - ancs.clear_bonds: my_ancs
```

---

## Build & Flash

```bash
esphome run tests/test-ancs.yaml
```

To compile without flashing:

```bash
esphome compile tests/test-ancs.yaml
```

---

## On-Device Verification Checklist

The following steps require a real iPhone; they cannot be tested in a simulator.

1. Flash and boot the ESP32. Watch serial output for `NimBLE synced` and `advertising started`.
2. Open **nRF Connect** on your iPhone → **SCANNER** → **SCAN** → tap your device name → **CONNECT**.
3. Tap **Pair** in the iOS system dialog that appears. *(Settings → Bluetooth does not work on modern iOS — see the Pairing section above.)*
4. The "iPhone Connected" binary sensor turns ON; logs show `ANCS chars done` and `iPhone connected`.
5. Call the iPhone from another number → the Alert Light flashes; "Call Active" turns ON; "Last Caller" populates with the caller name.
6. End the call → the light turns off; "Call Active" turns OFF.
7. Toggle iPhone Airplane mode off and back on → the node auto-reconnects with no re-pairing required.
8. To re-pair after iOS "Forget This Device": forget the device on iOS, then open nRF Connect and connect again — the pairing dialog reappears automatically.

---

## Known Limitations

- **Focus / Do Not Disturb**: iOS can suppress ANCS notifications at the OS level when Focus modes are active. There is no workaround from the ESP32 side.
- **One active connection**: only one iPhone can be connected at a time. Up to `max_bonds` paired iPhones are stored so a known device auto-reconnects when in range.
- **Resolvable private addresses**: iOS uses rotating Bluetooth MAC addresses. The component identifies paired iPhones via the bonded IRK (identity resolving key) stored in NVS, not by MAC address.
- **No Bluedroid coexistence**: because the component takes ownership of the BLE radio via NimBLE/ESP-IDF, it cannot run alongside `bluetooth_proxy`, `esp32_ble_tracker`, or `ble_client`.

---

## Testing

Host-side unit tests for the pure protocol layer (no hardware required):

```bash
cd test && make test
```

The demo configuration must compile cleanly:

```bash
esphome compile tests/test-ancs.yaml
```

---

## License

MIT © 2026 Brian Towles — see [LICENSE](LICENSE).

---

## AI Assistance

This component was developed with assistance from [Claude](https://claude.ai), Anthropic's AI coding assistant.
