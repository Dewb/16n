/*
  -------------------------------------------------------------------------------------------

  I2C2MIDI MK2 
  – Firmware v4_0

  https://github.com/attowatt/i2c2midi

  -------------------------------------------------------------------------------------------

  Copyright (c) 2022 attowatt (http://www.attowatt.com)
  
  MIT License
  
  Permission is hereby granted, free of charge, to any person obtaining a copy of this 
  software and associated documentation files (the "Software"), to deal in the Software 
  without restriction, including without limitation the rights to use, copy, modify, merge, 
  publish, distribute, sublicense, and/or sell copies of the Software, and to permit 
  persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or 
  substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, 
  INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR 
  PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE 
  LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT 
  OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR 
  OTHER DEALINGS IN THE SOFTWARE.
  
  -------------------------------------------------------------------------------------------
*/ 

// Turn on MK2 features
#define MK2

// USB Device
//   i2c2midi can also act as a USB device and send MIDI data over the Teensy Micro USB jack to a host (e.g. a computer).
//   Please note: Do not connect Teensy USB and Euro Power at the same time! Please cut the 5V pads on the Teensy!
//   Select Tools -> USB Type "MIDI" in Teensyduino, and uncomment the next line to turn the MIDI device feature on:
//#define USB_DEVICE

// Turn on debugging mode
//#define DEBUG      

// Turn on testing mode
//   Sending Channel 1, note 60, velocity 127
//#define TEST


// -------------------------------------------------------------------------------------------


#include <MIDI.h>                   
// MIT License – Copyright (c) 2016 Francois Best
// https://github.com/FortySevenEffects/arduino_midi_library
// Used for MIDI communication

#include <i2c_t3.h>                 
// MIT License – Copyright (c) 2013-2018, Brian (nox771 at gmail.com)
// https://github.com/nox771/i2c_t3
// Used for I2C communication 

// #ifdef MK2
//   #include <USBHost_t36.h>            
//   // MIT License – Copyright 2017 Paul Stoffregen (paul@pjrc.com)
//   // https://github.com/PaulStoffregen/USBHost_t36
//   // Used for USB MIDI Host
// #endif

#include <Ramp.h>
// GPL-3.0 License – Sylvain GARNAVAULT - 2016/08/07
// https://github.com/siteswapjuggler/RAMP
// Used for ramping CC values


// -------------------------------------------------------------------------------------------


// USB Host
//   The front panel USB jack ...
//   ... receives data from MIDI controllers (MIDI in)
//   ... sends out MIDI data to devices (MIDI out)
// #ifdef MK2
//   USBHost myusb;                                        
//   USBHub hub1(myusb);                                   // USB host: MIDI in     
//   MIDIDevice_BigBuffer midiDevice(myusb);               // USB host: MIDI out
// #endif

// I2C
// #define MEM_LEN 256                 // save I2C data in 8 bit chunks
// uint8_t i2cData[MEM_LEN];           // save I2C data in variable "i2cData"
// volatile uint8_t received;          // variable to store if there are received I2C messages
// void i2cReceiveEvent(size_t count); // function for receiving I2C messages, count = number of bites
// void i2cRequestEvent(void);            // function for receiving I2C messages, count = number of bites

// I2C Address
//   i2c2midi acts as a I2C follower and listens to messages on address 0x3F (63).
//   To use Teletype's distingEX OPs, change the address back to 0x42. 
//byte i2cAddress = 0x3F;             // official I2C address for Teletype I2M OPs
//byte i2cAddress = 0x42;           // legacy I2C address for Teletype distingEX OPs (EX1: 0x41, EX2: 0x42, EX3: 0x43, EX4: 0x44)

// MIDI TRS
//MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, MIDI);  

 


// -------------------------------------------------------------------------------------------

// channels
//#ifdef MK2
//  const byte channelsOut = 32;    // number of MIDI channels OUT (1-16 for TRS out, 17-32 for USB out)
//  const byte channelsIn = 16;     // number of MIDI channels IN  (16 for USB in)
//#else
  const byte channelsOut = 16;    // number of MIDI channels OUT
  const byte channelsIn = 0;     // number of MIDI channels IN
//#endif

// notes
const byte maxNotes = 8;
unsigned long notes[channelsOut][maxNotes][7];  
// array to store notes: 
// 0 : note number
// 1 : start time
// 2 : duration
// 3 : currently on/off
// 4 : velocity
// 5 : ratchet count
// 6 : repeat count
int noteCount[channelsOut];                        // total note count, per channel
int currentNote[channelsOut];                      // current note number between 0 and maxNotes, per channel
int currentNoteDuration = 100;                     // global setting for note duration
int8_t currentNoteShift = 0;                       // global setting for note shift
byte currentRepetition = 1;                        // global setting for note repetition
byte currentRatcheting = 1;                        // global setting for note ratcheting
const int ratchetingLength = 75;                   // in percent: 75 means 75% of original note length for racheted notes
byte noteUpperLimit = 127;                         // global setting for highest allowed midi note
byte noteLowerLimit = 0;                           // global setting for lowest allowed midi note

// CCs
int CCs[channelsOut][127][3];                    
// array to store CC values: 
// 1 : current value
// 2 : slew time in ms
// 3 : offset

// NRPNs
const byte maxNRPNs = 8;
int NRPNs[maxNRPNs][4];
// array to store NRPN settings
// 0 : channel
// 1 : controller
// 2 : offset
// 3 : slew time
byte nrpnCount = 0;

// Chords
const byte maxChords = 8;                          // number of chords
const byte chordMaxLength = 8;                     // maximum allowed length of chords
int chord[maxChords][chordMaxLength];              // array to store chords
byte chordNoteCount[maxChords];                    // number of added notes per chord
byte chordLength[maxChords];                       // size of chord that should be played
int currentChord[chordMaxLength];                  // array to store current chord with transformations
byte currentChordLength;                           // length of current chord
byte currentChordNoteCount;                        // number of added notes in current chord
int chordReverse[maxChords];                       // reverse transformation setting per chord
int chordRotate[maxChords];                        // rotate transformation setting per chord
int chordInversion[maxChords];                     // inversion transformation setting per chord
int chordStrumming[maxChords];                     // strumming transformation setting per chord

// Scheduled notes
const byte maxNotesScheduled = 42;                 // maximum allowed notes for scheduling
unsigned long scheduledNotes[maxNotesScheduled][5];
// array to store scheduled notes: 
// 0 : note number
// 1 : start time scheduled
// 2 : duration
// 3 : velocity
// 4 : channel
byte scheduledNoteCount = 0;                       // total count of scheduled notes

// Slew (Ramps)
const byte maxRamps = 8;                           // maximum allowed ramps
rampInt* myRamps = new rampInt[maxRamps];          // intialize 8 ramps 
int lastRampValues[maxRamps];                      // store the latest ramp values (used for comparison with new values)
int rampsAssignedCCs[maxRamps][3];                 // which CC was assigned to each ramp: channel, controller, NRPN?
const byte maxRampSpeed = 30;                      // shortest intervall between outgoing MIDI CC messages when ramping
byte rampCount;                                    // total ramp count
byte currentRamp;                                  // current ramp number between 0 and maxRamps

// USB MIDI in
byte CCsIn[channelsIn][127];                       // array to store MIDI CCs values received via USB MIDI in
byte noteHistoryIn[channelsIn][16][2];             // array to store the last 8 received MIDI notes: note number, velocity
const byte noteHistoryInLength = 16;               // length of the note history per channel
bool latch = true;                                 // latch setting for note history 
byte lastChannelIn = 1;                            // the last channel received via MIDI in
byte lastNoteIn = 0;                               // the last note number of Note On message received via MIDI in
byte lastVelocityIn = 0;                           // the last note velocity of Note On messagereceived via MIDI in
byte lastNoteOffIn = 0;                            // the last note number of Note Off message received via MIDI in  
byte lastCIn = 0;                                  // the last controller number received via MIDI in
byte lastCCIn = 0;                                 // the last CC value received via MIDI in

// LEDs
const byte led1 = 3;                               // pin definition for led 1
const byte led2 = 2;                               // pin definition for led 2
unsigned long lastLEDMillis1 = 0;                  // last time LED 1 turned on
unsigned long lastLEDMillis2 = 0;                  // last time LED 2 turned on
const byte animationSpeed = 100;                   // start up animation speed



// forward declarations

void blinkLED(int led) {}
void checkLEDs() {}

void midiCC(int channel, int controller, int value_, bool useRamp);
void handleRamp(int channel, int controller, int value, int slewTime, bool type);
void updateRamps();
int getNextFreeRamp();
int scaleUp (int value);
int scaleDown (int value);
void sendMidiCC(int channel, int controller, int value);
void playChord(int channel, int noteNumber, int velocity, int noteDuration, int chordNumber);
void removeFromChord(int chordNumber, int noteNumber);
void insertIntoChord(int8_t chordNumber, int8_t index, int8_t noteNumber);
void deleteFromChord(int8_t chordNumber, int8_t index);
void setChord(int8_t chordNumber, int8_t index, int8_t noteNumber);
void clearChord(int chordNumber);
void reverseChord();
void rotateChordLeft(int amount);
void rotateChordRight(int amount);
int mod(int a, int b);
void sendMidiProgramChange(int channel, int programNumber);
void sendMidiPitchBend(int channel, int value);
void sendMidiAftertouch(int channel, int value);
void sendMidiClock();
void sendMidiClockStart();
void sendMidiClockStop();
void sendMidiClockContinue();
void panic();
bool isTRS(int channel);
void midiNoteOn(int channel, int noteNumber_, int velocity, int noteDuration);
void checkNoteDurations();
void midiNoteOff(int channel, int noteNumber);
void sendMidiNoteOn(int channel, int noteNumber, int velocity);
void sendMidiNoteOff(int channel, int noteNumber);
void addToNoteHistoryIn(int channel, int noteNumber, int velocity);
void removeFromNoteHistoryIn(int channel, int noteNumber);
void setLatch(int value);
void scheduleNote(int channel, int noteNumber, int velocity, int noteDuration, int delay);
void checkScheduledNotes();
void NRPN(int channel, int controller, int value_, bool useRamp);
void sendNRPN(int channel, int controller, int value);
int8_t getNextFreeNRPN(int channel, int controller);
int getNRPNvalue(int channel, int controller, int index);
void opFunctions(bool isRequest, int8_t data[]);
void printNoteHistory(int channel);




// -------------------------------------------------------------------------------------------
// SETUP
// -------------------------------------------------------------------------------------------


