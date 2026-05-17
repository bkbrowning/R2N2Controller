/* --------------------------
   R2N2DomeFeather.ino
   Part of a control system for R2 models, this code controls a receiver unit within a group of a single transmitter and multiple receivers.
   This specific reseciver is configured to operate the mechanisms on the DOME portion of the R2 unit.

   This specific code controls the receiver that is focused on servo motor controls.  Specific hardware for this solution includes an Adafruit
   Feather M0 RFM69HCW 900MHz receiver attached to an Adafruit PCA9685 12-bit PWM servo controller.

   Version history
   v0.1 - Initial version for design and testing
*/

/* 
--------------------------------------------------------------
    INCLUDES
--------------------------------------------------------------
*/
#include <SPI.h>                      // Serial programmable interface support code
#include <Wire.h>                     // Support code for the I2C interface
#include <RH_RF69.h>                  // Support code for the RFM69HCW's onboard radio...code from RadioHead (http://www.airspayce.com/mikem/arduino/RadioHead)
#include <RHReliableDatagram.h>       // Support code for the radio datagram packet management
#include <Adafruit_PWMServoDriver.h>  // Support code for the PWM servo board attached to the Adafruit Feather via I2C


/* 
--------------------------------------------------------------
    INITIALIZATION AND SETUP
--------------------------------------------------------------
*/

//
//  Setup the PWM servo driver
//
// NOTE:  the servo driver board has an assignable address that is set by bridging solder pads on the board.  This allows you to daisy-chain
//        multiple PWM control boards together to increase the number of controlled servos.  Each board must be initialized with the hex code
//        for its address.  The default (no pads soldered) is 0x40.
Adafruit_PWMServoDriver pwmBoard1 = Adafruit_PWMServoDriver(0x40);
#define SERVO_FREQ   60  // This is the maximum PWM frequency in Hz;  tune to the requirements for your servos.  Typical analog servos seem to work around 60 Hz

//
// Setup the RFM69 Radio
//
#define RF69_FREQ      919.0  // specific operating frequency for our radio (all radios in this circuit need to share the same frequency)
#define RFM69_SS           8  // slave select (CS) pin on the Adafruit Feather...which is connected to pin #8 on the Adafruit Feather M0 RFM95
#define RFM69_INT          3  // interrupt (IRQ) pin on the Adafruit Feather...which is connected to pin #3 on the Adafruit Feather M0 RFM95
#define RFM69_RST          4  // reset (RST) pin on the Adafruit Feather...which is connected to pin#4 on the Adafruit Feather M0 RFM95
#define MY_RADIO_ADDRESS  40  // Must be a UNIQUE number for each radio within the radio group
#define RADIO_POWER       20  // power in dbm for the HCW69 radio...range is from 14-20
// NOTE AND CAUTION:  The following radio encryption key must be unique for your and must not be shared by any other radio group in the area 
//                    or else you will have radio conflicts.  Make it unique, make it safe.  The below example IS JUST AN EXAMPLE...PLEASE CHANGE IT!
uint8_t encryptRadioKey[] = {0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03};

RH_RF69 rf69(RFM69_SS, RFM69_INT);  // initialize the radio driver
RHReliableDatagram rf69_manager(rf69, MY_RADIO_ADDRESS); // initialize the datagram manager for this specific radio

//
// Setup the I2C device
//
#define DOMEI2C 40
int myi2c = DOMEI2C; // the i2c address of this device

//
// Setup Servos
//
// servo#, servoOpen, servoClosed
// Define and initialize the array to hold all of our servo data for servos connected to the PWM control board attached to this Feather
//    The array below is of the format...
//    {PWMPinNum, ServoClosedPosition, ServoOpenPosition, ServoCloseSpeed, ServoOpenSpeed}
#define NUMBER_OF_SERVOS 1 // total number of servos in the servo array below
//VarSpeedServo Servos[NUMBER_OF_SERVOS];  // array to link all servos to the VarSpeedServo library
int myServos[NUMBER_OF_SERVOS][5] = {{ 1, 40, 175, 1, 1 }};
// To make referring to our panels easier, we'll give them some more human-friendly names
// Note that the references below are to the servo array ROWS.  Should be one of these for each of NUMBER_OF_SERVOS
#define DATA_PANEL 0
#define LF_DOOR 1
#define CHARGE_BAY 2
#define RF_DOOR 3
#define RR_DOOR 4
#define LR_DOOR 5
// need to add all of the others and sort this to make most sense
// And we will define some constants that we can use to make referring to the servo array COLUMNS in the array above easier to remember
#define PWMPIN_NUM 0
#define CLOSED_POS 1
#define OPEN_POS 2
#define CLOSE_SPEED 3
#define OPEN_SPEED 4

