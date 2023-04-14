// -*- eval: (platformio-mode 1); -*-
#include <Arduino.h>
#include <limits.h>
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

bool state =
#ifdef PRIME_ON_RELEASE
true;
#else
false;
#endif
bool int_activated = false, int_need_reset = false;
time_t last_trigger = TIME_T_MAX;

IRAM_ATTR void isr_trap(void) {
  state = !state;
  INT_DETACH;
  int_activated = true;
}

void setup(void) {
  Serial.begin(9600);
  INT_ATTACH;
}

void loop(void) {
  if(int_activated) {
    LOGF("New state: %s\n", state ? "Primed" : "Activated");

    int_activated = false;
    last_trigger = millis();
    int_need_reset = true;
  }

  /* prevent multiple interrupt triggers in a row */
  if(millis() > last_trigger + 200 && int_need_reset) {
    INT_ATTACH;
    int_need_reset = false;
  }
}
