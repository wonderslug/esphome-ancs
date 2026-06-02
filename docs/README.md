# ESPHome ANCS Component — Documentation

Quick links to everything in this repository.

---

## Getting started

| Document | Contents |
|---|---|
| [Root README](../README.md) | Installation, full config reference, actions, sensors, on-device verification checklist |
| [Pairing guide](pairing.md) | **Why nRF Connect is required**, step-by-step pairing, bond mismatch recovery, auto-reconnect, troubleshooting |
| [Architecture](architecture.md) | Three-layer design, threading model, BLE lifecycle, key design decisions |

---

## ANCS category reference

Each notification iOS delivers carries a category that determines which apps trigger it and how to react.

| Document | Apps / use cases |
|---|---|
| [Category overview](categories/README.md) | Full table, available trigger variables, common patterns |
| [incoming_call](categories/incoming_call.md) | Phone, FaceTime, WhatsApp/Telegram/Signal calls |
| [missed_call](categories/missed_call.md) | Unanswered call badge |
| [voicemail](categories/voicemail.md) | Visual Voicemail |
| [social](categories/social.md) | iMessage, Instagram, Twitter/X, Snapchat |
| [email](categories/email.md) | Mail, Gmail, Outlook |
| [schedule](categories/schedule.md) | Calendar, Reminders |
| [news](categories/news.md) | Apple News, RSS readers |
| [health_fitness](categories/health_fitness.md) | Fitness rings, Strava, MyFitnessPal |
| [business_finance](categories/business_finance.md) | Banking, PayPal, Venmo |
| [location](categories/location.md) | Location-based Reminders, Find My |
| [entertainment](categories/entertainment.md) | Spotify, Netflix, ESPN |
| [other](categories/other.md) | Catch-all; WhatsApp, Telegram, Signal, Discord, Slack |

---

## Examples

Ready-to-flash YAML files for the WEMOS D1 Mini ESP32 (`wemos_d1_mini32`).
All use `framework: esp-idf` and the `external_components` local path.

| File | What it does |
|---|---|
| [d1-mini-blink-on-call.yaml](../examples/d1-mini-blink-on-call.yaml) | 3-blink groups while a call is ringing; stops on answer/decline |
| [d1-mini-blink-on-imessage.yaml](../examples/d1-mini-blink-on-imessage.yaml) | 5 slow blinks when an iMessage/SMS arrives (filtered by `app_id`) |
| [d1-mini-call-and-missed-alerts.yaml](../examples/d1-mini-call-and-missed-alerts.yaml) | Fast blink on incoming call + slow PWM breathe on missed call or voicemail |

---

## Remote installation

See [Root README § Installation](../README.md#installation) for the ESPHome `packages:` snippet.

```yaml
# Minimal — component only:
packages:
  ancs_component: github://wonderslug/esphome-ancs/packages/ancs.yaml@master

# With binary_sensor + text_sensor pre-wired:
substitutions:
  friendly_name: "Living Room"
packages:
  ancs_component: github://wonderslug/esphome-ancs/packages/ancs-with-sensors.yaml@master
```
