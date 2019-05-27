"""
R2N2HeadsetPi
-
A RaspberryPi application that will display the resulting menu sets from our R2 controller system.  The intent is to use the
  RaspberryPi to drive a heads-up display that will show an 720p HDMI driven picture...hence the Pi controller.  The RaspberryPi
  Zero W is attached to an Adafruit OLED RFM69 radio through the use of the Adafruit RFM69HCW Transceiver Radio Bonnet 915MHz.

Menu changes are sent from the R2N2BodyFeather and this RPi is used, for now, to just control display to an HDMI driven heads up
  display unit.
  
--- Version notes ---
  -v0.1 - Initial version to configure and test the display components of the menu system
  -v0.2 - Added radio handling
"""

# imports
import tkinter as tk                                # to establish the user interface
import tkinter.font as tkFont                       # to use the tkinter fonts
import busio
from digitalio import DigitalInOut, Direction, Pull # to control the IO pins on the PiZero board
import board                                        # allows reference to board's components by logical name
import adafruit_ssd1306                             # Import the SSD1306 module for the onboard OLED screen
import adafruit_rfm69                               # Import the RFM69 radio module

#############################################
# Board physical device setup
#############################################

# The Adafruit Radio Bonnet has three external buttons that we can use to drive inputs.  Enabling those three buttons here
# Button A function
btnA = DigitalInOut(board.D5)    # Connected to board's pin D5
btnA.direction = Direction.INPUT
btnA.pull = Pull.UP
# Button B function
btnB = DigitalInOut(board.D6)    # Connected to board's pin D6
btnB.direction = Direction.INPUT
btnB.pull = Pull.UP
# Button C function
btnC = DigitalInOut(board.D12)   # Connected to board's pin D12
btnC.direction = Direction.INPUT
btnC.pull = Pull.UP

# The Adafruit Radio Bonnet has a radio transmitter/receiver that we use to communicate to/from R2's other controllers
SPI = busio.SPI(board.SCK, MOSI=board.MOSI, MISO=board.MISO)
CS = DigitalInOut(board.CE1)
RESET = DigitalInOut(board.D25)
FREQ = 915.0
rfm69 = adafruit_rfm69.RFM69(SPI, CS, RESET, FREQ)
rfm69.encryption_key = b'\x01\x02\x03\x04\x05\x06\x07\x08\x01\x02\x03\x04\x05\x06\x07\x08'
print('Radio initialized!  Waiting on receive...')

##############################################


##############################################
# GLOBAL VARIABLES
##############################################
menuPage = None
menuItem = None
menuPageNum = None
menuPageTitle = None
menuPageItem0 = None
menuPageItem1 = None
menuPageItem2 = None
menuPageItem3 = None
menuPageItem4 = None
menuPageItem5 = None
menuPageItem6 = None
menuPageItem7 = None
fullscreen = False
prev_packet = None
nulllines = 0
font_size = -12      # Set default font size...which will adjust later based on window size
##############################################


##############################################
# CLASSES
##############################################

# extended the tk.Label class to add a flashing version
# swaps foreground and background colors every 1000ms for a period of 30 cycles
class FlashableLabel(tk.Label):
    def flash(self,count):
        bg = self.cget('background')
        fg = self.cget('foreground')
        self.configure(background=fg,foreground=bg)
        count +=1
        if (count < 30):
            self.after(1000,self.flash, count)

##############################################

##############################################
# LOCAL FUNCTIONS
##############################################

# toggle_fullscreen = toggles screen from full to windowed
def toggle_fullscreen(event=None):
    global root
    global fullscreen
    fullscreen = not fullscreen
    root.attributes('-fullscreen', fullscreen)
    resize_font()

# end_fullscreen = returns to windowed mode
def end_fullscreen(event=None):
    global root
    global fullscreen
    fullscreen = False
    root.attributes('-fullscreen', False)
    resize_font()

