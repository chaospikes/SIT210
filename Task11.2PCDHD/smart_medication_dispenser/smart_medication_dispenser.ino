#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <time.h>
#include <HTTPClient.h>
#include "Adafruit_HX711.h"
#include <ESP32Servo.h>
#include "secrets.h"

//  Pins 

#define HX711_DOUT D0
#define HX711_SCK  D1

#define BUTTON_PIN D2
#define BUZZER_PIN D3

#define SDA_PIN D4
#define SCL_PIN D5

#define RGB_RED_PIN   D6
#define RGB_GREEN_PIN D7
#define RGB_BLUE_PIN  D8

#define SERVO_PIN D9

//  OLED 

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
bool oledOk = false;

//  Wi-Fi / Cloud 

char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASS;

const unsigned long WIFI_CONNECT_TIMEOUT_MS = 15000;
const unsigned long WIFI_RETRY_INTERVAL_MS = 30000;
const unsigned long NETWORK_RETRY_INTERVAL_MS = 30000;
const unsigned long HTTP_TIMEOUT_MS = 2500;

bool wifiConnecting = false;
unsigned long wifiConnectStartTime = 0;
unsigned long nextWifiAttemptTime = 0;

//  RTC 

#define DS3231_ADDR 0x68
bool rtcOk = false;

// GMT+8 for WA/Perth
const long GMT_OFFSET_SEC = 8 * 60 * 60;
const int DAYLIGHT_OFFSET_SEC = 0;

bool ntpConfigured = false;
bool rtcSyncedFromNtp = false;
unsigned long lastNtpCheckTime = 0;
const unsigned long NTP_CHECK_INTERVAL_MS = 1000;

//  Automatic daily reminder 

// The prototype automatically opens the pill box at 5:00 PM each day. Button-controlled behaviour is used for demonstration and user override
const byte AUTO_REMINDER_HOUR = 17;
const byte AUTO_REMINDER_MINUTE = 0;

int lastAutoReminderYear = -1;
int lastAutoReminderMonth = -1;
int lastAutoReminderDay = -1;

//  HX711 

Adafruit_HX711 hx711(HX711_DOUT, HX711_SCK);

const int32_t WEIGHT_REDUCTION_THRESHOLD = 1000;
const int32_t MAX_REASONABLE_REDUCTION = 80000;

const int32_t STABLE_WEIGHT_TOLERANCE = 1200;
const unsigned long STABLE_WEIGHT_TIME_MS = 2500;
const unsigned long MAX_STABILITY_WAIT_MS = 12000;

const unsigned long HX711_SAMPLE_INTERVAL_MS = 50;
const int WEIGHT_AVG_WINDOW = 5;

int32_t weightZeroOffset = 0;
int32_t reminderBaseline = 0;

int32_t weightSamples[WEIGHT_AVG_WINDOW];
int weightSampleIndex = 0;
int weightSampleCount = 0;
int64_t weightSampleTotal = 0;

bool latestWeightValid = false;
int32_t latestRawWeightAverage = 0;
int32_t latestZeroedWeight = 0;
unsigned long lastHx711SampleTime = 0;

//  Stability task 

bool stabilityActive = false;
bool stabilityResultReady = false;
unsigned long stabilityStartTime = 0;
unsigned long stableStartTime = 0;
unsigned long lastStabilityCheckTime = 0;
int32_t stabilityLastReading = 0;
int32_t stabilityResultRaw = 0;

const unsigned long STABILITY_CHECK_INTERVAL_MS = 250;

//  Servo 

const int SERVO_CLOSED_ANGLE = 10;
const int SERVO_OPEN_ANGLE   = 150;

const unsigned long SERVO_STEP_INTERVAL_MS = 15;

Servo lidServo;
int currentServoAngle = SERVO_CLOSED_ANGLE;
int targetServoAngle = SERVO_CLOSED_ANGLE;
bool servoMoving = false;
unsigned long lastServoStepTime = 0;

//  Timing 

const unsigned long REMINDER_TIMEOUT_MS = 45000;
const unsigned long LID_SETTLE_AFTER_OPEN_MS = 3000;
const unsigned long IGNORE_WEIGHT_AFTER_BASELINE_MS = 3000;
const unsigned long WEIGHT_CONFIRM_DELAY_MS = 5000;
const unsigned long LID_CLOSE_AFTER_TAKEN_MS = 8000;

const unsigned long DOUBLE_PRESS_WINDOW_MS = 500;

unsigned long reminderStartTime = 0;
unsigned long baselineTakenTime = 0;
unsigned long possibleDoseTakenTime = 0;
unsigned long stateStartTime = 0;
unsigned long firstButtonPressTime = 0;

bool possibleDoseTaken = false;
bool waitingForSecondPress = false;
bool firstPressReleased = false;

//  Button handling 