void i2c2midi_setup() {

  // LEDs
  //pinMode(led1,OUTPUT); 
  //pinMode(led2,OUTPUT); 

  // I2C
  // Wire.begin(I2C_SLAVE, i2cAddress, I2C_PINS_18_19, I2C_PULLUP_EXT, 400000);  // setup for slave mode, address, pins 18/19, external pullups, 400kHz
  // received = 0;                                                               // i2c data initalize
  // memset(i2cData, 0, sizeof(i2cData));                                        // save I2C data in memory
  // Wire.onReceive(i2cReceiveEvent);                                            // register i2c events
  // Wire.onRequest(i2cRequestEvent);
  
  // serial
  //Serial.begin(115200);

  // MIDI & USB MIDI
  // MIDI.begin();
  // #ifdef MK2
  //   myusb.begin();
  // #endif

  // setup up ramps
  for (int i = 0; i < maxRamps; i++) {
    myRamps[i].setGrain(maxRampSpeed);
  }

  //start up animation
  // for (int i = 0; i < 4; i++) {
  //   digitalWrite(led2,HIGH); delay(animationSpeed);
  //   digitalWrite(led1,HIGH); delay(animationSpeed);
  //   digitalWrite(led2,LOW);  delay(animationSpeed);
  //   digitalWrite(led1,LOW);  delay(animationSpeed);
  // }

  // #ifdef DEBUG
  //   Serial.println("started");
  // #endif


}


// -------------------------------------------------------------------------------------------
// LOOP
// -------------------------------------------------------------------------------------------


void i2c2midi_loop(uint8_t* received, uint8_t i2cData[256]) {

  // I2C 
  if(*received > 0) {
    #ifdef DEBUG
      Serial.print("I2C received"); Serial.print(": ");
      Serial.print("0: "); Serial.print(i2cData[0]); Serial.print(";  ");
      Serial.print("1: "); Serial.print(i2cData[1]); Serial.print(";  ");
      Serial.print("2: "); Serial.print(i2cData[2]); Serial.print(";  ");
      Serial.print("3: "); Serial.print(i2cData[3]); Serial.print(";  ");
      Serial.print("4: "); Serial.print(i2cData[4]); Serial.print(";  ");
      Serial.print("5: "); Serial.print(i2cData[5]); Serial.println(";  ");
    #endif

    opFunctions(false, (int8_t*)i2cData); // call the respective OP with isRequest = false
    *received = 0;                // reset back to 0

  }
  
  // #ifdef MK2
  //   // USB MIDI
  //   if (midiDevice.read()) {  
    
  //     uint8_t type =       midiDevice.getType();
  //     uint8_t channel =    midiDevice.getChannel();
  //     uint8_t data1 =      midiDevice.getData1();
  //     uint8_t data2 =      midiDevice.getData2();
  //     //const uint8_t *sys = midiDevice.getSysExArray();            // not used at the moment

  //     #ifdef DEBUG
  //       Serial.print("USB received");
  //       Serial.print("Type: "); Serial.println(type);
  //       Serial.print("channel: "); Serial.println(channel);
  //       Serial.print("data1: "); Serial.println(data1);
  //       Serial.print("data2: "); Serial.println(data2);
  //     #endif

  //     if (channel < 1 || channel > 16) return;

  //     // NOTE ON (status 144-159)
  //     if (type >= 144 && type <= 159) {
  //       int noteNumber = data1;
  //       if (noteNumber < 0 || noteNumber > 127) return;
  //       int velocity = data2;
  //       if (velocity < 0) {                                         // velocity = 0 is treated as Note Off
  //         if (!latch) {
  //           removeFromNoteHistoryIn(channel-1, noteNumber);         
  //         }
  //       };
  //       if (velocity > 127) velocity = 127;
  //       addToNoteHistoryIn(channel-1, noteNumber, velocity);        // store the note number to the history
  //       lastNoteIn = noteNumber;                                    // store note number as last received
  //       lastVelocityIn = velocity;
  //     }

  //     // NOTE OFF (status 128-143)
  //     else if (type >= 128 && type <= 143) {
  //       int noteNumber = data1;
  //       if (noteNumber < 0 || noteNumber > 127) return;
  //       if (!latch) {
  //         removeFromNoteHistoryIn(channel-1, noteNumber);           // remove the note number from the history if latch = 0 
  //       }
  //       lastNoteOffIn = noteNumber;                                 // store note number as last received
  //     }
      
  //     // CC (status 176-191)
  //     else if (type >= 176 && type <= 191) {
  //       int controller = data1;
  //       if (controller < 0 || controller > 127) return;
  //       int value = data2;
  //       if (value < 0)   value = 0;
  //       if (value > 127) value = 127;
  //       CCsIn[channel-1][controller] = value;                       // store CC value 
  //       lastCIn = controller;                                       // store controller number as last received
  //       lastCCIn = value;                                           // store CC value as last received
  //     }
      
  //     lastChannelIn = channel;                                      // store the channel as last used channel

  //     blinkLED(2);

  //   }
  // #endif

  checkNoteDurations();             // check if there are notes to turn off
  checkScheduledNotes();            // check scheduled notes
  updateRamps();                    // update all ramps for MIDI CC 
  checkLEDs();                      // check if the LEDs should be turned off
  
  #ifdef TEST
    TESTFunction();                 // function for testing
  #endif

}




// -------------------------------------------------------------------------------------------
// MIDI CCs
// -------------------------------------------------------------------------------------------


// function for handling MIDI CCs
void midiCC(int channel, int controller, int value_, bool useRamp) {
  
  // keep values in range
  if (channel < 0 || channel >= channelsOut) return;
  if (controller < 0 || controller > 127) return;
  
  int offset = CCs[channel][controller][2];          
  int value = value_ + offset;
  if (value < 0) value = 0;
  if (value > 16383) value = 16383;
  
  int slewTime = CCs[channel][controller][1];             // get the set slew time for controller

  // if ramp is allowed and slew time higher than zero: use a ramp
  if (useRamp == true && slewTime != 0) {
    handleRamp(channel, controller, value, slewTime, 0);
  } 
  // if ramp is not allowed and slew time higher than zero: 
  // CC.SET, use a ramp with slew time = 1 to set it to new value immediately but using a ramp
  else if (useRamp == false && slewTime != 0) {
    handleRamp(channel, controller, value, 1, 0);
  }
  // if slewTime is zero, don't use a ramp
  else if (slewTime == 0) {
    sendMidiCC(channel, controller, scaleDown(value)); 
  }
  CCs[channel][controller][0] = value;                    // store the new CC value in the CC array
}


// -------------------------------------------------------------------------------------------


// function for handling ramps
void handleRamp(int channel, int controller, int value, int slewTime, bool type) {
  int oldValue = CCs[channel][controller][0];             // get the old CC value temporarily
  int rampToUse = 0;                                      // set to zero, because no ramp is yet chosen
    // check if this controller is already assigned to a ramp, if yes: use the same ramp, if no: get a new ramp to use
    for (int i = 0; i < maxRamps; i++) {
      if (rampsAssignedCCs[i][0] == channel && rampsAssignedCCs[i][1] == controller) {
        #ifdef DEBUG
          Serial.println("This CC already has a ramp assigned");
        #endif
        rampToUse = i;                                    // use the ramp to which this controller is already assigned
        break;
      }
    }
    if (!rampToUse) {                                     // if there's not yet a ramp assigned ...
        rampToUse = getNextFreeRamp();                    // check which new ramp to use
        rampsAssignedCCs[rampToUse][0] = channel;         // assign channel to this ramp
        rampsAssignedCCs[rampToUse][1] = controller;      // assign controller to this ramp
        rampsAssignedCCs[rampToUse][2] = type;            // Type: 0 = CC, 1 = NRPN
        myRamps[rampToUse].go(oldValue, 1);               // set the new ramp to the old value before using it
        #ifdef DEBUG
          Serial.println("Using a new ramp"); 
        #endif
    }
    #ifdef DEBUG   
      Serial.print("Using ramp: "); Serial.print(rampToUse); Serial.print(" for CC: "); Serial.println(controller);
    #endif 
                      
    // start the ramp
    myRamps[rampToUse].go(value, slewTime);
}


// -------------------------------------------------------------------------------------------


// function for updating ramps
void updateRamps() {
  for (int i = 0; i < maxRamps; i++) {                                      // go through all ramps
    if (myRamps[i].isRunning()) {                                       // check if the ramp is running
      int currentValue = myRamps[i].update();                           // update the ramp
      //int currentValueScaled = scaleDown(currentValue);               // scale the value down from 14 bit to 0..127
      if (scaleDown(currentValue) != lastRampValues[i]) {               // if the updated ramp value is different...
        int channel = rampsAssignedCCs[i][0];                           // get the channel of the ramp
        int controller = rampsAssignedCCs[i][1];                        // get the controller of the ramp
        int type = rampsAssignedCCs[i][2];                              // get the type: 0 = CC, 1 = NRPN
        if (type == 1) {
          //Serial.print("CURRENT VALUE"); Serial.println(currentValue);
          sendNRPN(channel, controller, currentValue);                  // send NRPN
        } else {
          sendMidiCC(channel, controller, scaleDown(currentValue));     // send MIDI CC with new ramp value
        }
      }
      lastRampValues[i] = scaleDown(currentValue);                      // store the new value for comparison
    }  
  }
}


// -------------------------------------------------------------------------------------------


// function for getting next free ramp to use
int getNextFreeRamp() {
  rampCount += 1;                                         // count one ramp up                           
  currentRamp = rampCount % maxRamps;                     // determine the current ramp number
  // check if next ramp number is still running; if yes, skip to next ramp number; 
  // if there's no more space available, replace the ramp
  for (int i = 0; i < maxRamps; i++) {                        
    if (myRamps[currentRamp].isRunning()) {
      rampCount += 1;
      currentRamp = rampCount % maxRamps;
    }
    else {
      break;
    }
  }
  return currentRamp;
}


// -------------------------------------------------------------------------------------------


// functions to scale up/down between 0..127 and 14 bit range
int scaleUp (int value) {
  return value * 129;                     // 129 == 16383 / 127
}
int scaleDown (int value) {
  int result = (value << 1) / 129;        // multiply by 2 so we can round below
  return (result >> 1) + (result & 1);    // divide by 2 and round
}


// -------------------------------------------------------------------------------------------


// function for sending MIDI CC
void sendMidiCC(int channel, int controller, int value) {
  if (isTRS(channel)) {
    MIDI.sendControlChange(controller, value, channel+1);
    #ifdef USB_DEVICE
      usbMIDI.sendControlChange(controller, value, channel+1);
    #endif
  } else {
    #ifdef MK2
      //midiDevice.sendControlChange(controller, value, channel+1-16);
    #endif
  }
  blinkLED(1); 
  #ifdef DEBUG  
    Serial.print("Sending MIDI CC: ");
    Serial.print(controller); Serial.print(", ");
    Serial.print("Ch: "); Serial.print(channel+1); Serial.print(", ");
    Serial.print("Val: "); Serial.println(value); 
  #endif 
}


// -------------------------------------------------------------------------------------------
// Chord
// -------------------------------------------------------------------------------------------

// order of chord transformations:
// REVERSE -> ROTATE -> INVERSION -> STRUMMING
// more to come ...



