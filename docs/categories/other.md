# Category: other

**Category ID:** 0  
**ESPHome string:** `"other"`

## Description

The catch-all category. Any app whose App Store category does not map to one
of the named ANCS categories lands here. This includes most third-party
messaging apps (WhatsApp, Telegram, Signal), some versions of iMessage, many
utility apps, and any app Apple hasn't formally categorised.

**`other` is the most common category you will encounter for third-party
apps.** Do not assume an app is absent just because it doesn't show up in
`social` or `entertainment` — check `other` first, then use `app_id` to
identify it precisely.

## Why iMessage sometimes appears here

iOS categorises notifications by the app's App Store **primary category**,
not by the content. The Messages app's primary category shifts between iOS
versions. The reliable approach for iMessage detection is always `app_id`:

```yaml
lambda: 'return app_id == "com.apple.MobileSMS";'
```

## Common sources (partial list)

| App | Bundle ID |
|---|---|
| iMessage / SMS (some iOS) | `com.apple.MobileSMS` |
| WhatsApp | `net.whatsapp.WhatsApp` |
| Telegram | `ph.telegra.Telegraph` |
| Signal | `org.whispersystems.signal` |
| Discord | `com.hammerandchisel.discord` |
| Slack | `com.tinyspeck.chatlyio` |
| Microsoft Teams | `com.microsoft.Teams` |
| Messenger | `com.facebook.Messenger` |
| Threads | `com.burbn.barcelona` |
| Reddit | `com.reddit.Reddit` |
| Duolingo | `com.duolingo.DuolingoMobile` |
| Uber / Lyft | `com.ubercab.UberClient` / `com.zimride.Lyft` |

## Example — react to any `other` notification

```yaml
ancs:
  auto_fetch_attributes: false

  on_notification_added:
    - if:
        condition:
          lambda: 'return category == "other";'
        then:
          - light.turn_on:
              id: onboard_led
              flash_length: 1s
```

## Example — filter to messaging apps within `other`

```yaml
ancs:
  auto_fetch_attributes: true
  fetch_attributes: [app_id, title, message]

  on_notification_attributes:
    - if:
        condition:
          lambda: >
            return category == "other" && (
              app_id == "com.apple.MobileSMS"           ||
              app_id == "net.whatsapp.WhatsApp"          ||
              app_id == "ph.telegra.Telegraph"           ||
              app_id == "org.whispersystems.signal"      ||
              app_id == "com.hammerandchisel.discord"
            );
        then:
          - logger.log:
              format: "[%s] %s: %s"
              args: [app_id.c_str(), title.c_str(), message.c_str()]
          - script.execute: blink_message

script:
  - id: blink_message
    mode: restart
    then:
      - repeat:
          count: 3
          then:
            - light.turn_on: { id: onboard_led, brightness: 100%, transition_length: 0s }
            - delay: 250ms
            - light.turn_off: onboard_led
            - delay: 250ms
```

## Example — discover which apps are sending notifications

If you're not sure which category or `app_id` an app uses, log everything
temporarily and watch the serial output:

```yaml
ancs:
  auto_fetch_attributes: true
  fetch_attributes: [app_id, title, message]

  on_notification_added:
    - logger.log:
        format: "NOTIF added  uid=%u  cat=%s  count=%u"
        args: [uid, category.c_str(), category_count]

  on_notification_attributes:
    - logger.log:
        format: "ATTRS  app=%s  title=%s  msg=%s"
        args: [app_id.c_str(), title.c_str(), message.c_str()]
```

Watch the log output with `esphome logs your-config.yaml` while triggering
notifications on the iPhone. Capture the `app_id` for any app you want to
react to specifically.

## Notes

- `other` is a very wide net. Avoid reacting to every `other` notification
  without filtering — high-volume apps like Reddit or Duolingo can flood the
  category.
- `category_count` aggregates across **all** apps in `other`, so it is less
  meaningful here than in single-source categories like `email`.
- When a new iOS version reclassifies apps, notifications you were seeing in
  `other` may start appearing in a named category. If alerts stop working,
  check whether the category string changed.
