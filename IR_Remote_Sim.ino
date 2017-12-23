// Steve Quinn 11/06/17
//
// Copyright 2017 Steve Quinn
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//
//  Requires IRRemote Library found here: https://github.com/z3t0/Arduino-IRremote
//
//
// Compiled using Arduino 1.6.9, Tested with Arduino Uno R3 
//
// Turns Arduino into an I2C Slave device to control an IR emitter
//
// Register allocations
// --------------------
// 
// Address 0 = IR remote control encoding, RC6 (Sky) = 0, SONY = 1, SAMSUNG = 2, NEC = 3, LG = 4
//
// Address 1 ... 4 = Data bytes (unsigned long). 4 bytes in total LSByte ... MSByte
//
// Address 5  = Number of Bits in the data (Max of 32)
//
// Address 6 = How many repeats of this pulse train 1 ... 255 . Typically 3 repeats.
//
// Address 7 = Delay between repeats of this pulse train 1 ... 255mS.
//
// Address 8 = Button repeats 1 ... 256. Simulates repeated pressing of the same button (but doesn't support the modified code like an Apple remote, it just repeats the button code).
//
// Address 9 ... 10 = Delay between button repeats (unsigned int). 2 bytes in total LSByte ... MSByte. 1 ... 65535mS. 
//
// Address 11 = Fresh data. A non-zero value. Written last, triggers the IR TX sequence.
//
//
//             +-----+-----+-----+-----+-----+-----+-----+-----+
// Address 0   |  Encoding                                     |
//             +-----+-----+-----+-----+-----+-----+-----+-----+
// Address 1   |  Data Byte1 (LSByte)                          | 
//             +-----+-----+-----+-----+-----+-----+-----+-----+
// Address 2   |  Data Byte2                                   |
//             +-----+-----+-----+-----+-----+-----+-----+-----+
// Address 3   |  Date Byte3                                   |
//             +-----+-----+-----+-----+-----+-----+-----+-----+
// Address 4   |  Data Byte4 (MSByte)                          |
//             +-----+-----+-----+-----+-----+-----+-----+-----+
// Address 5   |  Number of Bits in the data                   |
//             +-----+-----+-----+-----+-----+-----+-----+-----+
// Address 6   |  Pulse Train Repeats                          |
//             +-----+-----+-----+-----+-----+-----+-----+-----+
// Address 7   |  Delay Between Pulse Train Repeats            |
//             +-----+-----+-----+-----+-----+-----+-----+-----+
// Address 8   |  Button Repeats                               |
//             +-----+-----+-----+-----+-----+-----+-----+-----+
// Address 9   |  Delay Between Button Repeats Byte1 (LSByte)  |
//             +-----+-----+-----+-----+-----+-----+-----+-----+
// Address 10  |  Delay Between Button Repeats Byte2 (MSByte)  |
//             +-----+-----+-----+-----+-----+-----+-----+-----+
// Address 11  |  Fresh Data                                   |
//             +-----+-----+-----+-----+-----+-----+-----+-----+
//
//
//
// Usage
// -----
// Data describing one remote key press is written to the IR Remote Control Simulator as a sequence of MAX_BUFFER bytes.
// A sequence of up to MAX_SEQUENCES can be sequentially written representing that many simulated key presses.
// The last key press must have 'bFreshData' set to non-zero. This will trigger the IR Remote control Simulator to start
// Sending the IR signal.
// If a key is to be pressed multiple times, then only one key need be sent with a value 'bButtonRepeats' set along with
// a delay in mS 'ui16DelayBetweenButtonRepeats' between repeat presses. Again 'bFreshData' must be set to non-zero.
// Whilst sending data the IR Remote Control Simulator asserts Digital Pin 4, IC Pin 6 to indicate it is busy.
//
//  ie. Writing
//  uData.ra.bEncoding = bEncoding;
//  uData.ra.ui32Data = (uint32_t) ui32Data;
//  uData.ra.bNumberOfBitsInTheData = bNumberOfBitsInTheData;
//  uData.ra.bPulseTrainRepeats = bPulseTrainRepeats;
//  uData.ra.bDelayBetweenPulseTrainRepeats = bDelayBetweenPulseTrainRepeats;
//  uData.ra.bButtonRepeats = bButtonRepeats;
//  uData.ra.ui16DelayBetweenButtonRepeats = (uint16_t) ui16DelayBetweenButtonRepeats;
//  uData.ra.bFreshData = (byte) 0xFF;
//  
//  Wire.beginTransmission(SLAVE_ADDR); 
//  Wire.write(uData.da.ucDataArray,MAX_BUFFER);
//  Wire.endTransmission();
//
//
//
// Maintenance History
//
// 25/06/17 : 4     Tidied up union and made device agnostic. ie. copes with different sizes of int, uint etc.
//
//
//


