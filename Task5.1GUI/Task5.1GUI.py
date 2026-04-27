# Import libraries and modules
from PyQt5 import QtWidgets
from PyQt5.QtWidgets import QApplication, QMainWindow, QWidget, QVBoxLayout
from gpiozero import LED
import sys

# Define LEDs connected to Raspberry Pi GPIO pins
living_led = LED(17)   # Living room light
bath_led = LED(27)     # Bathroom light
closet_led = LED(22)   # Closet light

class ControlLights(QMainWindow):
    def __init__(self):
        super(ControlLights, self).__init__()
        # Set window size and title
        self.setGeometry(200, 200, 250, 200)
        self.setWindowTitle("Light Controller")
        # Initialize UI and connect signals
        self.initUI()
        self.connectSignals()

    def initUI(self):
        # Create main container and layout
        central_widget = QWidget()
        layout = QVBoxLayout()

        # Title label
        label = QtWidgets.QLabel("Linda's House")

        # Check boxes for controlling each light
        self.living_check = QtWidgets.QCheckBox("Living Room")
        self.bath_check = QtWidgets.QCheckBox("Bathroom")
        self.closet_check = QtWidgets.QCheckBox("Closet")

        # Button to turn all lights off
        self.all_off_button = QtWidgets.QPushButton("All Off")

        # Button to close application
        self.exit_button = QtWidgets.QPushButton("Close")

        # Add widgets to layout
        layout.addWidget(label)
        layout.addWidget(self.living_check)
        layout.addWidget(self.bath_check)
        layout.addWidget(self.closet_check)
        layout.addWidget(self.all_off_button)
        layout.addWidget(self.exit_button)

        # Apply layout to window
        central_widget.setLayout(layout)
        self.setCentralWidget(central_widget)

    def connectSignals(self):
        # Connect check box changes to light update function
        self.living_check.toggled.connect(self.updateLights)
        self.bath_check.toggled.connect(self.updateLights)
        self.closet_check.toggled.connect(self.updateLights)

        # Connect all off button
        self.all_off_button.clicked.connect(self.allOff)

        # Connect close button to exit function
        self.exit_button.clicked.connect(self.closeApp)

    def updateLights(self):
        # Control each light based on its checkbox
        if self.living_check.isChecked():
            living_led.on()
        else:
            living_led.off()

        if self.bath_check.isChecked():
            bath_led.on()
        else:
            bath_led.off()

        if self.closet_check.isChecked():
            closet_led.on()
        else:
            closet_led.off()

    def allOff(self):
        # Turn all lights off
        living_led.off()
        bath_led.off()
        closet_led.off()

        # Uncheck all checkboxes
        self.living_check.setChecked(False)
        self.bath_check.setChecked(False)
        self.closet_check.setChecked(False)

    def closeApp(self):
        # Ensure all lights are off before closing
        self.allOff()
        self.close()

def main():
    # Create application instance
    app = QApplication(sys.argv)

    # Create and show main window
    win = ControlLights()
    win.show()

    # Start event loop
    sys.exit(app.exec_())


# Program entry point
if __name__ == "__main__":
    main()
