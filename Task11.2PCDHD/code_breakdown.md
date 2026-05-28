# Smart Medication Dispenser - Full Code Explanation


This document explains the full smart medication dispenser program.

The final program is a non-blocking ESP32-based smart medication reminder system. It uses:

- A DS3231 RTC for timekeeping
- NTP over Wi-Fi to synchronise the RTC
- An automatic daily reminder at 5:00 PM
- A manual button override to start a reminder
- A double-press refill mode
- A servo-controlled lid
- An HX711 and load cell to detect whether a dose was removed
- An OLED screen for user messages
- An RGB LED for status indication
- A buzzer for sound feedback
- ThingSpeak cloud logging
- IFTTT missed-dose Telegram alerts
- A state machine to control the system without long blocking delays

The main design idea is that the program repeatedly calls small update functions inside loop(). Each update function checks whether it needs to do anything, performs a small action if needed, and then returns. This allows the system to keep responding to the button, servo, buzzer, load cell, RTC, and network events at the same time.


## 1. Library includes


```cpp
#include <Wire.h>
```

This includes the Arduino I2C library. The OLED screen and the DS3231 RTC both use I2C communication, so this library is required.

```cpp
#include <Adafruit_GFX.h>
```

This includes the Adafruit graphics library. The SSD1306 OLED library depends on it for drawing text.

```cpp
#include <Adafruit_SSD1306.h>
```

This includes the library for the SSD1306 OLED display.

```cpp
#include <WiFi.h>
```

This includes Wi-Fi support for the ESP32 board. The system uses Wi-Fi for NTP time synchronisation, ThingSpeak logging, and IFTTT alerts.

```cpp
#include <time.h>
```

This includes time functions used by the ESP32 to get internet time from NTP servers.

```cpp
#include <HTTPClient.h>
```

This includes the HTTP client library. The program uses HTTP requests to send data to ThingSpeak and IFTTT.

```cpp
#include "Adafruit_HX711.h"
```

This includes the HX711 load-cell amplifier library. The HX711 reads the load cell used to detect medication removal.

```cpp
#include <ESP32Servo.h>
```

This includes the ESP32 servo library. It is used to control the servo motor that opens and closes the medication dispenser lid.

```cpp
#include "secrets.h"
```

This includes private configuration values such as Wi-Fi name, Wi-Fi password, ThingSpeak API key, IFTTT event name, and IFTTT key.


## 2. Pin definitions


```cpp
#define HX711_DOUT D0
```

This defines the data output pin for the HX711 module.

```cpp
#define HX711_SCK D1
```

This defines the clock pin for the HX711 module.

```cpp
#define BUTTON_PIN D2
```

This defines the push button pin. The button is used for manual reminder override and refill mode.

```cpp
#define BUZZER_PIN D3
```

This defines the buzzer pin.

```cpp
#define SDA_PIN D4
```

This defines the I2C SDA data pin used by the OLED and RTC.

```cpp
#define SCL_PIN D5
```

This defines the I2C SCL clock pin used by the OLED and RTC.

```cpp
#define RGB_RED_PIN D6
```

This defines the red pin of the RGB LED.

```cpp
#define RGB_GREEN_PIN D7
```

This defines the green pin of the RGB LED.

```cpp
#define RGB_BLUE_PIN D8
```

This defines the blue pin of the RGB LED.

```cpp
#define SERVO_PIN D9
```

This defines the servo control pin.


## 3. OLED display variables


```cpp
#define SCREEN_WIDTH 128
```

This sets the OLED display width to 128 pixels.

```cpp
#define SCREEN_HEIGHT 64
```

This sets the OLED display height to 64 pixels.

```cpp
#define OLED_RESET -1
```

This tells the OLED library that no separate reset pin is used.

```cpp
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
```

This creates the OLED display object. It uses the screen size values, the I2C bus, and the reset setting.

```cpp
bool oledOk = false;
```

This records whether the OLED was detected successfully. If the OLED is not found, the rest of the medication dispenser program can still continue running.


## 4. Wi-Fi and cloud variables


```cpp
char ssid[] = SECRET_SSID;
```

This loads the Wi-Fi network name from secrets.h.

```cpp
char pass[] = SECRET_PASS;
```

This loads the Wi-Fi password from secrets.h.

```cpp
const unsigned long WIFI_CONNECT_TIMEOUT_MS = 15000;
```

This sets the maximum time allowed for a Wi-Fi connection attempt to 15 seconds.

```cpp
const unsigned long WIFI_RETRY_INTERVAL_MS = 30000;
```

This sets the time between Wi-Fi retry attempts to 30 seconds.

```cpp
const unsigned long NETWORK_RETRY_INTERVAL_MS = 30000;
```

This sets the retry interval for failed cloud events (ThingSpeak or IFTTT requests).

```cpp
const unsigned long HTTP_TIMEOUT_MS = 2500;
```

This sets the timeout for HTTP requests. This prevents web requests from hanging for too long.

```cpp
bool wifiConnecting = false;
```

This records whether the ESP32 is currently trying to connect to Wi-Fi.

```cpp
unsigned long wifiConnectStartTime = 0;
```

This records when the current Wi-Fi connection attempt started.

```cpp
unsigned long nextWifiAttemptTime = 0;
```

This records when the next Wi-Fi connection attempt is allowed.

This Wi-Fi design is non-blocking in the sense that the program does not sit forever waiting for Wi-Fi. It starts a connection attempt, checks on it during later loop cycles, and retries later if it fails.


## 5. RTC and NTP variables


```cpp
#define DS3231_ADDR 0x68
```

This defines the I2C address of the DS3231 RTC module. 0x68 is the standard address for the DS3231.

```cpp
bool rtcOk = false;
```

This records whether the RTC module was detected.

```cpp
const long GMT_OFFSET_SEC = 8 * 60 * 60;
```

This sets the timezone offset to UTC+8, which matches Perth / Western Australia time.

```cpp
const int DAYLIGHT_OFFSET_SEC = 0;
```

This sets daylight saving to zero because Western Australia does not use daylight saving (included because Deakin is based in VIC).

```cpp
bool ntpConfigured = false;
```

This records whether the ESP32 NTP time system has been configured.

```cpp
bool rtcSyncedFromNtp = false;
```

This records whether the DS3231 RTC has already been synchronised from NTP during this program run.

```cpp
unsigned long lastNtpCheckTime = 0;
```

This records the last time the program checked whether NTP time was available.

```cpp
const unsigned long NTP_CHECK_INTERVAL_MS = 1000;
```

