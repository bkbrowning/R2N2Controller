"""
Radio Check and my frist python program
"""

import time
import busio
from digitalio import DigitalInOut, Direction, Pull
import board
# Import the SSD1306 module for the onboard OLED screen
import adafruit_ssd1306
# Import the RFM69 radio module
import adafruit_rfm69

# Button A function
btnA = DigitalInOut(board.D5)
btnA.direction = Direction.INPUT
btnA.pull = Pull.UP

# Button B function
btnB = DigitalInOut(board.D6)
btnB.direction = Direction.INPUT
btnB.pull = Pull.UP

# Button C function
btnC = DigitalInOut(board.D12)
btnC.direction = Direction.INPUT
btnC.pull = Pull.UP

# Create the I2C interface
#i2c = busio.I2C(board.SCL, board.SDA)

# 128x32 OLED Display
#display = adafruit_ssd1306.SSD1306_I2C(128, 32, i2c, addr=0x3c)
# Clear display
#display.fill(0)
#display.show()
#width=display.width
#height=display.height

# RFM69 radio config
CS = DigitalInOut(board.CE1)
RESET = DigitalInOut(board.D25)
spi = busio.SPI(board.SCK, MOSI=board.MOSI, MISO=board.MISO)
rfm69 = adafruit_rfm69.RFM69(spi, CS, RESET, 915.0)
rfm69.encryption_key = b'\x01\x02\x03\x04\x05\x06\x07\x08\x01\x02\x03\x04\x05\x06\x07\x08'
print('Radio initialized!  Waiting on receive...')
prev_packet = None
nulllines = 0

while True:
    packet = None
    
    # check for pacjet receive
    packet = rfm69.receive()
    if packet is None:
        # do nothing for now
        nulllines = nulllines + 1
    else:
        #display the packet text and rssi
        prev_packet = packet
        packet_text = str(prev_packet, "utf-8")
        print('RX: ' + packet_text + ' in time: ' + str(nulllines))
        #time.sleep(1)

    # Check buttons
    if not btnA.value:
        # Button A pressed
#        display.text('Ada', width-85, height-7, 1)
#        display.show()
        print('Ada')
    if not btnB.value:
        # Button B pressed
#        display.text('Fruit', width-75, height-7, 1)
#        display.show()
        print('Fruit')
    if not btnC.value:
        # Button C pressed
#        display.text('Radio', width-65, height-7, 1)
#        display.show()
        print('Radio')

#    display.show()
    #time.sleep(0.1)
