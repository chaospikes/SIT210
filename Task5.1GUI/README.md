# Task 5.1 – Making a Graphical User Interface

For this task, I have utilised my Raspberry Pi 4B running Python together with the following components:

- The `gpiozero` library to control GPIO output pins  
- The `PyQt5` library to create a graphical user interface  
- A LED connected to GPIO 17 to simulate the living room light  
- A LED connected to GPIO 27 to simulate the bathroom light  
- A LED connected to GPIO 22 to simulate the closet light  

The system uses a desktop GUI with checkboxes to allow the user to control which room lights should be turned on. Unlike the previous radio button version, the checkbox version allows multiple lights to be turned on at the same time.


## Expected System Behaviour

### GUI Control:
- The GUI window displays the title **Linda's House**
- Three checkboxes are shown:
  - Living Room
  - Bathroom
  - Closet
- An **All Off** button is displayed
- A **Close** button is also displayed  

### Light Selection:
- When the **Living Room** checkbox is selected, the living room LED turns ON
- When the **Bathroom** checkbox is selected, the bathroom LED turns ON
- When the **Closet** checkbox is selected, the closet LED turns ON
- When a checkbox is unselected, the matching LED turns OFF

### Light Logic:
- Each checkbox controls its own LED
- Multiple lights can be ON at the same time
- The state of each LED matches the state of its checkbox
- The **All Off** button turns all LEDs OFF and unchecks all checkboxes

### Closing the Application:
- When the **Close** button is pressed:
  - The `closeApp()` function calls `allOff()`
  - All LEDs are turned OFF
  - All checkboxes are unchecked
  - The application window closes


## GPIO Pin Setup

The following GPIO pins are used for output:

- GPIO 17 -> Living room LED  
- GPIO 27 -> Bathroom LED  
- GPIO 22 -> Closet LED  

Each GPIO pin is controlled using a `gpiozero.LED` object:

- `living_led = LED(17)`
- `bath_led = LED(27)`
- `closet_led = LED(22)`


## Class: `ControlLights(QMainWindow)`

This class defines the main GUI window and contains the logic for the light controller.

### `__init__()`

- Calls the parent `QMainWindow` constructor
- Sets the window size and position using `setGeometry()`
- Sets the window title to **Light Controller**
- Calls `initUI()` to build the interface
- Calls `connectSignals()` to connect user actions to program functions


## `initUI()`

This function builds the graphical user interface.

- Creates a central widget for the main window
- Creates a vertical box layout (`QVBoxLayout`)
- Adds a label displaying **Linda's House**
- Creates three checkboxes:
  - Living Room
  - Bathroom
  - Closet
- Creates an **All Off** push button
- Creates a **Close** push button
- Adds all widgets to the layout
- Sets the completed layout as the central widget of the main window


## `connectSignals()`

This function connects GUI events to their corresponding program actions.

- Connects the `toggled` signal of each checkbox to `updateLights()`
- Connects the `clicked` signal of the **All Off** button to `allOff()`
- Connects the `clicked` signal of the **Close** button to `closeApp()`

This allows the program to respond immediately whenever the user selects or unselects a room, turns all lights off, or closes the application.


## `updateLights()`

This function controls the LEDs based on which checkboxes are currently selected.

### Behaviour:
- Checks the state of each checkbox
- Turns each LED ON or OFF depending on whether its checkbox is selected

### Logic:
- If **Living Room** is checked, `living_led.on()` is called, otherwise `living_led.off()` is called
- If **Bathroom** is checked, `bath_led.on()` is called, otherwise `bath_led.off()` is called
- If **Closet** is checked, `closet_led.on()` is called, otherwise `closet_led.off()` is called

This allows each light to be controlled independently.


## `allOff()`

This function turns off all lights and resets the GUI checkbox states.

### Behaviour:
- Turns OFF the living room LED
- Turns OFF the bathroom LED
- Turns OFF the closet LED
- Unchecks the Living Room checkbox
- Unchecks the Bathroom checkbox
- Unchecks the Closet checkbox

This is used when the user presses the **All Off** button, and it is also reused by `closeApp()` before closing the application.


## `closeApp()`

This function is called when the user presses the **Close** button.

### Behaviour:
- Calls `allOff()` to turn all LEDs OFF and uncheck all checkboxes
- Closes the application window

This avoids repeating the same LED shutdown code and ensures the system shuts down cleanly.


## `main()`

The `main()` function starts the application.

### Behaviour:
- Creates a `QApplication` object
- Creates an instance of the `ControlLights` window
- Displays the window using `show()`
- Starts the PyQt event loop using `app.exec_()`
