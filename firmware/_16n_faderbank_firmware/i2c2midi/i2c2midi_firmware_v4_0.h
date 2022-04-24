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


//#include <MIDI.h>                   
// MIT License – Copyright (c) 2016 Francois Best
// https://github.com/FortySevenEffects/arduino_midi_library
// Used for MIDI communication

//#include <i2c_t3.h>                 
// MIT License – Copyright (c) 2013-2018, Brian (nox771 at gmail.com)
// https://github.com/nox771/i2c_t3
// Used for I2C communication 

#ifdef MK2
  //  #include <USBHost_t36.h>            
  // MIT License – Copyright 2017 Paul Stoffregen (paul@pjrc.com)
  // https://github.com/PaulStoffregen/USBHost_t36
  // Used for USB MIDI Host
#endif

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

// send channels 17-32 to the host instead
#define midiDevice usbMIDI


// -------------------------------------------------------------------------------------------

// channels
#ifdef MK2
  const byte channelsOut = 32;    // number of MIDI channels OUT (1-16 for TRS out, 17-32 for USB out)
  const byte channelsIn = 16;     // number of MIDI channels IN  (16 for USB in)
#else
  const byte channelsOut = 16;    // number of MIDI channels OUT
  const byte channelsIn = 0;     // number of MIDI channels IN
#endif

// notes


const byte maxNotes = 8;
struct NOTE {
  uint8_t number;
  long start;
  long duration;
  bool on;
  uint8_t velocity;
  uint8_t rachet;
  uint8_t repeat;
} notes[channelsOut][maxNotes];
// array to store notes: 
// 0 : note number
// 1 : start time
// 2 : duration
// 3 : currently on/off
// 4 : velocity
// 5 : ratchet count
// 6 : repeat count
int16_t noteCount[channelsOut];                        // total note count, per channel
int16_t currentNote[channelsOut];                      // current note number between 0 and maxNotes, per channel
int16_t currentNoteDuration = 100;                     // global setting for note duration
int8_t currentNoteShift = 0;                       // global setting for note shift
byte currentRepetition = 1;                        // global setting for note repetition
byte currentRatcheting = 1;                        // global setting for note ratcheting
const uint8_t ratchetingLength = 75;                   // in percent: 75 means 75% of original note length for racheted notes
byte noteUpperLimit = 127;                         // global setting for highest allowed midi note
byte noteLowerLimit = 0;                           // global setting for lowest allowed midi note

// CCs
int16_t CCs[channelsOut][127][3];
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
int16_t chord[maxChords][chordMaxLength];              // array to store chords
byte chordNoteCount[maxChords];                    // number of added notes per chord
byte chordLength[maxChords];                       // size of chord that should be played
int16_t currentChord[chordMaxLength];                  // array to store current chord with transformations
byte currentChordLength;                           // length of current chord
byte currentChordNoteCount;                        // number of added notes in current chord
int16_t chordReverse[maxChords];                       // reverse transformation setting per chord
int16_t chordRotate[maxChords];                        // rotate transformation setting per chord
int16_t chordInversion[maxChords];                     // inversion transformation setting per chord
int16_t chordStrumming[maxChords];                     // strumming transformation setting per chord

// Scheduled notes
const byte maxNotesScheduled = 42;                 // maximum allowed notes for scheduling
struct SCHEDULED_NOTE {
  uint8_t number;
  long start;
  long duration;
  uint8_t velocity;
  uint8_t channel;
} scheduledNotes[maxNotesScheduled];
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
// const byte led1 = 3;                               // pin definition for led 1
// const byte led2 = 2;                               // pin definition for led 2
// unsigned long lastLEDMillis1 = 0;                  // last time LED 1 turned on
// unsigned long lastLEDMillis2 = 0;                  // last time LED 2 turned on
// const byte animationSpeed = 100;                   // start up animation speed



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


#include "i2c2midi_firmware_v4_0.cpp"
#include "midiCCs.cpp"
#include "midiChord.cpp"
#include "midiMisc.cpp"
#include "midiNotes.cpp"
#include "nrpns.cpp"
#include "ops.cpp"
