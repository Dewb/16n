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


void i2c2midi_runops(uint8_t* received, uint8_t i2cData[256]) {

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
}

void i2c2midi_readmidi()
{
  #ifdef MK2
    // USB MIDI
    if (midiDevice.read()) {  
    
      uint8_t type =       midiDevice.getType();
      uint8_t channel =    midiDevice.getChannel();
      uint8_t data1 =      midiDevice.getData1();
      uint8_t data2 =      midiDevice.getData2();
      //const uint8_t *sys = midiDevice.getSysExArray();            // not used at the moment

      #ifdef DEBUG
        Serial.print("USB received");
        Serial.print("Type: "); Serial.println(type);
        Serial.print("channel: "); Serial.println(channel);
        Serial.print("data1: "); Serial.println(data1);
        Serial.print("data2: "); Serial.println(data2);
      #endif

      if (channel < 1 || channel > 16) return;

      // NOTE ON (status 144-159)
      if (type >= 144 && type <= 159) {
        int noteNumber = data1;
        if (noteNumber < 0 || noteNumber > 127) return;
        int velocity = data2;
        if (velocity < 0) {                                         // velocity = 0 is treated as Note Off
          if (!latch) {
            removeFromNoteHistoryIn(channel-1, noteNumber);         
          }
        };
        if (velocity > 127) velocity = 127;
        addToNoteHistoryIn(channel-1, noteNumber, velocity);        // store the note number to the history
        lastNoteIn = noteNumber;                                    // store note number as last received
        lastVelocityIn = velocity;
      }

      // NOTE OFF (status 128-143)
      else if (type >= 128 && type <= 143) {
        int noteNumber = data1;
        if (noteNumber < 0 || noteNumber > 127) return;
        if (!latch) {
          removeFromNoteHistoryIn(channel-1, noteNumber);           // remove the note number from the history if latch = 0 
        }
        lastNoteOffIn = noteNumber;                                 // store note number as last received
      }
      
      // CC (status 176-191)
      else if (type >= 176 && type <= 191) {
        int controller = data1;
        if (controller < 0 || controller > 127) return;
        int value = data2;
        if (value < 0)   value = 0;
        if (value > 127) value = 127;
        CCsIn[channel-1][controller] = value;                       // store CC value 
        lastCIn = controller;                                       // store controller number as last received
        lastCCIn = value;                                           // store CC value as last received
      }
      
      lastChannelIn = channel;                                      // store the channel as last used channel

      //blinkLED(2);

    }
  #endif
}

void i2c2midi_update()
{
  checkNoteDurations();             // check if there are notes to turn off
  checkScheduledNotes();            // check scheduled notes
  updateRamps();                    // update all ramps for MIDI CC 
  checkLEDs();                      // check if the LEDs should be turned off
  
  #ifdef TEST
    TESTFunction();                 // function for testing
  #endif

}

