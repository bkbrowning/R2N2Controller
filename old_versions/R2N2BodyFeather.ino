/* --------------------------
  R2N2BodyFeather.ino
   Part of a control system for R2 models, this code controls the "main body" Feather board within a group of multiple receivers.  Specifically,
   this solution uses a STEALTH controller as the main processing board.  The STEALTH relays commands to this "main body" board which then processes
   ongoing events/routines.  Why do this?  Because the STEALTH, as great as it is, is not an open platform that will allow us to write complex routines.
   This setup offloads those routines from the STEALTH.

   This specific code controls the transmitter that is contained within a hybrid motion/actions controller setup.  Specific hardware for this
   solution includes an Adafruit Feather M0 RFM69HCW 900MHz transmitter.

   SPECIAL THANKS AND CREDIT:  Most especially to Kevin Holme over at Astromech.net who created the initial version of this setup and who deserves
                               all of the credit for sparking the great idea that this controller setup entails!

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
#include <RH_ASK.h>                   // info?


/* 
--------------------------------------------------------------
    INITIALIZATION AND SETUP
--------------------------------------------------------------
*/

//
// Setup the RFM69 Radio
//
#define RF69_FREQ      915.0  // specific operating frequency for our radio (all radios in this circuit need to share the same frequency)
#define RFM69_SS           8  // slave select (CS) pin on the Adafruit Feather...which is connected to pin #8 on the Adafruit Feather M0 RFM95
#define RFM69_INT          3  // interrupt (IRQ) pin on the Adafruit Feather...which is connected to pin #3 on the Adafruit Feather M0 RFM95
#define RFM69_RST          4  // reset (RST) pin on the Adafruit Feather...which is connected to pin#4 on the Adafruit Feather M0 RFM95
#define MY_RADIO_ADDRESS  10  // Must be a UNIQUE number for each radio within the radio group
#define RADIO_POWER       20  // power in dbm for the HCW69 radio...range is from 14-20
#define DEST_ADDRESS     255  // "255" is the "broadcast to all radios" address;  using this since we're sending to all radios and not expecting ping back
// NOTE AND CAUTION:  The following radio encryption key must be unique for your specific setup and must not be shared by any other radio group in the area 
//                    or else you will have radio conflicts.  Make it unique, make it safe.  The below example IS JUST AN EXAMPLE...PLEASE CHANGE IT!
uint8_t encryptRadioKey[] = {0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03};
RH_RF69 rf69(RFM69_SS, RFM69_INT);  // initialize the radio driver
RHReliableDatagram rf69_manager(rf69, MY_RADIO_ADDRESS); // initialize the datagram manager for this specific radio

//
// Setup the I2C device
//
#define MAINBODYI2C 10
int myi2c = MAINBODYI2C; // the i2c address of this device

/* 
--------------------------------------------------------------
    LOCAL CONSTANTS
--------------------------------------------------------------
*/
#define NUM_OF_MENU_PAGES 11  // the number of screen menu pages to be shown


// timing controls that allow certain functions in the loop() to only run periodically instead of each loop() cycle
const int showMonitorTimeDelay = 1000;  // sets the time delay (in ms) for the update of the serial monitor
const int radioSendTimeDelay = 200;     // sets the time delay (in ms) between broadcast events over the radio trasmitter


/* 
--------------------------------------------------------------
    LOCAL VARIABLES
--------------------------------------------------------------
*/

int i2cInputCommand = 0;                   // holds currently processing I2C command

//
// Setup the data structure used to send/receive messages across the radio network
//
// NOTE: defines the radio packet's structure for transmitting data
//       - part1 and part2 are for the menu screen number
//       - part3 is the pushbutton choice
//       - the current design limits us to 99 screens of 8 menu choices per screen, for a total of 792 individual commands.  Is that enough?
struct messageStruct {
  byte part1;  // menu page most significant digit
  byte part2;  // menu page least significant digit
  byte part3;  // menu selection value, if any
} RadioPacket;
// placeholders for the temporary values before transmitting
byte val1;
byte val2;
byte val3;