# resize_font = resize the screen fonts based on the window's size
def resize_font(event=None):
    global title_dfont
    global items_dfont
    global frame
    # a negative number is used for "pixels" in lieu of "points"
    # the minimum font size is "12"
    new_size = -max(12, int((frame.winfo_height() / 10)))
    items_dfont.configure(size=new_size)
    new_size = -max(12, int((frame.winfo_height() / 20)))
    title_dfont.configure(size=new_size)

##############################################

# Create the main display window
root = tk.Tk()
root.title("R2N2 Headset")
# create the main container and set the background color
frame = tk.Frame(root, bg='black')

# Define the main window controller and allow it to grow/shrink with the window size
frame.pack(fill=tk.BOTH, expand=1)

# create dynamic fonts for holding the window's text
items_dfont = tkFont.Font(family='Courier New', size=font_size)
title_dfont = tkFont.Font(size=font_size)

# tkinter variables for updating the on-screen information
menuPageNum = tk.IntVar()
menuPageTitle = tk.StringVar()
menuPageItem0 = tk.StringVar()
menuPageItem1 = tk.StringVar()
menuPageItem2 = tk.StringVar()
menuPageItem3 = tk.StringVar()
menuPageItem4 = tk.StringVar()
menuPageItem5 = tk.StringVar()
menuPageItem6 = tk.StringVar()
menuPageItem7 = tk.StringVar()


menuPage = 6

# menuData is a multi-dimensional list that contains the detailed data to be displayed in menu system
#   COL0 = menu number
#   COL1 = menu title
#   COL2 - COL9 = menu choice values
# NOTE:  This initial set of data was originally created by Kevin Holme for his R2 control system.  It has been modified
#   to reflect the features of my specific R2 droid
menuData = [
    ["10",  "Random Sounds", "R Whistle",    "R Sad",            "R Chat",      "R Ack",        "R Razz",         "R Scream",         "R Alarm",        "R Hum",           "10"],
    ["20",  "Front Body",    "Open All",     "Wave 1",           "Wave 3",      "Alt 1",        "Close All",      "Wave 2",           "Wave 4",         "Alt 2",           "20"],
    ["30",  "Lights",        "Knight R1",    "Rainbow",          "Dual Bnc",    "Auto Off",     "Knight R2",      "Short Cir",        "Zig Zag",        "Auto On",         "30"],
    ["40",  "Change Stance", "Two Legs",     "Three Legs",       "Vex Only",    "<OPEN4",       "Look Up Max",    "Look Up",          "Look Down",      "Look Down Max",   "40"],
    ["50",  "Toys",          "Rockets Open", "Rocket Light On",  "CPU Tip",     "CPU Open",     "Rockets Closed", "Rocket Light Off", "<OPEN7",         "CPU Closed",      "50"],
    ["60",  "Lifters",       "Periscope Up", "Periscope Random", "Speaker Up",  "Saber Up",     "Periscope Down", "Saber Light",      "Speaker Down",   "Saber Down",      "60"],
    ["70",  "Shows",         "Rocket Man",   "Leia Holo",        "Zap",         "Open Zapper",  "Fav Things",     "TBD",              "TBD",            "Close Zapper",    "70"],
    ["80",  "Songs 1",       "Main Theme",   "Vaders Theme",     "Leias Theme", "Cantina Song", "Staying Alive",  "R2 Rocket",        "Gangam Style",   "Disco Star Wars", "80"],
    ["90",  "Songs 2",       "Mana Mana",    "PBJ Time",         "Low Rider",   "Rocket Man",   "Happy B-Day",    "Macho Man",        "Harlem Shuffle", "Reset",           "90"],
    ["100", "ServoTest",     "Open Servo 1", "Open Servo 2",     "Open Servo 3","Open Servo 4", "Close Servo 1",  "Close Servo 2",    "Close Servo 3",  "Close Servo 4",   "100"],
    ["110", "Open Menu 11",  "<OPEN1>",      "<OPEN2>",          "<OPEN3>",     "<OPEN4>",      "<OPEN5>",        "<OPEN6>",          "<OPEN7>",        "<OPEN8>",         "110"]
]

