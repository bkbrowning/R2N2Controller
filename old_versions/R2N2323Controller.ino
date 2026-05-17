/*
 * R2N2323Controller.ino
 *
 * Based heavily on the original 323 controller code written by Kevin Holme.  Original comments from Kevin:
 *   this is the code I used when I took my bare 3-2-3 frame to R2LA.  
 *   My goal here was to make as simple a system as possible with enough safeguards 
 *   as to not function, rather than faceplant when something goes wrong. 
 *   Only 4 limit switches, up and down for both tilt and center foot.  If it is not in one of those 2 positions , nothing works. 
 * 
 *   triggering was accomplished by using the spring loaded joystick  on an rc radio for up and down, with a toggle switch on the radio as a master switch. 
 * 
 *   This was written for a Pro Mini. But that was because I had one.  Any arduino will do.
 *
 *---------------------------------------------------------------------------------------------
 * Versioning
 *   - 0.1 - Initial version updated by Brent Browning.  Organized code and modified some syntax and arrangements for personal clarity.
 *   - 0.2 - Removed inputs from radio controller and replaced with I2C inputs.  Triggering from I2C within my droid configuration
 */


/*
 * INCLUDES
 */
#include <Wire.h>           // I2C communications
#include <USBSabertooth.h>  // Sabretooth motor controller

// setup the Sabretooth controller 
USBSabertoothSerial C;             
USBSabertooth       ST(C, 128);              // Use address 128.


// TIMING VARS
const int LoopTime = 1000;
const int ReadInputsInterval= 101;
const int DisplayInterval = 1000;
const int StanceInterval = 100;
const int ShowTimeInterval = 100;
unsigned long currentMillis = 0;      // stores the value of millis() in each iteration of loop()
unsigned long PreviousReadInputsMillis = 0;   // 
unsigned long PreviousDisplayMillis = 0; 
unsigned long PreviousStanceMillis = 0;
unsigned long PreviousShowTimeMillis = 0; 
unsigned long lastDebounceTime = 0;  // the last time the output pin was toggled
unsigned long debounceDelay = 50;    // the debounce time; increase if the output flickers
unsigned long ShowTime = 1;

// PIN ASSIGNMENTS for the Arduino
const byte TiltUpPin = 6;   //Limit switch input pin, Grounded when closed
const byte TiltDnPin = 7;   //Limit switch input pin, Grounded when closed
const byte LegUpPin = 8;    //Limit switch input pin, Grounded when closed
const byte LegDnPin = 9;    //Limit switch input pin, Grounded when closed

// GLOBAL VARS
int TiltUp;
int TiltDn;
int LegUp;
int LegDn;
int Stance;
int StanceTarget;
int LegHappy;
int TiltHappy;
int i2cCommand = 0;
int myi2c = 15; // the i2c address of this device


//-----------------------------------------------------------------Setup----------------------------------
void setup() {
  pinMode(6, INPUT_PULLUP);
  pinMode(7, INPUT_PULLUP);
  pinMode(8, INPUT_PULLUP);
  pinMode(9, INPUT_PULLUP);  
  SabertoothTXPinSerial.begin(9600); // 9600 is the default baud rate for Sabertooth Packet Serial.
  Serial.begin(9600);

  Serial.print("My i2C Address: ");
  Serial.println(myi2c);
  
  Wire.begin(myi2c);  // Start I2C communication Bus as a Slave (Device Number 9)
  Wire.onReceive(ReceiveI2CEvent); // routine to call/run when we receive an i2c command
}


void loop() {

  currentMillis = millis();  // this updates the current time each loop
  if (TiltDn == 0) {   // when the tilt down switch opens, the timer starts
    ShowTime = 0;
  }
  if (millis() - PreviousReadInputsMillis >= ReadInputsInterval) {
    PreviousReadInputsMillis = currentMillis;
    ReadInputs();
  }
  if (millis() - PreviousDisplayMillis >= DisplayInterval) {
    PreviousDisplayMillis = currentMillis;
    DisplayStatus();
  }
  if (millis() - PreviousStanceMillis >= StanceInterval) {
    PreviousStanceMillis = currentMillis;
    CheckStance();
  }

  Move();
  // the following lines triggers my showtime timer to advance one number every 100ms.
  // I find it easier to work with a smaller number, and it is all trial and error anyway. 
  if (millis() - PreviousShowTimeMillis >= ShowTimeInterval) {
    PreviousShowTimeMillis = currentMillis;
    ShowTime++;
  }

}

//
// ReceiveI2CEvent - detects incoming I2C data and updates latest stance target request when received
//
void ReceiveI2CEvent() {
  i2cCommand = Wire.read();    // receive transmission in its entirety;  we assume it to be valid (0 or 1 or 2) and we aren't doing any validation for now
  Serial.print("Received I2C command of ");
  Serial.println(i2cCommand); 
  // process stance target
  StanceTarget = i2cCommand;
  // NOTE:  May want to add both validation of the data and a timer to throw out any rapid changes from the I2C bus
}