This means the program checks for NTP time once per second while waiting for valid time.


## 6. Automatic daily reminder variables


```cpp
const byte AUTO_REMINDER_HOUR = 17;
```

This sets the automatic reminder hour to 17 (5PM in 24-hour time).

```cpp
const byte AUTO_REMINDER_MINUTE = 0;
```

This sets the automatic reminder minute to 0, giving a scheduled time of 17:00.

```cpp
int lastAutoReminderYear = -1;
int lastAutoReminderMonth = -1;
int lastAutoReminderDay = -1;
```

These variables record the date when the automatic reminder last triggered. They prevent the system from repeatedly opening the lid throughout the entire 5:00PM minute.
Because the loop runs many times per second, without these date checks, the system could see that the time is 17:00 repeatedly and keep trying to start the reminder. These variables ensure the automatic reminder only triggers once per day.


## 7. HX711 and load-cell variables


```cpp
Adafruit_HX711 hx711(HX711_DOUT, HX711_SCK);
```

This creates the HX711 object using the data and clock pins.

```cpp
const int32_t WEIGHT_REDUCTION_THRESHOLD = 1000;
```

This is the minimum raw weight change required before the system considers that a dose may have been removed.

```cpp
const int32_t MAX_REASONABLE_REDUCTION = 80000;
```

This is the maximum expected valid weight change. A value larger than this is ignored because it is probably caused by handling the box, pressing on the scale, or other interference.

```cpp
const int32_t STABLE_WEIGHT_TOLERANCE = 1200;
```

This defines how much a weight reading can vary while still being considered stable (there can be some drift because the load cell is so sensitive).

```cpp
const unsigned long STABLE_WEIGHT_TIME_MS = 2500;
```

The weight must remain stable for 2.5 seconds before it is accepted.

```cpp
const unsigned long MAX_STABILITY_WAIT_MS = 12000;
```

This is the maximum time the program will wait for a stable weight reading. If the reading does not stabilise in this time, the latest reading is used.

```cpp
const unsigned long HX711_SAMPLE_INTERVAL_MS = 50;
```

This means the load cell is sampled every 50 milliseconds.

```cpp
const int WEIGHT_AVG_WINDOW = 5;
```

This means the program averages the last five HX711 readings to reduce noise.

```cpp
int32_t weightZeroOffset = 0;
```

This stores the raw value that represents zero weight after taring.

```cpp
int32_t reminderBaseline = 0;
```

This stores the baseline weight at the start of a reminder. The system compares later weight readings against this baseline to detect a removed dose.

```cpp
int32_t weightSamples[WEIGHT_AVG_WINDOW];
```

This array stores the most recent raw HX711 readings.

```cpp
int weightSampleIndex = 0;
```

This tracks where the next load-cell sample should be placed inside the array.

```cpp
int weightSampleCount = 0;
```

This tracks how many valid samples have been collected so far.

```cpp
int64_t weightSampleTotal = 0;
```

This stores the running total of the readings in the sample window, to use for averaging.

```cpp
bool latestWeightValid = false;
```

This is false until at least one valid HX711 sample has been read.

```cpp
int32_t latestRawWeightAverage = 0;
```

This stores the latest averaged raw HX711 value.

```cpp
int32_t latestZeroedWeight = 0;
```

This stores the latest averaged reading after subtracting the tare offset.

```cpp
unsigned long lastHx711SampleTime = 0;
```

This records when the last HX711 sample was taken.


## 8. Stability task variables


```cpp
bool stabilityActive = false;
```

This records whether the program is currently waiting for a stable weight reading.

```cpp
bool stabilityResultReady = false;
```

This becomes true once a stable reading has been found.

```cpp
unsigned long stabilityStartTime = 0;
```

This records when the current stability check started.

```cpp
unsigned long stableStartTime = 0;
```

This records when the reading first became stable.

```cpp
unsigned long lastStabilityCheckTime = 0;
```

This records when the stability task last compared readings.

```cpp
int32_t stabilityLastReading = 0;
```

This stores the previous weight reading used for comparison.

```cpp
int32_t stabilityResultRaw = 0;
```

This stores the final stable raw weight value.

```cpp
const unsigned long STABILITY_CHECK_INTERVAL_MS = 250;
```

This means the stability task checks the reading every 250 milliseconds.

The stability system is used whenever the program needs a reliable weight reading, such as startup tare, reminder baseline, final dose confirmation, and after refill.


## 9. Servo variables


```cpp
const int SERVO_CLOSED_ANGLE = 10;
```

This is the servo angle used when the lid is closed.

```cpp
const int SERVO_OPEN_ANGLE = 150;
```

This is the servo angle used when the lid is open.

```cpp
const unsigned long SERVO_STEP_INTERVAL_MS = 15;
```

This sets the time between servo movement steps, so that it doesn't open too quickly.

```cpp
Servo lidServo;
```

This creates the servo object.

```cpp
int currentServoAngle = SERVO_CLOSED_ANGLE;
```

This stores the servo's current angle. It starts at the closed position.

```cpp
int targetServoAngle = SERVO_CLOSED_ANGLE;
```

This stores the angle the servo should move toward.

```cpp
bool servoMoving = false;
```

This records whether the servo is currently moving.

```cpp
unsigned long lastServoStepTime = 0;
```

This records when the last servo step occurred.

The servo is moved gradually without a blocking for loop. The updateServo() function moves the servo one degree at a time when enough time has passed.


## 10. Timing variables


```cpp
const unsigned long REMINDER_TIMEOUT_MS = 45000;
```

This gives the user 45 seconds to remove a dose before it is considered missed (this timing is for demo purposes, a real-world solution would give the user more time).

```cpp
const unsigned long LID_SETTLE_AFTER_OPEN_MS = 3000;
```

After the lid opens, the system waits 3 seconds before taking the baseline weight. This lets the servo movement and load cell settle.

```cpp
const unsigned long IGNORE_WEIGHT_AFTER_BASELINE_MS = 3000;
```

After the baseline is taken, the program ignores weight changes for 3 seconds to avoid false detections.

```cpp
const unsigned long WEIGHT_CONFIRM_DELAY_MS = 5000;
```

After a possible dose removal is detected, the program waits 5 seconds before confirming it.

```cpp
const unsigned long LID_CLOSE_AFTER_TAKEN_MS = 8000;
```

After a dose is confirmed as taken, the lid stays open for 8 seconds before closing.

```cpp
const unsigned long DOUBLE_PRESS_WINDOW_MS = 500;
```