// function for playing chord
void playChord(int channel, int noteNumber, int velocity, int noteDuration, int chordNumber) {
  
  // keep values in range
  if (channel < 0 || channel >= channelsOut) return;
  if (noteNumber < 0 || noteNumber > 127) return;
  if (noteDuration < 0) return;
  if (chordNumber < 0 || chordNumber >= maxChords) return;
  
  currentChordLength = chordLength[chordNumber];
  currentChordNoteCount = chordNoteCount[chordNumber];

  // if no notes are defined for the chord, don't do anything
  if (currentChordLength == 0) return;

  int currentReverse = chordReverse[chordNumber];
  int currentRotate = chordRotate[chordNumber];
  int currentInversion = chordInversion[chordNumber];
  int currentStrumming = chordStrumming[chordNumber];

  // notes of the original chord are added to "currentChord" to which transformations are applied
  // this is done in reverse, so notes are played in the order they have been added
  for(int i = currentChordNoteCount-1; i >= 0; i--){
    currentChord[currentChordNoteCount-1-i] = chord[chordNumber][i];
  }

  // Transformation: REVERSE
  if (currentReverse) {
    reverseChord();
  }

  // Transformation: ROTATE
  if (currentRotate != 0) {
    if (currentRotate > 0) rotateChordRight(currentRotate);
    else if (currentRotate < 0) rotateChordLeft(abs(currentRotate)); 
  }

  for (int i = 0; i < currentChordLength; i++) {
    
    // Transformation: INVERSION
    int inversionAddon = 0;
    if (currentInversion >= 0) {           
      inversionAddon = floor((currentInversion + ((currentChordLength-1) - i)) / currentChordLength) * 12;
    } else if (currentInversion < 0) {     
      inversionAddon = floor((currentInversion + (0 - i)) / currentChordLength) * 12;
    }
    
    int chordNote = noteNumber + currentChord[i] + inversionAddon;   

    if (i == 0) {   
      midiNoteOn(channel, chordNote, velocity, noteDuration);
    }
    else {
        // Transformation: STRUMMING
        if (currentStrumming == 0) {
          midiNoteOn(channel, chordNote, velocity, noteDuration);
        } else {
          scheduleNote(channel, chordNote, velocity, noteDuration, currentStrumming * i);
        }
    }
  }
}


// -------------------------------------------------------------------------------------------


// function to remove a note from the chord based on noteNumber
void removeFromChord(int chordNumber, int noteNumber) {
  if (chordNumber < 0 || chordNumber >= maxChords) return;
  if (noteNumber < 0 || noteNumber > 127) return; 
  int c = chordNumber;
  int position = -1;
  for(int j = 0; j < chordMaxLength; j++){                       
     if(chord[c][j] == noteNumber){                   // find the position of the note
         position = j;                                // store the position
         break;                                       // stop the for loop
     }
  }
  if (position >= 0) {  
    deleteFromChord(chordNumber, position);
  }
}


// -------------------------------------------------------------------------------------------


// function to insert a note into chord at a specific index
void insertIntoChord(int8_t chordNumber, int8_t index, int8_t noteNumber) {
  if (chordNumber < 0 || chordNumber >= maxChords) return;
  int c = chordNumber;
  if (index < 0 || index > chordNoteCount[c]) return;
  if (noteNumber < -127 || noteNumber > 127) return;
  
  for (int i = chordMaxLength - 1; i >= index; i--) {
    chord[c][i] = chord[c][i-1];                             // shift note number values to the right by 1 index
  }
  chord[c][index] = noteNumber;                              // store new note number
  
  // set chord length automatically when new note is pushed  
  int newChordNoteCount = chordNoteCount[c] + 1;
  if (newChordNoteCount > 8) newChordNoteCount = 8;
  chordNoteCount[c] = newChordNoteCount;
  chordLength[c] = newChordNoteCount;
  
  #ifdef DEBUG
  Serial.print("chordNumber: "); Serial.println(c);
  Serial.print("chordLength: "); Serial.println(c);
    for (int i = 0; i < chordMaxLength-1; i++) {
      Serial.print(chord[c][i]); Serial.print(",");
    }
    Serial.println(chord[c][chordMaxLength-1]);
  #endif

}


// -------------------------------------------------------------------------------------------


// function to delete a note at specific index from chord
void deleteFromChord(int8_t chordNumber, int8_t index) {
  if (chordNumber < 0 || chordNumber >= maxChords) return;
  int c = chordNumber;
  if (index < 0 || index > chordNoteCount[c]) return;
  int position = index;
  
  for (int i = position; i < chordMaxLength; i++) {         // go through all elements right to the found position
    if (i == chordMaxLength) {                                                     
      chord[c][i] = 0;                               
    } else {
      chord[c][i] = chord[c][i+1];                          // shift note number values to the left by 1 index
    }
  }

  // set chord length automatically when a note is removed  
  int newChordNoteCount = chordNoteCount[c] - 1;
  if (newChordNoteCount < 0) newChordNoteCount = 0;
  chordNoteCount[c] = newChordNoteCount;
  chordLength[c] = newChordNoteCount;

  #ifdef DEBUG
    for (int i = 0; i < chordMaxLength-1; i++) {
      Serial.print(chord[c][i]); Serial.print(",");
    }
    Serial.println(chord[c][chordMaxLength-1]);
  #endif
}


// -------------------------------------------------------------------------------------------


// function to set specific index in chord
void setChord(int8_t chordNumber, int8_t index, int8_t noteNumber) {
  if (chordNumber < 0 || chordNumber >= maxChords) return;
  int c = chordNumber;
  if (index < 0 || index > chordNoteCount[c]) return;
  if (noteNumber < 0 || noteNumber > 127) return;
  
  chord[c][index] = noteNumber;

  #ifdef DEBUG
    for (int i = 0; i < chordMaxLength-1; i++) {
      Serial.print(chord[c][i]); Serial.print(",");
    }
    Serial.println(chord[c][chordMaxLength-1]);
  #endif
}


// -------------------------------------------------------------------------------------------


// function to clear chord
void clearChord(int chordNumber) {
  if (chordNumber < 0 || chordNumber >= maxChords) return;
  int c = chordNumber;
  for (int i = 0; i < chordMaxLength; i++) {
    chord[c][i] = 0;
  }
  chordNoteCount[c] = 0;                           
  chordLength[c] = 0;  
  #ifdef DEBUG
    Serial.println("Chord cleared");
  #endif
}


// -------------------------------------------------------------------------------------------


// function to reverse chord
void reverseChord() {
  int reversedChord[currentChordNoteCount];
  for(int i = currentChordNoteCount-1; i >= 0; i--){
    reversedChord[currentChordNoteCount-1-i] = currentChord[i];
  }
  for (int i = 0; i < currentChordNoteCount; i++) {
    currentChord[i] = reversedChord[i];
  }
  #ifdef DEBUG
    Serial.println("Chord reversed");
    for (int i = 0; i < chordMaxLength-1; i++) {
        Serial.print(currentChord[i]); Serial.print(",");
      }
      Serial.println(currentChord[chordMaxLength-1]);
  #endif
}


// -------------------------------------------------------------------------------------------


// function to rotate chord left
void rotateChordLeft(int amount) {
  if (amount < -127 || amount > 127 || amount == 0) return;
  
  for (int i = 0; i < amount; i++) {
    int firstElement = currentChord[0];
    for (int i = 0; i < currentChordNoteCount-1; i++) {
        currentChord[i] = currentChord[i+1];                        // shift left
      }
    currentChord[currentChordNoteCount-1] = firstElement;
  }
  #ifdef DEBUG
    Serial.println("Chord rotated left");
    for (int i = 0; i < chordMaxLength-1; i++) {
        Serial.print(currentChord[i]); Serial.print(",");
      }
      Serial.println(currentChord[chordMaxLength-1]);
  #endif
}


// function to rotate chord right
void rotateChordRight(int amount) {
  if (amount < -127 || amount > 127 || amount == 0) return;
  for (int i = 0; i < amount; i++) {
    int lastElement = currentChord[currentChordNoteCount-1];
    for (int i = currentChordNoteCount-1; i > 0; i--) {
        currentChord[i] = currentChord[i-1];                        // shift right
      }
    currentChord[0] = lastElement;
  }
  #ifdef DEBUG
    Serial.println("Chord rotated right");
    for (int i = 0; i < chordMaxLength-1; i++) {
        Serial.print(currentChord[i]); Serial.print(",");
      }
      Serial.println(currentChord[chordMaxLength-1]);
  #endif
}


// -------------------------------------------------------------------------------------------


// modulo helper function (that handles negative values correctly)
int mod(int a, int b) {
  int c = a % b;
  return (c < 0) ? c + b : c;
}


// -------------------------------------------------------------------------------------------
// MIDI misc
// -------------------------------------------------------------------------------------------


// function for sending MIDI Program Change
void sendMidiProgramChange(int channel, int programNumber) { 
  
  // keep values in range
  if (channel < 0 || channel >= channelsOut) return;
  if (programNumber < 0 || programNumber > 127) return;

  if (isTRS(channel)) {
    MIDI.sendProgramChange(programNumber, channel+1);
    #ifdef USB_DEVICE
      usbMIDI.sendProgramChange(programNumber, channel+1);
    #endif
  } else {
    #ifdef MK2
      // midiDevice.sendProgramChange(programNumber, channel+1-16);
    #endif
  }
  blinkLED(1);

  #ifdef DEBUG  
    Serial.print("Sending MIDI Program Change: ");
    Serial.print(programNumber); Serial.print(", ");
    Serial.println(channel+1);
  #endif 
}


// -------------------------------------------------------------------------------------------


// function for sending MIDI Pitch Bend
void sendMidiPitchBend(int channel, int value) {
  
  // keep values in range
  if (channel < 0 || channel >= channelsOut) return;
  if (value < -8192) value = -8192;
  else if (value > 8191) value = 8191;

  if (isTRS(channel)) {
    MIDI.sendPitchBend(value, channel+1);
    #ifdef USB_DEVICE
      usbMIDI.sendPitchBend(value, channel+1);
    #endif
  } else {
    #ifdef MK2
      // midiDevice.sendPitchBend(value, channel+1-16);
    #endif
  }
  blinkLED(1);
  
  #ifdef DEBUG  
    Serial.print("Sending MIDI Pitch Bend: ");
    Serial.print(value); Serial.print(", ");
    Serial.println(channel+1);
  #endif 
}


// -------------------------------------------------------------------------------------------


// function for sending MIDI Aftertouch
void sendMidiAftertouch(int channel, int value) {
  
  // keep values in range
  if (channel < 0 || channel >= channelsOut) return;
  if (value < 0)   value = 0;
  if (value > 127) value = 127;

  if (isTRS(channel)) {
    MIDI.sendAfterTouch(value, channel+1);
    #ifdef USB_DEVICE
      usbMIDI.sendAfterTouch(value, channel+1);
    #endif   
  } else {
    #ifdef MK2
      // midiDevice.sendAfterTouch(value, channel+1-16);
    #endif
  }        
  blinkLED(1);

  #ifdef DEBUG  
    Serial.print("Sending MIDI Aftertouch: ");
    Serial.print(value); Serial.print(", ");
    Serial.println(channel+1);
  #endif 
}


// -------------------------------------------------------------------------------------------


// function for sending MIDI Clock
void sendMidiClock() {
  MIDI.sendRealTime(midi::Clock);
  #ifdef USB_DEVICE
    usbMIDI.sendRealTime(usbMIDI.Clock);
  #endif   
  #ifdef MK2
    // midiDevice.sendRealTime(midiDevice.Clock);
  #endif
}


// -------------------------------------------------------------------------------------------