menuPageNum.set(menuData[menuPage][0])
menuPageTitle.set(menuData[menuPage][1])
menuPageItem0.set(menuData[menuPage][2])
menuPageItem1.set(menuData[menuPage][3])
menuPageItem2.set(menuData[menuPage][4])
menuPageItem3.set(menuData[menuPage][5])
menuPageItem4.set(menuData[menuPage][6])
menuPageItem5.set(menuData[menuPage][7])
menuPageItem6.set(menuData[menuPage][8])
menuPageItem7.set(menuData[menuPage][9])

# create the window widgets
label_menuPageNum = tk.Label(frame, textvariable=menuPageNum, font=title_dfont, fg='white', bg='black')
label_menuPageTitle = tk.Label(frame, textvariable=menuPageTitle, font=title_dfont, fg='white', bg='black')
label_menuPageItem0 = FlashableLabel(frame, textvariable=menuPageItem0, font=items_dfont, fg='yellow', bg='blue')
label_menuPageItem1 = FlashableLabel(frame, textvariable=menuPageItem1, font=items_dfont, fg='yellow', bg='blue')
label_menuPageItem2 = FlashableLabel(frame, textvariable=menuPageItem2, font=items_dfont, fg='yellow', bg='blue')
label_menuPageItem3 = FlashableLabel(frame, textvariable=menuPageItem3, font=items_dfont, fg='yellow', bg='blue')
label_menuPageItem4 = FlashableLabel(frame, textvariable=menuPageItem4, font=items_dfont, fg='yellow', bg='blue')
label_menuPageItem5 = FlashableLabel(frame, textvariable=menuPageItem5, font=items_dfont, fg='yellow', bg='blue')
label_menuPageItem6 = FlashableLabel(frame, textvariable=menuPageItem6, font=items_dfont, fg='yellow', bg='blue')
label_menuPageItem7 = FlashableLabel(frame, textvariable=menuPageItem7, font=items_dfont, fg='yellow', bg='blue')

# lay out widgets for window
label_menuPageNum.grid(row=0, column=0, padx=0, pady=0)
label_menuPageTitle.grid(row=0, column=1, padx=0, pady=0)
label_menuPageItem0.grid(row=1, column=0, padx=0, pady=0)
label_menuPageItem1.grid(row=1, column=1, padx=0, pady=0)
label_menuPageItem2.grid(row=2, column=0, padx=0, pady=0)
label_menuPageItem3.grid(row=2, column=1, padx=0, pady=0)
label_menuPageItem4.grid(row=3, column=0, padx=0, pady=0)
label_menuPageItem5.grid(row=3, column=1, padx=0, pady=0)
label_menuPageItem6.grid(row=4, column=0, padx=0, pady=0)
label_menuPageItem7.grid(row=4, column=1, padx=0, pady=0)

# expand the layout's cells to fill the window's size
frame.rowconfigure(0, weight=100)
frame.rowconfigure(1, weight=225)
frame.rowconfigure(2, weight=225)
frame.rowconfigure(3, weight=225)
frame.rowconfigure(4, weight=225)
frame.columnconfigure(0, weight=1)
frame.columnconfigure(1, weight=1)

# bind the escape key and F11 to control the window's size
root.bind('<F11>', toggle_fullscreen)
root.bind('<Escape>', end_fullscreen)

# execute the resize function each time the window is resized
root.bind('<Configure>', resize_font)

# temporary flash test
label_menuPageItem6.flash(0)
label_menuPageItem7.flash(16)

#############################################
# MAIN LOOP
#############################################
while True:
    packet = None
    
    # Check for the receipt of a new packet on the radio
    packet = rfm69.receive()
    if packet is None:
        # do nothing for now
        nulllines = nulllines + 1
    else:
        #display the packet text and time since last receive event
        prev_packet = packet
        packet_text = str(prev_packet, "utf-8")
        print('RX: ' + packet_text + ' in time: ' + str(nulllines))
        
    # Check for a local button press
    if not btnA.value:
        print('Button A pressed')
    if not btnB.value:
        print('Button B pressed')
    if not btnC.value:
        print('Button C pressed')

    # Update the tk window
    root.update_idletasks()
    root.update()