const bool BUTTON_ACTIVE_LOW = true;
const unsigned long BUTTON_DEBOUNCE_MS = 75;

bool buttonStablePressed = false;
bool lastButtonStablePressed = false;
bool lastButtonRawPressed = false;
bool buttonPressedEvent = false;
unsigned long lastButtonChangeTime = 0;

//  Buzzer task 

const int MAX_BUZZER_STEPS = 10;

bool buzzerActive = false;
bool buzzerLevels[MAX_BUZZER_STEPS];
unsigned long buzzerDurations[MAX_BUZZER_STEPS];
int buzzerStepCount = 0;
int buzzerStepIndex = 0;
unsigned long buzzerStepStartTime = 0;

//  Network event queue 

bool networkEventPending = false;
int queuedDoseTakenValue = 0;
bool queuedSendMissedAlert = false;
String queuedStatusText = "";
unsigned long nextNetworkAttemptTime = 0;

//  State machine 

enum SystemState
{
    STATE_STARTUP_TARING,
    STATE_READY,

    STATE_REFILL_OPENING,
    STATE_REFILL_ACTIVE,
    STATE_REFILL_CLOSING,
    STATE_REFILL_TARING,

    STATE_REMINDER_OPENING,
    STATE_REMINDER_SETTLING,
    STATE_REMINDER_BASELINE,
    STATE_REMINDER_MONITORING,
    STATE_REMINDER_CONFIRMING,

    STATE_DOSE_TAKEN_HOLD_OPEN,
    STATE_DOSE_TAKEN_CLOSING,
    STATE_DOSE_TAKEN_TARING,

    STATE_DOSE_MISSED_CLOSING
};

SystemState state = STATE_STARTUP_TARING;

//  Function prototypes 

void oled_show_message(
    const String& line1,
    const String& line2,
    const String& line3 = "",
    const String& line4 = ""
);

void oled_show_ready(int32_t weight);
void oled_show_reminder();
void updateAutomaticReminder();

//  Utility helpers 

void enterState(SystemState newState)
{
    state = newState;
    stateStartTime = millis();
}

//  RTC helpers 

byte bcdToDec(byte value)
{
    return ((value >> 4) * 10) + (value & 0x0F);
}

byte decToBcd(byte value)
{
    return ((value / 10) << 4) | (value % 10);
}

bool rtcDetected()
{
    Wire.beginTransmission(DS3231_ADDR);
    return Wire.endTransmission() == 0;
}

void setDS3231Time(byte second, byte minute, byte hour, byte dayOfWeek, byte dayOfMonth, byte month, byte year)
{
    Wire.beginTransmission(DS3231_ADDR);
    Wire.write(0x00);

    Wire.write(decToBcd(second));
    Wire.write(decToBcd(minute));
    Wire.write(decToBcd(hour));
    Wire.write(decToBcd(dayOfWeek));
    Wire.write(decToBcd(dayOfMonth));
    Wire.write(decToBcd(month));
    Wire.write(decToBcd(year));

    Wire.endTransmission();
}

bool readDS3231Time(byte& second, byte& minute, byte& hour, byte& dayOfMonth, byte& month, byte& year)
{
    Wire.beginTransmission(DS3231_ADDR);
    Wire.write(0x00);

    if (Wire.endTransmission(false) != 0)
    {
        return false;
    }

    if (Wire.requestFrom(DS3231_ADDR, (uint8_t)7) != 7)
    {
        return false;
    }

    second = bcdToDec(Wire.read() & 0x7F);
    minute = bcdToDec(Wire.read());
    hour = bcdToDec(Wire.read() & 0x3F);

    Wire.read();

    dayOfMonth = bcdToDec(Wire.read());
    month = bcdToDec(Wire.read() & 0x1F);
    year = bcdToDec(Wire.read());

    return true;
}

String getRtcTimeString()
{
    if (!rtcOk)
    {
        return "RTC unavailable";
    }

    byte second, minute, hour, dayOfMonth, month, year;

    if (!readDS3231Time(second, minute, hour, dayOfMonth, month, year))
    {
        return "RTC read failed";
    }

    char buffer[25];

    snprintf(
        buffer,
        sizeof(buffer),
        "20%02d-%02d-%02d %02d:%02d:%02d",
        year,
        month,
        dayOfMonth,
        hour,
        minute,
        second
    );

    return String(buffer);
}

