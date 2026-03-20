# Task 2.2 – IFTTT Trigger / Notification System (I2C Sensor)

For this task, I have utilized my Arduino Nano 33 IoT with WiFi capability using the WiFiNINA library so that the device can connect to the internet and send trigger events to IFTTT using Webhooks. The circuit makes use of the following components:

- an XC4496 magnetometer connected via I2C (SDA/SCL) to measure magnetic field strength
- the Arduino Nano 33 IoT built-in WiFi module for network communication
- the built-in LED to indicate when a trigger event occurs
- WiFi credentials and IFTTT Webhook key stored in `secrets.h`
- the Adafruit HMC5883 library to interface with the magnetometer

The original objective of this task was to measure sunlight exposure using a BH1750 light sensor and trigger notifications when sunlight begins and ends. As the BH1750 I received was not working, I sourced a XC4496 magnetometer from my local Jaycar as an alternative I2C sensor to demonstrate the same trigger and notification mechanism. Instead of detecting light levels, the system detects changes in magnetic field strength. The same threshold-based trigger logic applies and would work identically with a BH1750 using lux values.

The objective of this program is to continuously read sensor values, determine whether a threshold condition has been met, and send a notification via IFTTT when the condition changes state (magnetic field detected / lost). The LED blinks to provide a visual indication that a trigger event has been sent.

Our code begins by including the required libraries for WiFi, I2C communication, and the magnetometer sensor. It then defines the threshold, timing values, and IFTTT configuration. A struct (`Sensor_Readings`) is used to store the X, Y, Z magnetic field values and the calculated magnitude.

### setup()

Prepares the system for operation by calling a series of setup functions:
- `setup_serial()` starts serial communication at 115200 baud so that messages and readings can be viewed in the Serial Monitor
- `setup_pins()` configures the built-in LED as an output and ensures it starts in the OFF state
- `setup_wifi_module()` checks whether the WiFi module is there and is working correctly
- `setup_sensors()` initializes the magnetometer over I2C

### main loop()

The main program loop repeatedly performs the following steps:
- `check_wifi_connect()` ensures the nano is connected to WiFi and reconnects if required
- creates a `Sensor_Readings` variable called `readings` to store the latest sensor values
- `read_sensors(readings)` reads the magnetometer data and calculates the magnetic field magnitude
- if the sensor data is invalid, an error message is printed and the loop continues
- `print_readings(readings)` displays the sensor values in the Serial Monitor
- `process_trigger(readings)` checks whether the threshold condition has changed and sends an IFTTT event if required
- waits for a short delay before repeating

### setup_serial()

  - starts serial communication with 115200 baud
  - waits for the Serial Monitor to be ready
  - prints a startup message

### setup_pins()

  - sets `LED_PIN` (Built-in LED) as an OUTPUT
  - ensures the LED is initially turned OFF

### setup_wifi_module()

  - checks whether the WiFi hardware is available
  - if no WiFi module is detected, it prints an error message and stops the program (putting it into an infinite loop until the WiFi module is deteced).

### setup_sensors()

- initializes I2C communication using `Wire.begin()`
- attempts to initialize the magnetometer sensor
- if the sensor is not detected, an error message is printed and the program stops

### check_wifi_connect()

- checks whether the nano is already connected to WiFi
- if connected, the function returns and the program continues
- if not connected, it prints the SSID it is trying to connect to
- repeatedly calls WiFi.begin(ssid, pass) until a connection is established
- once connected, it prints a confirmation message and WiFi status

### read_sensors()

- reads magnetic field values from the sensor (X, Y, Z axes)
- calculates the total magnetic field magnitude (magnitude = √(x² + y² + z²))
- stores all values in the `Sensor_Readings` struct
- checks for invalid (NaN) values
- returns false if the sensor reading failed, otherwise returns true

### print_readings()

- prints the X, Y, and Z magnetic field values in microtesla (µT) units
- prints the calculated magnitude

This allows the user to monitor sensor data locally and verify correct operation.

### process_trigger()

- compares the calculated magnitude against the predefined threshold
- determines whether a “magnet present” or “magnet removed” condition exists
- uses a boolean variable to track the previous state
- uses a cooldown timer to prevent repeated triggers from small fluctuations
If the magnetic field rises above the threshold:
- the system detects a new event
- prints a message to Serial
- calls `send_ifttt_event()` with the detection event
- blinks the LED
If the magnetic field drops below the threshold:
- the system detects a removal event
- prints a message to Serial
- calls `send_ifttt_event()` with the detection event
- blinks the LED

This same logic would be used in the original task with a BH1750 sensor, where lux values would replace magnetic magnitude to detect sunlight conditions.

### send_ifttt_event()

- builds a Webhooks URL using the event name and IFTTT key
- formats the sensor value into a query string (`value1`, `value2`)
- connects to the IFTTT server (`maker.ifttt.com`) over HTTP
- sends a GET request containing the event and data
- reads and prints the server response
- closes the connection

This function is responsible for sending the trigger event to IFTTT, which then generates the final notification.

### blink_led()

  - turns the LED ON
  - waits 500 ms
  - turns the LED OFF
  - waits another 500 ms

This provides a simple visual indication that a trigger event has occurred.

### print_wifi_status()

  - prints the connected WiFi SSID
  - prints the local IP address assigned to the board
  - prints the WiFi signal strength in dBm

This helps confirm that the board is properly connected to the network and able to communicate with IFTTT.
