# Category: email

**Category ID:** 6  
**ESPHome string:** `"email"`

## Description

Fires for email notifications from any mail app. Volume is typically high —
most users receive many emails per day. React selectively using `category_count`
or `app_id`/`title` filtering rather than alerting on every message.

`title` is usually the sender's display name. `message` is the email subject
line or a short preview. `subtitle` is the subject when `title` is the sender.

## Common sources

| App | Bundle ID |
|---|---|
| Apple Mail | `com.apple.mobilemail` |
| Gmail | `com.google.Gmail` |
| Outlook | `com.microsoft.Outlook` |
| Spark | `com.readdle.smartemail` |
| Airmail | `it.bloop.airmail2` |

## Example — alert only on the first new email

`category_count` is 1 when the first email arrives (inbox was empty before).
Use it to avoid alerting on subsequent messages in a burst.

```yaml
ancs:
  auto_fetch_attributes: false

  on_notification_added:
    - if:
        condition:
          lambda: 'return category == "email" && category_count == 1;'
        then:
          - light.turn_on:
              id: onboard_led
              flash_length: 2s
```

## Example — log sender and subject

```yaml
ancs:
  auto_fetch_attributes: true
  fetch_attributes: [app_id, title, message]

  on_notification_attributes:
    - if:
        condition:
          lambda: 'return category == "email";'
        then:
          - logger.log:
              format: "[%s] From: %s — %s"
              args: [app_id.c_str(), title.c_str(), message.c_str()]
```

## Example — react to Gmail only and pass to Home Assistant

```yaml
ancs:
  auto_fetch_attributes: true
  fetch_attributes: [app_id, title, message]

  on_notification_attributes:
    - if:
        condition:
          lambda: >
            return category == "email" && app_id == "com.google.Gmail";
        then:
          - homeassistant.service:
              service: notify.mobile_app
              data_template:
                title: "New Gmail"
                message: !lambda 'return title + ": " + message;'
```

## Notes

- `category_count` can be a useful throttle — you might only want to react
  when the count crosses a threshold (e.g. `category_count <= 3`).
- Some email clients send a notification per email; others batch them into
  one. `category_count` reflects the current unread count at the iOS level,
  not the number of ANCS events.
- `message` is often the first line of the email body or the subject,
  depending on the email app's notification settings.
- Filtering by sender in `title` with a `find()` check works for VIP-style
  alerts: `title.find("Boss Name") != std::string::npos`.
