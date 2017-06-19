#include <TM1638.h>
#include <TM1638QYF.h>
#include <TM16XX.h>
#include <TM16XXFonts.h>

#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

#include <Wire.h>
#include "RTClib.h"
#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
#include <avr/power.h>
#endif

RTC_DS3231 rtc;

#define NEOPIXEL_PIN 6
#define NEOPIXEL_COUNT 6
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NEOPIXEL_COUNT, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

#define DELAY_MS 5
#define RTC_CHECK_MS 500
#define RTC_CHECK_COUNTER (RTC_CHECK_MS / DELAY_MS)

#define INITIAL_BEDTIME_LEVEL 100
#define BEDTIME_TIME_MINUTES 30
#define SECONDS_BETWEEN_CHANGE (BEDTIME_TIME_MINUTES * 60) / INITIAL_BEDTIME_LEVEL

#define WAKE_TIME (7UL * 60 * 60 + 0 * 60)
#define WAKE_TIMESPAN (15 * 60)
#define AWAKE_TIMESPAN (2 * 60 * 60)

//#define WAKE_TIME (16UL * 60 * 60 + 15 * 60)
//#define WAKE_TIMESPAN (15 * 60)
//#define AWAKE_TIMESPAN (1 * 60 * 60)

// Fill the dots one after the other with a color
void colorWipe(uint32_t c, uint8_t wait) {
  for (uint16_t i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, c);
    strip.show();
    delay(wait);
  }
}

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if (WheelPos < 85) {
    return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if (WheelPos < 170) {
    WheelPos -= 85;
    return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}

// Slightly different, this makes the rainbow equally distributed throughout
void rainbowCycle(uint8_t wait, uint8_t count = 5) {
  uint16_t i, j;

  for (j = 0; j < 256 * count; j++) { // 5 cycles of all colors on wheel
    for (i = 0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, Wheel(((i * 256 / strip.numPixels()) + j) & 255));
    }
    strip.show();
    delay(wait);
  }
}

typedef enum {
  st_none,      // no current activity
  st_sleeping,  // dimming for sleep
  st_waking,    // lighting up for waking
  st_awake      // fully lit for waking
} state_t;

uint16_t loop_counter = 0;
state_t state = st_none;
int16_t current_bedtime_level = 0;
uint32_t next_change_seconds = 0;
uint8_t awake_count = 0;
TM1638 tm1638(8, 9, 10);
DHT_Unified dht(3, DHT11);

void setup () {
  // Buttons pull down; enable internal pullups
  pinMode(4, INPUT_PULLUP);
  pinMode(5, INPUT_PULLUP);

#ifndef ESP8266
  while (!Serial); // for Leonardo/Micro/Zero
#endif
  Serial.begin(9600);

  // Initial neopixels
  strip.begin();
  strip.show(); // Initialize all pixels to 'off'

  dht.begin();

  delay(1000); // wait for console opening

  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }

  if (rtc.lostPower()) {
    Serial.println("RTC lost power, resetting time to compile time");
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

    tm1638.clearDisplay();
    tm1638.setDisplayToString("NO CLOCK");
    delay(3000);
  }  
  
  
  Serial.print("Seconds between change: ");
  Serial.print(SECONDS_BETWEEN_CHANGE);
  Serial.println();

  rainbowCycle(10, 2);
  colorWipe(strip.Color(0, 0, 00), 133);
}

typedef enum {
  time_e,
  environ_e,
  none_e
} display_mode_t;

display_mode_t display_mode = none_e;

void loop () {
  byte buttons = tm1638.getButtons();
  
  if (buttons & 1) { // button 1: activate or cancel sleep mode
    if (state == st_sleeping) {
      state = st_none;
      colorWipe(strip.Color(0, 0, 00), 133);
    }
    else {
      state = st_sleeping;
      current_bedtime_level = INITIAL_BEDTIME_LEVEL;
      next_change_seconds = rtc.now().secondstime() + SECONDS_BETWEEN_CHANGE;
      colorWipe(strip.Color(current_bedtime_level, 0, 0), 100);
    }
    while (tm1638.getButtons() & 1)
      delay(1);
  }  
  else if (buttons & 2) { // button 2: 5 cycles of rainbows
    rainbowCycle(10, 5);
    colorWipe(strip.Color(0, 0, 0), 0);
  }
  else if (buttons & 4) {
    display_mode = time_e;
  }
  else if (buttons & 8) {
    display_mode = environ_e;
  }
  else if (buttons & 16) {
    tm1638.clearDisplay();
    display_mode = none_e;
  }

  if ((loop_counter % RTC_CHECK_COUNTER) == 0) {
    DateTime now = rtc.now();

    if (display_mode == time_e) {
      tm1638.setDisplayToDecNumber(now.hour() * 10000UL + now.minute() * 100 + now.second(), 1 << 4 | 1 << 2, false);
    }
    else if (display_mode == environ_e) {
      sensors_event_t event;  
      dht.temperature().getEvent(&event);
      if (!isnan(event.temperature)) {
        tm1638.setDisplayDigit((int)event.temperature % 10, 7, 0, NUMBER_FONT);
        tm1638.setDisplayDigit(((int)(event.temperature / 10)) % 10, 6, 0, NUMBER_FONT);
      }

      dht.humidity().getEvent(&event);
      if (isnan(event.relative_humidity)) {
        Serial.println("Error reading humidity!");
      }
      else {
        tm1638.setDisplayDigit((int)event.relative_humidity % 10, 1, 0, NUMBER_FONT);
        tm1638.setDisplayDigit(((int)(event.relative_humidity / 10)) % 10, 0, 0, NUMBER_FONT);
      }
    }

    uint32_t now_seconds = now.secondstime();

    if (now_seconds >= next_change_seconds)
      switch (state) {
        case st_sleeping:
          next_change_seconds = now_seconds + SECONDS_BETWEEN_CHANGE;
          current_bedtime_level--;
          if (current_bedtime_level <= 0) {
            state = st_none;
            current_bedtime_level = 0;
          }
          colorWipe(strip.Color(current_bedtime_level, 0, 0), 0);
          break;

        case st_waking:
          if (awake_count == 5) {
            rainbowCycle(10, 10);
            colorWipe(strip.Color(0, 255, 0), 0);
          }
          else
            strip.setPixelColor(awake_count, strip.Color(130, 130, 0));
          strip.show();

          awake_count++;
          if (awake_count > 5) {
            state = st_awake;
            next_change_seconds = now_seconds + AWAKE_TIMESPAN;
          }
          else
            next_change_seconds = now_seconds + WAKE_TIMESPAN / 5;
          break;

        case st_awake:
          colorWipe(strip.Color(0, 0, 0), 0);
          state = st_none;
          break;

        case st_none:
          if (now_seconds % 86400 > WAKE_TIME - WAKE_TIMESPAN && now_seconds % 86400 < WAKE_TIME + AWAKE_TIMESPAN) {
            state = st_waking;
            awake_count = 0;
          }
          next_change_seconds = now_seconds + 2;
          break;
      }
  }

  loop_counter++;
  if (loop_counter == 60000)
    loop_counter = 0;
  delay(DELAY_MS);
}
