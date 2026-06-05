# ANCS Category Reference

Each ANCS notification carries a **category ID** set by iOS. The ESPHome `ancs` component
maps this to a string exposed as `category` in every trigger lambda.

## Category Table

| String | ID | Typical apps | REMOVED meaningful? |
|---|---|---|---|
| `incoming_call` | 1 | Phone, FaceTime, WhatsApp calls | ✅ Yes — call ended |
| `missed_call` | 2 | Phone, FaceTime | ⚠️ When user clears notification |
| `voicemail` | 3 | Phone (Visual Voicemail) | ⚠️ When voicemail is played |
| `social` | 4 | Instagram, Twitter/X, Snapchat, LinkedIn, sometimes iMessage | ⚠️ When dismissed |
| `schedule` | 5 | Calendar, Reminders, third-party calendars | ⚠️ When dismissed |
| `email` | 6 | Mail, Gmail, Outlook, Spark | ⚠️ When read/deleted |
| `news` | 7 | News, Feedly, Flipboard, RSS readers | ⚠️ When dismissed |
| `health_fitness` | 8 | Fitness, Health, MyFitnessPal, Strava | ⚠️ When dismissed |
| `business_finance` | 9 | Chase, PayPal, Robinhood, banking apps | ⚠️ When dismissed |
| `location` | 10 | Reminders (location-based), Find My | ⚠️ When dismissed |
| `entertainment` | 11 | Netflix, Spotify, Apple TV, Podcasts | ⚠️ When dismissed |
| `other` | 0 | Everything else — catch-all | ⚠️ When dismissed |

> **Note on `other`:** Many third-party apps land in `other` when iOS hasn't categorised
> them. If a notification type is missing from your expected category, check `other` and
> use `app_id` filtering instead (requires `auto_fetch_attributes: true`).

## What data is available and when

### Immediately (no GATT fetch — `on_notification_added`)

| Variable | Type | Description |
|---|---|---|
| `uid` | `uint32_t` | Unique notification ID for this session |
| `category` | `std::string` | Category string from the table above |
| `category_count` | `uint8_t` | Active notifications of this category on the iPhone right now |
| `flags` | `uint8_t` | Bitmask — see flags below |

### After attribute fetch (~1 s delay — `on_notification_attributes`)

Requires `auto_fetch_attributes: true` and the relevant key in `fetch_attributes`.

| Variable | Type | Description |
|---|---|---|
| `app_id` | `std::string` | Bundle ID of the source app, e.g. `com.apple.MobileSMS` |
| `title` | `std::string` | Sender name, event title, email subject, etc. |
| `subtitle` | `std::string` | Thread name, sender detail, etc. |
| `message` | `std::string` | Notification body — empty if iOS previews are off |
| `date` | `std::string` | iOS-assigned timestamp in `YYYYMMDD'T'HHmmSS` format — empty if `date` is not in `fetch_attributes` |
| `device_name` | `std::string` | BLE display name of the connected iPhone |

### Event flags

```cpp
0x01  FLAG_SILENT           // notification was delivered silently
0x02  FLAG_IMPORTANT        // notification is flagged as important
0x04  FLAG_PRE_EXISTING     // notification existed before ANCS connected — already discarded by the component
0x08  FLAG_POSITIVE_ACTION  // a positive action is available (e.g. "Answer")
0x10  FLAG_NEGATIVE_ACTION  // a negative action is available (e.g. "Decline")
```

## Common trigger patterns

### Immediate reaction by category (no fetch needed)

```yaml
ancs:
  auto_fetch_attributes: false
  on_notification_added:
    - if:
        condition:
          lambda: 'return category == "incoming_call";'
        then:
          - script.execute: my_script
```

### Filter by both category and app (requires fetch)

```yaml
ancs:
  auto_fetch_attributes: true
  fetch_attributes: [app_id, title]
  on_notification_attributes:
    - if:
        condition:
          lambda: 'return category == "social" && app_id == "net.whatsapp.WhatsApp";'
        then:
          - script.execute: my_script
```

### React only to the first notification of a category

```yaml
on_notification_added:
  - if:
      condition:
        lambda: 'return category == "email" && category_count == 1;'
      then:
        - light.turn_on: alert_led
```

### Check if notification has an action available

```yaml
on_notification_added:
  - if:
      condition:
        # FLAG_POSITIVE_ACTION = 0x08 — "Answer" button present
        lambda: 'return category == "incoming_call" && (flags & 0x08);'
      then:
        - script.execute: alert_ringing
```

---

## Per-category documentation

- [incoming_call](incoming_call.md) — Phone / FaceTime ringing
- [missed_call](missed_call.md) — Unanswered calls
- [voicemail](voicemail.md) — Visual Voicemail
- [social](social.md) — Social media and messaging apps
- [email](email.md) — Email notifications
- [schedule](schedule.md) — Calendar and reminders
- [news](news.md) — News and RSS apps
- [health_fitness](health_fitness.md) — Activity and workout notifications
- [business_finance](business_finance.md) — Banking and financial apps
- [location](location.md) — Location-triggered notifications
- [entertainment](entertainment.md) — Media and streaming apps
- [other](other.md) — Catch-all category
