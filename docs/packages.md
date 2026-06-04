# Packages Reference

This component ships three ESPHome packages that you pull in via the `packages:` key.
Each package is a self-contained YAML fragment that extends your device config — you
never need to clone or fork the repository. Use them to get from zero to working
notifications with minimal boilerplate, or compose them together for a richer setup.

---

## Package overview

| Package | What it provides | Requires |
|---|---|---|
| `ancs.yaml` | `external_components` declaration only — the component code itself | Nothing beyond the `packages:` include |
| `ancs-with-sensors.yaml` | Component code + `binary_sensor`, `text_sensor`, and `button` platforms pre-wired for HA | `substitutions.friendly_name`, `id: my_ancs` on your `ancs:` block, `api:` or `mqtt:` |
| `ancs-ha-events.yaml` | `on_notification_attributes` and `on_notification_removed` triggers that fire three HA events | `id: my_ancs`, `auto_fetch_attributes: true`, `fetch_attributes: [app_id, title, subtitle, message]`, `api:` |

Packages can be stacked. For example, you can include both `ancs-with-sensors.yaml`
and `ancs-ha-events.yaml` in the same device config to get both the HA entity sensors
and the event stream.

---

## `ancs.yaml` — component only

### What it does

Declares the `external_components` block that tells ESPHome to pull the `ancs`
component from GitHub. This is the foundation that all other packages build on.
It provides no sensors, no triggers, and no automations — just the component code.

### When to use it

Use `ancs.yaml` when you want full control over every sensor, trigger, and action
in your own YAML. This is the right choice if you are building device-level automations
(blinking LEDs, controlling relays) directly in ESPHome and do not want the sensor
or event packages.

### What it requires

Nothing beyond the `packages:` include itself. You add the `ancs:` block, any
triggers, and any sensors in your own config.

### Minimal working config

```yaml
esphome:
  name: my-ancs-device

esp32:
  board: esp32dev
  framework:
    type: esp-idf

api:
ota:
  - platform: esphome

packages:
  ancs_component: github://wonderslug/esphome-ancs/packages/ancs.yaml@master

ancs:
  id: my_ancs
  name: "My ANCS"
  on_notification_added:
    - logger.log:
        format: "Notification: category=%s uid=%d"
        args: [category.c_str(), uid]
```

---

## `ancs-with-sensors.yaml` — component + sensors

### What it does

A standalone package that declares the same `external_components` block as
`ancs.yaml` and adds pre-wired Home Assistant entities — it does not depend on
or inherit from `ancs.yaml`:

- **Binary sensors** — `connected` (HA entity: **"[friendly_name] iPhone Connected"**)
  and `call_active` (HA entity: **"[friendly_name] Call Active"**).
- **Text sensors:**
  - `connected_device` → HA entity **"[friendly_name] Connected Device"** — name of the
    paired iPhone.
  - `last_title` → HA entity **"[friendly_name] Last Notification"** — the notification
    title (not "Last Title"; the friendly label is "Last Notification").
  - `last_message` → HA entity **"[friendly_name] Last Message"** — the notification
    body text.
  - `last_caller` → HA entity **"[friendly_name] Last Caller"** — the resolved caller
    name from the most recent incoming-call notification.
  - `last_app_id` → HA entity **"[friendly_name] Last App ID"** — the bundle ID of the
    app that sent the most recent notification.
- **Buttons** — **"[friendly_name] Clear Bonds"** (removes all BLE bonds and restarts
  advertising) and **"[friendly_name] Disconnect iPhone"** (drops the current BLE link).

All entity names are prefixed with `${friendly_name}` so they appear cleanly in
Home Assistant. The internal sensor IDs (e.g. `last_title`) differ from the HA-facing
names shown above — always look up entities by their friendly name or entity ID in HA,
not by the YAML key.

### When to use it

Use this package when you want HA entities without writing any sensor YAML by hand.
It is also a good starting point if you want a quick working setup before adding
custom triggers.

### What it requires

- `substitutions.friendly_name` — a string used as the entity name prefix.
- `id: my_ancs` — you must set this on your `ancs:` block exactly as written;
  the package references it by this ID.
