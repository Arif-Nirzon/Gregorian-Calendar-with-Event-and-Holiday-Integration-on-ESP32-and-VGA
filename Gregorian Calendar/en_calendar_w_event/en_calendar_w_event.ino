#include <ESP32Video.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include "RTClib.h"

// Font
#include <Ressources/CodePage437_8x8.h>
#include <Ressources/CodePage437_8x14.h>
#include <Ressources/CodePage437_8x16.h>
#include <Ressources/CodePage437_8x19.h>
#include <Ressources/Font6x8.h>

// VGA Pins
const int redPin = 25, greenPin = 14, bluePin = 13, hsyncPin = 32, vsyncPin = 33;
VGA3Bit videodisplay;

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
int startDay = 0, daysInMonth = 30;
bool showDate = true;
unsigned long lastToggle = 0;
const int blinkInterval = 500;
int prevSec = -1, prevDate = -1, prevMonth = -1;

// ==== Event and Holiday Handling ====
struct Holiday {
  int day;
  int month;
  const char* name;
};

Holiday holidayList[20] = {
  {15, 2, "Shab e-Barat"},
  {21, 2, "Language Martyrs' Day"},
  {26, 3, "Independence Day"},
  {28, 3, "Shab-e-qadr"},
  {31, 3, "Eid ul-Fitr (tentative)"},
  {14, 4, "Bengali New Year"},
  {1, 5, "May Day"},
  {5, 6, "Buddha Purnima/Vesak"},
  {6, 6, "Eid al-Adha (tentative)"},
  {1, 7, "Bank Holiday"},
  {6, 7, "Ashura (tentative)"},
  {16, 8, "Janmashtami"},
  {28, 9, "Eid e-Milad-un Nabi (tentative)"},
  {1, 10, "Mahanabami"},
  {2, 10, "Durga Puja"},
  {16, 12, "Victory Day"},
  {25, 12, "Christmas Day"},
  {31, 12, "New Year's Eve"}
};
int totalHolidays = 18;

void sortHolidays() {
  for (int i = 0; i < totalHolidays - 1; i++) {
    for (int j = i + 1; j < totalHolidays; j++) {
      bool shouldSwap = false;

      if (holidayList[i].month > holidayList[j].month) {
        shouldSwap = true;
      } else if (holidayList[i].month == holidayList[j].month &&
                 holidayList[i].day > holidayList[j].day) {
        shouldSwap = true;
      }

      if (shouldSwap) {
        Holiday temp = holidayList[i];
        holidayList[i] = holidayList[j];
        holidayList[j] = temp;
      }
    }
  }
}

bool needRedraw = true;

// ==== WiFi & WebServer ====
const char* ssid = "ESP32_UI";
const char* password = "12345678";
WebServer server(80);

const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Event Scheduler</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: Arial; text-align: center; margin-top: 50px; }
    input[type=number], input[type=text] {
      width: 200px; padding: 10px; margin: 5px;
    }
    button { padding: 10px 20px; margin: 10px; }
  </style>
</head>
<body>
  <h2>Add Holiday/Event</h2>
  <form action="/submit" method="POST">
    <input type="number" name="day" min="1" max="31" placeholder="Day" required><br>
    <input type="number" name="month" min="1" max="12" placeholder="Month" required><br>
    <input type="text" name="event" placeholder="Event Name" required><br>
    <button type="submit">Save</button>
  </form>
</body>
</html>
)rawliteral";

void handleRoot() {
  server.send(200, "text/html", htmlPage);
}

void handleSubmit() {
  int d = server.arg("day").toInt();
  int m = server.arg("month").toInt();
  String ev = server.arg("event");

  if (totalHolidays < 20) {
    holidayList[totalHolidays++] = {d, m, strdup(ev.c_str())};
    sortHolidays();
    needRedraw = true;
  }

  String response = "<h3>Saved!</h3><p>Date: " + String(d) + "/" + String(m) +
                    "<br>Event: " + ev + "</p><a href='/'>Back</a>";
  server.send(200, "text/html", response);
}

// ==== Calendar Logic ====
int getDaysInMonth(int month, int year) {
  if (month == 2) return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) ? 29 : 28;
  if (month == 4 || month == 6 || month == 9 || month == 11) return 30;
  return 31;
}

int getStartDay(int year, int month) {
  if (month < 3) { month += 12; year--; }
  int K = year % 100, J = year / 100;
  int h = (1 + (13*(month + 1))/5 + K + K/4 + J/4 + 5*J) % 7;
  return (h + 6) % 7;
}

void updateDateTime() {
  DateTime now = rtc.now();
  current_year = now.year();
  current_month = now.month();
  current_date = now.day();
  current_day_str = daysOfWeek[now.dayOfTheWeek()];
  current_hour = now.hour() % 12; if (current_hour == 0) current_hour = 12;
  current_min = now.minute();
  current_sec = now.second();
  ampm = (now.hour() >= 12) ? "PM" : "AM";
  daysInMonth = getDaysInMonth(current_month, current_year);
  startDay = getStartDay(current_year, current_month);
}

// ==== Display ====

void show_day(const char* day) {
  videodisplay.setFont(CodePage437_8x19);
  videodisplay.setCursor(0, 15);
  videodisplay.setTextColor(videodisplay.RGB(255,255,255));
  videodisplay.print(day);
}

void show_time(int hh, int mm, int ss, const char* ampm) {
  videodisplay.setFont(CodePage437_8x19);
  
  // Define the rectangle area to clear time
  int x = 70;
  int y = 15;
  int w = 100;
  int h = 19;

  // Clear the area by filling a rectangle with background color
  videodisplay.fillRect(x, y, w, h, videodisplay.RGB(0, 0, 0));

  // Draw the time
  videodisplay.setCursor(x, y);
  char buf[16];
  sprintf(buf, "- %02d:%02d:%02d %s", hh, mm, ss, ampm);
  videodisplay.setTextColor(videodisplay.RGB(255,255,255));
  videodisplay.print(buf);
}

