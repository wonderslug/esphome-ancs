# Category: missed_call

**Category ID:** 2  
**ESPHome string:** `"missed_call"`

## Description

Fires when a call goes unanswered. iOS delivers this as a new `ADDED` event
immediately after the `REMOVED` event for the ringing `incoming_call`.

Unlike `incoming_call`, there is no time-critical response needed — a calm,
finite visual alert works well (a slow breathe or a brief flash rather than
a continuous blink loop).

`on_notification_removed` fires when the user clears the missed-call
notification from the notification center, which may be minutes or hours later.
It is not useful as a stop signal for a timed alert.

## Common sources

- Phone (cellular)
- FaceTime
- Third-party VoIP apps (WhatsApp, Telegram, Signal) — also send `incoming_call` first

## Event lifecycle

```
ADDED   → user missed the call     → start a timed alert (finite cycles)
REMOVED → user cleared the badge   → (usually too late to be actionable)
```

## Example — slow breathe for ~9 s

```yaml
ancs:
  auto_fetch_attributes: false

  on_notification_added:
    - if:
        condition:
          lambda: 'return category == "missed_call";'
        then:
          - script.execute: alert_breathe

script:
  # Slow breathe — 3 full cycles at 3 s each, then off.
  # Requires a monochromatic light with a LEDC output for PWM.
  - id: alert_breathe
    mode: restart
    then:
      - light.turn_on:
          id: onboard_led
          effect: "Breathe Slow"
      - delay: 9000ms
      - light.turn_off: onboard_led

light:
  - platform: monochromatic
    output: led_gpio
    id: onboard_led
    restore_mode: ALWAYS_OFF
    effects:
      - pulse:
          name: "Breathe Slow"
          transition_length: 1500ms
          update_interval: 16ms
          min_brightness: 0%
          max_brightness: 80%
```

## Example — log who called and notify Home Assistant

```yaml
ancs:
  auto_fetch_attributes: true
  fetch_attributes: [title]

  on_notification_attributes:
    - if:
        condition:
          lambda: 'return category == "missed_call";'
        then:
          - logger.log:
              format: "Missed call from: %s"
              args: [title.c_str()]
          - homeassistant.service:
              service: notify.notify
              data_template:
                title: "Missed Call"
                message: !lambda 'return "From: " + title;'
```

## Notes

- `category_count` tells you how many missed-call notifications are currently
  active. Use `category_count == 1` to react only to the **first** missed call
  in a session and stay quiet for subsequent ones.
- iOS sends `missed_call` even when the phone is in DND/Focus mode if the
  caller is in the bypass list. ANCS respects the OS-level suppression for
  `incoming_call`, but the **missed_call** badge usually still appears.