//
// ReadInputs - records the current position data from the position sensors
//
void ReadInputs() {     //this reads our switch inputs
   TiltUp = digitalRead(TiltUpPin);
   TiltDn = digitalRead(TiltDnPin);
   LegUp = digitalRead(LegUpPin);
   LegDn = digitalRead(LegDnPin);
}


//------------------------------------------------Display Values-----------------------
// a bunch of serial display info for debugging
void DisplayStatus() {
  Serial.print("  Tilt Up  ");
  Serial.print(TiltUp);
  Serial.print("  Tilt Down  ");
  Serial.print(TiltDn); 
  Serial.print("  Leg Up  ");
  Serial.print(LegUp);
  Serial.print("  Leg Down  ");
  Serial.println(LegDn); 
  Serial.print("  Stance  ");
  Serial.print(Stance); 
  Serial.print("  Stance Target  ");
  Serial.print(StanceTarget); 
  Serial.print(" Leg Happy  ");
  Serial.print(LegHappy); 
  Serial.print(" Tilt Happy  ");
  Serial.print(TiltHappy); 
  Serial.print("  Show Time  ");
  Serial.println(ShowTime); 
}


//--------------------------------------------------------------------Check Stance-----------------------
// This is simply taking all of the possibilities of the switch positions and giving them a number. 
//and this loop is only allowerd to run if both my happy flags have been triggered. 
// at any time, including power up, the droid can run a check and come up with a number as to how he is standing. 

void CheckStance() {
  if(LegHappy == 0 && TiltHappy == 0) {
    if (LegUp == 0 && LegDn == 1 && TiltUp == 0 && TiltDn == 1 ) {
      Stance = 1;  // 3 leg mode
    }
    if (LegUp == 1 && LegDn == 0 && TiltUp == 1 && TiltDn == 0 ) {
      Stance = 2;  // 2 leg mode
    }
    if (LegUp == 0 && LegDn == 1 && TiltUp == 1 && TiltDn == 1 ) {
      Stance = 3;  // switch error state
    }
    if (LegUp == 1 && LegDn == 1 && TiltUp == 0 && TiltDn == 1 ) {
      Stance = 4;  // switch error state
    }
    if (LegUp == 1 && LegDn == 0 && TiltUp == 0 && TiltDn == 1 ) {
      Stance = 4;  // tilting with no center foot deployed...so likely in a bad state
    }
    if (LegUp == 1 && LegDn == 0 && TiltUp == 1 && TiltDn == 1 ) {
      Stance = 5;  // switch error state
    }
    if (LegUp == 1 && LegDn == 1 && TiltUp == 1 && TiltDn == 0 ) {
      Stance = 6;  // switch error state
    }
    if (LegUp == 1 && LegDn == 1 && TiltUp == 1 && TiltDn == 1 ) {
      Stance = 7;  // both switch sets in error state
    }
  }
}


//---------------------------------------------------------MOVE-----------------------------------
void Move() {
  // there is no stance target 0, so turn off your motors and do nothing. 
  if(StanceTarget == 0){
    ST.motor(1, 0);
    ST.motor(2, 0);
    LegHappy = 0;
    TiltHappy = 0;
    
  }
  // if you are told to go where you are, then do nothing
  if(StanceTarget == Stance){
    ST.motor(1, 0);
    ST.motor(2, 0);
    LegHappy = 0;
    TiltHappy = 0;
  }
  // Stance 7 is bad, all 4 switches open, no idea where anything is.  do nothing. 
  if(Stance == 7){
    ST.motor(1, 0);
    ST.motor(2, 0);
    LegHappy = 0;
    TiltHappy = 0;
  }
  // if you are in three legs and told to go to 2
  if(StanceTarget == 1 && Stance == 2){
    LegHappy = 1;
    TiltHappy = 1;
    ThreeToTwo();
  }
  // This is the first of the slight unknowns, target is two legs,  look up to stance 3, the center leg is up, but the tilt is unknown.
  //You are either standing on two legs, or already in a pile on the ground. Cant hurt to try tilting up. 
  if(StanceTarget == 1 && Stance == 3){
    TiltHappy = 1;
    MoveTiltUp();
  }
  // target two legs, tilt is up, center leg unknown, Can not hurt to try and lift the leg again. 
  if(StanceTarget == 1 && Stance == 4){
    LegHappy = 1;
    MoveLegUp();
  } 
  //Target is two legs, center foot is down, tilt is unknown, too risky do nothing.  
  if(StanceTarget == 1 && Stance == 5){
    ST.motor(1, 0);
    ST.motor(2, 0);
    LegHappy = 0;
    TiltHappy = 0;
  // target is two legs, tilt is down, center leg is unknown,  too risky, do nothing. 
  if(StanceTarget == 1 && Stance == 6){
    ST.motor(1, 0);
    ST.motor(2, 0);
    LegHappy = 0;
    TiltHappy = 0;
  } 
  // target is three legs, stance is two legs, run two to three. 
  if(StanceTarget == 2 && Stance == 1){
    LegHappy = 1;
    TiltHappy = 1;
    TwoToThree();
  } 
  //Target is three legs. center leg is up, tilt is unknown, safer to do nothing, Recover from stance 3 with the up command
  if(StanceTarget == 2 && Stance == 3){
    ST.motor(1, 0);
    ST.motor(2, 0);
    LegHappy = 0;
    TiltHappy = 0;
  }
  // target is three legs, but don't know where the center leg is.   Best to not try this, 
  // recover from stance 4 with the up command, 
  if(StanceTarget == 2 && Stance == 4){
    ST.motor(1, 0);
    ST.motor(2, 0);
    LegHappy = 0;
    TiltHappy = 0;
  }
  // Target is three legs, the center foot is down, tilt is unknownm. either on 3 legs now, or a smoking mess, 
  // nothing to loose in trying to tilt down again
  if(StanceTarget == 2 && Stance == 5){
    TiltHappy = 1;
    MoveTiltDn();
  }
  // kinda like above, Target is 3 legs, tilt is down, center leg is unknown, ......got nothing to loose. 
  if(StanceTarget == 2 && Stance == 6){
    LegHappy = 1;
    MoveLegDn();
  }
 }
}


