#include <WiFiNINA.h>
#include "secrets.h"
#include "ThingSpeak.h"
#include <DHT.h>

#define DHTPIN 5
#define DHTTYPE DHT11

const int LDR_PIN = A0;
const int LED_PIN = 12;

const unsigned long CHANNEL_NUMBER = SECRET_CH_ID;
const char* WRITE_API_KEY = SECRET_WRITE_APIKEY;

const unsigned long THINGSPEAK_UPDATE_INTERVAL_MS = 30000;
const unsigned long BLINK_TIME_MS = 500;

char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASS;

WiFiClient client;
DHT dht(DHTPIN, DHTTYPE);

struct Sensor_Readings
{
    int light_level;
    float humidity;
    float temperature;
};

void setup()
{
    setup_serial();
    setup_pins();
    setup_wifi_module();
    setup_thingspeak();
    setup_sensors();
}

void loop()
{
    check_wifi_connect();

    Sensor_Readings readings;

    if (!read_sensors(readings))
    {
        Serial.println("Failed to read from DHT sensor!");
        delay(THINGSPEAK_UPDATE_INTERVAL_MS);
        return;
    }

    print_readings(readings);
    upload_to_thingspeak(readings);
    blink_led();

    delay(THINGSPEAK_UPDATE_INTERVAL_MS);
}

void setup_serial()
{
    Serial.begin(115200);
    while (!Serial)
    {
        ;
    }

    Serial.println("System starting...");
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

void setup_thingspeak()
{
    ThingSpeak.begin(client);
}

void setup_sensors()
{
    dht.begin();
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
    readings.light_level = analogRead(LDR_PIN);
    readings.humidity = dht.readHumidity();
    readings.temperature = dht.readTemperature();

    if (isnan(readings.humidity) || isnan(readings.temperature))
    {
        return false;
    }

    return true;
}

void print_readings(const Sensor_Readings& readings)
{
    Serial.print("Light Level: ");
    Serial.println(readings.light_level);

    Serial.print("Humidity: ");
    Serial.print(readings.humidity);
    Serial.print("%  Temperature: ");
    Serial.print(readings.temperature);
    Serial.println(" C");
}

bool upload_to_thingspeak(const Sensor_Readings& readings)
{
    ThingSpeak.setField(1, readings.temperature);
    ThingSpeak.setField(2, readings.light_level);
    ThingSpeak.setField(3, readings.humidity);

    ThingSpeak.setStatus("Temp, light and humidity updated");

    int responseCode = ThingSpeak.writeFields(CHANNEL_NUMBER, WRITE_API_KEY);

    if (responseCode == 200)
    {
        Serial.println("Channel update successful.");
        return true;
    }

    Serial.print("Problem updating channel. HTTP error code ");
    Serial.println(responseCode);
    return false;
}

void blink_led()
{
    digitalWrite(LED_PIN, HIGH);
    delay(BLINK_TIME_MS);
    digitalWrite(LED_PIN, LOW);
    delay(BLINK_TIME_MS);
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
}