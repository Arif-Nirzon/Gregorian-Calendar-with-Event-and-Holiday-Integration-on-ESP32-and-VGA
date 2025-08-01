#include <ESP32Video.h>
#include <Wire.h>

// Fonts
#include <Ressources/CodePage437_8x8.h>
#include <Ressources/CodePage437_8x14.h>
#include <Ressources/CodePage437_8x16.h>
#include <Ressources/CodePage437_8x19.h>
#include <Ressources/Font6x8.h>

// VGA Pins
const int redPin = 25, greenPin = 14, bluePin = 13, hsyncPin = 32, vsyncPin = 33;
VGA3Bit videodisplay;

// Date-Time Globals
const char* months[] = {"Invalid", "January", "February", "March", "April", "May", "June",
                        "July", "August", "September", "October", "November", "December"};
const char* daysOfWeek[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

int current_date = 15;
int current_month = 1;
int current_year = 2025;

int current_hour = 10;
int current_min = 30;
int current_sec = 0;
const char* ampm = "AM";
const char* current_day_str = "Wednesday";  // Fake static value

int startDay = 0, daysInMonth = 30;
bool showDate = true;
unsigned long lastToggle = 0;
const int blinkInterval = 500;
unsigned long lastMonthSwitch = 0;
const unsigned long monthSwitchInterval = 3000;  // 3 seconds
bool needRedraw = true;

// ==== Holiday Data ====
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
      if (holidayList[i].month > holidayList[j].month ||
         (holidayList[i].month == holidayList[j].month && holidayList[i].day > holidayList[j].day)) {
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
    y += fontH + spacing;
  };

  videodisplay.setTextColor(videodisplay.RGB(255, 0, 0));
  printWrapped("----------------");
  printWrapped(" Holidays/Events");
  printWrapped("   This Month   ");
  printWrapped("----------------");

  for (int i = 0; i < totalHolidays; i++) {
    if (holidayList[i].month == current_month) {
      char buf[64];
      snprintf(buf, sizeof(buf), "%d - %s", holidayList[i].day, holidayList[i].name);
      videodisplay.setTextColor(videodisplay.RGB(255, 255, 255));
      printWrapped(buf);
    }
  }
}

void show_day(const char* day) {
  videodisplay.setFont(CodePage437_8x19);
  videodisplay.setCursor(0, 15);
  videodisplay.setTextColor(videodisplay.RGB(255,255,255));
  videodisplay.print(day);
}

void show_time(int hh, int mm, int ss, const char* ampm) {
  videodisplay.setFont(CodePage437_8x19);
  int x = 70;
  int y = 15;
  int w = 100;
  int h = 19;
  videodisplay.fillRect(x, y, w, h, videodisplay.RGB(0, 0, 0));
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
  sprintf(buf, "%d %s, %d", current_date, month, year);
  videodisplay.setTextColor(videodisplay.RGB(255,255,255));
  videodisplay.println(buf);
}

void updateCalendar() {
  daysInMonth = getDaysInMonth(current_month, current_year);
  startDay = getStartDay(current_year, current_month);
}

void setup() {
  Serial.begin(115200);
  videodisplay.init(VGAMode::MODE320x200, redPin, greenPin, bluePin, hsyncPin, vsyncPin);
  videodisplay.line(180, 0, 180, 199, videodisplay.RGB(255, 255, 255));
  sortHolidays();
  updateCalendar();
}

void loop() {
  if (millis() - lastMonthSwitch >= monthSwitchInterval) {
    current_month++;
    if (current_month > 12) current_month = 1;
    updateCalendar();
    needRedraw = true;
    lastMonthSwitch = millis();
  }

  if (needRedraw) {
    videodisplay.clear(videodisplay.RGB(0, 0, 0));
    videodisplay.line(180, 0, 180, 199, videodisplay.RGB(255, 255, 255));

    show_day(current_day_str);
    show_time(current_hour, current_min, current_sec, ampm);
    show_month_year(months[current_month], current_year);

    const char* days[7] = {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};
    show_weekdays(days);
    show_date(current_date, startDay, daysInMonth);
    show_holiday_list(current_month);

    needRedraw = false;
  }

  if (millis() - lastToggle >= blinkInterval) {
    showDate = !showDate;
    drawDate(current_date, showDate, startDay);
    lastToggle = millis();
  }
}
