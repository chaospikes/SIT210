#ifndef SECRETS_H
#define SECRETS_H

// Wi-Fi settings
#define SECRET_SSID "your_wifi_name"
#define SECRET_PASS "your_wifi_password"

// ThingSpeak settings
// Field 1 stores:
// 1 = dose taken
// 0 = dose missed
#define SECRET_WRITE_APIKEY "your_thingspeak_write_api_key"

// IFTTT Webhooks settings
// Used to send missed-dose email alerts.
#define SECRET_IFTTT_EVENT_NAME "your_ifttt_event_name"
#define SECRET_IFTTT_KEY "your_ifttt_webhooks_key"

#endif