//
// loop timings controls
//
unsigned long currentTime = millis();      // stores the value of millis() in each iteration of loop()
unsigned long previousRadioMonitorTime = millis();
unsigned long previousRadioSendTime = millis();

//
// values to control the menu page selection
//
int menuPage = 1; //variable to increment through menu list;  begin on the first menu page = 1

bool testingTF = false;
int16_t packetnum = 0;


/* 
--------------------------------------------------------------
    ARDUINO SETUP - RUNS ONCE
--------------------------------------------------------------
*/
void setup() {

  delay(1000); // I have had various issues with Arduino-style chips not processing some of the Setup commands correctly.  Adding a delay here seems to help.  Your mileage may vary.
  
  Serial.begin(115200);  // initialize the serial communicator (for monitoring);
  Serial.println("Beginning setup and intialization routines for the MAIN BODY controller...");

  // Set all of the physical Arduino pins' modes as needed
  pinMode(RFM69_RST, OUTPUT);  // sets the radio's reset pin so that we can control it to turn the radio on/off
  
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

  // Initialize the I2C
  Serial.print("Beginning the I2C controller...");
  Wire.begin(myi2c);  // Start I2C communication Bus
  Wire.onReceive(receiveI2CEvent); // routine to call/run when we receive an i2c command
  Serial.println("...I2C setup now complete!");

  // Intialize the radio message packet
  RadioPacket.part1 = 0;
  RadioPacket.part2 = 0;
  RadioPacket.part3 = 0;

  delay(500);  //pause for 1/2 second and let the radio settle

  Serial.println("Setup COMPLETE for the MAINBODY controller!");
  
} //setup()


/* 
--------------------------------------------------------------
    ARDUINO LOOP - RUNS CONTINUOUSLY
--------------------------------------------------------------
*/
void loop() {

  if (testingTF) {
  	delay(1000);
  	char radiopacketdata[20]="Hello World #";
  	itoa(packetnum++, radiopacketdata+13, 10);
  	Serial.print("Sending ");
  	Serial.println(radiopacketdata);
  	rf69.send((uint8_t *)radiopacketdata, strlen(radiopacketdata));
  	rf69.waitPacketSent();
  } else {

	  currentTime = millis(); // hold the current system time

	  // if debugging the radio, we can show its values periodically
	  //if (millis() - previousRadioMonitorTime >= showMonitorTimeDelay) {
	  //  previousRadioMonitorTime = currentTime;   
	    //DebugRadio();
	  //}

	  if (i2cInputCommand == -1) {
	    // do nothing as the I2C value has not changed or is a default
	  } else {
	    commandHandler(i2cInputCommand);
	    SendRadio();
	    Serial.print("On menu page ");
	    Serial.print(val1);
	    Serial.print(val2);
	    Serial.print(" and button option ");
	    Serial.println(val3);
	  }

	  // send the current menu screen and selection choice to the radio for receivers to "hear" it...but only do it periodically
	  //if (millis() - previousRadioSendTime >= radioSendTimeDelay) {
	  //  previousRadioSendTime = currentTime;
	  //  SendRadio();
	  //}
  }
} // loop()



/* 
--------------------------------------------------------------
    SUPPORTING FUNCTIONS
--------------------------------------------------------------
*/

//-----------------------------------------------------
// i2c Command Event Handler
//-----------------------------------------------------

void receiveI2CEvent(int howMany) {
  Serial.print("Received I2C value of: ");
  i2cInputCommand = Wire.read();
  Serial.println(i2cInputCommand);
  //delay(1000);
  //commandHandler(i2cInputCommand);
}

