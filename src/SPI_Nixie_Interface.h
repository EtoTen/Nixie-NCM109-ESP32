#ifndef SPI_Nixie_Interface_H
#define SPI_Nixie_Interface_H

#include <Wire.h>
#include <TimeLib.h>
#include <SPI.h>
#include "LEDControl.h"

// esp32 PIN Config

// SPI Bus VSPI
/*
this goes to a level shifter to shift 3v3 to 5v (a TXS0108E)
this is then connected to a CD4504BM with inline 1K resistors to the CD4504BM (u2) on the NCM109
to shift the signal again from 5v to 12v
and then it goes to the HV5122 1.0/1.1 revision board or HV5222 on the 1.2 revision board
which outputs high voltage (HV) to drive the Nixies
*/
#define ANTI_POISON_ITERATIONS 40

#define SPI_SS 5    // pin 16 on atmega328
#define SPI_MOSI 16 // pin 17 on atmega328
#define SPI_SCLK 18 // pin 19 on atmega328
#define SPI_MISO 19 // pin 18 on atmega328  -- not used

// LEDs under the nixies
#define LED_GREEN 26 // pin 12 on atmega328 | 6 on NCM109 sketch
#define LED_RED 12   // pin 15 on atmega328 | 9 on NCM109 sketch
#define LED_BLUE 27  // pin 05 on atmega328 | 3 on NCM109 sketch

// ***************************************************************
// Digit Data Definitions
// Each digit, except blank, has a single active low bit in a
// field of 10 bits
// ***************************************************************
#define DIGIT_0 0
#define DIGIT_1 1
#define DIGIT_2 2
#define DIGIT_3 3
#define DIGIT_4 4
#define DIGIT_5 5
#define DIGIT_6 6
#define DIGIT_7 7
#define DIGIT_8 8
#define DIGIT_9 9
// #define DIGIT_BLANK 0xFF
#define DIGIT_BLANK 10

#define UpperDotsMask 0x2000000
#define LowerDotsMask 0x1000000

#define AnodeGroup1ON 0x100000
#define AnodeGroup2ON 0x200000
#define AnodeGroup3ON 0x400000

#define TubeON 0xFFFFFFFF
#define TubeOFF 0xFC00

unsigned long anodesGroupMasks[3] = {AnodeGroup1ON, AnodeGroup2ON, AnodeGroup3ON};

// Class Definition
class SPINixieInterface : public LEDControl
{
  unsigned int SymbolArray[10] = {1, 2, 4, 8, 16, 32, 64, 128, 256, 512};

public:
  SPINixieInterface() : LEDControl(LED_RED, LED_GREEN, LED_BLUE)
  {
    // Wire.begin(I2C_SDA, I2C_SDC);
    //  pinMode(NEON_DOTS_LOWER,    OUTPUT);
    //  Initialize digit storage to all ones so all digits are off
    for (int i = 0; i < 6; i++)
    {
      digits[i] = DIGIT_BLANK;
    }
  }

  // Neon lamp control
  void dotsEnable(boolean state)
  {
    dotsEnabled = state;
  }
  // Reverse dot state
  void switchdDots()
  {
    dotsEnabled = !dotsEnabled;
  }

  // Set the NX1 (most significant) digit
  void setNX1Digit(int d)
  {
    digits[0] = NUMERIC_DIGITS[d];
  }

  // Set the NX2 digit
  void setNX2Digit(int d)
  {
    digits[1] = NUMERIC_DIGITS[d];
  }

  // Set the NX3 digit
  void setNX3Digit(int d)
  {
    digits[2] = NUMERIC_DIGITS[d];
  }

  // Set the NX4 digit
  void setNX4Digit(int d)
  {
    digits[3] = NUMERIC_DIGITS[d];
  }

  // Set the NX5 digit
  void setNX5Digit(int d)
  {
    digits[4] = NUMERIC_DIGITS[d];
  }