void show_month_year(const char* month, int year) {
  videodisplay.setFont(CodePage437_8x19);
  videodisplay.setCursor(0, 47);
  char buf[24];
  sprintf(buf, "%d %s, %d", current_date, month, year);  // Format: 31 July, 2025
  videodisplay.setTextColor(videodisplay.RGB(255,255,255));
  videodisplay.println(buf);
}

void show_weekdays(const char* days[7]) {
  videodisplay.setFont(CodePage437_8x16);
  videodisplay.setCursor(0, 75);
  for (int i = 0; i < 7; i++) {
    if (i == 4 || i == 5)
      videodisplay.setTextColor(videodisplay.RGB(255,0,0), videodisplay.RGB(0,0,0));
    else
      videodisplay.setTextColor(videodisplay.RGB(255,255,255), videodisplay.RGB(0,0,0));
    videodisplay.print(days[i]); videodisplay.print(" ");
  }
  videodisplay.println();
}

bool isHoliday(int date) {
  for (int i = 0; i < totalHolidays; i++)
    if (holidayList[i].month == current_month && holidayList[i].day == date) return true;

  int dow = (startDay + date - 1) % 7;
  return (dow == 4 || dow == 5); // Thu, Fri
}

void drawDate(int date, bool visible, int offset) {
  videodisplay.setFont(CodePage437_8x16);
  int x = ((date - 1 + offset) % 7) * 3;
  int y = 6 + ((date - 1 + offset) / 7); 

  videodisplay.setCursor(x * 8, y * 16); 
  char buf[4]; sprintf(buf, date < 10 ? " %d" : "%d", date);

  if (!visible)
    videodisplay.setTextColor(videodisplay.RGB(0,0,0), videodisplay.RGB(0,0,0));
  else if (date == current_date)
    videodisplay.setTextColor(videodisplay.RGB(0,255,255), videodisplay.RGB(0,0,0));
  else if (isHoliday(date))
    videodisplay.setTextColor(videodisplay.RGB(255,0,0), videodisplay.RGB(0,0,0));
  else
    videodisplay.setTextColor(videodisplay.RGB(255,255,255), videodisplay.RGB(0,0,0));

  videodisplay.print(buf);
}


void show_date(int current_date, int startDayOffset, int daysInMonth) {
  for (int date = 1; date <= daysInMonth; date++) {
    if (date == current_date) continue;
    drawDate(date, true, startDayOffset);
  }
}

void show_holiday_list(int currMonth) {
  videodisplay.setFont(CodePage437_8x8);
  const int fontW = 8;
  const int fontH = 8;
  const int spacing = 2;
  const int x = 184;
  const int maxWidth = 136; 
  int y = 5;

  auto printWrapped = [&](const char* text) {
    int lineLen = 0;
    const char* ptr = text;

    videodisplay.setCursor(x, y);
    while (*ptr) {
      if (lineLen >= maxWidth / fontW || *ptr == '\n') {
        y += fontH + spacing;
        videodisplay.setCursor(x, y);
        lineLen = 0;
      }
      videodisplay.print(*ptr++);
      lineLen++;
    }
    y += fontH + spacing;  // prepare for next entry
  };

  // Title block
  videodisplay.setTextColor(videodisplay.RGB(255, 0, 0));
  printWrapped("----------------");
  printWrapped(" Holidays/Events");
  printWrapped("   This Month   ");
  printWrapped("----------------");

  // List holidays
  for (int i = 0; i < totalHolidays; i++) {
    if (holidayList[i].month == current_month) {
      char buf[64];
      snprintf(buf, sizeof(buf), "%d - %s", holidayList[i].day, holidayList[i].name);
      videodisplay.setTextColor(videodisplay.RGB(255, 255, 255));
      printWrapped(buf);
    }
  }
}


// ==== Setup & Loop ====
void setup() {
  Serial.begin(115200);
  Wire.begin(sda, scl);
  rtc.begin();
  if (rtc.lostPower()) rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  videodisplay.init(VGAMode::MODE320x200, redPin, greenPin, bluePin, hsyncPin, vsyncPin);
  videodisplay.line(180, 0, 180, 199, videodisplay.RGB(255, 255, 255));

  WiFi.softAP(ssid, password);
  server.on("/", handleRoot);
  server.on("/submit", HTTP_POST, handleSubmit);
  server.begin();
  Serial.println("Web UI ready.");
}

void loop() {
  server.handleClient();
  updateDateTime();

  if (current_sec != prevSec) {
    show_day(current_day_str);
    show_time(current_hour, current_min, current_sec, ampm);
    prevSec = current_sec;
  }

  if (current_date != prevDate || current_month != prevMonth || needRedraw) {
    videodisplay.clear(videodisplay.RGB(0, 0, 0));
    videodisplay.line(180, 0, 180, 199, videodisplay.RGB(255, 255, 255));

    show_day(current_day_str);
    show_time(current_hour, current_min, current_sec, ampm);
    show_month_year(months[current_month], current_year);

    const char* days[7] = {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};
    show_weekdays(days);
    show_date(current_date, startDay, daysInMonth);
    show_holiday_list(current_month);

    prevDate = current_date;
    prevMonth = current_month;
    needRedraw = false;
  }

  if (millis() - lastToggle >= blinkInterval) {
    showDate = !showDate;
    drawDate(current_date, showDate, startDay);
    lastToggle = millis();
  }
}
