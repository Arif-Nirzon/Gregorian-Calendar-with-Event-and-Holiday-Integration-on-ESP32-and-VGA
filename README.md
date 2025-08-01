# Gregorian Month-view Calendar with Event and Holiday Integration on ESP32 and VGA
This was a part of our EEE 416 (Microprocessor and Embedded Systems Laboratory) project.
## Features
1. Real-time date and time display using DS3231 RTC
2. Weekends and Holidays are highlighted red, current date highlighted in cyan.
3. Web Interface for adding calendar events
4. VGA outout to a 720p monitor (320x240 @ 60Hz, 3bit color)
5. Two separate sections for calendar and holiday/event titles of a specific month.
## Libraries required
1. Bitluni's ESP32 VGA library: https://github.com/bitluni/ESP32Lib
2. RTC library

Both of the libraries can be found inside Arduino IDE's library section.
## Demonstration Videos
### Final Calendar
https://github.com/user-attachments/assets/e812fd1a-64ca-4ed8-913e-1c3c50d59489
### Adding events from web
1. Connect to the ESP32 accesspoint using the credentials inside the code
2. Use web/curl/requests to send http request to ESP32 using its IP.

https://github.com/user-attachments/assets/e314b82c-2355-466f-a74f-3f678c55acb9
### Cycling through the months keeping everything static
https://github.com/user-attachments/assets/45a917b6-e1ea-4f67-8c97-f9f793347139
## Issues
1. The holidays are hardcoded. Using Google calendar's API to syncrhonously update holidays can be an option.
2. The events added through web are stored in voltatile memory and will be deleted with restart. External SD card can be used to store the events and holidays.
3. The maximum resolution we got on a 720p monitor is 320x240. For a 1080p monitor however, the maximum we got is 200x150. So it is adviced to use lower resolution monitor.

