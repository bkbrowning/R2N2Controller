"""
R2N2HeadsetPi
-
A RaspberryPi application that will display the resulting menu sets from our R2 controller system.  The intent is to use the RaspberryPi to
drive a heads-up display that will show an 720p HDMI driven picture...hence the Pi controller.  Also forms the basis for a new control system...more to come on that one!
"""

# imports
import tkinter as tk
import tkinter.font as tkFont

# Set default font size...which will adjust later based on window size
font_size = -12

# Declare global variables
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

# variables for iterating
menuPage = None
menuItem = None

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
# FUNCTIONS
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

# temporarily, set the values to specific data
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

# start with window as fullscreen and run the loop
#toggle_fullscreen
root.mainloop()