void updateNtpSync()
{
    if (!rtcOk || rtcSyncedFromNtp)
    {
        return;
    }

    if (WiFi.status() != WL_CONNECTED)
    {
        return;
    }

    if (!ntpConfigured)
    {
        configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, "pool.ntp.org", "time.nist.gov");
        ntpConfigured = true;
        lastNtpCheckTime = millis();

        Serial.println("NTP configured. Waiting for valid time.");
        oled_show_message("WiFi connected", "Getting time...", "", "");
        return;
    }

    if (millis() - lastNtpCheckTime < NTP_CHECK_INTERVAL_MS)
    {
        return;
    }

    lastNtpCheckTime = millis();

    time_t now = time(nullptr);

    if (now < 1700000000)
    {
        return;
    }

    struct tm timeInfo;

    if (!getLocalTime(&timeInfo, 10))
    {
        return;
    }

    int year = timeInfo.tm_year + 1900;
    int month = timeInfo.tm_mon + 1;
    int day = timeInfo.tm_mday;
    int hour = timeInfo.tm_hour;
    int minute = timeInfo.tm_min;
    int second = timeInfo.tm_sec;

    byte dayOfWeek = timeInfo.tm_wday == 0 ? 7 : timeInfo.tm_wday;

    setDS3231Time(
        second,
        minute,
        hour,
        dayOfWeek,
        day,
        month,
        year - 2000
    );

    rtcSyncedFromNtp = true;

    Serial.print("RTC updated from NTP: ");
    Serial.println(getRtcTimeString());

    oled_show_message("Time sync OK", getRtcTimeString(), "", "");
}


void updateAutomaticReminder()
{
    if (!rtcOk)
    {
        return;
    }

    if (state != STATE_READY)
    {
        return;
    }

    byte second, minute, hour, dayOfMonth, month, year;

    if (!readDS3231Time(second, minute, hour, dayOfMonth, month, year))
    {
        return;
    }

    bool alreadyTriggeredToday =
        lastAutoReminderYear == year &&
        lastAutoReminderMonth == month &&
        lastAutoReminderDay == dayOfMonth;

    if (alreadyTriggeredToday)
    {
        return;
    }

    if (hour == AUTO_REMINDER_HOUR && minute == AUTO_REMINDER_MINUTE)
    {
        lastAutoReminderYear = year;
        lastAutoReminderMonth = month;
        lastAutoReminderDay = dayOfMonth;

        Serial.println("Automatic 5 PM medication reminder triggered.");
        startReminder();
    }
}

//  OLED helpers 

void setup_oled()
{
    Serial.println("Starting OLED...");

    if (display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
    {
        oledOk = true;
        Serial.println("OLED found at 0x3C.");
    }
    else
    {
        oledOk = false;
        Serial.println("OLED not found. Continuing without OLED.");
        return;
    }

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.display();
}

void oled_show_message(
    const String& line1,
    const String& line2,
    const String& line3,
    const String& line4
)
{
    if (!oledOk)
    {
        return;
    }

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    display.setCursor(0, 0);
    display.println(line1);

    display.setCursor(0, 16);
    display.println(line2);

    display.setCursor(0, 32);
    display.println(line3);

    display.setCursor(0, 48);
    display.println(line4);

    display.display();
}

void oled_show_ready(int32_t weight)
{
    if (!oledOk)
    {
        return;
    }

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    display.setCursor(0, 0);
    display.println("Smart Medication Dispenser");

    display.setCursor(0, 16);
    display.println("Status: Ready");

    display.setCursor(0, 32);
    display.print("Weight: ");
    display.println(weight);

    display.setCursor(0, 48);
    display.println(getRtcTimeString());

    display.display();
}

void oled_show_reminder()
{
    if (!oledOk)
    {
        return;
    }

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    display.setCursor(0, 0);
    display.println("Medication Time");

    display.setCursor(0, 16);
    display.println("Please take dose");

    display.setCursor(0, 32);
    display.println("Lid is open");

    display.setCursor(0, 48);
    display.println("Monitoring...");

    display.display();
}

//  Wi-Fi / cloud helpers 

void startWifiConnect()
{
    if (WiFi.status() == WL_CONNECTED)
    {
        return;
    }

    if (wifiConnecting)
    {
        return;
    }

    if (millis() < nextWifiAttemptTime)
    {
        return;
    }

    Serial.print("Starting WiFi connection to SSID: ");
    Serial.println(ssid);

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pass);

    wifiConnecting = true;
    wifiConnectStartTime = millis();
}

void updateWifi()
{
    if (WiFi.status() == WL_CONNECTED)
    {
        if (wifiConnecting)
        {
            wifiConnecting = false;

            Serial.println("WiFi connected.");
            Serial.print("IP Address: ");
            Serial.println(WiFi.localIP());
            Serial.print("RSSI: ");
            Serial.print(WiFi.RSSI());
            Serial.println(" dBm");
        }

        return;
    }

    if (!wifiConnecting)
    {
        startWifiConnect();
        return;
    }

    if (millis() - wifiConnectStartTime >= WIFI_CONNECT_TIMEOUT_MS)
    {
        Serial.println("WiFi connection timed out. Will retry later.");

        WiFi.disconnect();
        wifiConnecting = false;
        nextWifiAttemptTime = millis() + WIFI_RETRY_INTERVAL_MS;
    }
}