// Some variables to keep track of open/close status
boolean allServosOpen=false;
boolean servoStatus[NUMBER_OF_SERVOS] = {false};  // (false=closed; open=true)






/* 
--------------------------------------------------------------
    LOCAL CONSTANTS
--------------------------------------------------------------
*/
// NOTE:  If we were using specific pins on the Adafruit Feather board to drive things here, we would #define them as needed
// Other local constants
const long DISPLAY_INTERVAL = 1000;  // time between radio status updates being pushed to the serial monitor (in ms)
const long RADIO_INTERVAL = 250;     // time between radio reads for new information (in ms)
const long SHOW_INTERVAL = 100;      // time between processing of the shows to slow overall cycles (in ms)


/* 
--------------------------------------------------------------
    LOCAL VARIABLES
--------------------------------------------------------------
*/
unsigned long currentTime = 0;  // to hold the current time stamp value at the point of reading (typically, in ms since power up)
unsigned long previousDisplayTime = 0;
unsigned long previousRadioTime = 0;
unsigned long previousShowTime = 0;
unsigned long showStartingTime = 0;
int currentShow = 0;  // integer indicating the current Show that is processing
int fullMessageIntCurrent;  // integer to receive the value passed from the radio
int fullMessageIntPrevious; // integer to hold the previously received incoming value from the radio
boolean readyForNewShow = true;  // boolean to hold whether or not the current show has finished processing
//
// Setup the data structure used to send/receive messages across the radio network
//
// NOTE: defines the radio packet's structure for transmitting data
//       - part1 and part2 are for the menu screen number
//       - part3 is the pushbutton choice
//       - the current design limits us to 99 screens of 8 menu choices per screen, for a total of 792 individual commands.  Is that enough?
struct messageStruct {
	byte part1;  // menu screen
	byte part2;  // menu screen
	byte part3;  // pushbutton
} RadioPacket;


/* 
--------------------------------------------------------------
    ARDUINO SETUP - RUNS ONCE
--------------------------------------------------------------
*/
void setup() {
   
  delay(1000); // I have had various issues with Arduino-style chips not processing some of the Setup commands correctly.  Adding a delay here seems to help.  Your mileage may vary.
  
  Serial.begin(115200);  // initialize the serial communicator (for monitoring);
  Serial.println("Beginning setup and intialization routines for the DOME controller...");
   
  // Set all of the physical Arduino pins' modes as needed
  pinMode(RFM69_RST, OUTPUT);  // sets the radio's reset pin so that we can control it to turn the radio on/off

  // Intitialize each of the PWM Servo boards attached
  Serial.println("Intializing each of the attached PWM Servo boards...");
  pwmBoard1.begin();
  pwmBoard1.setPWMFreq(SERVO_FREQ);
  Serial.println("Servo board #1 complete!");
  Serial.println("All PWM Servo boards are now initialized.");

  // Intiialize the RFM69HCW Radio
  Serial.print("Initializing the RFM69 Radio...");
  // switch the radio on/off
  digitalWrite(RFM69_RST, LOW);
  delay(10);
  digitalWrite(RFM69_RST, HIGH);
  delay(10);
  digitalWrite(RFM69_RST, LOW);
  delay(10);
  if (!rf69.init()) {
    Serial.println("RFM69 radio initialization failed!  Holding here until hardware reset!");
    while (1);  // hold the processing at this line
  } else {
    Serial.println("RFM69 radio initialized!");
  }
  // set the radio's frequency 
  if (!rf69.setFrequency(RF69_FREQ)) {
    Serial.println("Could not set the radio's frequency!  Holding here until hardware reset!");
    while (1);  // hold processing at this line
  } else {
  	Serial.print("RFM69 radio set to ");  Serial.print((int)RF69_FREQ);  Serial.println(" MHz");
  }
  // set the radio's transmission power
  // NOTE:  because this is a high power 69HCW radio, the second argument ("ishighpowermodule") must be included and set to TRUE 
  rf69.setTxPower(RADIO_POWER, true);
  // set the radio's encryption key
  rf69.setEncryptionKey(encryptRadioKey);
  
  // Intialize the radio message packet
  RadioPacket.part1 = 0;
  RadioPacket.part2 = 0;
  RadioPacket.part3 = 0;

  delay(500);
  Serial.println("Setup COMPLETE for the DOME controller!");
} // setup()


