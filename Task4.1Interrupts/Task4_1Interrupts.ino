#include <Arduino.h>
#include <Wire.h>
#include <hp_BH1750.h>

const int button_pin = 2;
const int pir_pin = 3;
const int porch_led = 12;
const int hallway_led = 10;

bool porch_on = false;
bool hallway_on = false;

unsigned long porch_start = 0;
unsigned long hallway_start = 0;

// light sensor
hp_BH1750 lightMeter;

const float DARKNESS_THRESHOLD = 20.0;

// interrupt flags
volatile bool button_event = false;
volatile bool pir_event = false;

// interrupt service routines
void buttonISR()
{
  button_event = true;
}

void pirISR()
{
  pir_event = true;
}

void setup()
{
  Serial.begin(115200);
  while (!Serial);

  pin_setup();

  bool init = lightMeter.begin(BH1750_TO_GROUND); // attempt to initialize the BH1750 sensor

  if (init) // check the flag if the BH1750 was initialized successfully
  {
    Serial.println("BH1750 initialized successfully!");
  }
  else
  {
    Serial.println("Error: BH1750 not detected!");
    while (true) {}
  }

  attachInterrupt(digitalPinToInterrupt(button_pin), buttonISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(pir_pin), pirISR, RISING);

  Serial.println("Ready to go!");
}

void loop()
{
  monitor_sensors();
  handle_button_event();
  handle_pir_event();
  update_lights();
}

void pin_setup()
{
  pinMode(button_pin, INPUT_PULLUP);
  pinMode(pir_pin, INPUT);
  pinMode(porch_led, OUTPUT);
  pinMode(hallway_led, OUTPUT);

  digitalWrite(porch_led, LOW);
  digitalWrite(hallway_led, LOW);
}

void handle_button_event()
{
  if (button_event) // check button push event flag
  {
    noInterrupts(); // pause interrupts
    button_event = false; // update the flag safely
    interrupts(); // resume interrupts

    if (digitalRead(button_pin) == LOW) // check the button was pushed
    {
      Serial.println("Backup button pushed. Lights turned ON");
      turn_on_lights(); // turn on the lights
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

    lightMeter.start(); // light sensor begin reading
    float lux = lightMeter.getLux();

    Serial.print("PIR: ");
    Serial.print(pir_state);

    Serial.print(" | Lux: ");
    Serial.print(lux);
  }
}
