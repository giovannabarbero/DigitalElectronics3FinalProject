# DigitalElectronics3FinalProject
LPC1769 Decibel Meter
📖 Description
We developed a program for the LPC1769 device using CMSIS libraries and drivers developed by Eng. Trujillo. It takes an input from a microphone, processes the information according to standards derived from a standardized decibel meter, and outputs this processed reading through a corresponding number of lit LEDs.

This scale emulates a VU meter, indicating whether the volume level is Low, Normal, or High in terms of how harmful it is to the human ear. Furthermore, it includes an output on 7-segment displays showing the integer dB value and the weighting curve applied, two LEDs indicating the acquisition mode (Fast or Slow), and finally, an output via UART protocol to a Hercules client. The LPC is turned on and off using an external button.

✨ Features
* Audio Processing: Standardized decibel meter algorithms applied to microphone input.

* VU Meter: LED scale indicating safe vs. harmful volume levels (Low, Normal, High).

* Numeric Display: 7-segment displays showing integer dB values and the current weighting curve.

* Acquisition Modes: Two dedicated LEDs indicating Fast or Slow acquisition modes.

* Serial Communication: UART output formatted for Hercules client.

* Hardware Control: External push-button for system power/initiation.

🛠️ Hardware Requirements
* NXP LPC1769 Microcontroller

* Microphone Module (Analog input)

* LEDs (for mode indicators)

* Servo (for VU meter)

* 7-Segment Displays

* Push button

💻 Software & Libraries
* CMSIS (Cortex Microcontroller Software Interface Standard)

* Custom drivers by Eng. Trujillo

* Hercules Setup Utility (for UART client monitoring)

👥 Credits
* Developers: Thomas von Büren, Giovanna Luz Barbero

* Driver Development: Eng. Trujillo