- `api:` or `mqtt:` — to surface the sensors in Home Assistant.

### Minimal working config

```yaml
esphome:
  name: my-ancs-device

esp32:
  board: esp32dev
  framework:
    type: esp-idf

substitutions:
  friendly_name: "Living Room"

api:
ota:
  - platform: esphome

packages:
  ancs_component: github://wonderslug/esphome-ancs/packages/ancs-with-sensors.yaml@master

ancs:
  id: my_ancs
  name: "Living Room ANCS"
```

This produces HA entities named `Living Room iPhone Connected`, `Living Room Call Active`,
`Living Room Last Notification`, and so on.

---

## `ancs-ha-events.yaml` — Home Assistant events

### What it does

Wires `on_notification_attributes` and `on_notification_removed` triggers to fire
three HA events over the ESPHome native API:

| Event | When it fires | Payload fields |
|---|---|---|
| `esphome.ancs_incoming_call` | An incoming call notification has its attributes available | `uid`, `caller`, `app_id`, `device_name`, `iphone_name` |
| `esphome.ancs_notification` | Any non-call notification has its attributes available | `uid`, `category`, `app_id`, `title`, `subtitle`, `message`, `device_name`, `iphone_name` |
| `esphome.ancs_call_ended` | An incoming call is declined or answered | `uid`, `device_name`, `iphone_name` |

### When to use it

Use this package when you want to react to iPhone notifications from Home Assistant
automations rather than (or in addition to) on-device ESPHome triggers. It offloads
all notification logic to HA, where you have access to the full automation engine,
other devices, and more complex conditions.

### What it requires

You must define these in your own config:

- `id: my_ancs` on your `ancs:` block.
- `auto_fetch_attributes: true` on your `ancs:` block.
- `fetch_attributes: [app_id, title, subtitle, message]` on your `ancs:` block —
  the package reads these fields to build event payloads.
- `api:` — events travel over the ESPHome native API. The device must be connected
  to Home Assistant.

### Why `on_notification_attributes` instead of `on_notification_added`

`on_notification_added` fires as soon as iOS delivers the 8-byte Notification Source
record — before any attribute data (title, message, caller name, app ID) is available.
`on_notification_attributes` fires after the component has fetched and assembled all
requested attributes via the ANCS Data Source characteristic. The package uses
`on_notification_attributes` because the event payloads would be empty strings if
fired earlier.

### `caller` field

`caller` appears only in `esphome.ancs_incoming_call` events. It contains the
caller's display name as iOS resolves it — typically a contact name such as "Mom"
or "John Smith", or the raw phone number if the caller is not saved in Contacts.

The value comes from the ANCS `title` attribute for the incoming call notification.
It is the same string that would be exposed as `title` in an
`on_notification_attributes` trigger on the device.

### `device_name` field

`device_name` contains the ESPHome device name from the `esphome.name` key in your
config (retrieved via `App.get_name()`). If you have more than one ANCS device
paired to Home Assistant, every event from every device arrives under the same
event type names. Use `trigger.event.data.device_name` in your HA automation
condition to target events from a specific device.

### `iphone_name` field

`iphone_name` contains the display name of the connected iPhone as iOS advertises
it over Bluetooth — typically something like "Brian's iPhone" or "iPhone". It is
the same name shown in iOS Settings → General → About → Name.

The value is set when the iPhone connects and cleared to an empty string when it
disconnects. Because events only fire while a device is connected, `iphone_name`
will always be non-empty in practice.

If you have more than one ANCS device paired, `iphone_name` identifies which phone
sent the notification (while `device_name` identifies which ESP32 fired the event).

### `uid` field

`uid` is a decimal string containing the notification UID assigned by iOS. The same
UID is present in both `esphome.ancs_incoming_call` and the corresponding
`esphome.ancs_call_ended` event for the same call. Use it in HA automations when
you need to correlate "this call ended" with "this call started" — for example,
to cancel a flashing light scene that was triggered by a specific call.