//-----------------------------------------------------
// commandHandler - process the received command
//   NOTE:  This MAIN BODY Feather board is mostly acting as a relay to get the STEALTH's I2C commands to the wireless radios.
//-----------------------------------------------------
void commandHandler(int i2cCommandValue){

  // setting the three values for the radio transmission...
  //   val1 = menu page most significant digit
  //   val2 = menu page least significant digit
  //   val3 = menu selection value, if any

  // expecting the STEALTH to send one of the following via I2C
  // "66" = move the menu one page forward
  // "44" = move the menu one page backward
  // "2" through "2222" = button 1 through 4
  // "8" through "8888" = button 5 through 8

  switch(i2cCommandValue) {
    case 66: // increase the menu page value...we only allow menu pages 1 through 11 and will rotate at the end
      if (menuPage == 11) {
        menuPage = 1;
      } else {
        menuPage++;
      }
      val1 = menuPage / 10;
      val2 = menuPage % 10;
      val3 = 0;
      i2cInputCommand=-1;
      break;

    case 44: // decrease the menu page value...we only allow menu pages 1 through 11 and will rotate at the end
      if (menuPage == 1) {
        menuPage = 11;
      } else {
        menuPage--;
      }
      val1 = menuPage / 10;
      val2 = menuPage % 10;
      val3 = 0;
      i2cInputCommand=-1;
      break;

    case 2: // button choice 1 selected
      val1 = menuPage / 10;
      val2 = menuPage % 10;
      val3 = 1;
      i2cInputCommand=-1;
      break;

    case 22: // button choice 2 selected
      val1 = menuPage / 10;
      val2 = menuPage % 10;
      val3 = 2;
      i2cInputCommand=-1;
      break;

    case 222: // button choice 3 selected
      val1 = menuPage / 10;
      val2 = menuPage % 10;
      val3 = 3;
      i2cInputCommand=-1;
      break;

    case 2222: // button choice 4 selected
      val1 = menuPage / 10;
      val2 = menuPage % 10;
      val3 = 4;
      i2cInputCommand=-1;
      break;

    case 8: // button choice 5 selected
      val1 = menuPage / 10;
      val2 = menuPage % 10;
      val3 = 5;
      i2cInputCommand=-1;
      break;

    case 88: // button choice 6 selected
      val1 = menuPage / 10;
      val2 = menuPage % 10;
      val3 = 6;
      i2cInputCommand=-1;
      break;

    case 888: // button choice 7 selected
      val1 = menuPage / 10;
      val2 = menuPage % 10;
      val3 = 7;
      i2cInputCommand=-1;
      break;

    case 8888: // button choice 8 selected
      val1 = menuPage / 10;
      val2 = menuPage % 10;
      val3 = 8;
      i2cInputCommand=-1;
      break;

    default:
      i2cInputCommand=-1;
      break;       
  } // end SWITCH
}

// -------------------------------------------------------------------------------------
// void SendRadio() - activates the radio and broadcasts the values stored in the buffer
// -------------------------------------------------------------------------------------
void SendRadio() {
  // set the values in the data packet to be transmitted
  RadioPacket.part1 = val1;
  RadioPacket.part2 = val2;
  RadioPacket.part3 = val3;
  
  byte packetSize = sizeof(RadioPacket);
  byte buf[sizeof(RadioPacket)] = {0};
  memcpy (buf, &RadioPacket, packetSize);  // copy the data packet to the memory buffer 
 
  Serial.print("Sending radio data: ");
  Serial.print(val1);
  Serial.print(" / ");
  Serial.print(val2);
  Serial.print(" / ");
  Serial.println(val3);

  if (rf69_manager.sendtoWait(buf, packetSize, DEST_ADDRESS)) {
    //  success!
  } else {
    Serial.println("Something went wrong with the radio tranmission!");
  }
}
  

// --------------------------------------------------------------------------------------------
// void DebugRadio() - evaluates the radio and signal status and echos some info to the monitor
// --------------------------------------------------------------------------------------------
void DebugRadio() {
  Serial.print("RSSI: ");
  Serial.println(rf69.lastRssi(), DEC);
  Serial.print("Sending...");
  //Serial.println(switchesInt);
   
  byte packetSize=sizeof(RadioPacket);
  byte buf[sizeof(RadioPacket)] = {0};
  if (rf69_manager.sendto(buf, packetSize, DEST_ADDRESS)) {
    Serial.println("Message sent successfully!");   
  } else {
    Serial.println("Radio send FAILED!!!");
  }    
}
