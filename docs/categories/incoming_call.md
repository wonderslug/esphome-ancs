# Category: incoming_call

**Category ID:** 1  
**ESPHome string:** `"incoming_call"`

## Description

Fires when the iPhone receives a voice or video call. This is the most
time-critical ANCS category — you typically want to react within milliseconds.

The category arrives in the `on_notification_added` header with no GATT
roundtrip, so you can start blinking immediately. The caller's name arrives
~1 s later via `on_notification_attributes` if you enable attribute fetching.

`on_notification_removed` is **reliable** for this category — it fires as soon
as the call ends (answered, declined, or missed), making it safe to stop an
alert loop.

## Common sources

- Phone (cellular calls)
- FaceTime (audio and video)
- WhatsApp calls (`net.whatsapp.WhatsApp`)
- Telegram calls (`ph.telegra.Telegraph`)
- Signal calls (`org.whispersystems.signal`)

## Event lifecycle

```
ADDED   → phone is ringing        → start alert
REMOVED → call answered/declined  → stop alert
```

After the REMOVED for a missed call, iOS sends a new `missed_call` ADDED
immediately — see [missed_call.md](missed_call.md).

## Example — fast blink until answered

```yaml
ancs:
  id: my_ancs
  auto_fetch_attributes: false   # category is enough to start; no fetch needed

  on_notification_added:
    - if:
        condition:
          lambda: 'return category == "incoming_call";'
        then:
          - script.execute: alert_ringing

  on_notification_removed:
    - if:
        condition:
          lambda: 'return category == "incoming_call";'
        then:
          - script.stop: alert_ringing
          - light.turn_off: onboard_led

  on_disconnect:
    - script.stop: alert_ringing
    - light.turn_off: onboard_led

script:
  # 3 fast blinks (100 ms on/off), 500 ms pause, repeat indefinitely.
  - id: alert_ringing
    mode: restart
    then:
      - while:
          condition:
            lambda: 'return true;'
          then:
            - repeat:
                count: 3
                then:
                  - light.turn_on: { id: onboard_led, brightness: 100%, transition_length: 0s }
                  - delay: 100ms
                  - light.turn_off: onboard_led
                  - delay: 100ms
            - delay: 500ms
```

## Example — log caller name when it arrives

Enable attribute fetching to get the caller's name ~1 s after the call starts.
Useful if you have a display or want to log to Home Assistant.

```yaml
ancs:
  auto_fetch_attributes: true
  fetch_attributes: [title]    # title = caller name or number

  on_notification_added:
    - if:
        condition:
          lambda: 'return category == "incoming_call";'
        then:
          - script.execute: alert_ringing   # start immediately, before name arrives

  on_notification_attributes:
    - if:
        condition:
          lambda: 'return category == "incoming_call";'
        then:
          - logger.log:
              format: "Incoming call from: %s"
              args: [title.c_str()]
```

## Example — only react to calls with a positive (answer) action

The `FLAG_POSITIVE_ACTION` bit (0x08) is set when an "Answer" button is
available. Filtering on it avoids double-firing on edge cases.

```yaml
on_notification_added:
  - if:
      condition:
        lambda: 'return category == "incoming_call" && (flags & 0x08);'
      then:
        - script.execute: alert_ringing
```

## Notes

- Do **not** rely on the `uid` matching between an ADDED and its REMOVED — iOS
  can reuse UIDs across connections. React to the category, not the UID.
- WhatsApp and Telegram calls arrive as `incoming_call` even though they are VoIP.
  If you want to distinguish them, check `app_id` via attribute fetch.
- FaceTime sends `incoming_call`; the caller's name is in `title`.