`esphome.ancs_call_ended` fires when the incoming call notification is declined
or answered. It does not fire for missed calls. Missed calls produce a
separate notification under the `missed_call` category and arrive as
`esphome.ancs_notification` with `category: missed_call`.

### Minimal working config

```yaml
esphome:
  name: ancs-ha-bridge

esp32:
  board: esp32dev
  framework:
    type: esp-idf

api:
ota:
  - platform: esphome

packages:
  ancs_ha_events: github://wonderslug/esphome-ancs/packages/ancs-ha-events.yaml@master

ancs:
  id: my_ancs
  name: "ANCS HA Bridge"
  auto_fetch_attributes: true
  fetch_attributes: [app_id, title, subtitle, message]
```

### Home Assistant automation examples

Paste these skeletons into **Settings → Automations → New automation → Edit as YAML**.

#### (a) Flash a light on incoming call

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

> **Note:** `flash: long` triggers a single flash sequence that stops on its own.
> To flash continuously until the call ends, use a script with a repeat loop — see
> the [blink-on-call example](../examples/d1-mini-blink-on-call.yaml) for a pattern
> you can adapt as an HA script.

#### (b) Stop flashing when the call ends

```yaml
trigger:
  - platform: event
    event_type: esphome.ancs_call_ended
action:
  - service: light.turn_off
    target:
      entity_id: light.YOUR_LIGHT
```

#### (c) Announce the caller's name via TTS

```yaml
trigger:
  - platform: event
    event_type: esphome.ancs_incoming_call
action:
  - service: tts.cloud_say
    data:
      entity_id: media_player.YOUR_SPEAKER
      message: "Incoming call from {{ trigger.event.data.caller }}"
```

> **Note:** `tts.cloud_say` requires a Home Assistant Cloud (Nabu Casa) subscription.
> If you don't have HA Cloud, use `tts.speak` with your preferred local or cloud TTS
> provider instead (e.g., Piper, Google TTS). `tts.speak` is available since HA 2023.x
> and works with any configured TTS integration.

#### (d) Filter by app ID

React only when a specific app sends a notification. See
[app-id-reference.md](app-id-reference.md) for bundle IDs.

```yaml
trigger:
  - platform: event
    event_type: esphome.ancs_notification
    event_data:
      app_id: com.apple.MobileSMS
action:
  - service: notify.YOUR_NOTIFY_SERVICE
    data:
      message: "iMessage from {{ trigger.event.data.title }}: {{ trigger.event.data.message }}"
```

#### (e) Filter by category

React to all notifications of a given category. See
[categories/README.md](categories/README.md) for the full category list.

```yaml
trigger:
  - platform: event
    event_type: esphome.ancs_notification
    event_data:
      category: email
action:
  - service: script.announce_email
    data:
      sender: "{{ trigger.event.data.title }}"
      subject: "{{ trigger.event.data.message }}"
```

---

## Combining packages: `ancs-with-sensors.yaml` + `ancs-ha-events.yaml`

You can include both packages in the same config to get HA entity sensors and the
event stream simultaneously.

```yaml
esphome:
  name: ancs-ha-bridge

esp32:
  board: esp32dev
  framework:
    type: esp-idf

substitutions:
  friendly_name: "Living Room"

api:
ota:
  - platform: esphome

packages:
  ancs_sensors:    github://wonderslug/esphome-ancs/packages/ancs-with-sensors.yaml@master
  ancs_ha_events:  github://wonderslug/esphome-ancs/packages/ancs-ha-events.yaml@master

ancs:
  id: my_ancs
  name: "Living Room ANCS"
  auto_fetch_attributes: true
  fetch_attributes: [app_id, title, subtitle, message]
```

`ancs-with-sensors.yaml` already declares `external_components`, so you do not
also need `ancs.yaml` — the sensors package is a superset of it.

Both packages reference `id: my_ancs`, so the single `ancs:` block satisfies both.
The `fetch_attributes` list must include all four attributes (`app_id`, `title`,
`subtitle`, `message`) because `ancs-ha-events.yaml` requires them for event payloads.
`ancs-with-sensors.yaml` does not care about `fetch_attributes` — its text sensors
are updated on every notification regardless.
