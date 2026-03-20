#include <WiFiNINA.h>
#include "secrets.h"
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_HMC5883_U.h>

const int LED_PIN = LED_BUILTIN;

const unsigned long TRIGGER_COOLDOWN_MS = 10000;
const unsigned long LOOP_DELAY_MS = 500;
const unsigned long BLINK_TIME_MS = 500;

const float MAGNET_THRESHOLD = 120.0;

const char HOST_NAME[] = "maker.ifttt.com";
const int HTTP_PORT = 80;

const char* IFTTT_KEY = SECRET_IFTTT_KEY;
const char* EVENT_MAGNET_DETECTED = "Magnetic_Field_Detected";
const char* EVENT_MAGNET_REMOVED = "Magnetic_Field_Lost";

char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASS;

WiFiClient client;
Adafruit_HMC5883_Unified magnetometer = Adafruit_HMC5883_Unified(12345);

struct Sensor_Readings
{
    float magnetic_x;
    float magnetic_y;
    float magnetic_z;
    float magnetic_magnitude;
};

bool magnet_detected = false;
unsigned long last_trigger_time = 0;

void setup()
{
    setup_serial();
    setup_pins();
    setup_wifi_module();
    setup_sensors();
}

void loop()
{
    check_wifi_connect();

    Sensor_Readings readings;

    if (!read_sensors(readings))
    {
        Serial.println("Failed to read from magnetometer!");
        delay(LOOP_DELAY_MS);
        return;
    }

    print_readings(readings);
    process_trigger(readings);

    delay(LOOP_DELAY_MS);
}

void setup_serial()
{
    Serial.begin(115200);
    while (!Serial)
    {
        ;
    }

    Serial.println("System starting...");
    Serial.println();
}

void setup_pins()
{
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
}

void setup_wifi_module()
{
    if (WiFi.status() == WL_NO_MODULE)
    {
        Serial.println("Communication with WiFi module failed!");
        while (true)
        {
            ;
        }
    }
}

void setup_sensors()
{
    Wire.begin();

    if (!magnetometer.begin())
    {
        Serial.println("Could not find a valid magnetometer!");
        while (true)
        {
            ;
        }
    }
}

void blink_led()
{
    digitalWrite(LED_PIN, HIGH);
    delay(BLINK_TIME_MS);
    digitalWrite(LED_PIN, LOW);
    delay(BLINK_TIME_MS);
}

void check_wifi_connect()
{
    if (WiFi.status() == WL_CONNECTED)
    {
        return;
    }

    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);

    while (WiFi.status() != WL_CONNECTED)
    {
        WiFi.begin(ssid, pass);
        Serial.print(".");
        delay(5000);
    }

    Serial.println();
    Serial.println("Connected to WiFi.");
    print_wifi_status();
}

bool read_sensors(Sensor_Readings& readings)
{
    sensors_event_t event;
    magnetometer.getEvent(&event);

    readings.magnetic_x = event.magnetic.x;
    readings.magnetic_y = event.magnetic.y;
    readings.magnetic_z = event.magnetic.z;

    readings.magnetic_magnitude = sqrt(readings.magnetic_x * readings.magnetic_x + readings.magnetic_y * readings.magnetic_y + readings.magnetic_z * readings.magnetic_z);

    if (isnan(readings.magnetic_x) || isnan(readings.magnetic_y) || isnan(readings.magnetic_z) || isnan(readings.magnetic_magnitude))
    {
        return false;
    }

    return true;
}

void print_readings(const Sensor_Readings& readings)
{
    Serial.print("Mag X: ");
    Serial.print(readings.magnetic_x);
    Serial.print(" uT,  Mag Y: ");
    Serial.print(readings.magnetic_y);
    Serial.print(" uT,  Mag Z: ");
    Serial.print(readings.magnetic_z);
    Serial.print(" uT,  MAGNITUDE: ");
    Serial.print(readings.magnetic_magnitude);
    Serial.println(" uT");
}

void process_trigger(const Sensor_Readings& readings)
{
    bool magnet_present = readings.magnetic_magnitude > MAGNET_THRESHOLD;
    unsigned long current_time = millis();

    if (magnet_present && !magnet_detected && current_time - last_trigger_time > TRIGGER_COOLDOWN_MS)
    {
        magnet_detected = true;
        last_trigger_time = current_time;

        Serial.println("Magnet detected.");
        send_ifttt_event(EVENT_MAGNET_DETECTED, "Magnetic_Field_Detected", String(readings.magnetic_magnitude, 2));
        blink_led();
    }

    if (!magnet_present && magnet_detected && current_time - last_trigger_time > TRIGGER_COOLDOWN_MS)
    {
        magnet_detected = false;
        last_trigger_time = current_time;

        Serial.println("Magnet removed.");
        send_ifttt_event(EVENT_MAGNET_REMOVED, "Magnetic_Field_Lost", String(readings.magnetic_magnitude, 2));
        blink_led();
    }
}

bool send_ifttt_event(const char* event_name, String value1, String value2)
{
    String path_name = "/trigger/" + String(event_name) + "/with/key/" + String(IFTTT_KEY);
    String query_string = "?value1=" + value1 + "&value2=" + value2;

    if (!client.connect(HOST_NAME, HTTP_PORT))
    {
        Serial.println("Connection to IFTTT failed.");
        return false;
    }

    client.println("GET " + path_name + query_string + " HTTP/1.1");
    client.println("Host: " + String(HOST_NAME));
    client.println("Connection: close");
    client.println();

    unsigned long timeout = millis();

    while (client.connected() && millis() - timeout < 5000)
    {
        while (client.available())
        {
            char c = client.read();
            Serial.print(c);
            timeout = millis();
        }
    }

    client.stop();
    Serial.println();
    Serial.println("Disconnected from IFTTT.");

    return true;
}

void print_wifi_status()
{
    Serial.print("SSID: ");
    Serial.println(WiFi.SSID());

    IPAddress ip = WiFi.localIP();
    Serial.print("IP Address: ");
    Serial.println(ip);

    long rssi = WiFi.RSSI();
    Serial.print("Signal strength (RSSI): ");
    Serial.print(rssi);
    Serial.println(" dBm");
    Serial.println();
}