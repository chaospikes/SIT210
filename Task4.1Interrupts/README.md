
# Task 4.1 – Task 4.1P Handling Interrupts (Porch and Hallway Lights)

For this task, I have utilized my Arduino Nano 33 IoT together with the following components:

- A push button acting as a backup manual switch
- A PIR motion sensor for motion detection
- An Adafruit BH1750 (I2C) light sensor for ambient light measurement
- A blue LED to simulate the porch light (with 82Ω pull-down resistor)
- An orange LED to simulate the hallway light (with 120Ω pull-down resistor)

The system uses hardware interrupts to detect both button presses and motion events, ensuring immediate response without constant polling.

### Expected system behaviour

When motion is detected:
- The PIR interrupt triggers
- The system checks if it is dark using the BH1750
- If it is dark (below the darkness threshold) then both lights turn on.
When the button is pressed:
- The button interrupt triggers
- Both lights turn on regardless of light level
- Lights automatically turn off after their time durations
Serial monitor provides:
- PIR state
- Lux level
- Event notifications

### setup()

The `setup()` function initializes the system:

- Serial communication is started at 115200 baud for debugging and monitoring
- The `pin_setup()` function is called to configure I/O pins
- The BH1750 light sensor is initialized
- If initialization fails, an error message is printed and the system halts
- Button interrupt is attached using CHANGE
- PIR interrupt is attach using RISING

### loop()

The main loop continuously runs four functions:

- `monitor_sensors()` - continuously prints our sensor values to the serial monitor
- `handle_button_event()` - handles button push interrupts
- `handle_pir_event()` - handles motion detection interrupts
- `update_lights()` - handles the lights and timers

### pin_setup()

- Button is set as INPUT_PULLUP so it reads HIGH normally and LOW when pressed
- PIR sensor is set as INPUT
- LED's are set as OUTPUT
- LED's are turned off initially

### monitor_sensors()

- Runs every 1 second using millis() timing
- Reads the PIR state
- Starts a BH1750 reading and retrieves the lux value
- Prints PIR state and lux value to the serial monitor

### sliderISR() and pirISR()

These are our interrupt service routines:

- `sliderISR()` sets button_event = true
- `pirISR()` sets pir_event = true

We follow best practice for interrupt handling by doing no heavy processing inside the ISR.

### handle_button_event()

- Checks if button_event is true
- Temporarily disables interrupts to safely reset the flag
- Reads the button state to confirm it is pressed
- Turns on both lights
- Prints a notification to the serial monitor

### handle_pir_event()

- Checks if pir_event is true
- Temporarily disables interrupts to safely reset the flag
- Reads the current lux value from the BH1750
- Prints the detected light level to the serial monitor
- Checks if the lux reading is below the threshold:
  -- If it is then turns lights on and prints a notification
  -- If not then the lights stay off and prints a notification 

### turn_on_lights()

- Turns on both porch and hallway LED's
- Stores the current time using millis() for each light
- Sets porch_on and hallway_on flags to true

### update_lights()

- Continuously checks elapsed time using millis()
- Porch light turns OFF after 30 seconds and prints a notification
- Hallway light turns OFF after 60 seconds and prints a notification

