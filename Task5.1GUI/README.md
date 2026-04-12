# Task 5.1 – Making a Graphical User Interface

For this task, I have utilised my Raspberry Pi 4B running python together with the following components:

- The `gpiozero` library to control GPIO output pins  
- The `PyQt5` library to create a graphical user interface  
- A LED connected to GPIO 17 to simulate the living room light  
- A LED connected to GPIO 27 to simulate the bathroom light  
- A LED connected to GPIO 22 to simulate the closet light  

The system uses a desktop GUI with radio buttons to allow the user to select which room light should be turned on.

---

## Expected System Behaviour

### GUI Control:
- The GUI window displays the title **Linda's House**
- Three radio buttons are shown:
  - Living Room
  - Bathroom
  - Closet
- A **Close** button is also displayed  

### Light Selection:
- When the **Living Room** radio button is selected, the living room LED turns ON
- When the **Bathroom** radio button is selected, the bathroom LED turns ON
- When the **Closet** radio button is selected, the closet LED turns ON

### Light Logic:
- Only one light can be ON at a time
- Whenever a new room is selected, all LEDs are first turned OFF
- The selected room LED is then turned ON

### Closing the Application:
- When the **Close** button is pressed:
  - All LEDs are turned OFF
  - The application window closes

---

## GPIO Pin Setup

The following GPIO pins are used for output:

- GPIO 17 -> Living room LED  
- GPIO 27 -> Bathroom LED  
- GPIO 22 -> Closet LED  

Each GPIO pin is controlled using a `gpiozero.LED` object:

- `living_led = LED(17)`
- `bath_led = LED(27)`
- `closet_led = LED(22)`

---

## Class: `ControlLights(QMainWindow)`

This class defines the main GUI window and contains the logic for the light controller.

### `__init__()`

- Calls the parent `QMainWindow` constructor
- Sets the window size and position using `setGeometry()`
- Sets the window title to **Light Controller**
- Calls `initUI()` to build the interface
- Calls `connectSignals()` to connect user actions to program functions

---

## `initUI()`

This function builds the graphical user interface.

- Creates a central widget for the main window
- Creates a vertical box layout (`QVBoxLayout`)
- Adds a label displaying **Linda's House**
- Creates three radio buttons:
  - Living Room
  - Bathroom
  - Closet
- Creates a **Close** push button
- Adds all widgets to the layout
- Sets the completed layout as the central widget of the main window

---

## `connectSignals()`

This function connects GUI events to their corresponding program actions.

- Connects the `toggled` signal of each radio button to `updateLights()`
- Connects the `clicked` signal of the Close button to `closeApp()`

This allows the program to respond immediately whenever the user selects a different room or closes the application.

---

## `updateLights()`

This function controls the LEDs based on which radio button is currently selected.

### Behaviour:
- Turns all LEDs OFF first
- Checks which radio button is selected
- Turns ON only the LED for the selected room

### Logic:
- If **Living Room** is selected, `living_led.on()` is called
- Else if **Bathroom** is selected, `bath_led.on()` is called
- Else if **Closet** is selected, `closet_led.on()` is called

This ensures that only one light is active at any time.

---

## `closeApp()`

This function is called when the user presses the **Close** button.

### Behaviour:
- Turns OFF all LEDs
- Closes the application window

This ensures the system shuts down cleanly and no lights are left on when the GUI exits.

---

## `main()`

The `main()` function starts the application.

### Behaviour:
- Creates a `QApplication` object
- Creates an instance of the `ControlLights` window
- Displays the window using `show()`
- Starts the PyQt event loop using `app.exec_()`