bool sendThingSpeakNow(int doseTakenValue, const String& statusText)
{
    if (WiFi.status() != WL_CONNECTED)
    {
        return false;
    }

    String url = "http://api.thingspeak.com/update?api_key=";
    url += SECRET_WRITE_APIKEY;
    url += "&field1=";
    url += String(doseTakenValue);
    url += "&status=";
    url += statusText;

    HTTPClient http;
    http.setTimeout(HTTP_TIMEOUT_MS);
    http.begin(url);

    Serial.print("Sending ThingSpeak update: ");
    Serial.println(url);

    int responseCode = http.GET();

    Serial.print("ThingSpeak HTTP response: ");
    Serial.println(responseCode);

    http.end();

    return responseCode == 200;
}

bool sendIftttMissedDoseAlertNow()
{
    if (WiFi.status() != WL_CONNECTED)
    {
        return false;
    }

    String timestamp = getRtcTimeString();

    String url = "https://maker.ifttt.com/trigger/";
    url += SECRET_IFTTT_EVENT_NAME;
    url += "/with/key/";
    url += SECRET_IFTTT_KEY;

    String payload = "{";
    payload += "\"value1\":\"Linda's Medication\",";
    payload += "\"value2\":\"" + timestamp + "\",";
    payload += "\"value3\":\"Dose MISSED\"";
    payload += "}";

    HTTPClient http;
    http.setTimeout(HTTP_TIMEOUT_MS);
    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    Serial.print("Sending IFTTT missed-dose alert: ");
    Serial.println(payload);

    int responseCode = http.POST(payload);

    Serial.print("IFTTT HTTP response: ");
    Serial.println(responseCode);

    http.end();

    return responseCode > 0 && responseCode < 400;
}

void queueNetworkEvent(int doseTakenValue, const String& statusText, bool sendMissedAlert)
{
    queuedDoseTakenValue = doseTakenValue;
    queuedStatusText = statusText;
    queuedSendMissedAlert = sendMissedAlert;
    networkEventPending = true;
    nextNetworkAttemptTime = millis();

    Serial.print("Network event queued: ");
    Serial.println(statusText);
}

void updateNetworkQueue()
{
    if (!networkEventPending)
    {
        return;
    }

    if (millis() < nextNetworkAttemptTime)
    {
        return;
    }

    if (WiFi.status() != WL_CONNECTED)
    {
        startWifiConnect();
        nextNetworkAttemptTime = millis() + NETWORK_RETRY_INTERVAL_MS;
        return;
    }

    bool thingSpeakOk = sendThingSpeakNow(queuedDoseTakenValue, queuedStatusText);
    bool iftttOk = true;

    if (queuedSendMissedAlert)
    {
        iftttOk = sendIftttMissedDoseAlertNow();
    }

    if (thingSpeakOk && iftttOk)
    {
        Serial.println("Queued network event sent successfully.");
        networkEventPending = false;
        return;
    }

    Serial.println("Network event send failed. Will retry later.");
    nextNetworkAttemptTime = millis() + NETWORK_RETRY_INTERVAL_MS;
}

//  Output helpers 

void setRgbPwm(int redValue, int greenValue, int blueValue)
{
    analogWrite(RGB_RED_PIN, redValue);
    analogWrite(RGB_GREEN_PIN, greenValue);
    analogWrite(RGB_BLUE_PIN, blueValue);
}

void setReadyLight()
{
    setRgbPwm(0, 255, 0);         // green
}

void setReminderLight()
{
    setRgbPwm(255, 0, 255);       // purple
}

void setRefillLight()
{
    setRgbPwm(255, 60, 0);        // orange / warm yellow
}

void setTakenLight()
{
    setRgbPwm(0, 255, 0);         // green
}

void setMissedLight()
{
    setRgbPwm(255, 0, 0);         // red
}

void setLedOff()
{
    setRgbPwm(0, 0, 0);
}

//  Buzzer helpers 

void startBuzzerPattern(const bool levels[], const unsigned long durations[], int count)
{
    if (count <= 0)
    {
        return;
    }

    if (count > MAX_BUZZER_STEPS)
    {
        count = MAX_BUZZER_STEPS;
    }

    for (int i = 0; i < count; i++)
    {
        buzzerLevels[i] = levels[i];
        buzzerDurations[i] = durations[i];
    }

    buzzerStepCount = count;
    buzzerStepIndex = 0;
    buzzerStepStartTime = millis();
    buzzerActive = true;

    digitalWrite(BUZZER_PIN, buzzerLevels[0] ? HIGH : LOW);
}

