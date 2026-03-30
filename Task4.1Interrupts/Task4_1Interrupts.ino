#include <Arduino.h>
#include <Wire.h>
#include <hp_BH1750.h>

// function prototypes (ensures correct compilation order)
void sliderISR();
void pirISR();
void pin_setup();
void handle_slider_event();
void handle_pir_event();
void turn_on_lights();
void update_lights();
void monitor_sensors();

// pin definitions
const int slider_pin = 2;
const int pir_pin = 3;
const int porch_led = 12;
const int hallway_led = 10;

// light state tracking
bool porch_on = false;
bool hallway_on = false;
bool light_sensor_working = false;

// light timers
unsigned long porch_start = 0;
unsigned long hallway_start = 0;

// BH1750 light sensor
hp_BH1750 lightMeter;
const float DARKNESS_THRESHOLD = 20.0;

// debounce state tracking
bool last_slider_state = HIGH;
unsigned long slider_change_time = 0;
const unsigned long debounce_delay = 100; // debounce time in ms

// interrupt flags
volatile bool slider_event = false;
volatile bool pir_event = false;

// interrupt service routines
void sliderISR()
{
  slider_event = true; // flag that slider changed
}

void pirISR()
{
  pir_event = true; // flag that motion detected
}

void setup()
{
  Serial.begin(115200);

  unsigned long serial_start = millis();
  while (!Serial && millis() - serial_start < 3000) {} // non-blocking

  pin_setup();

  last_slider_state = digitalRead(slider_pin); // store initial slider state

  light_sensor_working = lightMeter.begin(BH1750_TO_GROUND); // attempt to initialize the BH1750 sensor

  if (light_sensor_working)
  {
    Serial.println("BH1750 initialized successfully!");
  }
  else
  {
    Serial.println("Error: BH1750 not detected!");
  }

  attachInterrupt(digitalPinToInterrupt(slider_pin), sliderISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(pir_pin), pirISR, RISING);

  Serial.println("Ready to go!");
}

void loop()
{
  monitor_sensors();
  handle_slider_event();
  handle_pir_event();
  update_lights();
}

void pin_setup()
{
  pinMode(slider_pin, INPUT_PULLUP);
  pinMode(pir_pin, INPUT);

  pinMode(porch_led, OUTPUT);
  pinMode(hallway_led, OUTPUT);

  digitalWrite(porch_led, LOW);
  digitalWrite(hallway_led, LOW);
}

// state-confirmed software debouncing to prevent rapidly repeating interrupt triggers
bool debounce_slider()
{
  static bool debounce_happening = false; // flag to track debouncing

  if (slider_event) // check a slider event has occurred
  {
    noInterrupts(); // pause interrupts
    slider_event = false; // update the flag safely
    interrupts(); // resume interrupts

    slider_change_time = millis(); // store current time
    debounce_happening = true; // set debounce flag
  }

  if (debounce_happening && (millis() - slider_change_time >= debounce_delay)) // check debounce is happening and the delay has passed
  {
    int current_state = digitalRead(slider_pin); // read the current slider state (now that it has stabilized)
    debounce_happening = false; // reset the flag

    if (current_state != last_slider_state) // if the state has changed from last confirmed state
    {
      last_slider_state = current_state; // update the stored stable state
      return true; // return true to indicate a valid state change
    }
  }
  return false; // return false if no valid change was detected
}

void handle_slider_event()
{
  if (debounce_slider()) // check the debounce function returned true
  {
    if (last_slider_state == LOW) // if the slider was switched to active
    {
      Serial.println("Slider switched ON. Lights turned ON!");
      turn_on_lights(); // turn on the lights
    }
    else
    {
      Serial.println("Slider switched OFF!");
    }
  }
}

void handle_pir_event()
{
  if (pir_event) // check motion detection event flag 
  {
    noInterrupts(); // pause interrupts
    pir_event = false; // update the flag safely
    interrupts(); // resume interrupts

    if (!light_sensor_working) // if the BH1750 is not working
    {
      Serial.println("Motion detected, but BH1750 is not working!");
      return;
    }

    lightMeter.start(); // light sensor begin reading
    float lux = lightMeter.getLux();

    Serial.print("Motion detected. Current light level: ");
    Serial.print(lux);
    Serial.println(" lux");

    if (lux < DARKNESS_THRESHOLD) // check if the light reading is below the threshold
    {
      Serial.println("It's dark. Lights turned ON automatically");
      turn_on_lights(); // turn on the lights
    }
    else
    {
      Serial.println("It's not dark enough. Keep lights OFF");
    }
  }
}

void turn_on_lights()
{
  digitalWrite(porch_led, HIGH);
  digitalWrite(hallway_led, HIGH);

  porch_start = millis();
  hallway_start = millis();

  porch_on = true;
  hallway_on = true;
}

void update_lights()
{
  unsigned long current_time = millis(); // read current time

  if (porch_on && current_time - porch_start >= 30000) // if porch light been on for 30 secs
  {
    digitalWrite(porch_led, LOW); // turn it off
    porch_on = false; // set flag
    Serial.println("Porch light turned OFF after 30 secs");
  }

  if (hallway_on && current_time - hallway_start >= 60000) // if hallway light been on for 60 secs
  {
    digitalWrite(hallway_led, LOW); // turn it off
    hallway_on = false; // set flag
    Serial.println("Hallway light turned OFF after 60 secs");
  }
}

void monitor_sensors()
{
  static unsigned long last_reading = 0;

  if (millis() - last_reading >= 1000) // print every 1 second
  {
    last_reading = millis(); // reset timer to current time

    int pir_state = digitalRead(pir_pin); // PIR polling

    Serial.print("PIR: ");
    Serial.print(pir_state);

    if (light_sensor_working) // check if the BH1750 has failed
    {
      lightMeter.start(); // light sensor begin reading
      float lux = lightMeter.getLux();

      Serial.print(" | Lux: ");
      Serial.println(lux);
    }
    else
    {
      Serial.println(" | Lux: n/a");
    }
  }
}
