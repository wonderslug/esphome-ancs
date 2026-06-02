# Category: social

**Category ID:** 4  
**ESPHome string:** `"social"`

## Description

Covers social media notifications and some messaging apps. iOS categorises an
app here based on its App Store category — any app marked as "Social Networking"
lands here.

`app_id` filtering (via `on_notification_attributes`) lets you react
specifically to one app rather than all social notifications.

## Common sources

| App | Bundle ID |
|---|---|
| iMessage / SMS | `com.apple.MobileSMS` |
| Instagram | `com.burbn.instagram` |
| Twitter / X | `com.atebits.Tweetie2` |
| Snapchat | `com.toyopagroup.picaboo` |
| LinkedIn | `com.linkedin.LinkedIn` |
| TikTok | `com.zhiliaoapp.musically` |
| BeReal | `AlexisBarreyat.BeReal` |

> iMessage lands in `social` on some iOS versions and `other` on others. See
> [other.md](other.md) for why `app_id` is more reliable than `category` for
> iMessage detection.

## Example — blink on any social notification

```yaml
ancs:
  auto_fetch_attributes: false

  on_notification_added:
    - if:
        condition:
          lambda: 'return category == "social";'
        then:
          - light.turn_on:
              id: onboard_led
              flash_length: 3s
```

## Example — react only to iMessage

```yaml
ancs:
  auto_fetch_attributes: true
  fetch_attributes: [app_id, title, message]

  on_notification_attributes:
    - if:
        condition:
          lambda: 'return app_id == "com.apple.MobileSMS";'
        then:
          - logger.log:
              format: "iMessage from %s: %s"
              args: [title.c_str(), message.c_str()]
          - script.execute: blink_message

script:
  - id: blink_message
    mode: restart
    then:
      - repeat:
          count: 3
          then:
            - light.turn_on:
                id: onboard_led
                brightness: 100%
                transition_length: 0s
            - delay: 200ms
            - light.turn_off: onboard_led
            - delay: 200ms
```

## Example — different blink for each social app

```yaml
ancs:
  auto_fetch_attributes: true
  fetch_attributes: [app_id]

  on_notification_attributes:
    - if:
        condition:
          lambda: 'return category == "social";'
        then:
          - if:
              condition:
                lambda: 'return app_id == "com.apple.MobileSMS";'
              then:
                - script.execute: blink_message    # iMessage — slow blink
          - if:
              condition:
                lambda: 'return app_id == "com.burbn.instagram";'
              then:
                - script.execute: blink_social     # Instagram — fast flash

script:
  - id: blink_message
    mode: restart
    then:
      - repeat:
          count: 5
          then:
            - light.turn_on: { id: onboard_led, brightness: 60%, transition_length: 0s }
            - delay: 300ms
            - light.turn_off: onboard_led
            - delay: 300ms

  - id: blink_social
    mode: restart
    then:
      - light.turn_on:
          id: onboard_led
          flash_length: 2s
```

## Notes

- Social apps tend to send notifications at high volume. Consider using
  `category_count` to only react to the **first** notification in a burst:
  `category == "social" && category_count == 1`.
- The `message` attribute may be empty for Instagram, Twitter DMs, and
  others that send privacy-preserving notifications (e.g. "New message"
  with no preview).
- `title` in iMessage is the contact name or group name; in Instagram it is
  usually the username followed by the notification type.
