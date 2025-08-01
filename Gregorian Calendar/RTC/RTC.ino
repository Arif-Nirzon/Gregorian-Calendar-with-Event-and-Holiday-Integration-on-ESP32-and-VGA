#include <Wire.h>
#include "RTClib.h"

// RTC Pins
const int sda = 21, scl = 22;
RTC_DS3231 rtc;

// Date-Time Globals
const char* months[] = {"Invalid", "January", "February", "March", "April", "May", "June",
                        "July", "August", "September", "October", "November", "December"};
const char* daysOfWeek[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

int current_date, current_month, current_year;
int current_hour, current_min, current_sec;
const char* ampm = "AM";
const char* current_day_str = "Monday";

void updateDateTime() {
  DateTime now = rtc.now();
  current_year = now.year();
  current_month = now.month();
  current_date = now.day();
  current_day_str = daysOfWeek[now.dayOfTheWeek()];
  current_hour = now.hour() % 12; 
  if (current_hour == 0) current_hour = 12;
  current_min = now.minute();
  current_sec = now.second();
  ampm = (now.hour() >= 12) ? "PM" : "AM";
}

void setup() {
  Serial.begin(115200);
  Wire.begin(sda, scl);
  rtc.begin();

  if (rtc.lostPower()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));  // set RTC to compile time
  }
}

void loop() {
  updateDateTime();

  Serial.print("Date: ");
  Serial.print(current_day_str); Serial.print(", ");
  Serial.print(current_date); Serial.print(" ");
  Serial.print(months[current_month]); Serial.print(" ");
  Serial.println(current_year);

  Serial.print("Time: ");
  Serial.print(current_hour); Serial.print(":");
  Serial.print(current_min); Serial.print(":");
  Serial.print(current_sec); Serial.print(" ");
  Serial.println(ampm);

  delay(1000);
}