The second button press must happen within 500 milliseconds to count as a double press.

```cpp
unsigned long reminderStartTime = 0;
```

This records when the current reminder started.

```cpp
unsigned long baselineTakenTime = 0;
```

This records when the reminder baseline weight was taken.

```cpp
unsigned long possibleDoseTakenTime = 0;
```

This records when a possible dose removal was first detected.

```cpp
unsigned long stateStartTime = 0;
```

This records when the current state began.

```cpp
unsigned long firstButtonPressTime = 0;
```

This records when the first button press occurred while checking for single or double press.

```cpp
bool possibleDoseTaken = false;
```

This records whether the system has detected a possible dose removal.

```cpp
bool waitingForSecondPress = false;
```

This records whether the program is waiting to see if a single press becomes a double press.

```cpp
bool firstPressReleased = false;
```

This records whether the user released the button after the first press.


## 11. Button variables


```cpp
const bool BUTTON_ACTIVE_LOW = true;
```

This means the button is considered pressed when the pin reads LOW (INPUT_PULLUP wiring).

```cpp
const unsigned long BUTTON_DEBOUNCE_MS = 75;
```

This means a button reading must remain stable for 75 milliseconds before being accepted.

```cpp
bool buttonStablePressed = false;
```

This stores the debounced button state.

```cpp
bool lastButtonStablePressed = false;
```

This stores the previous debounced button state.

```cpp
bool lastButtonRawPressed = false;
```

This stores the previous raw button reading.

```cpp
bool buttonPressedEvent = false;
```

This becomes true for one loop cycle when a new press is detected.

```cpp
unsigned long lastButtonChangeTime = 0;
```

This records when the raw button reading last changed.

The button code prevents one physical press from being counted multiple times due to button bounce.


## 12. Buzzer variables


```cpp
const int MAX_BUZZER_STEPS = 10;
```

This sets the maximum number of on/off steps in a buzzer pattern.

```cpp
bool buzzerActive = false;
```

This records whether a buzzer pattern is currently playing.

```cpp
bool buzzerLevels[MAX_BUZZER_STEPS];
```

This stores whether each step of the buzzer pattern is on or off.

```cpp
unsigned long buzzerDurations[MAX_BUZZER_STEPS];
```

This stores how long each buzzer step should last.

```cpp
int buzzerStepCount = 0;
```

This stores the number of steps in the current pattern.

```cpp
int buzzerStepIndex = 0;
```

This stores which step of the pattern is currently active.

```cpp
unsigned long buzzerStepStartTime = 0;
```

This records when the current buzzer step started.

The buzzer system allows sound patterns without using delay().


## 13. Network event queue variables


```cpp
bool networkEventPending = false;
```

This records whether there is a cloud event waiting to be sent.

```cpp
int queuedDoseTakenValue = 0;
```

This stores the value to send to ThingSpeak. 1 means dose taken, 0 means dose missed.

```cpp
bool queuedSendMissedAlert = false;
```

This records whether an IFTTT missed-dose alert should be sent.

```cpp
String queuedStatusText = "";
```

This stores the status text to send to ThingSpeak, such as Dose_taken or Missed_dose.

```cpp
unsigned long nextNetworkAttemptTime = 0;
```

This records when the next cloud send attempt should occur.

The queue allows the system to record an event locally and retry the network upload later if Wi-Fi is not available.


## 14. State machine definition


```cpp
enum SystemState
```

This defines all the possible states the smart medication dispenser can be in.

```cpp
STATE_STARTUP_TARING
```

The system is taring the load cell during startup.

```cpp
STATE_READY
```

The system is idle and waiting for either the 5PM automatic reminder or button input.

```cpp
STATE_REFILL_OPENING
```

The refill mode has started and the lid is opening.

```cpp
STATE_REFILL_ACTIVE
```

The lid is open and the user can add tablets.

```cpp
STATE_REFILL_CLOSING
```

The refill mode is closing the lid.

```cpp
STATE_REFILL_TARING
```

The system is re-taring after refill.

```cpp
STATE_REMINDER_OPENING
```

The medication reminder has started and the lid is opening.

```cpp
STATE_REMINDER_SETTLING
```

The lid has opened and the system is waiting for the load cell to settle.

```cpp
STATE_REMINDER_BASELINE
```

The system is taking a stable baseline weight.

```cpp
STATE_REMINDER_MONITORING
```

The system is monitoring for a dose removal.

```cpp
STATE_REMINDER_CONFIRMING
```

The system is confirming that the detected weight change is stable and valid.

```cpp
STATE_DOSE_TAKEN_HOLD_OPEN
```

The dose was taken and the lid remains open briefly.

```cpp
STATE_DOSE_TAKEN_CLOSING
```

The lid is closing after the dose was taken.

```cpp
STATE_DOSE_TAKEN_TARING
```

The system is re-taring after the dose was removed.

```cpp
STATE_DOSE_MISSED_CLOSING
```

The dose was missed and the lid is closing.

```cpp
SystemState state = STATE_STARTUP_TARING;
```

This creates the current state variable and starts the program in startup taring mode.

The state machine is the main control structure of the program. It ensures the medication dispenser always knows exactly what step it is currently performing.


## 15. Function prototypes


```cpp
void oled_show_message(...);
```

This tells the compiler that the OLED message function exists later in the file.

```cpp
void oled_show_ready(int32_t weight);
```

This tells the compiler that the ready screen function exists later.

```cpp
void oled_show_reminder();
```

This tells the compiler that the reminder screen function exists later.

```cpp
void updateAutomaticReminder();
```

This tells the compiler that the automatic 5PM reminder update function exists later.

Function prototypes are used because some functions call other functions before their full definitions appear in the code, this is to prevent errors during compiling.


## 16. enterState()


```cpp
void enterState(SystemState newState)
```

This function changes the current state.

```cpp
state = newState;
```

This updates the global state variable.

```cpp
stateStartTime = millis();
```

This records when the new state started. Other parts of the code use this to check how long the system has been in that state.

This helper makes the state machine cleaner because every state change automatically records its start time.


## 17. RTC helper functions


```cpp
byte bcdToDec(byte value)
```

The DS3231 stores time values in BCD format. This function converts a BCD value into normal decimal format.

```cpp
return ((value >> 4) * 10) + (value & 0x0F);
```

This extracts the tens digit and ones digit from the BCD value and combines them into a normal decimal number.

```cpp
byte decToBcd(byte value)
```

This converts a normal decimal number into BCD format for writing to the RTC.