// function for sending MIDI Clock Start
void sendMidiClockStart() {
  MIDI.sendRealTime(midi::Start);
  #ifdef USB_DEVICE
    usbMIDI.sendRealTime(usbMIDI.Start);
  #endif   
  #ifdef MK2
    // midiDevice.sendRealTime(midiDevice.Start);
  #endif
  #ifdef DEBUG
    Serial.println("Sending MIDI Clock Start");
  #endif
}


// -------------------------------------------------------------------------------------------


// function for sending MIDI Clock Stop
void sendMidiClockStop() {
  MIDI.sendRealTime(midi::Stop);
  #ifdef USB_DEVICE
    usbMIDI.sendRealTime(usbMIDI.Stop);
  #endif  
  #ifdef MK2 
    // midiDevice.sendRealTime(midiDevice.Stop);
  #endif
  #ifdef DEBUG
    Serial.println("Sending MIDI Clock Stop");
  #endif
}


// -------------------------------------------------------------------------------------------


// function for sending MIDI Clock Continue
void sendMidiClockContinue() {
  MIDI.sendRealTime(midi::Continue);
  #ifdef USB_DEVICE
    usbMIDI.sendRealTime(usbMIDI.Continue);
  #endif 
  #ifdef MK2  
    // midiDevice.sendRealTime(midiDevice.Continue);
  #endif
  #ifdef DEBUG
    Serial.println("Sending MIDI Clock Continue");
  #endif
}


// -------------------------------------------------------------------------------------------


// MIDI panic
void panic() {
  
  // reset module
  currentNoteDuration = 100;
  currentNoteShift = 0;
  currentRepetition = 1;
  currentRatcheting = 1; 
  noteUpperLimit = 127;
  noteLowerLimit = 0;   

  // send note offs and reset ratchets and repeats
  for (int j=0; j < 16; j++) {                           
    for (int i=0; i <= 127; i++) {                            
      midiNoteOff(j, i);
    }
  } 
  for (int j=0; j < channelsOut; j++) {                           
    for (int i=0; i < maxNotes; i++) {                            
      notes[j][i][5] = 0;   // reset ratchet count
      notes[j][i][6] = 0;   // reset repeat count
    }
  } 
  // reset scheduled notes
  for (int i = 0; i < maxNotesScheduled; i++) {
    for (int j = 0; j < 5; j++) {
      scheduledNotes[i][j] = 0;
    }
    scheduledNoteCount -= 1;
  }

  #ifdef DEBUG
    Serial.println("OMG!!!!1");
  #endif
}

// -------------------------------------------------------------------------------------------
// Notes
// -------------------------------------------------------------------------------------------


// function for getting output type (TRS or USB) for channel number
bool isTRS(int channel) {
    // if (channel < 0 || channel >= channelsOut) return false;
    return channel >= 0 && channel < 16;         // if channel is  0..15 -> true -> TRS
}  


// -------------------------------------------------------------------------------------------


// function for handling MIDI Note Ons
void midiNoteOn(int channel, int noteNumber_, int velocity, int noteDuration) {
  
  // keep values in range
  if (channel < 0 || channel >= channelsOut) return;
  int noteNumber = noteNumber_ + currentNoteShift;
  if (noteNumber < 0 || noteNumber > 127) return; 
  if (noteDuration < 0) return;
  if (velocity < 1) {
    sendMidiNoteOff(channel, noteNumber);         // velocity = 0 is treated as Note Off
    return;
  }
  if (velocity > 127) velocity = 127;

  // check if this note is already playing; if yes, send note off first
  for (int i=0; i < maxNotes; i++) {
    if (notes[channel][i][0] == noteNumber && notes[channel][i][3] == 1) {
      #ifdef DEBUG
        Serial.println("Note is already playing"); 
      #endif
      sendMidiNoteOff(channel, notes[channel][i][0]);
      notes[channel][i][3] = 0;
    }
  }
  noteCount[channel] += 1;                                        // count one note up
  currentNote[channel] = noteCount[channel] % maxNotes;           // determine the current note number
  // check if next note number is still playing; if yes, skip to next note number; 
  // if there's no more space available, replace the note
  for (int i=0; i < maxNotes; i++) {                        
    if (notes[channel][currentNote[channel]][3] == 1) {
      noteCount[channel] += 1;                              
      currentNote[channel] = noteCount[channel] % maxNotes;
    }
    else {
      break;
    }
  }

  int newNoteDuration = noteDuration;

  if (currentRepetition > 1 || currentRatcheting > 1) {
    
    newNoteDuration = noteDuration / currentRatcheting * ratchetingLength / 100;
    if (newNoteDuration <= 0) return; 
    notes[channel][currentNote[channel]][5] = currentRepetition * currentRatcheting;  
  }
  
  // store the values for the note in the notes array
  notes[channel][currentNote[channel]][0] = noteNumber;           // note number
  notes[channel][currentNote[channel]][1] = millis();             // note start time
  notes[channel][currentNote[channel]][2] = newNoteDuration;      // note duration
  notes[channel][currentNote[channel]][3] = 1;                    // note is on
  notes[channel][currentNote[channel]][4] = velocity;             // note is on
  sendMidiNoteOn(channel, noteNumber, velocity);  
}


// -------------------------------------------------------------------------------------------


// function for checking note duration
void checkNoteDurations() {
  unsigned long currentTime = millis();                           // get current time
  for (int j=0; j < channelsOut; j++) {                           // go through all channels
    for (int i=0; i < maxNotes; i++) {                            // go through all notes
    
      if (notes[j][i][3] != 0 && notes[j][i][2] != 0) {           // check if note is currently playing and duration is not 0
        if (currentTime - notes[j][i][1] > notes[j][i][2]) {      // if yes, check if the note duration has been reached
          sendMidiNoteOff(j, notes[j][i][0]);                     // if yes, send MIDI Note off
          notes[j][i][3] = 0;                                     // set note on/off to off
        }
      } 

      // note repetition & ratcheting
      if (currentRepetition > 1 || currentRatcheting > 1) {       // if racheting or repetition is set than 1
      if (notes[j][i][5] > 1) {                                   // index 5 is ratchet/rep count, do if higher than 1
          if (currentTime - notes[j][i][1] > notes[j][i][2] * 100 / ratchetingLength) { 
            notes[j][i][1] = millis();                            // note start time
            notes[j][i][3] = 1;                                   // set note is on
            sendMidiNoteOn(j, notes[j][i][0], notes[j][i][4]);    // send same note with same velocity
            notes[j][i][5] -= 1;                                  // update ratchet count
          }
        }
      }  
      
    }
  }   
}


// -------------------------------------------------------------------------------------------


// function for handling MIDI Note Offs 
void midiNoteOff(int channel, int noteNumber) {
  if (channel < 0 || channel >= channelsOut) return;
  if (noteNumber < 0 || noteNumber > 127) return; 
  sendMidiNoteOff(channel, noteNumber);
}


// -------------------------------------------------------------------------------------------


// function for sending MIDI Note On
void sendMidiNoteOn(int channel, int noteNumber, int velocity) {
  
  // check min max note setting
  if (noteNumber < noteLowerLimit || noteNumber > noteUpperLimit) {
    return; 
  }
  
  if (isTRS(channel)) {
    MIDI.sendNoteOn(noteNumber, velocity, channel+1);
    #ifdef USB_DEVICE
      usbMIDI.sendNoteOn(noteNumber, velocity, channel+1);
    #endif
  } else {
    #ifdef MK2
      // midiDevice.sendNoteOn(noteNumber, velocity, channel+1-16);
    #endif
  }
  blinkLED(1);
  
  #ifdef DEBUG  
    Serial.print("Sending MIDI Note On: ");
    Serial.print(noteNumber); Serial.print(", ");
    Serial.print("Vel: "); Serial.print(velocity); Serial.print(", ");
    Serial.print("Ch: "); Serial.println(channel+1);
  #endif
}


// -------------------------------------------------------------------------------------------


// function for sending MIDI Note Off
void sendMidiNoteOff(int channel, int noteNumber) {

  // check min max note setting
  // if (noteNumber < noteLowerLimit || noteNumber > noteUpperLimit) {
  //   return; 
  // }

  if (isTRS(channel)) {
    MIDI.sendNoteOff(noteNumber, 0, channel+1);
    #ifdef USB_DEVICE
      usbMIDI.sendNoteOff(noteNumber, 0, channel+1);
    #endif
  } else {
    #ifdef MK2
      // midiDevice.sendNoteOff(noteNumber, 0, channel+1-16);
    #endif
  }
  blinkLED(1);

  #ifdef DEBUG  
    Serial.print("Sending MIDI Note Off: ");
    Serial.print(noteNumber); Serial.print(", ");
    Serial.print("Ch: "); Serial.println(channel+1);
  #endif
}


// -------------------------------------------------------------------------------------------
// Note History


// function for adding a received note number and velocity to the note history 
void addToNoteHistoryIn(int channel, int noteNumber, int velocity) {
  if (channel < 0 || channel >= channelsOut) return;
  if (noteNumber < 0 || noteNumber > 127) return; 
  if (velocity < 0) return;
  if (velocity > 127) velocity = 127;
  for (int i = noteHistoryInLength; i > 0; i--) {
    noteHistoryIn[channel][i][0] = noteHistoryIn[channel][i-1][0];    // shift note number values to the right by 1 index
    noteHistoryIn[channel][i][1] = noteHistoryIn[channel][i-1][1];    // shift note velocity values to the right by 1 index
  }
  noteHistoryIn[channel][0][0] = noteNumber;                          // store new note number at index 0
  noteHistoryIn[channel][0][1] = velocity;                            // store new note velocity at index 0
  #ifdef DEBUG
    printNoteHistory(channel);
  #endif
}


// -------------------------------------------------------------------------------------------


// function for removing a received note number and velocity from the note history
void removeFromNoteHistoryIn(int channel, int noteNumber) {
  if (channel < 0 || channel >= channelsOut) return;
  if (noteNumber < 0 || noteNumber > 127) return; 
  int position = -1;
  for(int j = 0; j < noteHistoryInLength; j++){                       
     if(noteHistoryIn[channel][j][0] == noteNumber){                  // find the position of the note
         position = j;                                                // store the position
         break;                                                       // stop the for loop
     }
  }
  if (position >= 0) {  
    for (int i = position; i < noteHistoryInLength; i++) {            // go through all elements right to the found position
      if (i == 7) {                                                     
        noteHistoryIn[channel][i][0] = 0;                               
        noteHistoryIn[channel][i][1] = 0;
      } else {
        noteHistoryIn[channel][i][0] = noteHistoryIn[channel][i+1][0];  // shift note number values to the left by 1 index
        noteHistoryIn[channel][i][1] = noteHistoryIn[channel][i+1][1];  // shift note velocity values to the left by 1 index
      }
    }
  }
  #ifdef DEBUG
    printNoteHistory(channel);
  #endif
}


// -------------------------------------------------------------------------------------------


