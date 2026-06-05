# Quick Start: iPhone Notifications in Home Assistant

Get iPhone notifications flowing as Home Assistant events in under 10 minutes.
No sensors to configure. No custom triggers to write. One package, a wifi config, and you're done.

---

## What you need

- Any ESP32 board (tested with `esp32dev`; a WEMOS D1 Mini ESP32 works great)
- [ESPHome](https://esphome.io/guides/getting_started_hassio) installed (add-on or CLI)
- [nRF Connect for Mobile](https://apps.apple.com/app/nrf-connect-for-mobile/id1054362403) installed on your iPhone (free, required for the initial pairing — see [why](pairing.md))
- Home Assistant with the ESPHome integration added

---

## The config

Create a new file — call it `ancs-ha-bridge.yaml` — and paste this in:

```yaml
esphome:
  name: ancs-ha-bridge

esp32:
  board: esp32dev
  framework:
    type: esp-idf

# substitutions:
#   ancs_name: "My iPhone Bridge"   # optional — name shown in nRF Connect when pairing

api:
  # encryption:
  #   key: "your-32-byte-base64-key-from-ha-esphome-addon"

ota:
  - platform: esphome

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

packages:
  ancs_ha_events: github://wonderslug/esphome-ancs/packages/ancs-ha-events.yaml@master
```

That's the entire config. The package handles the component, the ANCS configuration, and the HA event wiring.

---

## Flash and pair

1. Flash the config: `esphome run ancs-ha-bridge.yaml` (or use the ESPHome add-on dashboard).
2. Watch the log for `NimBLE synced` and `advertising started`.
3. On your iPhone, open **nRF Connect** → **SCANNER** tab → tap **SCAN**.
4. Find `ANCS HA Bridge` in the list and tap **CONNECT**.
5. iOS shows a system dialog: **"ANCS HA Bridge" Would Like to Pair** → tap **Pair**.
6. iOS shows a second dialog: **Allow "ANCS HA Bridge" to Receive Your iPhone Notifications?** → tap **Allow**. This is what grants ANCS access.
7. Close nRF Connect. The bond is stored at the iOS system level.
8. The ESP32 log shows `ANCS chars done` and `iPhone connected`. You're paired.

From this point iOS reconnects automatically whenever the ESP32 is powered on and in range — nRF Connect is only needed for the first pairing.

> **Stuck?** See the full [pairing guide](pairing.md) for troubleshooting, re-pairing after "Forget This Device", and why Settings → Bluetooth doesn't work.

---

## Your first automation

In Home Assistant go to **Settings → Automations → New automation → Edit as YAML** and paste:

```yaml
trigger:
  - platform: event
    event_type: esphome.ancs_incoming_call
action:
  - service: light.turn_on
    target:
      entity_id: light.YOUR_LIGHT
    data:
      flash: long
```

Replace `light.YOUR_LIGHT` with any light entity in your home. Save and test it by calling yourself — the light flashes when the call arrives.

### Stop flashing when the call ends

Add a second automation:

```yaml
trigger:
  - platform: event
    event_type: esphome.ancs_call_ended
action:
  - service: light.turn_off
    target:
      entity_id: light.YOUR_LIGHT
```

---

## What's next

Three events are available:

| Event | When | Key payload fields |
|---|---|---|
| `esphome.ancs_incoming_call` | Incoming call arrives | `caller`, `app_id`, `date`, `iphone_name`, `device_name` |
| `esphome.ancs_notification` | Any other notification | `category`, `title`, `message`, `app_id`, `date`, `iphone_name`, `device_name` |
| `esphome.ancs_call_ended` | Call answered or declined | `uid`, `iphone_name`, `device_name` |

See [docs/packages.md](packages.md) for all payload fields, more automation examples (TTS, filtering by app ID, filtering by category), and how to add HA entity sensors alongside events.
