# ANCS App ID Quick Reference

A developer lookup table covering every app listed in the per-category docs.
Use this to find the `app_id` (iOS bundle ID) you need for a lambda filter,
and to confirm which `category` string the notification arrives under.

---

## Master Table (alphabetical by app)

| App | Bundle ID (`app_id`) | Category | Cat ID | Notes |
|---|---|---|---|---|
| Airmail | `it.bloop.airmail2` | `email` | 6 | — |
| Apple Mail | `com.apple.mobilemail` | `email` | 6 | — |
| Apple Music | `com.apple.Music` | `entertainment` | 11 | — |
| Apple News | `com.apple.news` | `news` | 7 | — |
| Apple Podcasts | `com.apple.podcasts` | `entertainment` | 11 | — |
| Apple TV | `com.apple.tv` | `entertainment` | 11 | — |
| Bank of America | `com.bankofamerica.BofA` | `business_finance` | 9 | — |
| BBC News | `uk.co.bbc.news` | `news` | 7 | — |
| BeReal | `AlexisBarreyat.BeReal` | `social` | 4 | — |
| Calendar | `com.apple.mobilecal` | `schedule` | 5 | Time-based reminders only; location-based → `location` |
| Cash App | `com.squarecash.cash` | `business_finance` | 9 | — |
| Chase | `com.chase.sig.ios` | `business_finance` | 9 | Preview text may be suppressed for security |
| CNN | `com.cnn.CNN` | `news` | 7 | — |
| Coinbase | `com.coinbase.Coinbase` | `business_finance` | 9 | — |
| Cron | `com.cron.app` | `schedule` | 5 | — |
| Discord | `com.hammerandchisel.discord` | `other` | 0 | — |
| Duolingo | `com.duolingo.DuolingoMobile` | `other` | 0 | High volume; use `category_count` |
| ESPN | `com.espn.ScoreCenter` | `entertainment` | 11 | Very high volume during live games |
| FaceTime | `com.apple.facetime` | `incoming_call` | 1 | Audio and video calls |
| Fantastical | `com.flexibits.fantastical2.iphone` | `schedule` | 5 | — |
| Feedly | `com.devhd.feedly` | `news` | 7 | — |
| Find My | `com.apple.findmy` | `location` | 10 | — |
| Fitness | `com.apple.Fitness` | `health_fitness` | 8 | Daily ring-close summary; `message` contains goal text |
| Garmin Connect | `com.garmin.connect.mobile` | `health_fitness` | 8 | — |
| Gmail | `com.google.Gmail` | `email` | 6 | — |
| Google Calendar | `com.google.calendar` | `schedule` | 5 | — |
| Google Maps | `com.google.Maps` | `location` | 10 | Location-sharing alerts only |
| Health | `com.apple.Health` | `health_fitness` | 8 | Heart-rate warnings, trends |
| iMessage / SMS | `com.apple.MobileSMS` | `social` or `other` | 4 or 0 | Category varies by iOS version; use `app_id` not `category` |
| Instagram | `com.burbn.instagram` | `social` | 4 | `message` often empty (privacy preview off) |
| LinkedIn | `com.linkedin.LinkedIn` | `social` | 4 | — |
| Lyft | `com.zimride.Lyft` | `other` | 0 | — |
| Messenger | `com.facebook.Messenger` | `other` | 0 | — |
| Microsoft Teams | `com.microsoft.Teams` | `other` | 0 | — |
| MyFitnessPal | `com.myfitnesspal.mfp` | `health_fitness` | 8 | — |
| Netflix | `com.netflix.Netflix` | `entertainment` | 11 | New-content alerts |
| Outlook | `com.microsoft.Outlook` | `email` or `schedule` | 6 or 5 | Same bundle ID for email and calendar events |
| PayPal | `com.paypal.PPClient` | `business_finance` | 9 | — |
| Phone (cellular) | `com.apple.mobilephone` | `incoming_call` / `missed_call` / `voicemail` | 1 / 2 / 3 | All three categories use same bundle ID |
| Plex | `com.plexapp.plex` | `entertainment` | 11 | — |
| Reddit | `com.reddit.Reddit` | `other` | 0 | High volume; filter with `category_count` |
| Reeder | `com.reederapp.5.iOS` | `news` | 7 | — |
| Reminders (time) | `com.apple.reminders` | `schedule` | 5 | — |
| Reminders (location) | `com.apple.reminders` | `location` | 10 | Same bundle ID; distinguish by `category` |
| Robinhood | `com.robinhood.release.Robinhood` | `business_finance` | 9 | Very high volume on active trading days |
| Signal | `org.whispersystems.signal` | `other` | 0 | Calls → `incoming_call` |
| Slack | `com.tinyspeck.chatlyio` | `other` | 0 | — |
| Snapchat | `com.toyopagroup.picaboo` | `social` | 4 | — |
| Spark | `com.readdle.smartemail` | `email` | 6 | — |
| Spotify | `com.spotify.client` | `entertainment` | 11 | "Now Playing" fires very frequently |
| Strava | `com.strava.stravaride` | `health_fitness` | 8 | — |
| Telegram | `ph.telegra.Telegraph` | `other` | 0 | Calls → `incoming_call` |
| Threads | `com.burbn.barcelona` | `other` | 0 | — |
| TikTok | `com.zhiliaoapp.musically` | `social` | 4 | — |
| Twitter / X | `com.atebits.Tweetie2` | `social` | 4 | — |
| Uber | `com.ubercab.UberClient` | `other` | 0 | — |
| Venmo | `net.venmo.Venmo` | `business_finance` | 9 | — |
| Visual Voicemail | `com.apple.mobilephone` | `voicemail` | 3 | Same bundle ID as Phone |
| WhatsApp | `net.whatsapp.WhatsApp` | `other` | 0 | Calls → `incoming_call` |
| Withings | `com.withings.wiscale2` | `health_fitness` | 8 | — |
| YouTube | `com.google.ios.youtube` | `entertainment` | 11 | — |

