// -*- eval: (platformio-mode 1); -*-
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <limits.h>
#include <stdio.h>
#include "config.h"

#ifndef IRAM_ATTR
# define IRAM_ATTR
#endif

/* macros */
#ifdef DEBUG_LOG
# define LOG(s)         Serial.print(s);
# define LOGLN(s)       Serial.println(s);
# define LOGF(fmt, ...) Serial.printf(fmt __VA_OPT__(,) __VA_ARGS__);
#else
# define LOG(s)
# define LOGLN(s)
# define LOGF(fmt, ...)
#endif

#define INT_ATTACH attachInterrupt(digitalPinToInterrupt(DI_TRAP), isr_trap, CHANGE)
#define INT_DETACH detachInterrupt(digitalPinToInterrupt(DI_TRAP))

#define TIME_T_MAX ((((time_t)1 << (sizeof(time_t) * CHAR_BIT - 2)) - 1) * 2 + 1)

enum trap_event {
TRAP_ACTIVATE,
TRAP_PRIME,
TRAP_N_EVENT,
};

#ifdef API_HTTPS
WiFiClientSecure client;
const char FINGERPRINT[] PROGMEM = API_FINGERPRINT;
#else
WiFiClient client;
#endif

bool state =
#ifdef PRIME_ON_RELEASE
true;
#else
false;
#endif
bool int_activated = false, int_need_reset = false;
time_t last_trigger = TIME_T_MAX;

void isr_trap(void);

bool api_connect(WiFiClient *c);
bool api_post(enum trap_event event);

void setup(void);
void loop(void);

IRAM_ATTR void isr_trap(void) {
  state = !state;
  INT_DETACH;
  int_activated = true;
}

bool api_connect(WiFiClient *c) {
  if(c->connected()) return true;

  LOG("Connecting to API");
  int r = 0;
  while(!c->connect(API_HOST, API_PORT) && r++ < API_RETRIES) {
    delay(100);
    LOG('.');
  }
  LOGLN();

  if(r > API_RETRIES) {
    LOGLN("Connection failed");
    return false;
  }
  LOGF("Connected in %d tries!\n", r);
  return true;
}

bool api_post(WiFiClient *c, enum trap_event event) {
  const char *event_str[TRAP_N_EVENT] = { "open", "close", };
  char payload[16];
  int payload_sz;

  if(!api_connect(c)) return false;

  snprintf(payload, sizeof payload, "type=%s%n",
           event_str[event], &payload_sz);
  LOGF("POST data: %s (%d)\n", payload, payload_sz);

  /* flush response */
  while(c->available()) c->read();

  c->printf("POST " API_BASEPATH "/events HTTP/1.1\n"
                "Host: " API_HOST "\n"
                "Content-Length: %d\n"
                "Content-Type: application/x-www-form-urlencoded\n"
                "Authorization: " API_SECRET "\n"
                "\n", payload_sz);

  if(!c->println(payload)) {
    LOGLN("Failed to POST event!");
    c->stop();
    return false;
  }

  /* ignore response */
  while(c->available()) c->read();

  return true;
}

void setup(void) {
#ifdef DEBUG_LOG
  Serial.begin(115200);
  while(!Serial) delay(10);
#endif

  pinMode(DI_TRAP, INPUT);

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    LOG('.');
  }
  LOGLN();

  LOGF("Connected! IP address: %s\n", WiFi.localIP().toString().c_str());
  LOGF("MAC address: %s\n", WiFi.macAddress().c_str());

#ifdef API_HTTPS
  LOGF("Fingerprint: %s\n", FINGERPRINT);
  client.setFingerprint(FINGERPRINT);
#endif
  client.setTimeout(15000);

  INT_ATTACH;
}

void loop(void) {
  if(int_activated) {
    LOGF("New state: %s\n", state ? "Primed" : "Activated");
    api_post(&client, state ? TRAP_PRIME : TRAP_ACTIVATE);

    int_activated = false;
    last_trigger = millis();
    int_need_reset = true;
  }

  /* prevent multiple interrupt triggers in a row */
  if(millis() > last_trigger + TRAP_TIMEOUT && int_need_reset) {
    INT_ATTACH;
    int_need_reset = false;
  }
}