// function for latch setting
void setLatch(int value) {
  if (value < 0 || value > 1) return;
  value ? latch = true : latch = false;

  // clear the note history when changing the latch setting
  for (int j = 0; j < channelsIn; j++) {
    for (int i = 0; i < noteHistoryInLength; i++) {
      noteHistoryIn[j][i][0] = 0;
      noteHistoryIn[j][i][1] = 0;
    }
  }

  #ifdef DEBUG
    Serial.println("op_I2M_Q_LATCH");
    Serial.print(" latch: "); Serial.println(value);  
  #endif
}

// -------------------------------------------------------------------------------------------


#ifdef DEBUG
  void printNoteHistory(int channel) {
    for (int i = 0; i < noteHistoryInLength; i++) {
      Serial.print(noteHistoryIn[channel][i][0]);
      Serial.print(",");
    }
    Serial.println(" ");
  }
#endif


// -------------------------------------------------------------------------------------------
// Schedule Notes


void scheduleNote(int channel, int noteNumber, int velocity, int noteDuration, int delay) {
  scheduledNotes[scheduledNoteCount][0] = noteNumber;
  scheduledNotes[scheduledNoteCount][1] = millis() + delay;
  scheduledNotes[scheduledNoteCount][2] = noteDuration;
  scheduledNotes[scheduledNoteCount][3] = velocity;
  scheduledNotes[scheduledNoteCount][4] = channel;
  #ifdef DEBUG  
    Serial.print("Scheduling note: "); Serial.println(scheduledNotes[scheduledNoteCount][0]);
  #endif
  scheduledNoteCount = (scheduledNoteCount + 1) % maxNotesScheduled;
}


// -------------------------------------------------------------------------------------------


void checkScheduledNotes() {
  unsigned long currentTime = millis();                   // get current time
  for (int i = 0; i < maxNotesScheduled; i++) {           // go through all scheduled notes
    if (scheduledNotes[i][1]) {                           // check if the entry is not empty
      if (currentTime > scheduledNotes[i][1]) {           // check if the scheduled time has been reached
        midiNoteOn(scheduledNotes[i][4], scheduledNotes[i][0], scheduledNotes[i][3], scheduledNotes[i][2]);
        #ifdef DEBUG  
          Serial.print("Playing scheduled note: "); Serial.println(scheduledNotes[i][0]);
        #endif
        for (int j = 0; j < 5; j++) {
          scheduledNotes[i][j] = 0;
        }
        scheduledNoteCount -= 1;
      }
    } 
  }
}

// -------------------------------------------------------------------------------------------
// NRPNs
// -------------------------------------------------------------------------------------------


// function for handling NRPN
void NRPN(int channel, int controller, int value_, bool useRamp) {
  
  // keep values in range
  if (channel < 0 || channel >= channelsOut) return;
  
  int offset = 0;
  int slewTime = 0;

  // find the position of the NRPN controller in the NRPN array
  for (int i=0; i < maxNRPNs; i++) {  
    if (NRPNs[i][1] == controller) {
        offset = NRPNs[i][2];   
        slewTime = NRPNs[i][3];
        break;
    } 
  } 

  // keep values in range
  int value = value_ + offset;
  if (value < 0)   value = 0;
  if (value > 16384) value = 16384;
  
  // if the slew time is higher than zero, a ramp is used to slew the value ...
  if (useRamp == true && slewTime != 0) {
    handleRamp(channel, controller, value, slewTime, 1);
  } else {
    sendNRPN(channel, controller, value);
  }

}


// -------------------------------------------------------------------------------------------


// function for sending NRPN
void sendNRPN(int channel, int controller, int value) {
  uint8_t controller_MSB = controller >> 7;
  uint8_t controller_LSB = controller & 0x7F;
  uint8_t value_MSB = value >> 7;
  uint8_t value_LSB = value & 0x7F;

  if (controller_MSB < 0 || controller_MSB > 127) return;
  if (controller_LSB < 0 || controller_LSB > 127) return;
  if (value_MSB < 0 || value_MSB > 127) return;
  if (value_LSB < 0 || value_LSB > 127) return;

  sendMidiCC(channel, 99, controller_MSB);
  sendMidiCC(channel, 98, controller_LSB);
  sendMidiCC(channel,  6, value_MSB);
  sendMidiCC(channel, 38, value_LSB);
}


// -------------------------------------------------------------------------------------------


// function for getting next free slot to store NRPN data
int8_t getNextFreeNRPN(int channel, int controller) {
  // check if controller is already stored in a slot
  for (int i=0; i < maxNRPNs; i++) {                        
    if (NRPNs[i][0] == channel && NRPNs[i][1] == controller) {
        Serial.println("already stored in NRPN array");
        return i;
    }
  }
  // if that did not find anything ...
  nrpnCount += 1;                                                   // count one slot up                
  byte currentNRPN = nrpnCount % maxNRPNs;                         // determine the current slot 
  // check if next slot is occupied; if yes, skip to next slot; 
  // if there's no more space available, replace the slot
  for (int i=0; i < maxNRPNs; i++) {                        
    if (NRPNs[currentNRPN][1] != 0) {
      nrpnCount += 1;
      currentNRPN = nrpnCount % maxNRPNs;
    }
    else {
      break;
    }
  }
  return currentNRPN;
}


// -------------------------------------------------------------------------------------------


int getNRPNvalue(int channel, int controller, int index) {
  for (int i=0; i < maxNRPNs; i++) {                               // go through the list of stored NRPN settings
    if (NRPNs[i][0] == channel && NRPNs[i][1] == controller) {     // check if there's data for requested CH & NRPN controller 
      return NRPNs[i][index];                                      // if yes, return the requested data
    }
  }
  return 0;                                                        // if no, return 0
}

// -------------------------------------------------------------------------------------------
// OPs
// -------------------------------------------------------------------------------------------




// -------------------------------------------------------------------------------------------
// MIDI out: Settings


void op_I2M_TIME_get(int8_t data[]) {
  const uint8_t response_MSB = currentNoteDuration >> 8;
  const uint8_t response_LSB = currentNoteDuration & 0xff;
  Wire.write(response_MSB);
  Wire.write(response_LSB);
  #ifdef DEBUG
    Serial.println("op_I2M_TIME_get");
    Serial.print(" note duration: "); Serial.println(currentNoteDuration);
  #endif  
}


void op_I2M_TIME_set(int8_t data[]) { 
  const uint8_t value_MSB = data[1];
  const uint8_t value_LSB = data[2];
  int16_t value = (value_MSB << 8 ) | value_LSB;
  if (value < 0) return;
  currentNoteDuration = value;
  #ifdef DEBUG
    Serial.println("op_I2M_TIME_set");
    Serial.print(" note duration: "); Serial.println(currentNoteDuration);
  #endif  
}


void op_I2M_SHIFT_get(int8_t data[]) {
  if (currentNoteShift < -127 || currentNoteShift > 127) {
    Wire.write(0);  
  } else {
    Wire.write(currentNoteShift);
  }
  #ifdef DEBUG
    Serial.println("op_I2M_SHIFT_get");
    Serial.print(" note shift: "); Serial.println(currentNoteShift);
  #endif
}


void op_I2M_SHIFT_set(int8_t data[]) {
  int8_t value = data[1];
  if (value < -127) value = -127;
  else if (value > 127) value = 127;
  currentNoteShift = value;
  #ifdef DEBUG
    Serial.println("op_I2M_SHIFT_get");
    Serial.print(" note shift: "); Serial.println(currentNoteShift);
  #endif
}


void op_I2M_REP_get(int8_t data[]) {
  const byte response = currentRepetition - 1; // -1 because internally 1 means "play note once" 
  if (response < 0 || response > 127) {
    Wire.write(255);
    Wire.write(255);
  } else {
    Wire.write(0);
    Wire.write(response);
  }
  #ifdef DEBUG
    Serial.println("op_I2M_REP_get");
    Serial.print(" note repetition: "); Serial.println(response);
  #endif
}


void op_I2M_REP_set(int8_t data[]) {
  const uint8_t value_MSB = data[1];
  const uint8_t value_LSB = data[2];
  int16_t value = (value_MSB << 8 ) | value_LSB;
  if (value < 0) value = 0;
  else if (value > 127) value = 127;
  currentRepetition = value + 1; // +1 because internally 1 means "play note once"    
  #ifdef DEBUG
    Serial.println("op_I2M_REP_set");
    Serial.print(" note repetition: "); Serial.println(currentRepetition);
  #endif
}


void op_I2M_RAT_get(int8_t data[]) {
  const byte response = currentRatcheting - 1; // -1 because internally 1 means "play note once" 
  if (response < 0 || response > 127) {
    Wire.write(255);
    Wire.write(255);
  } else {
    Wire.write(0);
    Wire.write(response);
  }
  #ifdef DEBUG
    Serial.println("op_I2M_RAT_get");
    Serial.print(" note ratcheting: "); Serial.println(response);
  #endif
}


void op_I2M_RAT_set(int8_t data[]) {
  const uint8_t value_MSB = data[1];
  const uint8_t value_LSB = data[2];
  int16_t value = (value_MSB << 8 ) | value_LSB;
  if (value < 0) value = 0;
  else if (value > 127) value = 127;
  currentRatcheting = value + 1; // +1 because internally 1 means "play note once"
  #ifdef DEBUG
    Serial.println("op_I2M_RAT_set");
    Serial.print(" note ratcheting: "); Serial.println(currentRatcheting);
  #endif
}


void op_I2M_MIN_get(int8_t data[]) {
  Wire.write(noteLowerLimit); 
}


void op_I2M_MIN_set(int8_t data[]) {
  int8_t value = data[1];
  if (value < 0) value = 0;
  else if (value > 127) value = 127;
  noteLowerLimit = value;
}


void op_I2M_MAX_get(int8_t data[]) {
  Wire.write(noteUpperLimit); 
}


void op_I2M_MAX_set(int8_t data[]) {
  int8_t value = data[1];
  if (value < 0) value = 0;
  else if (value > 127) value = 127;
  noteUpperLimit = value;
}


// -------------------------------------------------------------------------------------------
// MIDI out: Notes


void op_I2M_PANIC(int8_t data[]) {
  panic();
}


void op_I2M_NOTE(int8_t data[]) {
  const int8_t channel = data[1];
  const int8_t noteNumber = data[2];
  const int8_t velocity = data[3];
  #ifdef DEBUG
    Serial.println("op_I2M_NOTE"); 
    Serial.print(" channel: "); Serial.print(channel);
    Serial.print(" noteNumber: "); Serial.print(noteNumber);
    Serial.print(" velocity: "); Serial.println(velocity);
  #endif
  midiNoteOn(channel, noteNumber, velocity, currentNoteDuration);
}


void op_I2M_NOTE_O(int8_t data[]) {
  const int8_t channel = data[1];
  const int8_t noteNumber = data[2];
  #ifdef DEBUG
    Serial.println("op_I2M_NOTE_O");
    Serial.print(" channel: "); Serial.print(channel);
    Serial.print(" noteNumber: "); Serial.println(noteNumber);
  #endif
  midiNoteOff(channel, noteNumber);
}


// -------------------------------------------------------------------------------------------
// MIDI out: Chord


