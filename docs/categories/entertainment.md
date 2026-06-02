# Category: entertainment

**Category ID:** 11  
**ESPHome string:** `"entertainment"`

## Description

Streaming service alerts, podcast notifications, sports scores, and media
app updates. Volume depends heavily on which apps the user has — a heavy
Spotify user will see many of these. A subtle response is usually appropriate.

`title` is often the app or content title; `message` contains the detail
(episode title, artist, score update).

## Common sources

| App | Bundle ID |
|---|---|
| Spotify | `com.spotify.client` |
| Netflix | `com.netflix.Netflix` |
| Apple TV | `com.apple.tv` |
| Apple Podcasts | `com.apple.podcasts` |
| YouTube | `com.google.ios.youtube` |
| ESPN | `com.espn.ScoreCenter` |
| Plex | `com.plexapp.plex` |

## Example — log new Netflix episode alert

```yaml
ancs:
  auto_fetch_attributes: true
  fetch_attributes: [app_id, title, message]

  on_notification_attributes:
    - if:
        condition:
          lambda: >
            return category == "entertainment" &&
                   app_id == "com.netflix.Netflix";
        then:
          - logger.log:
              format: "Netflix: %s — %s"
              args: [title.c_str(), message.c_str()]
```

## Example — sports score alert with a flash

```yaml
ancs:
  auto_fetch_attributes: true
  fetch_attributes: [app_id, message]

  on_notification_attributes:
    - if:
        condition:
          lambda: >
            return category == "entertainment" &&
                   app_id == "com.espn.ScoreCenter";
        then:
          - logger.log:
              format: "Score: %s"
              args: [message.c_str()]
          - light.turn_on:
              id: onboard_led
              flash_length: 2s
```

## Example — gentle Spotify "now playing" indicator

Spotify sends `entertainment` notifications when a new track starts if
"Now Playing" notifications are enabled.

```yaml
ancs:
  auto_fetch_attributes: true
  fetch_attributes: [app_id]

  on_notification_attributes:
    - if:
        condition:
          lambda: 'return app_id == "com.spotify.client";'
        then:
          # Single short flash — subtle acknowledgement
          - light.turn_on:
              id: onboard_led
              flash_length: 200ms
```

## Example — any entertainment notification → Home Assistant sensor update

```yaml
ancs:
  auto_fetch_attributes: true
  fetch_attributes: [app_id, title, message]

  on_notification_attributes:
    - if:
        condition:
          lambda: 'return category == "entertainment";'
        then:
          - homeassistant.service:
              service: input_text.set_value
              data_template:
                entity_id: input_text.last_entertainment_alert
                value: !lambda 'return "[" + app_id + "] " + title + ": " + message;'
```

## Notes

- Spotify "Now Playing" notifications can fire very frequently if the user is
  actively listening. Consider filtering with `category_count` or adding a
  cooldown via a script with `mode: single`.
- Some streaming apps (Disney+, Hulu) send `entertainment` notifications for
  new content; others use `other`. Check with `app_id` if in doubt.
- Sports score apps often send one notification per scoring play during a
  game — this category can become very high volume. Use `category_count` to
  cap reactions.
