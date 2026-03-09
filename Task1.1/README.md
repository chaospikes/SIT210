# Task 1.1 – Lights ON

For this task, I have utilized an Arduino Uno (my Nano cable is faulty and I need to get a new one). I have connected the following using a breadboard:
  * a momentary (tactile) switch (D2)
  * an orange LED to simulate the porch light (D10)
  * a blue LED to simulate the hallway light (D12)
  * an 82Ω pull-down resistor for the blue LED
  * a 150Ω pull-down resistor for the Orange LED
  * necessary jumper cables

The button will make use of the Arduino's internal pull-up resistor to keep the pin stable when the button is not pressed, therefore we do not require a pull-down resistor for the button. This means the state of the button is HIGH when not pressed and LOW when pressed.

Our objective is to design a simple program that will simulate switching on Linda's porch and hallways lights when a button is pushed. The porch light (LED) will stay on for 30 seconds and the hallway light (LED) will stay on for 60 seconds.

Our code begins with setting our pin numbers for the buttons and LED's and declaring variables for the light timers and states.

### setup()
  - calls 'pin_setup()' to configure input/output pins. We could put this config directly underneath 'setup()' but our way is more aligned to a modular approach.
### loop()
the main program loop simply calls 2 functions repeatedly
  - 'check_button()' constantly checks for button pushes
  - 'update_lights()' checks timing and turns off the lights at the correct time
### pin_setup()
  - sets the button pin to 'INPUT_PULLUP'
  - sets the LED pins to OUTPUT
### check_button()
  - condition check on the button state from D2
  - if the button is pressed (LOW) then 'turn_on_lights()' is called
### turn_on_lights()
  - turn both LED's on (HIGH)
  - stores the start time for each light using 'millis()'
  - set state for 'porch_on' and 'hallway_on' to true to track the timers
### update_lights()
  - continuously check elapsed time using 'millis()'
  - condition check if the porch light is on and 30 seconds have passed, when true it turns off porch LED
  - condition check if the hallway light is on and 60 seconds have passed, when true it turns off hallway LED
