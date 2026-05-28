# Smart Medication Dispenser Prototype

## Project overview

This project is a smart medication dispenser prototype. The system automatically reminds a user at 5:00 PM each day, opens the lid, monitors whether a dose has been removed, logs the result to ThingSpeak, and sends a Telegram alert via IFTTT if the dose is missed.

The prototype was developed for SIT210 Task 11.2PCDHD as an integrated embedded system demonstration.

## Main features

- Automatic daily medication reminder at 5:00 PM using the RTC
- Manual button override to start a medication reminder
- Servo-controlled pill box lid
- Load-cell based dose detection using HX711
- OLED user interface
- RGB LED status indication
- Buzzer feedback patterns
- DS3231 real-time clock support
- NTP time synchronisation over Wi-Fi
- ThingSpeak cloud logging
- IFTTT missed-dose Telegram alert
- Refill mode using a double button press
- Non-blocking state-machine program structure

## Hardware used

- XIAO ESP32-S3 Dev Board
- HX711 load-cell amplifier
- 100g load cell
- DS3231 RTC module
- SSD1306 I2C OLED display
- Servo motor
- RGB LED
- Buzzer
- Push button
- 3D printed enclosure

## Pin mapping

The current program uses the following pin definitions:

| Function | Pin |
|---|---|
| HX711 DOUT | D0 |
| HX711 SCK | D1 |
| Button | D2 |
| Buzzer | D3 |
| I2C SDA | D4 |
| I2C SCL | D5 |
| RGB LED red | D6 |
| RGB LED green | D7 |
| RGB LED blue | D8 |
| Servo | D9 |

## Cloud services

### ThingSpeak

The prototype uses ThingSpeak to log the medication result.

Field 1 values:

| Value | Meaning |
|---|---|
| 1 | Dose taken |
| 0 | Dose missed |

The program sends updates using `HTTPClient` and a ThingSpeak HTTP GET request.

### IFTTT

The prototype uses IFTTT Webhooks to send a Telegram alert when a dose is missed.

The IFTTT payload includes:

- User's name
- RTC timestamp
- Dose status

## Code explanation

A full explanation of the program is included in the included code_breakdown.txt file

## Program structure

The program is structured around a non-blocking state machine. Instead of using long `delay()` calls, the main `loop()` repeatedly calls update functions:

```cpp
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
```

Each update function performs a small amount of work and then returns control to the main loop. This keeps the device responsive while monitoring the button, RTC, load cell, servo, buzzer, Wi-Fi, and cloud upload tasks.

## Operating modes

### Ready mode

The system waits for either the automatic 5:00 PM reminder or user input. The OLED displays the current system status and load-cell reading. The RGB LED is green.

### Automatic medication reminder

At 5:00 PM each day, the system automatically starts the medication reminder if it is in the ready state. The lid opens, the buzzer sounds, the RGB LED changes to purple, and the system records a baseline weight.

The system records that the automatic reminder has already triggered for the current date so that it does not repeatedly reopen during the 5:00 PM minute.

### Manual override

A single button press can also start the medication reminder. This allows the user to manually open the dispenser if they missed the automatic reminder or need to take the dose manually.

### Medication taken

If the measured weight change is above the configured threshold and remains stable, the system records the dose as taken, logs the result to ThingSpeak, waits briefly, closes the lid, and re-tares the scale.

### Missed dose

If the user does not remove a dose before the reminder timeout expires, the system records a missed dose. The RGB LED turns red, the buzzer sounds, the missed dose is logged to ThingSpeak, and an Telegram alert via IFTTT is queued.

### Refill mode

A double button press opens refill mode. The lid opens so tablets can be added. Pressing the button again closes the lid and re-tares the scale so the new pill weight becomes the baseline for future reminders.

## Setup instructions (for device replication)

1. Open the `smart_medication_dispenser` folder in the Arduino IDE.
2. Install the required libraries:
   - Adafruit GFX Library
   - Adafruit SSD1306
   - Adafruit HX711
   - ESP32Servo
3. Install the ESP32 board package in Arduino IDE.
4. Copy `secrets_template.h` and rename the copy to `secrets.h`.
5. Enter your own Wi-Fi, ThingSpeak, and IFTTT credentials in `secrets.h`.
6. Connect the hardware using the pin mapping above.
7. Upload `smart_medication_dispenser.ino` to the ESP32-based board.
8. Open Serial Monitor at 115200 baud to observe debug output.

## Demonstration video Link

https://deakin.au.panopto.com/Panopto/Pages/Viewer.aspx?id=35aedf17-1603-45da-b348-b45800e7dfb8

## Limitations and future improvements

This prototype demonstrates the key embedded system behaviour, but it could be improved further by:

- Adding a user interface to change/add medication times without editing the code
- Adding a mobile app or web dashboard to allow user name and notification changes, and to open the dispenser remotely
- Improving the physical pill storage mechanism
- Improved container to hide all wires and connections within the dispenser side
- Using calibrated gram values instead of raw HX711 readings
- Adding battery operation and power management
- Adding multiple compartments for different medications
