# Task 4.1 – Handling Interrupts (Porch and Hallway Lights)

For this task, I have utilized my Arduino Nano 33 IoT together with the following components:

- A slider switch acting as a manual override input  
- A PIR motion sensor for motion detection  
- An Adafruit BH1750 (I2C) light sensor for ambient light measurement  
- A blue LED to simulate the porch light (with 82Ω pull-down resistor)
- An orange LED to simulate the hallway light (with 120Ω pull-down resistor)

The system uses hardware interrupts to detect both slider changes and motion events, allowing responsive behaviour without constant polling.

---

## Expected System Behaviour

### Motion Detection:
- PIR interrupt triggers  
- System reads ambient light level using BH1750  
- If light level is below threshold then both lights turn ON  

### Manual Override (Slider switch):
- Slider interrupt triggers on state change  
- If switched ON, lights turn ON regardless of light level  
- If switched OFF, no action (lights continue normal timing behaviour)  

### Light Control:
- Porch light turns OFF after 30 seconds  
- Hallway light turns OFF after 60 seconds  

### Serial Monitor Output:
- PIR state  
- Lux readings  
- Event notifications

---

## setup()

- Initializes serial communication at 115200 baud  
- Uses a non-blocking timeout for Serial
- Calls `pin_setup()` to configure I/O
- The BH1750 light sensor is initialized
- If initialization fails, an error message is printed but system continues running without light sensing  
- Attaches interrupts:
  - Slider interrupt attached using `CHANGE`
  - PIR interrupt attached using `RISING`  

## loop()

The main loop continuously runs four functions, non-blocking:

- `monitor_sensors()` - continuously prints our sensor values to the serial monitor
- `handle_button_event()` - handles debounced slider interrupts
- `handle_pir_event()` - handles motion detection interrupts
- `update_lights()` - handles the lights and timers

## pin_setup()

- The slider pin is set as `INPUT_PULLUP`, the slider reads `HIGH` normally and `LOW` when switched to the active position  
- PIR sensor pin set as `INPUT`  
- LED pins set as `OUTPUT`  
- LED's are set to `LOW` initially so the lights start in the OFF state  

## monitor_sensors()

- Runs every 1 second using `millis()` timing  
- Reads the current PIR state  
- Prints the PIR state to the serial monitor  
- If the light sensor is working, reads and prints the lux value  
- If the sensor is unavailable, prints that lux is unavailable instead

## sliderISR() and pirISR()

These are our interrupt service routines:

- `sliderISR()` sets slider_event = true
- `pirISR()` sets pir_event = true

We follow best practice for interrupt handling by doing no heavy processing inside the ISR.

## debounce_slider()

The slider switch produces contact bounce, causing multiple rapid interrupt triggers. We solved this using state-confirmed software debouncing and separated the logic into a debounce function.

- Checks if slider_event is true (set by the interrupt)
- Temporarily disables interrupts to safely reset the flag
- Starts a debounce timer using `millis()`
- Waits until the debounce delay has passed to allow the signal to stabilise
- Reads the current slider state after the delay
- Compares the current state with the last confirmed stable state
- If the state has changed:
  * Updates the last known state
  * Returns true to indicate a valid slider event
- If no valid change is detected:
  * Returns false
 
## bh1750_connected()

- Use Wire.beginTransmission to initiate communication with the BH1750 using its I2C address
- Check if the status code returned is 0, returns true if the device acknowledges (ACK), meaning it is present on the bus

### handle_slider_event()

- Calls `debounce_slider()` to check for a valid, stable slider change
- If a valid change is detected:
  * Checks the current slider state
  * If the slider is ON (LOW):
    - Turns on both lights
    - Prints a notification to the serial monitor
  * If the slider is OFF:
    - Prints a notification to the serial monitor

## handle_pir_event()

- Checks whether a PIR interrupt event has occurred  
- Temporarily disables interrupts to safely reset the flag
- Verifies that the light sensor is still working  
- Reads the current lux value from the BH1750  
- Prints the detected light level to the serial monitor  
- If the lux value is below the threshold, both lights are turned ON  
- Otherwise, the lights remain OFF  

## turn_on_lights()

- Turns on both porch and hallway LED's
- Stores the current time using millis() for each light
- Sets porch_on and hallway_on flags to true

## update_lights()

- Uses non-blocking timing with `millis()`  
- Checks how long each light has been active  
- Turns the porch light OFF after 30 seconds  
- Turns the hallway light OFF after 60 seconds  
- Prints notifications when each light turns OFF  
