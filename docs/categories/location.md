# Category: location

**Category ID:** 10  
**ESPHome string:** `"location"`

## Description

Geofence-triggered notifications: location-based Reminders ("When I arrive
home"), Find My alerts, and location-aware app alerts. Volume is low — these
fire only when entering or leaving a defined area.

`title` is usually the reminder or alert text; `message` may be a location
name. These can be very useful for home-automation context — knowing the user
arrived or left somewhere can trigger scenes or presence updates.

## Common sources

| App | Bundle ID |
|---|---|
| Reminders (location-based) | `com.apple.reminders` |
| Find My | `com.apple.findmy` |
| Google Maps (location share) | `com.google.Maps` |

## Example — trigger a Home Assistant automation when arriving home

```yaml
ancs:
  auto_fetch_attributes: true
  fetch_attributes: [title]

  on_notification_attributes:
    - if:
        condition:
          lambda: >
            return category == "location" &&
                   title.find("home") != std::string::npos;
        then:
          - logger.log: "Arrived home"
          - homeassistant.service:
              service: scene.turn_on
              data:
                entity_id: scene.arrive_home
```

## Example — log all location alerts

```yaml
ancs:
  auto_fetch_attributes: true
  fetch_attributes: [title, message]

  on_notification_attributes:
    - if:
        condition:
          lambda: 'return category == "location";'
        then:
          - logger.log:
              format: "Location alert: %s — %s"
              args: [title.c_str(), message.c_str()]
```

## Example — Find My family member alerts

```yaml
ancs:
  auto_fetch_attributes: true
  fetch_attributes: [app_id, title]

  on_notification_attributes:
    - if:
        condition:
          lambda: >
            return category == "location" &&
                   app_id == "com.apple.findmy";
        then:
          - logger.log:
              format: "Find My: %s"
              args: [title.c_str()]
          - light.turn_on:
              id: onboard_led
              flash_length: 2s
```

## Notes

- Location-based Reminders fire as `location`, not `schedule` — even though
  they come from the Reminders app.
- The text of location reminders is exactly the reminder text you wrote, so
  `title.find("work")` or `title.find("store")` can match specific reminders.
- Find My location-sharing alerts arrive here only when the sharing circle
  has notifications enabled.
- These notifications rarely arrive in bursts, so `category_count` is less
  useful. React to each one individually.
