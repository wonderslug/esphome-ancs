# Category: health_fitness

**Category ID:** 8  
**ESPHome string:** `"health_fitness"`

## Description

Activity rings, workout alerts, step goals, heart-rate warnings, and
health reminders. Most events are celebratory ("You closed your rings!")
or motivational — a short, cheerful alert is appropriate.

`category_count` is particularly useful here because the iOS Fitness app
batches its daily summary into one notification, not a stream.

## Common sources

| App | Bundle ID |
|---|---|
| Fitness | `com.apple.Fitness` |
| Health | `com.apple.Health` |
| MyFitnessPal | `com.myfitnesspal.mfp` |
| Strava | `com.strava.stravaride` |
| Withings | `com.withings.wiscale2` |
| Garmin Connect | `com.garmin.connect.mobile` |

## Example — celebrate closing activity rings

```yaml
ancs:
  auto_fetch_attributes: true
  fetch_attributes: [app_id, message]

  on_notification_attributes:
    - if:
        condition:
          lambda: >
            return category == "health_fitness" &&
                   app_id == "com.apple.Fitness" &&
                   message.find("closed") != std::string::npos;
        then:
          - logger.log: "Rings closed!"
          - script.execute: celebrate_blink

script:
  # Rapid triple-flash to celebrate.
  - id: celebrate_blink
    mode: single
    then:
      - repeat:
          count: 5
          then:
            - light.turn_on: { id: onboard_led, brightness: 100%, transition_length: 0s }
            - delay: 80ms
            - light.turn_off: onboard_led
            - delay: 80ms
```

## Example — heart rate warning via Apple Health

```yaml
ancs:
  auto_fetch_attributes: true
  fetch_attributes: [app_id, title]

  on_notification_attributes:
    - if:
        condition:
          lambda: >
            return category == "health_fitness" &&
                   app_id == "com.apple.Health";
        then:
          - logger.log:
              format: "Health alert: %s"
              args: [title.c_str()]
          - homeassistant.service:
              service: notify.notify
              data_template:
                title: "Health Alert"
                message: !lambda 'return title;'
```

## Example — gentle reminder to move

The "Time to Stand" and "Move" reminders arrive throughout the day. A gentle
single blink avoids interrupting focus while still giving a visual cue.

```yaml
ancs:
  auto_fetch_attributes: false

  on_notification_added:
    - if:
        condition:
          lambda: 'return category == "health_fitness";'
        then:
          - light.turn_on:
              id: onboard_led
              flash_length: 1s
```

## Notes

- Apple Watch workout notifications (start, pause, end) appear here if the
  paired Watch is using the Fitness app.
- `message` content varies widely between apps — direct string matching for
  keywords like "closed", "goal", "warning" is the most portable approach.
- If you use multiple fitness apps, check `app_id` first to avoid reacting
  to all of them with the same response.