void op_I2M_C(int8_t data[]) {
  const int8_t channel = data[1];
  const int8_t chordNumber = data[2] - 1;
  const int8_t noteNumber = data[3];
  const int8_t velocity = data[4];
  #ifdef DEBUG
    Serial.println("op_I2M_C");
    Serial.print(" channel: "); Serial.print(channel);
    Serial.print(" chordNumber: "); Serial.print(chordNumber);
    Serial.print(" root note: "); Serial.print(noteNumber);
    Serial.print(" velocity: "); Serial.println(velocity);
  #endif
  playChord(channel, noteNumber, velocity, currentNoteDuration, chordNumber);
}


void op_I2M_C_ADD(int8_t data[]) {
  const int8_t chordNumber = data[1];
  const int8_t noteNumber = data[2]; 
  #ifdef DEBUG
    Serial.println("op_I2M_C_ADD");
  #endif
  
  if (chordNumber > 0) {
    insertIntoChord(chordNumber - 1, 0, noteNumber);
  } 
  else if (chordNumber == 0) {
    for (int i = 0; i < maxChords; i++) {
      insertIntoChord(i, 0, noteNumber);
    }
  }  

}


void op_I2M_C_RM(int8_t data[]) {
  const int8_t chordNumber = data[1];
  const int8_t noteNumber = data[2];
  #ifdef DEBUG
    Serial.println("op_I2M_C_RM");
    Serial.print(" chordNumber: "); Serial.print(chordNumber);
    Serial.print(" note: "); Serial.println(noteNumber);
  #endif
  
  if (chordNumber > 0) {
    removeFromChord(chordNumber - 1, noteNumber);
  } 
  else if (chordNumber == 0) {
    for (int i = 0; i < maxChords; i++) {
      removeFromChord(i, noteNumber);
    }
  }  

}


void op_I2M_C_CLR(int8_t data[]) {
  const int8_t chordNumber = data[1];
  #ifdef DEBUG
    Serial.println("op_I2M_C_CLR");
    Serial.print(" chordNumber: "); Serial.println(chordNumber);
  #endif
  
  if (chordNumber > 0) {
    clearChord(chordNumber - 1);
  } 
  else if (chordNumber == 0) {
    for (int i = 0; i < maxChords; i++) {
      clearChord(i);
    }
  }  
  
}


void op_I2M_C_L_get(int8_t data[]) {
  const int8_t chordNumber = data[1] - 1;
  const byte response = chordLength[chordNumber];
  if (chordNumber < 0 || chordNumber >= maxChords || response < 1 || response > 8) {
    Wire.write(-1);
  } else {
    Wire.write(response);
  }
  #ifdef DEBUG
    Serial.println("op_I2M_C_L_get");
    Serial.print(" chord length: "); Serial.println(response);
  #endif
}


void op_I2M_C_L_set(int8_t data[]) {
  const int8_t chordNumber = data[1];
  int8_t value = data[2];
  if (chordNumber < 0 || chordNumber > maxChords) return;
  if (value < 1 || value > 8) return;
  if (chordNumber > 0) {
    // user defined chord length must not be longer than notes in the chord
    if (value > chordNoteCount[chordNumber-1]) {
      value = chordNoteCount[chordNumber-1];
    }
    chordLength[chordNumber - 1] = value;
  } 
  else if (chordNumber == 0) {
    for (int i = 0; i < maxChords; i++) {
      if (value < chordNoteCount[i]) {
        chordLength[i] = value;
      }
    }
  }   
  #ifdef DEBUG
    Serial.println("op_I2M_C_L_set");
    Serial.print(" chord lengthh: "); Serial.println(chordLength[chordNumber-1]);
  #endif
}


void op_I2M_C_INV_get(int8_t data[]) {
  const int8_t chordNumber = data[1] - 1;
  const int response = chordInversion[chordNumber];
  if (chordNumber < 0 || chordNumber >= maxChords || response < -32 || response > 32) {
    Wire.write(0);
  } else {
    Wire.write(response);
  }
  #ifdef DEBUG
    Serial.println("op_I2M_C_INV_get");
    Serial.print(" chordNumber: "); Serial.print(chordNumber);
    Serial.print(" chord inversion: "); Serial.println(response);
  #endif
}


void op_I2M_C_INV_set(int8_t data[]) {
  const int8_t chordNumber = data[1];
  int8_t value = data[2];
  if (chordNumber < 0 || chordNumber > maxChords) return;
  if (value < -32 || value > 32) return;
  
  if (chordNumber > 0) {
    chordInversion[chordNumber - 1] = value;
  } 
  else if (chordNumber == 0) {
    for (int i = 0; i < maxChords; i++) {
      chordInversion[i] = value;
    }
  } 
  
  #ifdef DEBUG
    Serial.println("op_I2M_C_INV_set");
    Serial.print(" chordNumber: "); Serial.print(chordNumber);
    Serial.print(" chord inversion: "); Serial.println(chordInversion[chordNumber]);
  #endif  
}


void op_I2M_C_REV_get(int8_t data[]) {
  const int8_t chordNumber = data[1] - 1;
  const int response = chordReverse[chordNumber];
  if (chordNumber < 0 || chordNumber >= maxChords) {
    Wire.write(0);
  } else {
    Wire.write(response);
  }
  #ifdef DEBUG
    Serial.println("op_I2M_C_REV_get");
    Serial.print(" chordNumber: "); Serial.print(chordNumber);
    Serial.print(" chord reverse: "); Serial.println(response);
  #endif
}
void op_I2M_C_REV_set(int8_t data[]) {
  const int8_t chordNumber = data[1];
  int8_t value = data[2];
  if (chordNumber < 0 || chordNumber > maxChords) return;
  bool valueBool = mod(value, 2);

  if (chordNumber > 0) {
    chordReverse[chordNumber - 1] = valueBool;
  } 
  else if (chordNumber == 0) {
    for (int i = 0; i < maxChords; i++) {
      chordReverse[i] = valueBool;
    }
  } 
  
  #ifdef DEBUG
    Serial.println("op_I2M_C_REV_set");
    Serial.print(" chordNumber: "); Serial.print(chordNumber);
    Serial.print(" chord reverse: "); Serial.println(valueBool);
  #endif
}


void op_I2M_C_ROT_get(int8_t data[]) {
  const int8_t chordNumber = data[1] - 1;
  const int response = chordRotate[chordNumber];
  if (chordNumber < 0 || chordNumber >= maxChords) {
    Wire.write(0);
  } else {
    Wire.write(response);
  }
  #ifdef DEBUG
    Serial.println("op_I2M_C_ROT_get");
    Serial.print(" chordNumber: "); Serial.print(chordNumber);
    Serial.print(" chord rotation: "); Serial.println(response);
  #endif
}
void op_I2M_C_ROT_set(int8_t data[]) {
  const int8_t chordNumber = data[1];
  int8_t value = data[2];
  if (chordNumber < 0 || chordNumber > maxChords) return;
  if (value < -8 || value > 8) return;

  if (chordNumber > 0) {
    chordRotate[chordNumber - 1] = value;
  } 
  else if (chordNumber == 0) {
    for (int i = 0; i < maxChords; i++) {
      chordRotate[i] = value;
    }
  } 
    
  #ifdef DEBUG
    Serial.println("op_I2M_C_ROT_set");
    Serial.print(" chordNumber: "); Serial.print(chordNumber);
    Serial.print(" chord rotation: "); Serial.println(value);
  #endif  
}


void op_I2M_C_STR_get(int8_t data[]) {
  const int8_t chordNumber = data[1] - 1;
  if (chordNumber < 0 || chordNumber >= maxChords) {
    Wire.write(-1);
    Wire.write(-1);
  } else {
    const uint8_t response_MSB = chordStrumming[chordNumber] >> 8;
    const uint8_t response_LSB = chordStrumming[chordNumber] & 0xff;
    Wire.write(response_MSB);
    Wire.write(response_LSB);
  }
  #ifdef DEBUG
    Serial.println("op_I2M_C_STR_get");
    Serial.print(" chordNumber: "); Serial.print(chordNumber);
    Serial.print(" chord strumming: "); Serial.println(chordStrumming[chordNumber]);
  #endif  
}


void op_I2M_C_STR_set(int8_t data[]) { 
  const int8_t chordNumber = data[1];
  const uint8_t value_MSB = data[2];
  const uint8_t value_LSB = data[3];
  if (chordNumber < 0 || chordNumber > maxChords) return;
  const int16_t value = (value_MSB << 8 ) | (value_LSB & 0xff);
  if (value < 0) return;

  if (chordNumber > 0) {
    chordStrumming[chordNumber - 1] = value;
  } 
  else if (chordNumber == 0) {
    for (int i = 0; i < maxChords; i++) {
      chordStrumming[i] = value;  
    }
  } 
  
  #ifdef DEBUG
    Serial.println("op_I2M_C_STR_set");
    Serial.print(" chordNumber: "); Serial.print(chordNumber);
    Serial.print(" chord strumming: "); Serial.println(chordStrumming[chordNumber]);
  #endif  
}


void op_I2M_C_INS(int8_t data[]) {
  const int8_t chordNumber = data[1];
  const int8_t index = chordNoteCount[chordNumber] - data[2];
  const int8_t noteNumber = data[3];
  #ifdef DEBUG
    Serial.println("op_I2M_C_INS");
    Serial.print(" chordNumber: "); Serial.print(chordNumber);
    Serial.print(" index: "); Serial.print(index);
    Serial.print(" noteNumber: "); Serial.println(noteNumber);
  #endif  
  
  if (chordNumber > 0) {
    insertIntoChord(chordNumber - 1, index, noteNumber);
  } 
  else if (chordNumber == 0) {
    for (int i = 0; i < maxChords; i++) {
      insertIntoChord(i, index, noteNumber);
    }
  } 

}


void op_I2M_C_DEL(int8_t data[]) {
  const int8_t chordNumber = data[1];
  const int8_t index = chordNoteCount[chordNumber] - 1 - data[2];
  #ifdef DEBUG
    Serial.println("op_I2M_C_DEL");
    Serial.print(" chordNumber: "); Serial.print(chordNumber);
    Serial.print(" index: "); Serial.println(index);
  #endif  
  
  if (chordNumber > 0) {
    deleteFromChord(chordNumber - 1, index);
  } 
  else if (chordNumber == 0) {
    for (int i = 0; i < maxChords; i++) {
      deleteFromChord(i, index);
    }
  } 

}


void op_I2M_C_SET(int8_t data[]) {
  const int8_t chordNumber = data[1];
  const int8_t index = chordNoteCount[chordNumber] - 1 - data[2];
  const int8_t noteNumber = data[3];
  #ifdef DEBUG
    Serial.println("op_I2M_C_SET");
    Serial.print(" chordNumber: "); Serial.print(chordNumber);
    Serial.print(" index: "); Serial.print(index);
    Serial.print(" noteNumber: "); Serial.println(noteNumber);
  #endif  
  
  if (chordNumber > 0) {
    setChord(chordNumber - 1, index, noteNumber);
  } 
  else if (chordNumber == 0) {
    for (int i = 0; i < maxChords; i++) {
      setChord(i, index, noteNumber);
    }
  }
  
}


// -------------------------------------------------------------------------------------------
// MIDI out: CC