/*
 * Actual movement commands are here,  when we send the command to move leg down, first it checks the leg down limit switch, if it is closed it 
 * stops the motor, sets a flag (happy) and then exits the loop, if it is open the down motor is triggered. 
 * all 4 work the same way
 */
//--------------------------------------------------------------------Move Leg Down---------------------------
void MoveLegDn() {
  LegDn = digitalRead(LegDnPin);
  if (LegDn == 0) {
    ST.motor(1, 0);     // Stop. 
    LegHappy = 0;
  }

  if (LegDn == 1) {
    ST.motor(1, 2047);  // Go forward at full power. 
  }
} 

//--------------------------------------------------------------------Move Leg Up---------------------------
void MoveLegUp() {
  LegUp = digitalRead(LegUpPin);
  if (LegUp == 0) {
    ST.motor(1, 0);     // Stop. 
    LegHappy = 0;
  }

  if (LegUp == 1) {
    ST.motor(1, -2047);  // Go forward at full power.
  }
}

//--------------------------------------------------------------------Move Tilt down---------------------------
void MoveTiltDn() {
  TiltDn = digitalRead(TiltDnPin);
  if (TiltDn == 0) {
    ST.motor(2, 0);     // Stop. 
    TiltHappy = 0;
  }

 if (TiltDn == 1) {
   ST.motor(2, 2047);  // Go forward at full power. 
 }
}

//--------------------------------------------------------------------Move Tilt Up---------------------------
void MoveTiltUp() {
  TiltUp = digitalRead(TiltUpPin);
  if (TiltUp == 0) {
    ST.motor(2, 0);     // Stop. 
    TiltHappy = 0;
  }

  if (TiltUp   == 1) {
    ST.motor(2, -2047);  // Go forward at full power.
  }

}


//----------------------------------------------------------------Two To Three -------------------------------
/*
 * this command to go from two to three, ended up being a combo of tilt down and leg down 
 * with a last second check each loop on the limit switches
 * timing worked out great, by the time the tilt down needed a center foot, it was there.
 */
void TwoToThree() {
  TiltDn = digitalRead(TiltDnPin);
  LegDn = digitalRead(LegDnPin);
    
  Serial.print("Go from TWO to THREE");
  if (LegDn == 0) {
    ST.motor(1, 0);
    LegHappy = 0;
  }
  if (LegDn == 1) {
    ST.motor(1, 2047);  // Go forward at full power. 
  }
  if (TiltDn == 0) {
    ST.motor(2, 0);
    TiltHappy = 0;
  }
  if (TiltDn == 1) {
    ST.motor(2, 2047);  // Go forward at full power. 
  }
}

//----------------------------------------------------------------Three To Two -------------------------------
/*
 * going from three legs to two needed a slight adjustment. I start a timer, called show time, and use it to delay the 
 * center foot from retracting.
 */
void ThreeToTwo() {
  TiltUp = digitalRead(TiltUpPin);
  LegUp = digitalRead(LegUpPin);
  TiltDn = digitalRead(TiltDnPin);
  
  //  First if the center leg is up, do nothing. 
  Serial.print("Go from THREE to TWO");
  if (LegUp == 0) {
    ST.motor(1, 0);
    LegHappy = 0;
  }
  //  If leg up is open AND the timer is in the first 20 steps then lift the center leg at 25 percent speed
  if (LegUp == 1 &&  ShowTime >= 1 && ShowTime <= 20) {
    ST.motor(1, -500);
  }
  //  If leg up is open AND the timer is over 21 steps then lift the center leg at full speed
  if (LegUp == 1 && ShowTime >= 21) {
    ST.motor(1, -2047);
  }
  // at the same time, tilt up till the switch is closed
  if (TiltUp == 0) {
    ST.motor(2, 0);
    TiltHappy = 0;
  }
  if (TiltUp == 1 ) {
    ST.motor(2, -2047);  // Go forward at full power. 
  }
}