```cpp
return ((value / 10) << 4) | (value % 10);
```

This splits the decimal value into tens and ones and stores them in BCD format.

```cpp
bool rtcDetected()
```

This checks whether the DS3231 RTC is present on the I2C bus.

```cpp
Wire.beginTransmission(DS3231_ADDR);
```

This starts communication with the RTC address.

```cpp
return Wire.endTransmission() == 0;
```

If the transmission succeeds, the function returns true.

```cpp
void setDS3231Time(...)
```

This writes a new time and date into the DS3231 RTC.

```cpp
Wire.beginTransmission(DS3231_ADDR);
```

This starts communication with the RTC.

```cpp
Wire.write(0x00);
```

This tells the RTC that writing should begin at register 0, where the time registers start.

```cpp
Wire.write(decToBcd(...));
```

Each time/date value is converted to BCD and written to the RTC.

```cpp
Wire.endTransmission();
```

This ends the I2C write operation.

```cpp
bool readDS3231Time(...)
```

This reads the current time and date from the DS3231 RTC.

```cpp
Wire.beginTransmission(DS3231_ADDR);
Wire.write(0x00);
```

This tells the RTC to start reading from register 0.

```cpp
Wire.endTransmission(false)
```

This ends the write part of the I2C transaction while keeping the connection active for reading.

```cpp
Wire.requestFrom(DS3231_ADDR, (uint8_t)7)
```

This requests seven bytes from the RTC: seconds, minutes, hours, day of week, day of month, month, and year.

The function converts the BCD values into normal decimal values and returns true if the read succeeded.

```cpp
String getRtcTimeString()
```

This returns the RTC time as a readable string.

```cpp
if (!rtcOk)
```

If the RTC was not detected, it returns "RTC unavailable".

```cpp
if (!readDS3231Time(...))
```

If the RTC read fails, it returns "RTC read failed".

```cpp
snprintf(...)
```

This formats the date and time into a string (eg. 2026-05-28 17:00:00)

```cpp
return String(buffer);
```

This returns the formatted timestamp.


## 18. updateNtpSync()


```cpp
void updateNtpSync()
```

This function updates the RTC from internet time when Wi-Fi is available.

```cpp
if (!rtcOk || rtcSyncedFromNtp)
```

If there is no RTC, or the RTC has already been synced, the function exits.

```cpp
if (WiFi.status() != WL_CONNECTED)
```

If Wi-Fi is not connected, the function exits.

```cpp
if (!ntpConfigured)
```

If NTP has not been configured yet, the program calls configTime().

```cpp
configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, "pool.ntp.org", "time.nist.gov");
```

This configures internet time using NTP servers and the timezone offset.

```cpp
ntpConfigured = true;
```

This records that NTP has been configured.

```cpp
lastNtpCheckTime = millis();
```

This records when the NTP check started.

```cpp
oled_show_message("WiFi connected", "Getting time...", "", "");
```

This shows a message on the OLED.

```cpp
if (millis() - lastNtpCheckTime < NTP_CHECK_INTERVAL_MS)
```

This prevents checking NTP too frequently.

```cpp
time_t now = time(nullptr);
```

This gets the current time from the ESP32 time system.

```cpp
if (now < 1700000000)
```

This checks whether the time looks valid. Very small timestamps usually mean NTP has not synced yet.

```cpp
getLocalTime(&timeInfo, 10)
```

This tries to get local time with a short timeout.

The function then extracts year, month, day, hour, minute, and second from the internet time.

```cpp
setDS3231Time(...)
```

This writes the internet time into the RTC.

```cpp
rtcSyncedFromNtp = true;
```

This records that the RTC has been synced from NTP.

```cpp
oled_show_message("Time sync OK", getRtcTimeString(), "", "");
```

This shows the successful time sync on the OLED.


## 19. updateAutomaticReminder()


```cpp
void updateAutomaticReminder()
```

This function checks whether it is time to automatically start the daily medication reminder.

```cpp
if (!rtcOk)
```

If the RTC is not available, automatic scheduling cannot work, so the function exits.

```cpp
if (state != STATE_READY)
```

The automatic reminder only triggers when the system is in the ready state. This prevents it from interrupting refill mode, dose confirmation, or another active reminder.

```cpp
readDS3231Time(...)
```

This reads the current time from the RTC.

alreadyTriggeredToday

This checks whether the reminder has already triggered on the current date.

```cpp
if (alreadyTriggeredToday)
```

If the reminder already triggered today, the function exits.

```cpp
if (hour == AUTO_REMINDER_HOUR && minute == AUTO_REMINDER_MINUTE)
```

This checks whether the RTC time is 5:00PM.

```cpp
lastAutoReminderYear = year;
lastAutoReminderMonth = month;
lastAutoReminderDay = dayOfMonth;
```

These lines record that the automatic reminder has triggered today.

```cpp
Serial.println("Automatic 5PM medication reminder triggered.");
```

This prints a debug message.

```cpp
startReminder();
```

This starts the same reminder process used by the manual button override. This means the automatic reminder and manual reminder use the same lid-opening, weight-monitoring, cloud-logging, and missed-dose logic.


## 20. OLED functions


```cpp
void setup_oled()
```

This starts the OLED display.

```cpp
display.begin(SSD1306_SWITCHCAPVCC, 0x3C)
```

This tries to start the display at I2C address 0x3C.

```cpp
if successful:
oledOk = true;
```

This records that the OLED is working.

```cpp
if unsuccessful:
oledOk = false;
```

The program continues without the OLED.

```cpp
display.clearDisplay();
```

This clears the screen.

```cpp
display.setTextSize(1);
```

This sets the text size.

```cpp
display.setTextColor(SSD1306_WHITE);
```

This sets the text colour.

```cpp
display.display();
```

This updates the physical OLED screen.

```cpp
void oled_show_message(...)
```

This is a general function for showing up to four lines of text on the OLED.

```cpp
if (!oledOk)
```

If the OLED is not working, the function exits.

```cpp
display.clearDisplay();
```

This clears the screen.

```cpp
display.setCursor(...)
```

These lines position the text cursor.

```cpp
display.println(...)
```

These lines print each message line.

```cpp
display.display();
```

This sends the updated text to the OLED.

```cpp
void oled_show_ready(int32_t weight)
```

This displays the normal ready screen.

It shows:

- Smart Medication Dispenser
- Status: Ready
- Current weight reading
- Current RTC time

```cpp
void oled_show_reminder()
```

This displays the medication reminder screen.

It shows:

- Medication Time
- Please take dose
- Lid is open
- Monitoring...