void updateBuzzer()
{
    if (!buzzerActive)
    {
        return;
    }

    if (millis() - buzzerStepStartTime < buzzerDurations[buzzerStepIndex])
    {
        return;
    }

    buzzerStepIndex++;

    if (buzzerStepIndex >= buzzerStepCount)
    {
        buzzerActive = false;
        digitalWrite(BUZZER_PIN, LOW);
        return;
    }

    buzzerStepStartTime = millis();
    digitalWrite(BUZZER_PIN, buzzerLevels[buzzerStepIndex] ? HIGH : LOW);
}

void beepPatternReady()
{
    const bool levels[] = { true };
    const unsigned long durations[] = { 80 };
    startBuzzerPattern(levels, durations, 1);
}

void beepPatternReminder()
{
    const bool levels[] = { true, false, true };
    const unsigned long durations[] = { 200, 150, 200 };
    startBuzzerPattern(levels, durations, 3);
}

void beepPatternTaken()
{
    const bool levels[] = { true, false, true };
    const unsigned long durations[] = { 100, 100, 100 };
    startBuzzerPattern(levels, durations, 3);
}

void beepPatternMissed()
{
    const bool levels[] = { true, false, true, false, true };
    const unsigned long durations[] = { 250, 250, 250, 250, 250 };
    startBuzzerPattern(levels, durations, 5);
}

void beepPatternRefill()
{
    const bool levels[] = { true, false, true, false, true };
    const unsigned long durations[] = { 100, 100, 100, 100, 100 };
    startBuzzerPattern(levels, durations, 5);
}

//  Button helpers 

bool readButtonRawPressed()
{
    int reading = digitalRead(BUTTON_PIN);

    if (BUTTON_ACTIVE_LOW)
    {
        return reading == LOW;
    }

    return reading == HIGH;
}

void updateButton()
{
    buttonPressedEvent = false;

    bool rawPressed = readButtonRawPressed();

    if (rawPressed != lastButtonRawPressed)
    {
        lastButtonChangeTime = millis();
        lastButtonRawPressed = rawPressed;
    }

    if (millis() - lastButtonChangeTime >= BUTTON_DEBOUNCE_MS)
    {
        if (buttonStablePressed != rawPressed)
        {
            lastButtonStablePressed = buttonStablePressed;
            buttonStablePressed = rawPressed;

            if (buttonStablePressed && !lastButtonStablePressed)
            {
                buttonPressedEvent = true;
            }
        }
    }
}

//  Load cell helpers 

void updateWeightSampler()
{
    if (millis() - lastHx711SampleTime < HX711_SAMPLE_INTERVAL_MS)
    {
        return;
    }

    lastHx711SampleTime = millis();

    if (hx711.isBusy())
    {
        return;
    }

    int32_t raw = hx711.readChannelRaw(CHAN_A_GAIN_128);

    if (weightSampleCount < WEIGHT_AVG_WINDOW)
    {
        weightSamples[weightSampleIndex] = raw;
        weightSampleTotal += raw;
        weightSampleCount++;
        weightSampleIndex = (weightSampleIndex + 1) % WEIGHT_AVG_WINDOW;
    }
    else
    {
        weightSampleTotal -= weightSamples[weightSampleIndex];
        weightSamples[weightSampleIndex] = raw;
        weightSampleTotal += raw;
        weightSampleIndex = (weightSampleIndex + 1) % WEIGHT_AVG_WINDOW;
    }

    if (weightSampleCount > 0)
    {
        latestRawWeightAverage = weightSampleTotal / weightSampleCount;
        latestZeroedWeight = latestRawWeightAverage - weightZeroOffset;
        latestWeightValid = true;
    }
}

void startStabilityTask(const char* reason)
{
    stabilityActive = true;
    stabilityResultReady = false;
    stabilityStartTime = millis();
    stableStartTime = 0;
    lastStabilityCheckTime = 0;

    if (latestWeightValid)
    {
        stabilityLastReading = latestRawWeightAverage;
    }
    else
    {
        stabilityLastReading = 0;
    }

    Serial.print("Starting stability task: ");
    Serial.println(reason);

    oled_show_message("Checking weight", reason, "Please wait...", "");
}

void updateStabilityTask()
{
    if (!stabilityActive)
    {
        return;
    }

    if (!latestWeightValid)
    {
        return;
    }

    if (millis() - stabilityStartTime >= MAX_STABILITY_WAIT_MS)
    {
        stabilityResultRaw = latestRawWeightAverage;
        stabilityResultReady = true;
        stabilityActive = false;

        Serial.println("Stability task timed out. Using latest reading.");
        return;
    }

    if (millis() - lastStabilityCheckTime < STABILITY_CHECK_INTERVAL_MS)
    {
        return;
    }

    lastStabilityCheckTime = millis();

    int32_t currentReading = latestRawWeightAverage;
    int32_t change = abs(currentReading - stabilityLastReading);

    Serial.print("Stability | Current: ");
    Serial.print(currentReading);
    Serial.print(" | Last: ");
    Serial.print(stabilityLastReading);
    Serial.print(" | Change: ");
    Serial.println(change);

    if (change <= STABLE_WEIGHT_TOLERANCE)
    {
        if (stableStartTime == 0)
        {
            stableStartTime = millis();
        }

        if (millis() - stableStartTime >= STABLE_WEIGHT_TIME_MS)
        {
            stabilityResultRaw = currentReading;
            stabilityResultReady = true;
            stabilityActive = false;

            Serial.print("Stable raw weight confirmed: ");
            Serial.println(stabilityResultRaw);
        }
    }
    else
    {
        stableStartTime = 0;
    }

    stabilityLastReading = currentReading;
}