//#define DEBUG_GENERAL      // Undefine this for general debug information via the serial port. 


#include <Wire.h>
#include <IRremote.h>

IRsend irsend;

#define MAX_SEQUENCES 20

typedef struct __attribute((__packed__)) {
  byte bEncoding;
  uint32_t ui32Data;
  byte bNumberOfBitsInTheData;
  byte bPulseTrainRepeats;
  byte bDelayBetweenPulseTrainRepeats;
  byte bButtonRepeats;
  uint16_t ui16DelayBetweenButtonRepeats;
  byte bFreshData;
} registerAllocationType;

typedef struct __attribute((__packed__)) {  
  unsigned char ucDataArray[sizeof(registerAllocationType)];
} dataArrayType;  

typedef union {
  dataArrayType da;
  registerAllocationType ra;
} dataArrayRegisterAllocationUnionType;

#define MAX_BUFFER sizeof(registerAllocationType)

volatile dataArrayRegisterAllocationUnionType uDataArray[MAX_SEQUENCES];

typedef enum {
   eIDLE = 0,
   eBUSY = 1
} eSEND_STATE;

eSEND_STATE current_state = eIDLE;

#define DEBUG_GENERAL         // Define this to get debug information out of the serial port
#define SLAVE_ADDR            9
#define ENCODING_TYPE_RC6     ((byte)0)
#define ENCODING_TYPE_SONY    ((byte)1)
#define ENCODING_TYPE_SAMSUNG ((byte)2)
#define ENCODING_TYPE_NEC     ((byte)3)
#define ENCODING_TYPE_LG      ((byte)4)
#define READY_FOR_DATA_PIN    4

volatile unsigned int uiRegisterPointer  = 0;
volatile unsigned int uiDataArrayPointer = 0;
volatile boolean bFreshDataFlag          = false;

int iDataArrayIndex    = 0;
int iPulseTrainRepeats = 0;
int iButtonRepeats     = 0;

#ifdef DEBUG_GENERAL
#define __FILENAME__ (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)
#endif


void setup() {
  #ifdef DEBUG_GENERAL
  Serial.begin(115200);           // start serial for output
  Serial.println(__FILENAME__);
  Serial.println("Serial port Active");
  #endif
  Wire.begin(SLAVE_ADDR);         // join i2c bus with address SLAVE_ADDR
  Wire.onReceive(receiveEvent);   // register event

  pinMode(READY_FOR_DATA_PIN, OUTPUT);
  digitalWrite(READY_FOR_DATA_PIN,LOW);  // Digital pin 4 goes low when I2C IR Remote is busy

  current_state      = eIDLE;
  uiRegisterPointer  = 0;
  uiDataArrayPointer = 0;
  bFreshDataFlag     = false;
  iDataArrayIndex    = 0;

  for (int y = 0; y < MAX_SEQUENCES; y++)  // Clear receive buffer
    for (int x = 0; x < MAX_BUFFER; x++)  
      uDataArray[y].da.ucDataArray[x] = 0;

  digitalWrite(READY_FOR_DATA_PIN,HIGH);  // Digital pin 4 goes high when I2C IR Remote is ready for data
}


