// set pins, constants and other variables
int button_pin = 2;
int porch_led = 10;
int hallway_led = 12;

unsigned long porch_start = 0;
unsigned long hallway_start = 0;

bool porch_on = false;
bool hallway_on = false;

// call our pin setup funtion
void setup() {
  pin_setup();
}

// main program loop checks for button pushes and call the light function(s)
void loop() {
  check_button();
  update_lights();
}

// pin setup function
void pin_setup() {
  pinMode(button_pin, INPUT_PULLUP);
  pinMode(porch_led, OUTPUT);
  pinMode(hallway_led, OUTPUT);
}

// function to check for button pushes and call the light function(s)
void check_button() {
  if (digitalRead(button_pin) == LOW)
  {
    turn_on_lights();
  }
}

// function to turn on the lights and start the timers
void turn_on_lights() {
  digitalWrite(porch_led, HIGH);
  digitalWrite(hallway_led, HIGH);
  porch_start = millis();
  hallway_start = millis();
  porch_on = true;
  hallway_on = true;
}

// function to check the light timers and turn lights off
void update_lights() {
  unsigned long current_time = millis();
  if (porch_on && current_time - porch_start >= 30000)
  {
    digitalWrite(porch_led, LOW);
    porch_on = false;
  }
  if (hallway_on && current_time - hallway_start >= 60000)
  {
    digitalWrite(hallway_led, LOW);
    hallway_on = false;
  }
}