void finishTare(const char* reason)
{
    weightZeroOffset = stabilityResultRaw;

    Serial.print("Scale tared: ");
    Serial.print(reason);
    Serial.print(" | New zero offset: ");
    Serial.println(weightZeroOffset);

    oled_show_message("Scale zeroed", reason, "Weight = 0", "");
}

//  Servo helpers 

void startServoMove(int targetAngle)
{
    targetServoAngle = targetAngle;

    if (targetServoAngle == currentServoAngle)
    {
        servoMoving = false;
        return;
    }

    servoMoving = true;
    lastServoStepTime = millis();
}

void updateServo()
{
    if (!servoMoving)
    {
        return;
    }

    if (millis() - lastServoStepTime < SERVO_STEP_INTERVAL_MS)
    {
        return;
    }

    lastServoStepTime = millis();

    if (currentServoAngle < targetServoAngle)
    {
        currentServoAngle++;
    }
    else if (currentServoAngle > targetServoAngle)
    {
        currentServoAngle--;
    }

    lidServo.write(currentServoAngle);

    if (currentServoAngle == targetServoAngle)
    {
        servoMoving = false;
    }
}

void openLid()
{
    Serial.println("Opening lid...");
    startServoMove(SERVO_OPEN_ANGLE);
}

void closeLid()
{
    Serial.println("Closing lid...");
    startServoMove(SERVO_CLOSED_ANGLE);
}

//  High-level actions 

void resetButtonPressTracking()
{
    waitingForSecondPress = false;
    firstPressReleased = false;
    firstButtonPressTime = 0;
}

void startReminder()
{
    Serial.println();
    Serial.println("Medication reminder started.");

    resetButtonPressTracking();

    possibleDoseTaken = false;
    possibleDoseTakenTime = 0;
    reminderStartTime = millis();

    setReminderLight();
    oled_show_message("Medication Time", "Opening lid...", "", "");

    openLid();
    enterState(STATE_REMINDER_OPENING);
}

void startRefillMode()
{
    Serial.println();
    Serial.println("Refill mode started.");

    resetButtonPressTracking();

    setRefillLight();
    oled_show_message("Refill Mode", "Opening lid...", "", "");

    openLid();
    enterState(STATE_REFILL_OPENING);
}

void queueDoseTakenLog()
{
    queueNetworkEvent(1, "Dose_taken", false);
}

void queueMissedDoseAlert()
{
    queueNetworkEvent(0, "Missed_dose", true);
}

//  State machine update 