  // Set the NX6 (least significant) digit
  void setNX6Digit(int d)
  {
    digits[5] = NUMERIC_DIGITS[d];
  }
  /*
   2022/2/14
   condenced old NCT412 gbh code into a single function itiration, in order to simplify and fix functionality
   this is still crappy because I have to send 3 seperate SPI messages for the 3 Nixie groups
  */
  void show()
  {

    //do either lower/upper dots every show()
    static bool upper_or_lower_dots = false;
    if (upper_or_lower_dots)
      upper_or_lower_dots = false;
    else
      upper_or_lower_dots = true;

    // for the NCT412 there are 3 anode groups
    for (int anodeGroup = 1; anodeGroup <= 3; anodeGroup++)
    {

      int position = (anodeGroup * 2 - 2);
      // 0 2 4
      unsigned long var32 = 0;

      // |= is bit-wise OR operator
      var32 |= anodesGroupMasks[anodeGroup - 1];                         // apply the proper mask to turn on the anode (array is indexed at start 0, but we are starting at 1)
      var32 |= (unsigned long)(SymbolArray[digits[position]]);           // digit 1
      var32 |= (unsigned long)(SymbolArray[digits[position + 1]]) << 10; // digit 2 //bitwise shift operators << is shift left
                                                                         // var32 &=TubeON;

      // apply dot mask
      // the lower and upper masks can not be applied at the same time, if that happens then it messes up the other digits.
      // also apply it durring the first two anode group, otherwise there is slight flickering, if applied to all 3 then they are slightly brighter
      if (anodeGroup <=2 )
      {
        if (dotsEnabled)
        {
          if (upper_or_lower_dots)
            var32 |= LowerDotsMask;
          else
            var32 |= UpperDotsMask;
        }
        else
        {
          if (upper_or_lower_dots)
            var32 &= ~LowerDotsMask;
          else
            var32 &= ~UpperDotsMask;
        }
      }

      // SPI start for HV5222 (HV5122 is upside down in the messaging)
      unsigned long start = millis();
      /// a of 3 miliseconds delay  is needed before the 3 anode groups
      while (millis() - start < 3)
        ;
      // delay(3); //delay() is evil!!!!

      digitalWrite(SPI_SS, LOW);  // latching data
      SPI.transfer(var32);        //[LC7][LC6][LC5][LC4][LC3][LC2][LC1][LC0]  - LC9-LC0 - Left tubes cathodes
      SPI.transfer(var32 >> 8);   //[RC5][RC4][RC3][RC2][RC1][RC0][LC9][LC8]  - RC9-RC0 - Right tubes cathodes
      SPI.transfer(var32 >> 16);  //[x][A2][A1][A0][RC9][RC8][RC7][RC6]       - A0-A2 - anodes
      SPI.transfer(var32 >> 24);  //[x][x][x][x][x][x][L1][L0]                - L0, L1 - dots
      digitalWrite(SPI_SS, HIGH); // latching data

      // debug
      /*
      Serial.print("Anode: ");
      Serial.println(anodeGroup);
      Serial.print("Position: ");
      Serial.println(position);
      Serial.print("Byte data var32: ");
      Serial.println(var32);
      */
    }
  }

private:
  // Private data
  // Array of codes for each nixie digit

  int NUMERIC_DIGITS[11] = {
      DIGIT_0, DIGIT_1, DIGIT_2, DIGIT_3, DIGIT_4,
      DIGIT_5, DIGIT_6, DIGIT_7, DIGIT_8, DIGIT_9,
      DIGIT_BLANK};

  // int NUMERIC_DIGITS[11] = { DIGIT_BLANK, DIGIT_9,  DIGIT_8, DIGIT_7, DIGIT_6, DIGIT_5,  DIGIT_4, DIGIT_3, DIGIT_2, DIGIT_1, DIGIT_0};

  // Storage for codes for each nixie digit
  // Digit order is: 1,2,3,4,5,6
  uint16_t digits[6];

  bool dotsEnabled = false;
};

#endif