## 21. Wi-Fi functions


```cpp
void startWifiConnect()
```

This starts a Wi-Fi connection attempt if the ESP32 is not already connected or already trying.

```cpp
if (WiFi.status() == WL_CONNECTED)
```

If Wi-Fi is already connected, it exits.

```cpp
if (wifiConnecting)
```

If a connection attempt is already in progress, it exits.

```cpp
if (millis() < nextWifiAttemptTime)
```

If it is too early to retry, it exits.

```cpp
WiFi.mode(WIFI_STA);
```

This sets the ESP32 to station mode, meaning it connects to an existing Wi-Fi network.

```cpp
WiFi.begin(ssid, pass);
```

This starts the connection attempt.

```cpp
wifiConnecting = true;
```

This records that a connection attempt is active.

```cpp
wifiConnectStartTime = millis();
```

This records when the attempt started.

```cpp
void updateWifi()
```

This checks Wi-Fi progress.

```cpp
if (WiFi.status() == WL_CONNECTED)
```

If Wi-Fi is connected, the program clears the connecting flag and prints connection details.

```cpp
if (!wifiConnecting)
```

If not connected and not currently trying, it starts a new connection attempt.

```cpp
if (millis() - wifiConnectStartTime >= WIFI_CONNECT_TIMEOUT_MS)
```

If the attempt has lasted longer than 15 seconds, it is treated as failed.

```cpp
WiFi.disconnect();
```

This cancels the failed attempt.

```cpp
nextWifiAttemptTime = millis() + WIFI_RETRY_INTERVAL_MS;
```

This schedules the next retry.


## 22. ThingSpeak function


```cpp
bool sendThingSpeakNow(int doseTakenValue, const String& statusText)
```

This sends a medication result to ThingSpeak immediately.

```cpp
if (WiFi.status() != WL_CONNECTED)
```

If Wi-Fi is unavailable, the function returns false.

```cpp
String url = "http://api.thingspeak.com/update?api_key=";
```

This starts building the ThingSpeak update URL.

```cpp
url += SECRET_WRITE_APIKEY;
```

This adds the ThingSpeak write API key.

```cpp
url += "&field1=";
url += String(doseTakenValue);
```

This adds the dose result to field 1.

The values are:

1 = dose taken
0 = dose missed

```cpp
url += "&status=";
url += statusText;
```

This adds the status text, such as Dose_taken or Missed_dose.

```cpp
HTTPClient http;
```

This creates an HTTP client object.

```cpp
http.setTimeout(HTTP_TIMEOUT_MS);
```

This sets the HTTP timeout.

```cpp
http.begin(url);
```

This prepares the HTTP GET request.

```cpp
int responseCode = http.GET();
```

This sends the request to ThingSpeak.

```cpp
http.end();
```

This closes the HTTP connection.

```cpp
return responseCode == 200;
```

A response code of 200 means the request was successful.


## 23. IFTTT missed-dose function


```cpp
bool sendIftttMissedDoseAlertNow()
```

This sends an IFTTT Webhooks request when a dose is missed.

```cpp
if (WiFi.status() != WL_CONNECTED)
```

If Wi-Fi is unavailable, the function returns false.

```cpp
String timestamp = getRtcTimeString();
```

This gets the current RTC time for the alert.

The URL is built using:

- IFTTT event name
- IFTTT Webhooks key

The JSON payload includes:

value1 = Linda's Medication
value2 = timestamp
value3 = Dose MISSED

```cpp
HTTPClient http;
```

This creates the HTTP client.

```cpp
http.setTimeout(HTTP_TIMEOUT_MS);
```

This limits how long the request can wait.

```cpp
http.begin(url);
```

This prepares the IFTTT request.

```cpp
http.addHeader("Content-Type", "application/json");
```

This tells IFTTT that the request body is JSON.

```cpp
int responseCode = http.POST(payload);
```

This sends the missed-dose alert.

```cpp
return responseCode > 0 && responseCode < 400;
```

This returns true if the HTTP response indicates success.


## 24. Network queue functions


```cpp
void queueNetworkEvent(int doseTakenValue, const String& statusText, bool sendMissedAlert)
```

This stores a cloud event to be sent later.

```cpp
queuedDoseTakenValue = doseTakenValue;
```

This stores the ThingSpeak field value.

```cpp
queuedStatusText = statusText;
```

This stores the status text.

```cpp
queuedSendMissedAlert = sendMissedAlert;
```

This stores whether an IFTTT alert is needed.

```cpp
networkEventPending = true;
```

This marks that there is an event waiting to send.

```cpp
nextNetworkAttemptTime = millis();
```

This allows the program to try sending immediately.

```cpp
void updateNetworkQueue()
```

This checks whether there is a queued network event and tries to send it.

```cpp
if (!networkEventPending)
```

If there is nothing waiting, it exits.

```cpp
if (millis() < nextNetworkAttemptTime)
```

If it is not time to retry yet, it exits.

```cpp
if (WiFi.status() != WL_CONNECTED)
```

If Wi-Fi is not connected, it starts Wi-Fi connection and schedules another attempt later.

```cpp
sendThingSpeakNow(...)
```

This sends the ThingSpeak update.

```cpp
if (queuedSendMissedAlert)
```

If needed, it also sends the IFTTT alert.

```cpp
if (thingSpeakOk && iftttOk)
```

If all required cloud sends succeeded, the event is cleared. Otherwise, the event remains queued and will be retried later.


## 25. RGB LED functions


```cpp
void setRgbPwm(int redValue, int greenValue, int blueValue)
```

This writes PWM values to the red, green, and blue LED pins.

```cpp
analogWrite(RGB_RED_PIN, redValue);
analogWrite(RGB_GREEN_PIN, greenValue);
analogWrite(RGB_BLUE_PIN, blueValue);
```

These lines set the brightness of each LED colour.

```cpp
void setReadyLight()
```

Sets the LED to green.

```cpp
void setReminderLight()
```

Sets the LED to purple.

```cpp
void setRefillLight()
```

Sets the LED to yellow.

```cpp
void setTakenLight()
```

Sets the LED to green.

```cpp
void setMissedLight()
```

Sets the LED to red.

```cpp
void setLedOff()
```

Turns the RGB LED off.


## 26. Buzzer functions


```cpp
void startBuzzerPattern(const bool levels[], const unsigned long durations[], int count)
```

This starts a non-blocking buzzer pattern.

levels[] stores whether each step is ON or OFF.
durations[] stores how long each step lasts.
count stores how many steps are in the pattern.

