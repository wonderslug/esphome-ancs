# Category: schedule

**Category ID:** 5  
**ESPHome string:** `"schedule"`

## Description

Calendar event reminders and time-based notifications from any calendar or
reminder app. `title` is usually the event name; `message` is the time or
location. These are point-in-time alerts — a brief blink or a Home Assistant
scene change is a natural response.

## Common sources

| App | Bundle ID |
|---|---|
| Calendar | `com.apple.mobilecal` |
| Reminders | `com.apple.reminders` |
| Fantastical | `com.flexibits.fantastical2.iphone` |
| Outlook (calendar) | `com.microsoft.Outlook` |
| Google Calendar | `com.google.calendar` |
| Cron | `com.cron.app` |

## Example — flash LED for any calendar reminder

```yaml
ancs:
  auto_fetch_attributes: false

  on_notification_added:
    - if:
        condition:
          lambda: 'return category == "schedule";'
        then:
          - light.turn_on:
              id: onboard_led
              flash_length: 5s
```

## Example — log the event name and trigger a Home Assistant scene

```yaml
ancs:
  auto_fetch_attributes: true
  fetch_attributes: [title, message]

  on_notification_attributes:
    - if:
        condition:
          lambda: 'return category == "schedule";'
        then:
          - logger.log:
              format: "Reminder: %s (%s)"
              args: [title.c_str(), message.c_str()]
          - homeassistant.service:
              service: scene.turn_on
              data:
                entity_id: scene.meeting_mode
```

## Example — detect a specific event by name

Use `title.find()` to look for a keyword in the event name.

```yaml
ancs:
  auto_fetch_attributes: true
  fetch_attributes: [title]

  on_notification_attributes:
    - if:
        condition:
          # Match any reminder whose title contains "standup" (case-sensitive)
          lambda: 'return category == "schedule" && title.find("standup") != std::string::npos;'
        then:
          - script.execute: meeting_alert
```

## Notes

- `message` in calendar notifications usually contains the time ("in 15
  minutes") or the location — useful for context logging.
- Reminders.app sends location-based reminders as `location` (category 10),
  not `schedule`. Time-based reminders appear here.
- Calendar events rarely have a matching `REMOVED` event in useful time —
  the badge clears when the user dismisses the notification, not when the
  event ends.