---

## Category Summary

Quick reference from category string to apps. See the linked per-category doc
for trigger variables, examples, and timing details.

| Category string | Cat ID | ANCS constant | Typical sources |
|---|---|---|---|
| `incoming_call` | 1 | `Category::INCOMING_CALL` | Phone, FaceTime, WhatsApp, Telegram, Signal |
| `missed_call` | 2 | `Category::MISSED_CALL` | Phone, FaceTime |
| `voicemail` | 3 | `Category::VOICEMAIL` | Phone (Visual Voicemail) |
| `social` | 4 | `Category::SOCIAL` | iMessage\*, Instagram, Twitter/X, Snapchat, LinkedIn, TikTok |
| `schedule` | 5 | `Category::SCHEDULE` | Calendar, Reminders (time), Fantastical, Google Calendar, Outlook |
| `email` | 6 | `Category::EMAIL` | Mail, Gmail, Outlook, Spark, Airmail |
| `news` | 7 | `Category::NEWS` | Apple News, BBC News, CNN, Feedly, Reeder |
| `health_fitness` | 8 | `Category::HEALTH_FITNESS` | Fitness, Health, Strava, MyFitnessPal, Garmin |
| `business_finance` | 9 | `Category::BUSINESS_FINANCE` | Chase, Bank of America, PayPal, Venmo, Cash App, Robinhood |
| `location` | 10 | `Category::LOCATION` | Reminders (location), Find My, Google Maps |
| `entertainment` | 11 | `Category::ENTERTAINMENT` | Spotify, Netflix, Apple TV, Podcasts, YouTube, ESPN, Plex |
| `other` | 0 | `Category::OTHER` | WhatsApp, Telegram, Signal, Discord, Slack, Teams, Messenger, iMessage\*, Reddit |

\* iMessage (`com.apple.MobileSMS`) category varies by iOS version — always use `app_id`.

---

## Typical attribute content by app type

| App type | `title` | `subtitle` | `message` |
|---|---|---|---|
| Phone call (incoming) | Caller name or number | — | — |
| iMessage / SMS | Contact or group name | — | Message preview (empty if previews off) |
| Email | Sender name | Subject | First line of body |
| Calendar / Reminders | Event or reminder name | — | "in 15 minutes" or location |
| Social (Instagram, Twitter) | Username or notification type | — | Often empty (privacy) |
| Banking | Institution or alert type | — | Transaction detail (may be suppressed) |
| News | Publication name | — | Headline |
| Health / Fitness | App name or goal | — | Goal text (e.g., "You closed your rings!") |
| Streaming (Spotify, Netflix) | Content title | — | Detail (episode, artist) |
| Find My | Alert description | — | Location name |

---

## Cross-category apps

Some apps send notifications under **different categories** depending on the
notification type. Checking `app_id` is the only reliable way to identify these.

| App | Notification type | Category | Category ID |
|---|---|---|---|
| Phone | Ringing | `incoming_call` | 1 |
| Phone | Unanswered | `missed_call` | 2 |
| Phone | New voicemail | `voicemail` | 3 |
| WhatsApp | Message | `other` | 0 |
| WhatsApp | Call ringing | `incoming_call` | 1 |
| Telegram | Message | `other` | 0 |
| Telegram | Call ringing | `incoming_call` | 1 |
| Signal | Message | `other` | 0 |
| Signal | Call ringing | `incoming_call` | 1 |
| iMessage | Message | `social` or `other` | 4 or 0 |
| Reminders | Time-based | `schedule` | 5 |
| Reminders | Location-based | `location` | 10 |
| Outlook | Email | `email` | 6 |
| Outlook | Calendar event | `schedule` | 5 |

---

## ESPHome filter snippets

### Look up any app by bundle ID

```yaml
ancs:
  auto_fetch_attributes: true
  fetch_attributes: [app_id, title, message]

  on_notification_attributes:
    - if:
        condition:
          lambda: 'return app_id == "net.whatsapp.WhatsApp";'
        then:
          - logger.log:
              format: "WhatsApp: %s"
              args: [message.c_str()]
```

### Multi-app messaging filter

```yaml
on_notification_attributes:
  - if:
      condition:
        lambda: >
          return app_id == "com.apple.MobileSMS"        ||
                 app_id == "net.whatsapp.WhatsApp"       ||
                 app_id == "ph.telegra.Telegraph"        ||
                 app_id == "org.whispersystems.signal"   ||
                 app_id == "com.hammerandchisel.discord";
      then:
        - script.execute: message_alert
```

### Distinguish Reminders time vs. location

```yaml
on_notification_attributes:
  - if:
      condition:
        # Both come from com.apple.reminders — use category to tell them apart
        lambda: >
          return app_id == "com.apple.reminders" && category == "location";
      then:
        - homeassistant.service:
            service: scene.turn_on
            data:
              entity_id: scene.arrived_home
```

---

## Finding unlisted apps

If you need the bundle ID for an app not listed here, flash the
[log-everything example](../examples/log-everything.yaml), trigger a
notification from the target app, and read the `app=` field in the log output:

```
ATTRS  uid=12345  cat=other  app=com.example.MyApp  title=Hello  sub=  msg=World
```

That `app=` value is the bundle ID to use in your lambda.