void updateStateMachine()
{
    static unsigned long lastReadyPrintTime = 0;
    static unsigned long lastReminderPrintTime = 0;

    switch (state)
    {
        case STATE_STARTUP_TARING:
        {
            if (!stabilityActive && !stabilityResultReady)
            {
                startStabilityTask("startup tare");
            }

            if (stabilityResultReady)
            {
                finishTare("startup");
                stabilityResultReady = false;

                resetButtonPressTracking();

                setReadyLight();
                beepPatternReady();
                oled_show_message("Smart Pill Box", "Ready", "Press button", "");

                Serial.println("System ready.");
                Serial.println("Automatic reminder: 5:00 PM daily.");
                Serial.println("Single press: manual medication reminder.");
                Serial.println("Double press: refill mode.");

                enterState(STATE_READY);
            }

            break;
        }

        case STATE_READY:
        {
            if (millis() - lastReadyPrintTime >= 2000)
            {
                lastReadyPrintTime = millis();

                Serial.print("READY | Button: ");
                Serial.print(buttonStablePressed ? "PRESSED" : "released");

                Serial.print(" | Weight: ");
                Serial.print(latestZeroedWeight);

                Serial.print(" | ");
                Serial.println(getRtcTimeString());

                oled_show_ready(latestZeroedWeight);
            }

            if (waitingForSecondPress && !buttonStablePressed)
            {
                firstPressReleased = true;
            }

            if (buttonPressedEvent)
            {
                if (!waitingForSecondPress)
                {
                    waitingForSecondPress = true;
                    firstPressReleased = false;
                    firstButtonPressTime = millis();

                    Serial.println("First button press detected. Waiting for possible double press.");
                }
                else if (firstPressReleased)
                {
                    if (millis() - firstButtonPressTime <= DOUBLE_PRESS_WINDOW_MS)
                    {
                        Serial.println("Double press detected.");
                        startRefillMode();
                    }
                }
            }

            if (waitingForSecondPress && millis() - firstButtonPressTime > DOUBLE_PRESS_WINDOW_MS)
            {
                Serial.println("Single press confirmed.");
                startReminder();
            }

            break;
        }

        case STATE_REFILL_OPENING:
        {
            if (!servoMoving)
            {
                oled_show_message("Refill Mode", "Add tablets", "Press button", "to close");
                beepPatternRefill();

                Serial.println("Refill mode active. Press button once to close and tare.");

                enterState(STATE_REFILL_ACTIVE);
            }

            break;
        }

        case STATE_REFILL_ACTIVE:
        {
            if (buttonPressedEvent)
            {
                oled_show_message("Refill Mode", "Closing lid...", "", "");
                closeLid();
                enterState(STATE_REFILL_CLOSING);
            }

            break;
        }

        case STATE_REFILL_CLOSING:
        {
            if (!servoMoving)
            {
                startStabilityTask("after refill");
                enterState(STATE_REFILL_TARING);
            }

            break;
        }

        case STATE_REFILL_TARING:
        {
            if (stabilityResultReady)
            {
                finishTare("after refill");
                stabilityResultReady = false;

                resetButtonPressTracking();

                setReadyLight();
                oled_show_message("Smart Pill Box", "Ready", "Press button", "");

                Serial.println("Refill complete. System ready.");

                enterState(STATE_READY);
            }

            break;
        }

        case STATE_REMINDER_OPENING:
        {
            if (!servoMoving)
            {
                Serial.println("Lid opened. Settling before baseline.");
                oled_show_message("Medication Time", "Lid open", "Settling...", "");

                enterState(STATE_REMINDER_SETTLING);
            }

            break;
        }

        case STATE_REMINDER_SETTLING:
        {
            if (millis() - stateStartTime >= LID_SETTLE_AFTER_OPEN_MS)
            {
                startStabilityTask("baseline");
                enterState(STATE_REMINDER_BASELINE);
            }

            break;
        }

        case STATE_REMINDER_BASELINE:
        {
            if (stabilityResultReady)
            {
                reminderBaseline = stabilityResultRaw - weightZeroOffset;
                baselineTakenTime = millis();
                stabilityResultReady = false;

                Serial.print("Reminder baseline: ");
                Serial.println(reminderBaseline);

                oled_show_message("Medication Time", "Remove dose", "Monitoring...", "");
                beepPatternReminder();

                enterState(STATE_REMINDER_MONITORING);
            }

            break;
        }

        case STATE_REMINDER_MONITORING:
        {
            int32_t currentWeight = latestZeroedWeight;
            int32_t difference = currentWeight - reminderBaseline;

            if (millis() - lastReminderPrintTime >= 500)
            {
                lastReminderPrintTime = millis();

                Serial.print("Reminder active | Baseline: ");
                Serial.print(reminderBaseline);

                Serial.print(" | Current: ");
                Serial.print(currentWeight);

                Serial.print(" | Difference: ");
                Serial.print(difference);

                Serial.print(" | Possible taken: ");
                Serial.println(possibleDoseTaken ? "YES" : "NO");

                oled_show_reminder();
            }

            if (millis() - baselineTakenTime < IGNORE_WEIGHT_AFTER_BASELINE_MS)
            {
                return;
            }

            if (millis() - reminderStartTime >= REMINDER_TIMEOUT_MS)
            {
                Serial.println("Reminder timed out. Dose missed.");

                setMissedLight();
                beepPatternMissed();
                oled_show_message("Dose Missed", "Sending alert", "ThingSpeak/IFTTT", "");

                queueMissedDoseAlert();

                closeLid();
                enterState(STATE_DOSE_MISSED_CLOSING);
                return;
            }

            if (difference >= MAX_REASONABLE_REDUCTION)
            {
                Serial.println("Ignoring huge weight change. Likely handling/finger pressure.");
                possibleDoseTaken = false;
                return;
            }

            if (difference > WEIGHT_REDUCTION_THRESHOLD)
            {
                possibleDoseTaken = true;
                possibleDoseTakenTime = millis();

                Serial.println("Possible dose removal detected. Confirming after delay.");

                startStabilityTask("final confirmation");
                enterState(STATE_REMINDER_CONFIRMING);
            }

            break;
        }

        case STATE_REMINDER_CONFIRMING:
        {
            if (millis() - reminderStartTime >= REMINDER_TIMEOUT_MS)
            {
                Serial.println("Reminder timed out during confirmation. Dose missed.");

                setMissedLight();
                beepPatternMissed();
                oled_show_message("Dose Missed", "Sending alert", "ThingSpeak/IFTTT", "");

                queueMissedDoseAlert();

                closeLid();
                enterState(STATE_DOSE_MISSED_CLOSING);
                return;
            }

            if (!stabilityResultReady)
            {
                return;
            }

            if (millis() - possibleDoseTakenTime < WEIGHT_CONFIRM_DELAY_MS)
            {
                return;
            }

            int32_t finalWeight = stabilityResultRaw - weightZeroOffset;
            int32_t finalDifference = finalWeight - reminderBaseline;

            stabilityResultReady = false;

            Serial.print("Final confirmation weight: ");
            Serial.print(finalWeight);
            Serial.print(" | Final difference: ");
            Serial.println(finalDifference);

            if (finalDifference > WEIGHT_REDUCTION_THRESHOLD && finalDifference < MAX_REASONABLE_REDUCTION)
            {
                Serial.println("Dose taken confirmed.");

                setTakenLight();
                beepPatternTaken();
                oled_show_message("Dose Taken", "Confirmed", "Thank you", "");

                queueDoseTakenLog();

                enterState(STATE_DOSE_TAKEN_HOLD_OPEN);
            }
            else
            {
                Serial.println("Final difference invalid. Returning to monitoring.");
                possibleDoseTaken = false;
                enterState(STATE_REMINDER_MONITORING);
            }

            break;
        }

        case STATE_DOSE_TAKEN_HOLD_OPEN:
        {
            if (millis() - stateStartTime >= LID_CLOSE_AFTER_TAKEN_MS)
            {
                oled_show_message("Closing lid", "Please wait...", "", "");
                closeLid();
                enterState(STATE_DOSE_TAKEN_CLOSING);
            }

            break;
        }

        case STATE_DOSE_TAKEN_CLOSING:
        {
            if (!servoMoving)
            {
                startStabilityTask("after dose");
                enterState(STATE_DOSE_TAKEN_TARING);
            }

            break;
        }

        case STATE_DOSE_TAKEN_TARING:
        {
            if (stabilityResultReady)
            {
                finishTare("after dose");
                stabilityResultReady = false;

                possibleDoseTaken = false;
                resetButtonPressTracking();

                setReadyLight();
                oled_show_message("Smart Pill Box", "Ready", "Press button", "");

                enterState(STATE_READY);
            }

            break;
        }

        case STATE_DOSE_MISSED_CLOSING:
        {
            if (!servoMoving)
            {
                possibleDoseTaken = false;
                resetButtonPressTracking();

                setReadyLight();
                oled_show_message("Smart Pill Box", "Ready", "Press button", "");

                enterState(STATE_READY);
            }

            break;
        }

        default:
        {
            resetButtonPressTracking();
            setReadyLight();
            enterState(STATE_READY);
            break;
        }
    }
}

