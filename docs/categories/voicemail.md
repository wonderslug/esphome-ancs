# Category: voicemail

**Category ID:** 3  
**ESPHome string:** `"voicemail"`

## Description

Fires when a new Visual Voicemail is available. Behavior is very similar to
`missed_call` — a one-shot notification that warrants a calm, finite alert.

The `title` attribute typically contains the caller's name or number. The
`message` attribute is usually empty (the voicemail audio is not delivered
via ANCS).

`on_notification_removed` fires when the voicemail is played or the
notification is manually cleared — rarely actionable in real time.

## Common sources

- Phone (carrier Visual Voicemail)
- Google Voice
- Carrier voicemail apps

## Example — breathe once and log the caller

```yaml
ancs:
  auto_fetch_attributes: true
  fetch_attributes: [title]

  on_notification_added:
    - if:
        condition:
          lambda: 'return category == "voicemail";'
        then:
          - script.execute: alert_breathe   # start visual alert immediately

  on_notification_attributes:
    - if:
        condition:
          lambda: 'return category == "voicemail";'
        then:
          - logger.log:
              format: "Voicemail from: %s"
              args: [title.c_str()]

script:
  - id: alert_breathe
    mode: restart
    then:
      - light.turn_on:
          id: onboard_led
          effect: "Breathe Slow"
      - delay: 6000ms     # 2 breathe cycles
      - light.turn_off: onboard_led
```

## Example — combined missed call + voicemail handler

Both categories use the same calm alert. Share a single script:

```yaml
ancs:
  auto_fetch_attributes: false

  on_notification_added:
    - if:
        condition:
          lambda: >
            return category == "missed_call" || category == "voicemail";
        then:
          - script.execute: alert_breathe
```

## Notes

- Not all carriers support Visual Voicemail. On carriers without it, a
  voicemail is often delivered as an SMS/MMS from a short code and will
  appear as `other` or `social` depending on the messaging app, not as
  `voicemail`.
- `category_count` reflects the number of unplayed voicemails. If the user
  has several queued, only react when `category_count == 1` to avoid
  repeated alerts on reconnect pushing old voicemail counts.