```cpp
if (count <= 0)
```

If the pattern has no steps, the function exits.

```cpp
if (count > MAX_BUZZER_STEPS)
```

If the pattern is too long, it is capped.

The for loop copies the pattern into global arrays.

```cpp
buzzerStepCount = count;
```

This stores the number of steps.

```cpp
buzzerStepIndex = 0;
```

This starts from the first step.

```cpp
buzzerStepStartTime = millis();
```

This records when the current step started.

```cpp
buzzerActive = true;
```

This marks that a pattern is playing.

```cpp
digitalWrite(BUZZER_PIN, buzzerLevels[0] ? HIGH : LOW);
```

This turns the buzzer on or off for the first step.

```cpp
void updateBuzzer()
```

This advances the buzzer pattern without delay().

```cpp
if (!buzzerActive)
```

If no pattern is playing, it exits.

```cpp
if (millis() - buzzerStepStartTime < buzzerDurations[buzzerStepIndex])
```

If the current step is not finished yet, it exits.

```cpp
buzzerStepIndex++;
```

This moves to the next step.

If the pattern is finished, the buzzer is turned off. Otherwise, the next step starts.

```cpp
beepPatternReady()
```

This plays a short ready beep.

```cpp
beepPatternReminder()
```

This plays a reminder beep pattern.

```cpp
beepPatternTaken()
```

This plays a dose taken beep pattern.

```cpp
beepPatternMissed()
```

This plays a missed dose beep pattern.

```cpp
beepPatternRefill()
```

This plays a refill mode beep pattern.


## 27. Button functions


```cpp
bool readButtonRawPressed()
```

This reads the physical button pin.

If BUTTON_ACTIVE_LOW is true, LOW means pressed.

```cpp
void updateButton()
```

This debounces the button and detects new button press events.

```cpp
buttonPressedEvent = false;
```

This resets the event flag at the start of each update.

```cpp
bool rawPressed = readButtonRawPressed();
```

This reads the current raw button state.

```cpp
if (rawPressed != lastButtonRawPressed)
```

If the raw reading changed, the program records the time of the change.

```cpp
if (millis() - lastButtonChangeTime >= BUTTON_DEBOUNCE_MS)
```

If the reading has been stable long enough, it is accepted.

```cpp
if (buttonStablePressed != rawPressed)
```

If the stable state changed, the stable button state is updated.

```cpp
if (buttonStablePressed && !lastButtonStablePressed)
```

If the button has just changed from released to pressed, buttonPressedEvent becomes true for one loop cycle.

This design allows the state machine to react to clean button press events.


## 28. Load-cell sampling


```cpp
void updateWeightSampler()
```

This samples the HX711 at fixed intervals.

```cpp
if (millis() - lastHx711SampleTime < HX711_SAMPLE_INTERVAL_MS)
```

If it is not time for the next sample, it exits.

```cpp
if (hx711.isBusy())
```

If the HX711 is not ready, it exits.

```cpp
int32_t raw = hx711.readChannelRaw(CHAN_A_GAIN_128);
```

This reads the raw HX711 value.

The next section maintains a rolling average.

If the sample array is not full yet, the new sample is added.

If the sample array is full, the oldest sample is removed from the total and replaced by the new sample.

```cpp
latestRawWeightAverage = weightSampleTotal / weightSampleCount;
```

This calculates the average raw value.

```cpp
latestZeroedWeight = latestRawWeightAverage - weightZeroOffset;
```

This subtracts the tare offset.

```cpp
latestWeightValid = true;
```

This marks that a valid weight reading is available.


## 29. Stability task functions


```cpp
void startStabilityTask(const char* reason)
```

This starts a non-blocking stability check.

```cpp
stabilityActive = true;
```

This marks that the stability task is running.

```cpp
stabilityResultReady = false;
```

This clears any old result.

```cpp
stabilityStartTime = millis();
```

This records when the task started.

```cpp
stableStartTime = 0;
```

This clears the time when stability began.

```cpp
lastStabilityCheckTime = 0;
```

This allows the first check to happen immediately.

```cpp
if (latestWeightValid)
```

If a weight reading is available, it is used as the starting comparison value.

```cpp
oled_show_message("Checking weight", reason, "Please wait...", "");
```

This tells the user that the system is waiting for a stable weight.

```cpp
void updateStabilityTask()
```

This checks whether the weight has become stable.

```cpp
if (!stabilityActive)
```

If no stability check is running, it exits.

```cpp
if (!latestWeightValid)
```

If no weight reading is available, it exits.

```cpp
if (millis() - stabilityStartTime >= MAX_STABILITY_WAIT_MS)
```

If the system has waited too long, it uses the latest reading and finishes.

```cpp
if (millis() - lastStabilityCheckTime < STABILITY_CHECK_INTERVAL_MS)
```

If it is not time for the next check, it exits.

```cpp
int32_t change = abs(currentReading - stabilityLastReading);
```

This calculates how much the reading changed since the last check.

```cpp
if (change <= STABLE_WEIGHT_TOLERANCE)
```

If the reading is stable enough, the program starts or continues the stable timer.

```cpp
if (millis() - stableStartTime >= STABLE_WEIGHT_TIME_MS)
```

If the reading has remained stable long enough, the result is accepted.

```cpp
stabilityResultRaw = currentReading;
```

This stores the stable reading.

```cpp
stabilityResultReady = true;
```

This marks the result as ready.

```cpp
stabilityActive = false;
```

This ends the stability task.

If the reading changes too much, stableStartTime is reset to zero.


## 30. finishTare()


```cpp
void finishTare(const char* reason)
```

This finishes a tare operation using the result from the stability task.

```cpp
weightZeroOffset = stabilityResultRaw;
```

This makes the stable raw reading the new zero offset.

```cpp
oled_show_message("Scale zeroed", reason, "Weight = 0", "");
```

This tells the user that the scale has been zeroed.

Taring is used at startup, after dose taken, and after refill.


## 31. Servo functions


```cpp
void startServoMove(int targetAngle)
```

This starts moving the servo toward a target angle.

```cpp
targetServoAngle = targetAngle;
```

This stores where the servo should move.

```cpp
if (targetServoAngle == currentServoAngle)
```

If the servo is already there, no movement is needed.

```cpp
servoMoving = true;
```

This marks that the servo is moving.

```cpp
lastServoStepTime = millis();
```

This records when movement started.

```cpp
void updateServo()
```

This moves the servo one degree at a time without blocking.

```cpp
if (!servoMoving)
```

