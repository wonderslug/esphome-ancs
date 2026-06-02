# Category: news

**Category ID:** 7  
**ESPHome string:** `"news"`

## Description

Breaking-news alerts and article notifications from news readers and RSS apps.
Volume varies widely — a breaking-news day can send dozens of notifications.
A subtle, non-intrusive response is usually appropriate.

## Common sources

| App | Bundle ID |
|---|---|
| Apple News | `com.apple.news` |
| BBC News | `uk.co.bbc.news` |
| CNN | `com.cnn.CNN` |
| Feedly | `com.devhd.feedly` |
| Reeder | `com.reederapp.5.iOS` |

## Example — single short flash

```yaml
ancs:
  auto_fetch_attributes: false

  on_notification_added:
    - if:
        condition:
          lambda: 'return category == "news";'
        then:
          - light.turn_on:
              id: onboard_led
              flash_length: 500ms
```

## Example — log the headline and react only to breaking news

```yaml
ancs:
  auto_fetch_attributes: true
  fetch_attributes: [title, message]

  on_notification_attributes:
    - if:
        condition:
          lambda: >
            return category == "news" &&
                   title.find("BREAKING") != std::string::npos;
        then:
          - logger.log:
              format: "BREAKING: %s"
              args: [message.c_str()]
          - light.turn_on:
              id: onboard_led
              flash_length: 3s
```

## Example — throttle using category_count

Only react to news notifications up to a daily threshold.

```yaml
on_notification_added:
  - if:
      condition:
        # Blink for the first 3 news items only
        lambda: 'return category == "news" && category_count <= 3;'
      then:
        - light.turn_on:
            id: onboard_led
            flash_length: 500ms
```

## Notes

- `title` is typically the publication name (e.g. "BBC News"); `message` is
  the headline. The order can be swapped depending on the app.
- News apps often batch several stories into one notification. `category_count`
  reflects the total unread count, not a useful per-story counter in this case.
- Consider using Home Assistant automations to filter or rate-limit news
  notifications rather than handling all of it in the ESPHome firmware.