/* 
--------------------------------------------------------------
    ARDUINO LOOP - RUNS CONTINUOUSLY
--------------------------------------------------------------
*/
// NOTE:  This loop is adapted from that used by Kevin Holme in his "Taking Control" thread on Astromech.net.  Essentially, we
//        listen for the radio transmitter to broadcast a signal.  When that signal changes, we begin the execution of the command
//        represented by that new signal.  Groups of actions are organized into "Shows" with the intent that actions can be coordinated
//        across multiple receivers that are each processing the same "Show".  Specific actions are handled as follows
//        1) ProcessAction() - takes the received request and either performs a single action or launches a set of actions as a show
//        2) RunShows() - keeps the planned shows running and stops them upon their timing completion
//        3) Specific show details - coordinates the individual actions within a show that are processed by this receiver
//        The looping below does allow for new actions to be processed while a show is still completing
void loop() {
 
  currentTime = millis();   // capture the current timestamp
  
  // if the current message is different than the previous message, begin a new action
  // NOTE:  This is processed on EVERY loop
  if(fullMessageIntCurrent != fullMessageIntPrevious) {
    ProcessAction(fullMessageIntCurrent);
    fullMessageIntPrevious = fullMessageIntCurrent;
  }
  
  // updates the serial monitor with the latest radio status info
  // NOTE:  This only processes just after each DISPLAY_INTERVAL time has passed
  if (currentTime - previousDisplayTime >= DISPLAY_INTERVAL) {
    previousDisplayTime = currentTime;
    RadioStatus();
  }
  
  // checks the radio for new data being received
  // NOTE:  This only processes just after each RADIO_INTERVAL time has passed
  if (currentTime - previousRadioTime >= RADIO_INTERVAL) {
    previousRadioTime = currentTime;
    readRadio();
  }

  if (currentTime - previousShowTime >= SHOW_INTERVAL) {
    previousShowTime = currentTime;
    RunShows();
  }

}  // loop()


/* 
--------------------------------------------------------------
    SUPPORTING FUNCTIONS
--------------------------------------------------------------
*/



// ----------------------------------------------------------------------
// void readRadio() - reads incoming data package from the radio receiver
// ----------------------------------------------------------------------
void readRadio () {
  // local vars
  uint8_t buf[sizeof(RadioPacket)];  // hold our message structure as defined
  uint8_t from;
  uint8_t len = sizeof(buf);
  String msgPart1;     // string to hold radio message's first part
  String msgPart2;     // string to hold radio message's second part
  String msgPart3;     // string to hold radio message's third part
  String fullMessage;  // string to hold the concatenated message

  if (rf69_manager.available()) {      
    // Wait for a message addressed to us from the client
    if (rf69_manager.recvfrom(buf, &len, &from)) {
      memcpy(&RadioPacket, buf, sizeof(RadioPacket));  // copy the message from the buffer into the message structure
      if (!len) return;  // if we received nothing, stop processing here
      buf[len] = 0;  // reset the buffer
      // store the message packet's parts
      msgPart1 = String(RadioPacket.part1);
	    msgPart2 = String(RadioPacket.part2);
	    msgPart3 = String(RadioPacket.part3);
	    // concatenate the message packet to build the commensurate string
	    fullMessage = msgPart1 + msgPart2 + msgPart3;
      // convert the concatenated message to an integer
   	  // NOTE:  may want to introduce some error handling here
   	  fullMessageIntCurrent = fullMessage.toInt();
    }
  }
} // readRadio()


// -----------------------------------------------------------------------------------
// void ProcessAction() - processes a change in the requested action from the receiver
// -----------------------------------------------------------------------------------
void ProcessAction(int currentOrder) {
  
  switch(currentOrder) {
    case 11:
      Serial.print("Front DP Open");
      cycleServo(DATA_PANEL, true);
      break;
    
    case 15:
      Serial.print("Front DP Close");
      cycleServo(DATA_PANEL, false);
      break;
  
    case 21:
      Serial.print("Front DP Cycle");
      cycleServo(DATA_PANEL, !servoStatus[DATA_PANEL]);
      break;
 
    case 25:
      Serial.print("Front ALL Cycle");
      // need to loop through all servos
      break;

    case 71:
      showStartingTime = millis();
      currentShow = 71;
      break;
  
  } //switch()

} //ProcessAction()


// -----------------------------------------------------------------------------------
// void RunShows() - processes the handling of the group of Shows
// -----------------------------------------------------------------------------------
void RunShows() {

  // If the previous show has not completed OR no show is currently running (both are currentShow = 0), then don't allow
  //   a new show to begin.  Once currentShow has completed, we can then take a new show action.  NOTE that this dependency is not
  //   enforced across different receivers.  We could end up with different receivers on different shows if they are not timed properly
  //   in each receiver's code.
  if (readyForNewShow) {
    switch (currentShow) {
      case 71:
        RocketMan();
        break;

      default:
      case 0:// No Show Now
        currentShow = 0; 
    } // switch()
  }
} // RunShows()