If the servo is not moving, it exits.

```cpp
if (millis() - lastServoStepTime < SERVO_STEP_INTERVAL_MS)
```

If it is not time for the next servo step, it exits.

If the current angle is less than the target, it increases by one degree.

If the current angle is greater than the target, it decreases by one degree.

```cpp
lidServo.write(currentServoAngle);
```

This sends the new angle to the servo.

```cpp
if (currentServoAngle == targetServoAngle)
```

If the target has been reached, servoMoving becomes false.

```cpp
void openLid()
```

This starts moving the servo to the open angle.

```cpp
void closeLid()
```

This starts moving the servo to the closed angle.


## 32. High-level action functions


```cpp
void resetButtonPressTracking()
```

This clears the single/double press tracking variables.

```cpp
void startReminder()
```

This starts the medication reminder process.

It clears button press tracking.
It clears possible dose detection.
It records the reminder start time.
It changes the LED to reminder colour.
It shows "Medication Time" on the OLED.
It starts opening the lid.
It enters STATE_REMINDER_OPENING.

This function is used by both the manual button override and the automatic 5PM reminder.

```cpp
void startRefillMode()
```

This starts refill mode.

It clears button tracking.
It changes the LED to refill colour.
It shows "Refill Mode" on the OLED.
It opens the lid.
It enters STATE_REFILL_OPENING.

```cpp
void queueDoseTakenLog()
```

This queues a ThingSpeak event for dose taken.

```cpp
queueNetworkEvent(1, "Dose_taken", false);
```

1 means dose taken. No missed-dose alert is needed.

```cpp
void queueMissedDoseAlert()
```

This queues a ThingSpeak event and IFTTT alert for a missed dose.

```cpp
queueNetworkEvent(0, "Missed_dose", true);
```

0 means missed dose. true means send the IFTTT missed-dose alert.


## 33. updateStateMachine()


This is the main behaviour controller for the medication dispenser.

It uses switch(state) to choose what the system should do based on the current state.

```cpp
case STATE_STARTUP_TARING:
```

When the system first starts, it needs to tare the scale.

If no stability task is active and no result is ready, it starts a startup tare stability task.

When the stable result is ready, finishTare("startup") is called.

The system then resets button tracking, sets the LED to green, plays the ready beep, shows the ready screen, and enters STATE_READY.

```cpp
case STATE_READY:
```

This is the normal idle state.

Every 2 seconds, the program prints the button state, weight, and RTC time to Serial, and updates the OLED ready screen.

The state also handles button input:

- A single press starts the medication reminder.
- A double press starts refill mode.

The code waits briefly after the first press to see whether a second press occurs within DOUBLE_PRESS_WINDOW_MS.

If a second press occurs in time, startRefillMode() is called.

If no second press occurs, startReminder() is called.

The automatic 5PM reminder is handled outside this state machine by updateAutomaticReminder(), but it only triggers when state is STATE_READY.

```cpp
case STATE_REFILL_OPENING:
```

The lid is opening for refill mode.

When servoMoving becomes false, the lid has finished opening.

The OLED tells the user to add tablets and press the button to close.

The refill beep pattern plays.

The state changes to STATE_REFILL_ACTIVE.

```cpp
case STATE_REFILL_ACTIVE:
```

The lid remains open while the user adds tablets.

When the button is pressed, the OLED says the lid is closing.

```cpp
closeLid() is called.
```

The state changes to STATE_REFILL_CLOSING.

```cpp
case STATE_REFILL_CLOSING:
```

The system waits until the servo has finished closing the lid.

When the servo is finished, it starts a stability task for "after refill".

The state changes to STATE_REFILL_TARING.

```cpp
case STATE_REFILL_TARING:
```

The system waits until a stable weight result is ready.

When ready, finishTare("after refill") is called.

This makes the new tablet load the new zero/reference state.

The LED returns to green, the OLED shows ready, and the state returns to STATE_READY.

```cpp
case STATE_REMINDER_OPENING:
```

The medication reminder has started and the lid is opening.

When the servo has finished opening, the OLED shows that the lid is open and the system is settling.

The state changes to STATE_REMINDER_SETTLING.

```cpp
case STATE_REMINDER_SETTLING:
```

The system waits for LID_SETTLE_AFTER_OPEN_MS.

This gives the lid, servo, and load cell time to settle before taking a baseline.

After the delay period, the program starts a stability task called "baseline".

The state changes to STATE_REMINDER_BASELINE.

```cpp
case STATE_REMINDER_BASELINE:
```

The system waits for a stable baseline weight.

When ready, reminderBaseline is set to the stable raw reading minus the current zero offset.

The system records baselineTakenTime.

The OLED tells the user to remove the dose.

The reminder beep plays.

The state changes to STATE_REMINDER_MONITORING.

```cpp
case STATE_REMINDER_MONITORING:
```

This is where the system watches for dose removal.

currentWeight is set from latestZeroedWeight.

```cpp
difference = currentWeight - reminderBaseline;
```

This calculates how much the weight has changed since the reminder baseline.

The program prints debug information and updates the reminder OLED screen.

For the first few seconds after baseline, the system ignores weight changes to avoid false detections.

If the reminder has been active longer than REMINDER_TIMEOUT_MS, the dose is marked as missed:

- LED turns red
- missed beep plays
- OLED shows Dose Missed
- missed-dose network event is queued
- lid begins closing
- state changes to STATE_DOSE_MISSED_CLOSING

If the difference is larger than MAX_REASONABLE_REDUCTION, the change is ignored because it is probably handling or pressure.

If the difference is greater than WEIGHT_REDUCTION_THRESHOLD, the program treats it as a possible dose removal.

It records possibleDoseTakenTime, starts a final confirmation stability task, and enters STATE_REMINDER_CONFIRMING.

```cpp
case STATE_REMINDER_CONFIRMING:
```

This state confirms whether the possible dose removal is real.

If the reminder timeout occurs during confirmation, the dose is marked missed.

If the stability result is not ready, the function returns and waits.

If the confirmation delay has not passed yet, the function returns and waits.

Once both conditions are satisfied, finalWeight and finalDifference are calculated.

If finalDifference is above the threshold and below the maximum reasonable value, the dose is confirmed as taken.

The system then:

- Sets the LED green
- Plays the taken beep
- Shows Dose Taken on the OLED
- Queues a ThingSpeak dose taken log
- Enters STATE_DOSE_TAKEN_HOLD_OPEN

If the final difference is invalid, the program returns to STATE_REMINDER_MONITORING.