void loop() {
  if ((bFreshDataFlag) && (current_state == eIDLE))
  {
    digitalWrite(READY_FOR_DATA_PIN,LOW);  // IR Transmitter now busy
    current_state   = eBUSY;
    bFreshDataFlag  = false;
    iDataArrayIndex = 0;
  }

  if (current_state == eBUSY)
  {
    while (iDataArrayIndex < (uiDataArrayPointer)) {
      iButtonRepeats = 0;
      while (iButtonRepeats < uDataArray[iDataArrayIndex].ra.bButtonRepeats) {
        iPulseTrainRepeats = 0;
        while (iPulseTrainRepeats < uDataArray[iDataArrayIndex].ra.bPulseTrainRepeats) {
          switch (uDataArray[iDataArrayIndex].ra.bEncoding) {
            case ENCODING_TYPE_RC6 :  
                  irsend.sendRC6((unsigned long) uDataArray[iDataArrayIndex].ra.ui32Data, (int) uDataArray[iDataArrayIndex].ra.bNumberOfBitsInTheData);
                  break;
            case ENCODING_TYPE_SONY : 
                  irsend.sendSony((unsigned long) uDataArray[iDataArrayIndex].ra.ui32Data, (int) uDataArray[iDataArrayIndex].ra.bNumberOfBitsInTheData);
                  break;
            case ENCODING_TYPE_SAMSUNG : 
                  irsend.sendSAMSUNG((unsigned long) uDataArray[iDataArrayIndex].ra.ui32Data, (int) uDataArray[iDataArrayIndex].ra.bNumberOfBitsInTheData);
                  break;
            case ENCODING_TYPE_NEC  : 
                  irsend.sendNEC((unsigned long) uDataArray[iDataArrayIndex].ra.ui32Data, (int) uDataArray[iDataArrayIndex].ra.bNumberOfBitsInTheData);
                  break;
            case ENCODING_TYPE_LG  : 
                  irsend.sendLG((unsigned long) uDataArray[iDataArrayIndex].ra.ui32Data, (int) uDataArray[iDataArrayIndex].ra.bNumberOfBitsInTheData);
                  break;
          }
          delay((unsigned long) uDataArray[iDataArrayIndex].ra.bDelayBetweenPulseTrainRepeats);
          iPulseTrainRepeats++;
        }
        if (uDataArray[iDataArrayIndex].ra.ui16DelayBetweenButtonRepeats > 0)
          delay((unsigned long) uDataArray[iDataArrayIndex].ra.ui16DelayBetweenButtonRepeats);
        iButtonRepeats++;
      }
      iDataArrayIndex++;
    }
  }
  
  if ((iDataArrayIndex >= uiDataArrayPointer) && (current_state == eBUSY))
  {
    current_state = eIDLE;
    uiDataArrayPointer = 0;
    digitalWrite(READY_FOR_DATA_PIN,HIGH);  // IR Transmitter now idle
  }
}


// function that executes whenever data is received from master
// this function is registered as an event, see setup()
void receiveEvent(int howMany) {
  #ifdef DEBUG_GENERAL
  {
    Serial.println();
    Serial.println("receiveEvent");
    Serial.print("uiRegisterPointer : ");
    Serial.println(uiRegisterPointer);
    Serial.print("howMany : ");
    Serial.println(howMany);
  }
  #endif

  if ((howMany > 0) && (howMany == MAX_BUFFER)) {
    uiRegisterPointer  = 0;
    
    #ifdef DEBUG_GENERAL
      Serial.print("uiDataArrayPointer : ");
      Serial.println(uiDataArrayPointer);
    #endif
    
    for (int x = 0; x < howMany; x++)
    {
      uDataArray[uiDataArrayPointer].da.ucDataArray[uiRegisterPointer] = Wire.read();
      uiRegisterPointer++;
    }
    
    #ifdef DEBUG_GENERAL
    { 
      char cDataArray[20];
      for (int x = 0; x < MAX_BUFFER; x++)
      {
        sprintf(cDataArray,"0x%02X,",uDataArray[uiDataArrayPointer].da.ucDataArray[x]);
        Serial.print(cDataArray);
      }
      Serial.println();
      for (int x = 0; x < MAX_BUFFER; x++)
      {
        sprintf(cDataArray,"%d,",uDataArray[uiDataArrayPointer].da.ucDataArray[x]);
        Serial.print(cDataArray);
      }
      Serial.println();
    }
    #endif
    
    if (uDataArray[uiDataArrayPointer].ra.bFreshData != 0)
      bFreshDataFlag = true;
      
    if (uiDataArrayPointer < (MAX_SEQUENCES-1))
      uiDataArrayPointer++;    
  
  } else { // If the correct number of bytes were not received in one transaction, just clear the I2C RX buffer
    byte b;
    for (int x = 0; x < howMany-1; x++)
      b = Wire.read();
  }
  
  #ifdef DEBUG_GENERAL
    Serial.print("uiRegisterPointer : ");
    Serial.println(uiRegisterPointer);
    Serial.print("uiDataArrayPointer : ");
    Serial.println(uiDataArrayPointer);
  #endif
}