void op_I2M_CC(int8_t data[]) {
  const int8_t channel = data[1];
  const uint8_t controller = data[2];
  const uint8_t value_MSB = data[3];
  const uint8_t value_LSB = data[4];
  if (value_MSB < 0 || value_MSB > 127) return;
  if (value_LSB < 0 || value_LSB > 127) return;
  const int value = (value_MSB << 7) + value_LSB;
  #ifdef DEBUG
    Serial.println("op_I2M_CC");
    Serial.print(" channel: "); Serial.print(channel);
    Serial.print(" controller: "); Serial.print(controller);
    Serial.print(" value: "); Serial.print(value);
    Serial.print(" value scaled down: "); Serial.println(scaleDown(value));
  #endif
  midiCC(channel, controller, value, true);
}


void op_I2M_CC_OFF_get(int8_t data[]) {
  int8_t channel = data[1];
  int8_t controller = data[2];
  if (channel < 0 || channel >= channelsOut) channel = 0;
  if (controller < 0 || controller > 127) controller = 0;
  const int16_t response = CCs[channel][controller][2];
  Wire.write(response >> 8);
  Wire.write(response & 0xff);
  #ifdef DEBUG
    Serial.println("op_I2M_CC_OFF_get");
    Serial.print(" channel: "); Serial.print(channel);
    Serial.print(" controller: "); Serial.print(controller);
    Serial.print(" offset: "); Serial.println(response);
  #endif
}


void op_I2M_CC_OFF_set(int8_t data[]) {
  const int8_t channel = data[1];
  const int8_t controller = data[2];
  const uint8_t value_MSB = data[3];
  const uint8_t value_LSB = data[4];
  if (channel < 0 || channel >= channelsOut) return;
  if (controller < 0 || controller > 127) return;
  int16_t offset = (value_MSB << 8) + value_LSB;
  if (offset < -127 * 129)
    offset = -127 * 129;
  else if (offset > 127 * 129)
    offset = 127 * 129;
  CCs[channel][controller][2] = offset;       // store the offset in CCs array (index 2 = offset)
  #ifdef DEBUG
    Serial.println("op_I2M_CC_OFF_set");
    Serial.print(" channel: "); Serial.print(channel);
    Serial.print(" controller: "); Serial.print(controller);
    Serial.print(" offset: "); Serial.println(offset);
  #endif
}


void op_I2M_CC_SLEW_get(int8_t data[]) {
  int8_t channel = data[1];
  int8_t controller = data[2];
  if (channel < 0 || channel >= channelsOut) channel = 0;
  if (controller < 0 || controller > 127) controller = 0;
  const int16_t response = CCs[channel][controller][1];
  const int8_t response_MSB = response >> 8;
  const int8_t response_LSB = response & 0xff;
  Wire.write(response_MSB);
  Wire.write(response_LSB);
  #ifdef DEBUG
    Serial.println("op_I2M_CC_SLEW_get");
    Serial.print(" channel: "); Serial.print(channel);
    Serial.print(" controller: "); Serial.print(controller);
    Serial.print(" slew time: "); Serial.println(response);
  #endif
}


void op_I2M_CC_SLEW_set(int8_t data[]) {
  const int8_t channel = data[1];
  const int8_t controller = data[2];
  const uint8_t value_MSB = data[3];
  const uint8_t value_LSB = data[4];
  if (channel < 0 || channel >= channelsOut) return;
  if (controller < 0 || controller > 127) return;
  const int16_t value = (int16_t)(value_MSB << 8 ) | (value_LSB & 0xff);
  if (value < 0) return;
  CCs[channel][controller][1] = value;     // store the slew time in CCs array (index 1 = slew time)
  #ifdef DEBUG
    Serial.println("op_I2M_CC_SLEW_set");
    Serial.print(" channel: "); Serial.print(channel);
    Serial.print(" controller: "); Serial.print(controller);
    Serial.print(" slew time: "); Serial.println(value);
  #endif
}


void op_I2M_CC_SET(int8_t data[]) {
  const int8_t channel = data[1];
  const int8_t controller = data[2];
  const uint8_t value_MSB = data[3];
  const uint8_t value_LSB = data[4];
  if (value_MSB < 0 || value_MSB > 127) return;
  if (value_LSB < 0 || value_LSB > 127) return;
  const int value = (value_MSB << 7) + value_LSB;
  #ifdef DEBUG
    Serial.println("op_I2M_CC_SET");
    Serial.print(" channel: "); Serial.print(channel);
    Serial.print(" controller: "); Serial.print(controller);
    Serial.print(" value: "); Serial.print(value);
    Serial.print(" value scaled down: "); Serial.println(scaleDown(value));
  #endif
  midiCC(channel, controller, value, false);

}


// -------------------------------------------------------------------------------------------
// MIDI out: NRPN


void op_I2M_NRPN(int8_t data[]) {
  const int8_t channel = data[1];
  const uint8_t controller_MSB = data[2];
  const uint8_t controller_LSB = data[3];
  const uint8_t value_MSB = data[4];
  const uint8_t value_LSB = data[5];
  if (controller_MSB < 0 || controller_MSB > 127) return;
  if (controller_LSB < 0 || controller_LSB > 127) return;
  const int controller = (controller_MSB << 7) + controller_LSB;
  const int value = (value_MSB << 7) + value_LSB;
  #ifdef DEBUG
    Serial.println("op_I2M_NRPN");
    Serial.print(" channel: "); Serial.print(channel+1);
    Serial.print(" MSB: "); Serial.print(controller_MSB);
    Serial.print(" LSB: "); Serial.print(controller_LSB);
    Serial.print(" controller: "); Serial.print(controller);
    Serial.print(" value: "); Serial.println(value);
  #endif
  NRPN(channel, controller, value, true);
}


void op_I2M_NRPN_OFF_get(int8_t data[]) {
  const int8_t channel = data[1];
  const uint8_t controller_MSB = data[2];
  const uint8_t controller_LSB = data[3];
  const int controller = (controller_MSB << 7) + controller_LSB;
  int response;
  if (channel < 0 || channel >= channelsOut || controller_MSB < 0 || controller_MSB > 127 || controller_LSB < 0 || controller_LSB > 127)
    response = 0;
  else
    response = getNRPNvalue(channel, controller, 2);   // index 2 is offset
  Wire.write(response >> 8);
  Wire.write(response & 0xFF);
  #ifdef DEBUG
    Serial.println("op_I2M_NRPN_OFF_get");
    Serial.print(" channel: "); Serial.print(channel+1);
    Serial.print(" MSB: "); Serial.print(controller_MSB);
    Serial.print(" LSB: "); Serial.print(controller_LSB);
    Serial.print(" offset: "); Serial.println(response);
  #endif
}


void op_I2M_NRPN_OFF_set(int8_t data[]) {
  const int8_t channel = data[1];
  const uint8_t controller_MSB = data[2];
  const uint8_t controller_LSB = data[3];
  const uint8_t offset_MSB = data[4];
  const uint8_t offset_LSB = data[5];
  if (channel < 0 || channel >= channelsOut) return;
  if (controller_MSB < 0 || controller_MSB > 127) return;
  if (controller_LSB < 0 || controller_LSB > 127) return;
  const int controller = (controller_MSB << 7) + controller_LSB;
  int16_t offset = (offset_MSB << 8) | offset_LSB;  
  if (offset < -16384)
    offset = -16384;
  else if (offset > 16384)
    offset = 16384;
  // store data in the NRPNs array
  const int8_t position = getNextFreeNRPN(channel, controller);
  NRPNs[position][0] = channel;
  NRPNs[position][1] = controller;
  NRPNs[position][2] = offset;
  #ifdef DEBUG
    Serial.println("op_I2M_NRPN_OFF_set");
    Serial.print(" slot: "); Serial.print(position);
    Serial.print(" channel: "); Serial.print(channel+1);
    Serial.print(" MSB: "); Serial.print(controller_MSB);
    Serial.print(" LSB: "); Serial.print(controller_LSB);
    Serial.print(" offset: "); Serial.println(offset);
  #endif
}


void op_I2M_NRPN_SLEW_get(int8_t data[]) {
  const int8_t channel = data[1];
  const uint8_t controller_MSB = data[2];
  const uint8_t controller_LSB = data[3];
  const int controller = (controller_MSB << 7) + controller_LSB;
  int response;
  if (channel < 0 || channel >= channelsOut || controller_MSB < 0 || controller_MSB > 127 || controller_LSB < 0 || controller_LSB > 127)
    response = 0;
  else
   response = getNRPNvalue(channel, controller, 3); // index 3 is slew
  const uint8_t response_MSB = response >> 8;
  const uint8_t response_LSB = response & 0xFF;
  Wire.write(response_MSB);
  Wire.write(response_LSB);
  #ifdef DEBUG
    Serial.println("op_I2M_NRPN_SLEW_get");
    Serial.print(" channel: "); Serial.print(channel+1);
    Serial.print(" MSB: "); Serial.print(controller_MSB);
    Serial.print(" LSB: "); Serial.print(controller_LSB);
    Serial.print(" slew: "); Serial.println(response);
  #endif 
}


void op_I2M_NRPN_SLEW_set(int8_t data[]) {
  const int8_t channel = data[1];
  if (channel < 0 || channel >= channelsOut) return;
  const uint8_t controller_MSB = data[2];
  const uint8_t controller_LSB = data[3];
  const uint8_t slew_MSB = data[4];
  const uint8_t slew_LSB = data[5];
  const int controller = (controller_MSB << 7) + controller_LSB;
  const int16_t slew = (int16_t)(slew_MSB << 8 ) | (slew_LSB & 0xff);
  if (controller < 0 || controller > 16383) return;
  if (slew < 0) return;
  // store data in the NRPNs array
  const int8_t position = getNextFreeNRPN(channel, controller);
  NRPNs[position][0] = channel;
  NRPNs[position][1] = controller;
  NRPNs[position][3] = slew;
  #ifdef DEBUG
    Serial.println("op_I2M_NRPN_SLEW_set");
    Serial.print(" slot: "); Serial.print(position);
    Serial.print(" channel: "); Serial.print(channel+1);
    Serial.print(" MSB: "); Serial.print(controller_MSB);
    Serial.print(" LSB: "); Serial.print(controller_LSB);
    Serial.print(" controller: "); Serial.print(controller);
    Serial.print(" slew: "); Serial.println(slew);
  #endif
}


void op_I2M_NRPN_SET(int8_t data[]) {
  const int8_t channel = data[1];
  const uint8_t controller_MSB = data[2];
  const uint8_t controller_LSB = data[3];
  const uint8_t value_MSB = data[4];
  const uint8_t value_LSB = data[5];
  if (controller_MSB < 0 || controller_MSB > 127) return;
  if (controller_LSB < 0 || controller_LSB > 127) return;
  const int controller = (controller_MSB << 7) + controller_LSB;
  const int value = (value_MSB << 7) + value_LSB;
  #ifdef DEBUG
    Serial.println("op_I2M_NRPN_SET");
    Serial.print(" channel: "); Serial.print(channel+1);
    Serial.print(" MSB: "); Serial.print(controller_MSB);
    Serial.print(" LSB: "); Serial.print(controller_LSB);
    Serial.print(" controller: "); Serial.print(controller);
    Serial.print(" value: "); Serial.println(value);
  #endif
  NRPN(channel, controller, value, false);
}


