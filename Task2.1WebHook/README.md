# Task 2.1 – WebHook / ThingSpeak Sensor Upload

For this task, I have utilized an Arduino board with WiFi capability using the WiFiNINA library so that the device can connect to the internet and send sensor readings to ThingSpeak. The circuit makes use of the following components:

  - a DHT11 sensor connected to D5 to measure temperature and humidity
  - an LDR (light dependent resistor) connected to A0 to measure light level
  - an LED connected to D12 to indicate successful upload
  - WiFi credentials and ThingSpeak channel details stored in secrets.h
  - the ThingSpeak library to send data to the cloud platform

The objective of this program is to read environmental sensor values from the DHT11 and LDR modules, then upload the readings to my ThingSpeak channel every 30 seconds. The LED blinks briefly after each cycle to visually indicate that the system is active.

Our code begins by including the required libraries for WiFi, ThingSpeak, and the DHT sensor. It then defines the pin numbers, channel credentials, and timing intervals. I used a struct (Sensor_Readings) to neatly store the current light level, humidity, and temperature values together.

### setup()

`setup()` prepares the system for operation by calling a series of setup functions:

  - setup_serial() starts serial communication at 115200 baud so that messages and readings can be viewed in the Serial Monitor
  - setup_pins() configures the LED pin as an output and ensures it starts in the OFF state
  - setup_wifi_module() checks whether the WiFi module is present and working correctly
  - setup_thingspeak() initializes communication with ThingSpeak using the WiFi client
  - setup_sensors() starts the DHT11 sensor

### main loop()

The main program loop repeatedly performs the following steps:

  - check_wifi_connect() checks whether the board is connected to WiFi and reconnects if necessary
  - creates a Sensor_Readings variable called readings to store the latest sensor values
  - read_sensors(readings) reads the LDR and DHT11 values and stores them in the structure
  - if the DHT sensor fails to return valid humidity or temperature values, an error message is printed and the system waits for the next update interval
  - print_readings(readings) displays the sensor values in the Serial Monitor
  - upload_to_thingspeak(readings) sends the temperature, light level, and humidity values to ThingSpeak fields
  - blink_led() briefly flashes the LED to indicate that the cycle has completed
  - waits 30 seconds before repeating the process

### setup_serial()

  - starts serial communication with Serial.begin(115200)
  - uses while (!Serial) to wait until the serial monitor is ready
  - prints "System starting..." as a startup message

### setup_pins()

  - sets LED_PIN as an OUTPUT
  - ensures the LED is initially turned OFF with digitalWrite(LED_PIN, LOW)

### setup_wifi_module()

  - checks whether the WiFi hardware is available using WiFi.status()
  - if no WiFi module is detected (WL_NO_MODULE), it prints an error message and stops the program in an infinite loop

### setup_thingspeak()

  - initializes ThingSpeak communication using ThingSpeak.begin(client)

### setup_sensors()

  - starts the DHT11 sensor with dht.begin()

#### check_wifi_connect()

  - checks whether the Arduino is already connected to WiFi
  - if connected, the function simply returns and the program continues
  - if not connected, it prints the SSID it is trying to connect to
  - repeatedly calls WiFi.begin(ssid, pass) until a connection is established
  - once connected, it prints a confirmation message and calls print_wifi_status()

### read_sensors()

  - reads the light level from the LDR using analogRead(LDR_PIN)
  - reads humidity from the DHT11 using dht.readHumidity()
  - reads temperature from the DHT11 using dht.readTemperature()
  - stores all three readings inside the Sensor_Readings structure
  - checks whether the humidity or temperature result is invalid using isnan()
  - returns false if the DHT sensor reading failed, otherwise returns true

### print_readings()

  - prints the light level to the Serial Monitor
  - prints the humidity value as a percentage
  - prints the temperature value in degrees Celsius
  - This allows the user to monitor the sensor readings locally before or while they are uploaded to ThingSpeak.

### upload_to_thingspeak()

  - assigns the sensor values to ThingSpeak fields:
      - Field 1 = temperature
      - Field 2 = light level
      - Field 3 = humidity
  - sets a status message for the update
  - sends the data to the ThingSpeak channel using ThingSpeak.writeFields(CHANNEL_NUMBER, WRITE_API_KEY)
  - checks the returned HTTP response code
  - if the response code is 200, it prints that the channel update was successful
  - otherwise, it prints the HTTP error code and reports a failed upload

### blink_led()

  - turns the LED ON
  - waits 500 ms
  - turns the LED OFF
  - waits another 500 ms

This provides a simple visual indication that the program has completed one reading/upload cycle.

### print_wifi_status()

  - prints the connected WiFi SSID
  - prints the local IP address assigned to the board
  - prints the WiFi signal strength in dBm

This helps confirm that the board is properly connected to the network.