```cpp
case STATE_DOSE_TAKEN_HOLD_OPEN:
```

The dose has been taken and the lid stays open briefly.

When LID_CLOSE_AFTER_TAKEN_MS has passed, the OLED says the lid is closing, closeLid() is called, and the state changes to STATE_DOSE_TAKEN_CLOSING.

```cpp
case STATE_DOSE_TAKEN_CLOSING:
```

The system waits for the servo to finish closing.

Once the servo is finished, it starts a stability task called "after dose".

The state changes to STATE_DOSE_TAKEN_TARING.

```cpp
case STATE_DOSE_TAKEN_TARING:
```

The system waits for a stable weight after the dose was removed.

Once stable, finishTare("after dose") is called.

This resets the scale reference for the remaining tablets.

The system clears dose detection, resets button tracking, sets the LED green, shows ready, and returns to STATE_READY.

```cpp
case STATE_DOSE_MISSED_CLOSING:
```

The system waits for the lid to finish closing after a missed dose.

Once the servo is finished, the system clears dose detection, resets button tracking, sets the LED green, shows ready, and returns to STATE_READY.

```cpp
default:
```

This is a safety fallback. If the state somehow has an unexpected value, the program resets button tracking, sets the LED green, and returns to STATE_READY.


## 34. setup()


```cpp
void setup()
```

This function runs once when the ESP32 starts.

```cpp
Serial.begin(115200);
```

This starts Serial Monitor communication at 115200 baud.

```cpp
pinMode(BUTTON_PIN, INPUT_PULLUP);
```

This sets the button pin as an input using the internal pull-up resistor.

```cpp
pinMode(BUZZER_PIN, OUTPUT);
```

This sets the buzzer pin as an output.

```cpp
pinMode(RGB_RED_PIN, OUTPUT);
pinMode(RGB_GREEN_PIN, OUTPUT);
pinMode(RGB_BLUE_PIN, OUTPUT);
```

These set the RGB LED pins as outputs.

```cpp
setLedOff();
```

This turns the RGB LED off at startup.

```cpp
digitalWrite(BUZZER_PIN, LOW);
```

This ensures the buzzer is off.

```cpp
Wire.begin(SDA_PIN, SCL_PIN);
```

This starts the I2C bus using the defined SDA and SCL pins.

```cpp
Wire.setClock(100000);
```

This sets the I2C speed to 100 kHz.

```cpp
setup_oled();
```

This initialises the OLED display.

```cpp
oled_show_message("Smart medication dispenser", "Starting...", "", "");
```

This shows a startup message.

```cpp
rtcOk = rtcDetected();
```

This checks whether the DS3231 RTC is connected.

If the RTC is found, a message is printed to Serial.

If not found, the OLED shows that the RTC is not found and the program continues.

```cpp
hx711.begin();
```

This starts the HX711 load-cell module.

```cpp
lidServo.setPeriodHertz(50);
```

This sets the servo signal frequency to 50 Hz, which is standard for hobby servos.

```cpp
lidServo.attach(SERVO_PIN, 500, 2400);
```

This attaches the servo to the servo pin and sets pulse width limits.

```cpp
lidServo.write(SERVO_CLOSED_ANGLE);
```

This sends the servo to the closed position at startup.

```cpp
currentServoAngle = SERVO_CLOSED_ANGLE;
targetServoAngle = SERVO_CLOSED_ANGLE;
servoMoving = false;
```

These initialise the servo tracking variables.

```cpp
startWifiConnect();
```

This starts a Wi-Fi connection attempt, but does not block the whole program waiting for it.

```cpp
enterState(STATE_STARTUP_TARING);
```

This starts the system in startup taring mode.


## 35. loop()


```cpp
void loop()
```

This function runs repeatedly after setup() finishes.

```cpp
updateButton();
```

This reads and debounces the button.

```cpp
updateWifi();
```

This manages Wi-Fi connection attempts and retries.

```cpp
updateNtpSync();
```

This updates the RTC from internet time when Wi-Fi is connected.

```cpp
updateAutomaticReminder();
```

This checks whether the RTC time is 5:00PM and starts the reminder once per day if the system is ready.

```cpp
updateNetworkQueue();
```

This sends queued ThingSpeak and IFTTT events when Wi-Fi is available.

```cpp
updateWeightSampler();
```

This samples and averages the load-cell readings.

```cpp
updateStabilityTask();
```

This handles non-blocking stable-weight checks.

```cpp
updateServo();
```

This moves the servo one step at a time if it is opening or closing.

```cpp
updateBuzzer();
```

This advances any active buzzer pattern.

```cpp
updateStateMachine();
```

This runs the main medication dispenser behaviour logic.

The loop is the key to the non-blocking design. Instead of doing one long task and waiting, the program gives each subsystem a small chance to update on every pass through loop().


## 36. Overall behaviour summary


At startup, the system:

1. Starts Serial, I2C, OLED, RTC, HX711, servo, and Wi-Fi.
2. Closes the lid.
3. Starts in startup taring mode.
4. Waits for a stable load-cell reading.
5. Sets that stable reading as zero.
6. Enters ready mode.

In ready mode, the system waits for:

- The automatic 5:00 PM RTC reminder
- A single button press for manual medication reminder
- A double button press for refill mode

When a medication reminder starts:

1. The lid opens.
2. The system waits for the load cell to settle.
3. A stable baseline weight is recorded.
4. The user is prompted to remove a dose.
5. The load cell monitors for a valid weight reduction.
6. If the dose is removed and confirmed, ThingSpeak logs Dose_taken.
7. If the dose is not removed before timeout, ThingSpeak logs Missed_dose and IFTTT sends an alert.
8. The lid closes.
9. The system returns to ready mode.

When refill mode starts:

1. The lid opens.
2. The user adds tablets.
3. The user presses the button.
4. The lid closes.
5. The system waits for a stable weight.
6. The scale is re-tared.
7. The system returns to ready mode.


## 37. Notes about blocking


The program is designed to be non-blocking for its main embedded behaviour.

The following are non-blocking or timer-based:

- Button debounce
- Servo movement
- Buzzer patterns
- Weight sampling
- Stability checking
- Wi-Fi retry logic
- State transitions
- Automatic 5PM reminder checking

There are still some short synchronous library calls:

- HTTPClient GET and POST can block briefly while sending network requests.
- getLocalTime() has a small timeout.
- I2C communication with the OLED and RTC is synchronous.

These should be acceptable for this prototype, especially because HTTP requests have a timeout and cloud events are queued.
