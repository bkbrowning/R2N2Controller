# R2N2Controller
A control system for a robotic R2-D2 style robot.  Control system consists of
multiple components and the operating files for each component are contained
within this repo.

Current components include:
- R2N2HeadsetPi - A RaspberryPi based menu system for controling the display unit to the heads up display
- R2N2BodyFeather - An Adafruit Feather controller to manage the control menu and act as a relay for actions
  to other controllers
- R2N2DomeFeather - An Adafruit Feather controller to manage the control systems within an R2 dome
- R2N2FrontFeather - An Adafruit Feather controller to manage the control systems attached to the front skin on an R2 droid
- R2N2RearFeather - An Adafruit Feather controller to manage the control systems attached to the rear skin on an R2 droid
- R2N2323Controller - An Arduino compatible controller to manage the Kevin Holme created 3-2-3 droid augmentation