// -------------------------------------------------------------------------------------------
// MIDI out: Misc


void op_I2M_PRG(int8_t data[]) {
  const int8_t channel = data[1];
  const int8_t program = data[2];
  #ifdef DEBUG
    Serial.println("op_I2M_PRG"); 
    Serial.print(" channel: "); Serial.print(channel);
    Serial.print(" program: "); Serial.println(program);
  #endif
  sendMidiProgramChange(channel, program);
}


void op_I2M_PB(int8_t data[]) {
  const int8_t channel = data[1];
  const uint8_t value_MSB = data[2];
  const uint8_t value_LSB = data[3];
  int16_t value = (value_MSB << 8) | value_LSB;
  #ifdef DEBUG
    Serial.println("op_I2M_PB"); 
    Serial.print(" value: "); Serial.println(value);
  #endif
  sendMidiPitchBend(channel, value);
}


void op_I2M_AT(int8_t data[]) {
  const int8_t channel = data[1];
  const int8_t value = data[2];
  #ifdef DEBUG
    Serial.println("op_I2M_AT"); 
    Serial.print(" channel: "); Serial.print(channel);
    Serial.print(" value: "); Serial.println(value);
  #endif
  sendMidiAftertouch(channel, value);
}


void op_I2M_CLK(int8_t data[]) {
  sendMidiClock();
  #ifdef DEBUG
    Serial.println("op_I2M_CLK");
  #endif
}


void op_I2M_START(int8_t data[]) {
  sendMidiClockStart();
  #ifdef DEBUG
    Serial.println("op_I2M_START");
  #endif
}


void op_I2M_STOP(int8_t data[]) {
  sendMidiClockStop();
  #ifdef DEBUG
    Serial.println("op_I2M_STOP");
  #endif
}


void op_I2M_CONT(int8_t data[]) {
  sendMidiClockContinue(); 
  #ifdef DEBUG
    Serial.println("op_I2M_CONT");
  #endif
}


// -------------------------------------------------------------------------------------------
// MIDI in: Settings


void op_I2M_Q_LATCH(int8_t data[]) {
  const int8_t value = data[1];
  setLatch(value);
}


// -------------------------------------------------------------------------------------------
// MIDI in: Notes


void op_I2M_Q_NOTE(int8_t data[]) {
  int8_t channel = data[1];
  int8_t n = data[2] % noteHistoryInLength;
  const int8_t noteNumber = noteHistoryIn[channel][n][0];        // array index 0 = note number
  if (noteNumber < 0 || noteNumber > 127) {
    Wire.write(0);
  } else {
    Wire.write(noteNumber);
  }
  #ifdef DEBUG
    Serial.println("op_I2M_Q_NOTE");
    Serial.print(" channel: "); Serial.print(channel);  
    Serial.print(" index: "); Serial.print(n);
    Serial.print(" note number: "); Serial.println(noteNumber);  
  #endif
}


void op_I2M_Q_VEL(int8_t data[]) {
  int8_t channel = data[1];
  int8_t n = data[2] % noteHistoryInLength;
  const int8_t velocity = noteHistoryIn[channel][n][1];          // array index 0 = note number
  if (velocity < 1 || velocity > 127) {
    Wire.write(0);
  } else {
    Wire.write(velocity);
  }
  #ifdef DEBUG
    Serial.println("op_I2M_Q_VEL");
    Serial.print(" channel: "); Serial.print(channel);  
    Serial.print(" index: "); Serial.print(n);
    Serial.print(" velocity: "); Serial.println(velocity);  
  #endif
}


// -------------------------------------------------------------------------------------------
// MIDI in: CCs


void op_I2M_Q_CC(int8_t data[]) {
  int8_t channel = data[1];
  const int8_t controller = data[2];
  const int8_t value = CCsIn[channel][controller];
  if (value < 0 || value > 127) {
    Wire.write(-1);
  } else {
    Wire.write(value);
  }
  #ifdef DEBUG
    Serial.println("op_I2M_Q_CC");
    Serial.print(" channel: "); Serial.print(channel);  
    Serial.print(" controller: "); Serial.print(controller);
    Serial.print(" value: "); Serial.println(value);  
  #endif
}


// -------------------------------------------------------------------------------------------
// MIDI in: Get latest ...


void op_I2M_Q_LCH(int8_t data[]) {
  const int8_t response = lastChannelIn;
  if (response < 0 || response > 15) {
    Wire.write(-1);
  } else {
    Wire.write(response);
  }
  #ifdef DEBUG
    Serial.println("op_I2M_Q_LCH");
    Serial.print(" last channel: "); Serial.println(response);  
  #endif
}


void op_I2M_Q_LN(int8_t data[]) {
  const int8_t response = lastNoteIn;
  if (response < 0 || response > 127) {
    Wire.write(-1);
  } else {
    Wire.write(response);
  }
  #ifdef DEBUG
    Serial.println("op_I2M_Q_LN");
    Serial.print(" last note: "); Serial.println(response);  
  #endif
}


void op_I2M_Q_LV(int8_t data[]) {
  const int8_t response = lastVelocityIn;
  if (response < 0 || response > 127) {
    Wire.write(-1);
  } else {
    Wire.write(response);
  }
  #ifdef DEBUG
    Serial.println("op_I2M_Q_LV");
    Serial.print(" last velocity: "); Serial.println(response);  
  #endif
}


void op_I2M_Q_LO(int8_t data[]) {
  const int8_t response = lastNoteOffIn;
  if (response < 0 || response > 127) {
    Wire.write(-1);
  } else {
    Wire.write(response);
  }
  #ifdef DEBUG
    Serial.println("op_I2M_Q_LO");
    Serial.print(" last note off: "); Serial.println(response);  
  #endif
}


void op_I2M_Q_LC(int8_t data[]) {
  const int8_t response = lastCIn;
  if (response < 0 || response > 127) {
    Wire.write(-1);
  } else {
    Wire.write(response);
  }
  #ifdef DEBUG
    Serial.println("op_I2M_Q_LC");
    Serial.print(" last controller: "); Serial.println(response);  
  #endif
}


void op_I2M_Q_LCC(int8_t data[]) {
  const int8_t response = lastCCIn;
  if (response < 0 || response > 127) {
    Wire.write(-1);
  } else {
    Wire.write(response);
  }
  #ifdef DEBUG
    Serial.println("op_I2M_Q_LCC");
    Serial.print(" last controller value: "); Serial.println(response);  
  #endif
}


void opFunctions(bool isRequest, int8_t data[]) {
  uint8_t op = data[0];
  switch (op) {

    // MIDI out: Settings
    case 1:     if (isRequest) op_I2M_TIME_get(data);      break;
    case 2:                    op_I2M_TIME_set(data);      break;
    case 3:     if (isRequest) op_I2M_SHIFT_get(data);     break;
    case 4:                    op_I2M_SHIFT_set(data);     break;
    case 5:     if (isRequest) op_I2M_REP_get(data);       break;
    case 6:                    op_I2M_REP_set(data);       break;
    case 7:     if (isRequest) op_I2M_RAT_get(data);       break;
    case 8:                    op_I2M_RAT_set(data);       break;
    case 9:     if (isRequest) op_I2M_MIN_get(data);       break;
    case 10:                   op_I2M_MIN_set(data);       break;
    case 11:    if (isRequest) op_I2M_MAX_get(data);       break;
    case 12:                   op_I2M_MAX_set(data);       break;
    
    // MIDI out: Notes
    case 20:                   op_I2M_NOTE(data);          break;
    case 21:                   op_I2M_NOTE_O(data);        break;
    case 22:                   op_I2M_PANIC(data);         break;
    
    // MIDI out: Chord
    case 30:                   op_I2M_C(data);             break;
    case 31:                   op_I2M_C_ADD(data);         break;
    case 32:                   op_I2M_C_RM(data);          break;
    case 33:                   op_I2M_C_CLR(data);         break;
    case 34:    if (isRequest) op_I2M_C_L_get(data);       break;
    case 35:                   op_I2M_C_L_set(data);       break;
    case 36:    if (isRequest) op_I2M_C_INV_get(data);     break;
    case 37:                   op_I2M_C_INV_set(data);     break;
    case 38:    if (isRequest) op_I2M_C_REV_get(data);     break;
    case 39:                   op_I2M_C_REV_set(data);     break;  
    case 155:   if (isRequest) op_I2M_C_ROT_get(data);     break;
    case 156:                  op_I2M_C_ROT_set(data);     break;
    case 150:   if (isRequest) op_I2M_C_STR_get(data);     break;
    case 151:                  op_I2M_C_STR_set(data);     break;
    case 152:                  op_I2M_C_INS(data);         break;
    case 153:                  op_I2M_C_DEL(data);         break;
    case 154:                  op_I2M_C_SET(data);         break;
    
    // MIDI out: CC
    case 40:                   op_I2M_CC(data);            break;
    case 41:    if (isRequest) op_I2M_CC_OFF_get(data);    break;
    case 42:                   op_I2M_CC_OFF_set(data);    break;
    case 43:    if (isRequest) op_I2M_CC_SLEW_get(data);   break;
    case 44:                   op_I2M_CC_SLEW_set(data);   break;
    case 45:                   op_I2M_CC_SET(data);        break;
    
    // MIDI out: NRPN
    case 50:                   op_I2M_NRPN(data);          break;
    case 51:    if (isRequest) op_I2M_NRPN_OFF_get(data);  break;
    case 52:                   op_I2M_NRPN_OFF_set(data);  break;
    case 53:    if (isRequest) op_I2M_NRPN_SLEW_get(data); break;
    case 54:                   op_I2M_NRPN_SLEW_set(data); break;
    case 55:                   op_I2M_NRPN_SET(data);      break;
    
    // MIDI out: misc
    case 60:                   op_I2M_PRG(data);           break;
    case 61:                   op_I2M_PB(data);            break;
    case 62:                   op_I2M_AT(data);            break;
    case 63:                   op_I2M_CLK(data);           break;
    case 64:                   op_I2M_START(data);         break;
    case 65:                   op_I2M_STOP(data);          break;
    case 66:                   op_I2M_CONT(data);          break;
    
    // ----------------------------------------------------------
    
    // MIDI in: Settings
    case 100:                  op_I2M_Q_LATCH(data);       break;
    
    // MIDI in: Notes
    case 110:   if (isRequest) op_I2M_Q_NOTE(data);        break;
    case 111:   if (isRequest) op_I2M_Q_VEL(data);         break;
    
    // MIDI in: CCs
    case 120:   if (isRequest) op_I2M_Q_CC(data);          break;
    
    // MIDI in: Get latest 
    case 130:   if (isRequest) op_I2M_Q_LCH(data);         break;
    case 131:   if (isRequest) op_I2M_Q_LN(data);          break;
    case 132:   if (isRequest) op_I2M_Q_LV(data);          break;
    case 133:   if (isRequest) op_I2M_Q_LO(data);          break;
    case 134:   if (isRequest) op_I2M_Q_LC(data);          break;
    case 135:   if (isRequest) op_I2M_Q_LCC(data);         break;

  }
}

