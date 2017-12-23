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
//  Requires Bounce2 Library found here: https://github.com/thomasfredericks/Bounce2
//
//
//
// Compiled using Arduino 1.6.9, Tested with Arduino Uno R3 
//
// Code to test Arduino I2C Slave device to control an IR emitter
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
// Address 8 = Button repeats 1 ... 256. Simulates repeated pressing of the same button
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
//
//
// Maintenance History
//
// 25/06/17 : 4     Tidied up union and made device agnostic. ie. copes with different sizes of int, uint etc.
//
//
//

#include <Wire.h>
#include <Bounce2.h>


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

dataArrayRegisterAllocationUnionType uData1;
dataArrayRegisterAllocationUnionType uData2;
dataArrayRegisterAllocationUnionType uData3;



#define DEBUG_GENERAL         // Define this to get debug information out of the serial port
#define SLAVE_ADDR            9
#define ENCODING_TYPE_RC6     ((byte)0)
#define ENCODING_TYPE_SONY    ((byte)1)
#define ENCODING_TYPE_SAMSUNG ((byte)2)
#define ENCODING_TYPE_NEC     ((byte)3)
#define ENCODING_TYPE_LG      ((byte)4)
#define READY_FOR_DATA_PIN    4
#define INPUT_BUTTON1         7
#define INPUT_BUTTON2         6
#define OUTPUT_LED            8
#define IRTXBusy              (digitalRead(READY_FOR_DATA_PIN) == LOW)

#ifdef DEBUG_GENERAL
#define __FILENAME__ (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)
#endif


Bounce MyButton1;
Bounce MyButton2;

void setup() {
  #ifdef DEBUG_GENERAL
  Serial.begin(115200);           // start serial for output
  Serial.println(__FILENAME__);
  #endif
  Wire.begin();
  MyButton1.interval(50);
  MyButton1.attach(INPUT_BUTTON1);
  pinMode(INPUT_BUTTON1, INPUT);
  MyButton2.interval(50);
  MyButton2.attach(INPUT_BUTTON2);
  pinMode(INPUT_BUTTON2, INPUT);
  pinMode(READY_FOR_DATA_PIN, INPUT);
  Serial.println("Serial port Active");

  pinMode(OUTPUT_LED, OUTPUT);
  digitalWrite(OUTPUT_LED,LOW);

  // Sky - 1
  uData1.ra.bEncoding = ENCODING_TYPE_RC6;
  uData1.ra.ui32Data = (uint32_t) 0xC05C01;
  uData1.ra.bNumberOfBitsInTheData = 24;
  uData1.ra.bPulseTrainRepeats = 2;
  uData1.ra.bDelayBetweenPulseTrainRepeats = 124;
  uData1.ra.bButtonRepeats = 1;
  uData1.ra.ui16DelayBetweenButtonRepeats = (uint16_t) 400;
  uData1.ra.bFreshData = 0x00;

  // Sky - 0
  uData2.ra.bEncoding = ENCODING_TYPE_RC6;
  uData2.ra.ui32Data = (uint32_t) 0xC05C00;
  uData2.ra.bNumberOfBitsInTheData = 24;
  uData2.ra.bPulseTrainRepeats = 2;
  uData2.ra.bDelayBetweenPulseTrainRepeats = 124;
  uData2.ra.bButtonRepeats = 1;
  uData2.ra.ui16DelayBetweenButtonRepeats = (uint16_t) 400;
  uData2.ra.bFreshData = 0x00;

  // Sony AV - Mute 
  uData3.ra.bEncoding = ENCODING_TYPE_SONY;
  uData3.ra.ui32Data = (uint32_t) 0x140C;
  uData3.ra.bNumberOfBitsInTheData = 15;
  uData3.ra.bPulseTrainRepeats = 4;
  uData3.ra.bDelayBetweenPulseTrainRepeats = 22;
  uData3.ra.bButtonRepeats = 1;
  uData3.ra.ui16DelayBetweenButtonRepeats = (uint16_t) 0;
  uData3.ra.bFreshData = 0xFF;
}


void loop() {
  //while (IRTXBusy){};
  if (digitalRead(READY_FOR_DATA_PIN) == LOW)
    digitalWrite(OUTPUT_LED,HIGH);
  else
    digitalWrite(OUTPUT_LED,LOW);
  
  MyButton1.update();
  if ((MyButton1.read() == LOW) && (MyButton1.fell()) && (digitalRead(READY_FOR_DATA_PIN) == HIGH))
  {
    #ifdef DEBUG_GENERAL
    { 
      char cDataArray[20];
      for (int x = 0; x < MAX_BUFFER; x++)
      {
        sprintf(cDataArray,"0x%02X,",uData1.da.ucDataArray[x]);
        Serial.print(cDataArray);
      }
      Serial.println();
      for (int x = 0; x < MAX_BUFFER; x++)
      {
        sprintf(cDataArray,"%d,",uData1.da.ucDataArray[x]);
        Serial.print(cDataArray);
      }
      Serial.println();
    }
    #endif
    Wire.beginTransmission(SLAVE_ADDR); 
    uData1.ra.bFreshData = (byte)0x00;
    Wire.write(uData1.da.ucDataArray,MAX_BUFFER);
    Wire.endTransmission();
    Wire.beginTransmission(SLAVE_ADDR); 
    uData2.ra.bFreshData = (byte)0x00;
    Wire.write(uData2.da.ucDataArray,MAX_BUFFER);
    Wire.endTransmission();
    Wire.beginTransmission(SLAVE_ADDR); 
    uData1.ra.bFreshData = (byte)0xFF;  // <-- Triggers the send 
    Wire.write(uData1.da.ucDataArray,MAX_BUFFER);
    Wire.endTransmission();
  }

  MyButton2.update();
  if ((MyButton2.read() == LOW) && (MyButton2.fell()) && (digitalRead(READY_FOR_DATA_PIN) == HIGH))
  {
    #ifdef DEBUG_GENERAL
    { 
      char cDataArray[20];
      for (int x = 0; x < MAX_BUFFER; x++)
      {
        sprintf(cDataArray,"0x%02X,",uData3.da.ucDataArray[x]);
        Serial.print(cDataArray);
      }
      Serial.println();
      for (int x = 0; x < MAX_BUFFER; x++)
      {
        sprintf(cDataArray,"%d,",uData3.da.ucDataArray[x]);
        Serial.print(cDataArray);
      }
      Serial.println();
    }
    #endif
    Wire.beginTransmission(SLAVE_ADDR); 
    Wire.write(uData3.da.ucDataArray,MAX_BUFFER);
    Wire.endTransmission();
  }

}



