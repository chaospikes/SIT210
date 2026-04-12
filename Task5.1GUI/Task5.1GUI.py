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
        # Radio buttons for selecting which light to turn on
        self.living_radio = QtWidgets.QRadioButton("Living Room")
        self.bath_radio = QtWidgets.QRadioButton("Bathroom")
        self.closet_radio = QtWidgets.QRadioButton("Closet")
        # Button to close application
        self.exit_button = QtWidgets.QPushButton("Close")
        # Add widgets to layout
        layout.addWidget(label)
        layout.addWidget(self.living_radio)
        layout.addWidget(self.bath_radio)
        layout.addWidget(self.closet_radio)
        layout.addWidget(self.exit_button)
        # Apply layout to window
        central_widget.setLayout(layout)
        self.setCentralWidget(central_widget)

    def connectSignals(self):
        # Connect radio button changes to light update function
        self.living_radio.toggled.connect(self.updateLights)
        self.bath_radio.toggled.connect(self.updateLights)
        self.closet_radio.toggled.connect(self.updateLights)
        # Connect close button to exit function
        self.exit_button.clicked.connect(self.closeApp)

    def updateLights(self):
        # Turn all lights off first
        living_led.off()
        bath_led.off()
        closet_led.off()
        # Turn on the selected light
        if self.living_radio.isChecked():
            living_led.on()
        elif self.bath_radio.isChecked():
            bath_led.on()
        elif self.closet_radio.isChecked():
            closet_led.on()

    def closeApp(self):
        # Ensure all lights are off before closing
        living_led.off()
        bath_led.off()
        closet_led.off()
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