// -----------------------------------------------------------------------------------
// void RocketMan() - a sample show from Kevin Holme
// -----------------------------------------------------------------------------------
// NOTE:  this is a show that is set to Elton Johns Rocket man,  the song plays, and the booster rockets deplay whenever he sings
//        rocket man.  this unit however just has one job. 
void RocketMan() {
  if (millis() - showStartingTime <= 85000) {// when the show was triggered, the show timer was reset to 0 . This line is saying, If the time is less than 85000 milliseconds, Put up the speaker lifter.  also notice I repeat that command, With shows 
    readyForNewShow = false;  // still processing the current show
    Serial.print(" Speaker Up ");
    //digitalWrite(SpkrDown,HIGH);
    //digitalWrite(SpkrMid,HIGH);
    //digitalWrite(SpkrPow, LOW);
    delay (100);
    //digitalWrite(SpkrUp, LOW);
    delay (100);
    //digitalWrite(SpkrUp, LOW);
  }     

  if (millis() - showStartingTime >= 85000) {  // once the time gets past 85000 milliseconds  it is time to put down the speaker.
    readyForNewShow = false;  // still processing the current show    
    //digitalWrite(SpkrMid,HIGH);
    //digitalWrite(SpkrUp,HIGH);
    //digitalWrite(SpkrPow, LOW);
    //digitalWrite(SpkrDown,LOW);
    delay (100);
    //digitalWrite(SpkrDown, LOW);
  }

  if (millis() - showStartingTime >= 90000) {  // after another 5 seconds, power it all down
    readyForNewShow = false;  // still processing the current show
    //digitalWrite(SpkrDown, HIGH);
    //digitalWrite(PeriDown, HIGH); 
    //digitalWrite(PeriPow, HIGH);
    //digitalWrite(SpkrPow, HIGH);
  }

  if (millis() - showStartingTime >= 95000) { // and after another 5 seconds  change the show to 0
    readyForNewShow = true;  // still processing the current show
    currentShow = 0;
  }  
      
} // RocketMan()


// ------------------------------------------------------------------------------------------------------------------------------------------
// void cycleServo(int servoNumber, boolean servoOpenTF) - checks the current state of the servo and changes it from OPEN to CLOSED as needed
// NOTE:  servoOpenTF is TRUE for OPEN and FALSE for CLOSED
// ------------------------------------------------------------------------------------------------------------------------------------------
void cycleServo(int servoNumber, boolean servoOpenTF) {
  // assumes that servos are either OPEN or CLOSED;  does not handle interim positions
  // test for the current servo position relative to the command
  if (servoStatus[servoNumber] == servoOpenTF) {
  	// do nothing as the servo is already in that position
  } else {
  	// take some action on the servo
  	if (servoStatus[servoNumber]) {
  		// servo is open, so close it
  		for (int servoPos=myServos[servoNumber][OPEN_POS]; servoPos>myServos[servoNumber][CLOSED_POS]; servoPos-=CLOSE_SPEED){
  			pwmBoard1.setPWM(myServos[servoNumber][PWMPIN_NUM], 0, servoPos);
  		}
  		servoStatus[servoNumber] = false; // set servo's status
  	} else {
  		// servo is closed, so open it
  		for (int servoPos=myServos[servoNumber][CLOSED_POS]; servoPos<myServos[servoNumber][OPEN_POS]; servoPos+=OPEN_SPEED){
  			pwmBoard1.setPWM(myServos[servoNumber][PWMPIN_NUM], 0, servoPos);
  		}
  		servoStatus[servoNumber] = true; // set servo's status
  	}
  }
}

// --------------------------------------------------------------------------
// void RadioStatus() - checks the current radio signal strength and messages
// --------------------------------------------------------------------------
void RadioStatus() {
  // print the relative signal strength...-15 (high) to -100 (low)
  Serial.print("RSSI: ");
  Serial.println(rf69.lastRssi(), DEC);
  //print the data received from the radio
  Serial.print("Got message from unit: ");
  Serial.print("Switch Code=");
  Serial.print(RadioPacket.part1);
  Serial.print(RadioPacket.part2);
  Serial.print(RadioPacket.part3);
  Serial.print(" and Incoming Integer=");
  Serial.println(fullMessageIntCurrent);
}