//  Setup 

void setup()
{
    Serial.begin(115200);

    Serial.println();
    Serial.println("Smart Pill Box Non-Blocking Prototype");
    Serial.println("-----------------");

    pinMode(BUTTON_PIN, INPUT_PULLUP);
    pinMode(BUZZER_PIN, OUTPUT);

    pinMode(RGB_RED_PIN, OUTPUT);
    pinMode(RGB_GREEN_PIN, OUTPUT);
    pinMode(RGB_BLUE_PIN, OUTPUT);

    setLedOff();
    digitalWrite(BUZZER_PIN, LOW);

    Wire.begin(SDA_PIN, SCL_PIN);
    Wire.setClock(100000);

    setup_oled();
    oled_show_message("Smart Pill Box", "Starting...", "", "");

    Serial.println("Checking RTC...");
    rtcOk = rtcDetected();

    if (rtcOk)
    {
        Serial.print("RTC found at 0x");
        Serial.println(DS3231_ADDR, HEX);
    }
    else
    {
        Serial.println("RTC not found. Continuing without RTC.");
        oled_show_message("RTC not found", "Continuing...", "", "");
    }

    Serial.println("Starting HX711...");
    hx711.begin();

    Serial.println("Starting servo...");
    lidServo.setPeriodHertz(50);
    lidServo.attach(SERVO_PIN, 500, 2400);
    lidServo.write(SERVO_CLOSED_ANGLE);
    currentServoAngle = SERVO_CLOSED_ANGLE;
    targetServoAngle = SERVO_CLOSED_ANGLE;
    servoMoving = false;

    startWifiConnect();

    enterState(STATE_STARTUP_TARING);
}

//  Loop 

void loop()
{
    updateButton();
    updateWifi();
    updateNtpSync();
    updateAutomaticReminder();
    updateNetworkQueue();
    updateWeightSampler();
    updateStabilityTask();
    updateServo();
    updateBuzzer();
    updateStateMachine();
}
