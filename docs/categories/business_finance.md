# Category: business_finance

**Category ID:** 9  
**ESPHome string:** `"business_finance"`

## Description

Banking alerts, payment confirmations, stock notifications, and business app
messages. These are often high-importance, time-sensitive events — a payment
received or a significant price change warrants a noticeable alert. At the same
time, frequent apps like stock tickers can be very high volume.

`message` usually contains the amount or transaction detail. `title` is
typically the institution name or alert type.

## Common sources

| App | Bundle ID |
|---|---|
| Chase | `com.chase.sig.ios` |
| Bank of America | `com.bankofamerica.BofA` |
| PayPal | `com.paypal.PPClient` |
| Venmo | `net.venmo.Venmo` |
| Cash App | `com.squarecash.cash` |
| Robinhood | `com.robinhood.release.Robinhood` |
| Coinbase | `com.coinbase.Coinbase` |

## Example — alert on any finance notification

```yaml
ancs:
  auto_fetch_attributes: false

  on_notification_added:
    - if:
        condition:
          lambda: 'return category == "business_finance";'
        then:
          - light.turn_on:
              id: onboard_led
              flash_length: 3s
```

## Example — log payment received and push to Home Assistant

```yaml
ancs:
  auto_fetch_attributes: true
  fetch_attributes: [app_id, title, message]

  on_notification_attributes:
    - if:
        condition:
          lambda: >
            return category == "business_finance" &&
                   (app_id == "net.venmo.Venmo" || app_id == "com.squarecash.cash");
        then:
          - logger.log:
              format: "Payment: %s — %s"
              args: [title.c_str(), message.c_str()]
          - homeassistant.service:
              service: notify.notify
              data_template:
                title: "Payment Received"
                message: !lambda 'return message;'
          - script.execute: blink_payment

script:
  - id: blink_payment
    mode: single
    then:
      - repeat:
          count: 3
          then:
            - light.turn_on: { id: onboard_led, brightness: 100%, transition_length: 0s }
            - delay: 200ms
            - light.turn_off: onboard_led
            - delay: 200ms
```

## Example — filter out high-frequency stock alerts

Only react to balance/transaction alerts, not every price-change notification.

```yaml
ancs:
  auto_fetch_attributes: true
  fetch_attributes: [app_id, title]

  on_notification_attributes:
    - if:
        condition:
          lambda: >
            return category == "business_finance" &&
                   app_id != "com.robinhood.release.Robinhood";
        then:
          - script.execute: blink_payment
```

## Notes

- Banking app notification content can vary significantly by region and app
  version. Test your `message.find()` conditions against real notifications.
- Some banking apps suppress notification preview text for security reasons —
  `message` may be a generic "You have a new notification" rather than the
  actual transaction detail.
- `category_count` is less useful here compared to `incoming_call` — a bank
  may send several alerts in a row (e.g. purchase auth + confirmation) and
  you generally want to react to each one.
