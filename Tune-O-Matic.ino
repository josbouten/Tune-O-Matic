// This code is made using the very good sine wave freq detection by Amanda Ghassaei july 2012
// https://www.instructables.com/member/amandaghassaei/

// Changelog
// Code origin Sam / LookMumNoComputer and Amandaghassaei
// 9 Jan 2020: Jos Bouten aka Zaphod B: 
// - put frequencies in a table and simplified controlling the led display
// - put strings in flash memory to use less program memory space.

// 18 Jan 2020
// Added test of clipping led.

// 29 Febr 2020
// Added a switch mechanism to the code to make it easy to use either a common anode or common
// cathode LED-display.
// Set the const LED_DISPLAY_TYPE to the type of LED display you use (COMMON_ANODE or COMMON_CATHODE).

/*
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

*/

const int COMMON_CATHODE = 1;
const int COMMON_ANODE   = 2;
//
// SET YOUR DISPLAY TYPE HERE
//
const int LED_DISPLAY_TYPE = COMMON_ANODE;

// Uncomment the following #define if you want to check the 7-segment display
// 3 signalling LEDs and clipping LED continuously for testing purposes.
// Leave it commented out if you want to run the tuner.

//#define LED_TEST

// DO NOT CHANGE ANYTHING BELOW THIS LINE
//-------------------------------------------------------------------------------------------
// LED OUTPUT PINS
int LED3 = 18;  // A4 (low)
int LED4 = 19;  // A5 (ok)
int LED5 = 17;  // A3 (high)

// 9 segment display output pins;
int LEDE = 2;
int LEDD = 3;
int LEDC = 4;
int LEDDP = 5;
int LEDB = 9;
int LEDA = 8;
int LEDF = 7;
int LEDG = 6;

int CLIPPING_LED = 13;

// Data storage variables.
byte newData = 0;
byte prevData = 0;

// Freq variables.
unsigned int period;
int frequency;

#define HALF_SAMPLE_VALUE 127
#define TIMER_RATE 38462
#define TIMER_RATE_10 TIMER_RATE * 10

// Data storage variables.
unsigned int time = 0;   // Keeps time and sends values to store in timer[] occasionally.
#define BUFFER_SIZE 10
int timer[BUFFER_SIZE];  // Sstorage for timing of events.
int slope[BUFFER_SIZE];  // Storage for slope of events.
unsigned int totalTimer; // Used to calculate period.
byte index = 0;   // Current storage index.
int maxSlope = 0; // Used to calculate max slope as trigger point.
int newSlope;     // Storage for incoming slope data.

// Variables for decided whether you have a match.
#define MAX_NO_MATCH_VALUE 9
byte noMatch = 0;  // Counts how many non-matches you've received to reset variables if it's been too long.
byte slopeTol = 3; // Slope tolerance - adjust this if you need.
int timerTol = 10; // Timer tolerance - adjust this if you need.

// Variables for amp detection.
unsigned int ampTimer = 0;
byte maxAmp = 0;
byte checkMaxAmp;
byte ampThreshold = 30; // Raise if you have a very noisy signal.
long clippingTimer = 0;

// Clipping indicator variables.
boolean clipping = true;
#define CLIPPING_TIME 5 * TIMER_RATE // This should amount to 2 seconds.

ISR(ADC_vect) {       // When new ADC value ready.
  PORTB &= B11101111; // Set pin 12 low.
  prevData = newData; // Store previous value.
  newData = ADCH;     // Get value from A0.
  if (prevData < HALF_SAMPLE_VALUE && newData >= HALF_SAMPLE_VALUE){ // if increasing and crossing midpoint
    newSlope = newData - prevData; // Calculate slope
    if (abs(newSlope - maxSlope) < slopeTol){ // If slopes are ==
      // Record new data and reset time.
      slope[index] = newSlope;
      timer[index] = time;
      time = 0;
      if (index == 0){ // New max slope just reset.
        PORTB |= B00010000; // Set pin 12 high.
        noMatch = 0;
        index++; // Increment index.
      }
      else if (abs(timer[0] - timer[index]) < timerTol && abs(slope[0] - newSlope) < slopeTol){ //if timer duration and slopes match
        // Sum timer values.
        totalTimer = 0;
        for (byte i = 0; i < index; i++){
          totalTimer += timer[i];
        }
        period = totalTimer; // Set period.
        // Reset new zero index values to compare with.
        timer[0] = timer[index];
        slope[0] = slope[index];
        index = 1; // Set index to 1.
        PORTB |= B00010000; // Set pin 12 high.
        noMatch = 0;
      } else { // Crossing midpoint but not match.
        index++; // Increment index.
        if (index > BUFFER_SIZE - 1){
          reset();
        }
      }
    }
    else if (newSlope > maxSlope){ // If new slope is much larger than max slope.
      maxSlope = newSlope;
      time = 0; // Reset clock.
      noMatch = 0;
      index = 0; // Reset index.
    }
    else { // Slope not steep enough.
      noMatch++; // Increment no match counter.
      if (noMatch > MAX_NO_MATCH_VALUE){
        reset();
      }
    }
  }
    
  if (newData == 0 || newData == 1023){ // If clipping 
    PORTB |= B00100000; // set pin 13 high, i.e. turn on clipping indicator led.
    clipping = true; // Currently clipping.
  }
  
  time++; // Increment timer at rate of 38.5kHz
  clippingTimer++;
  if (clippingTimer > CLIPPING_TIME) {
    PORTB &= B11011111; // Set pin 13 low, i.e. turn off clipping indicator led.
    clipping = false;   // Currently not clipping.
    clippingTimer = 0;
  }
  
  ampTimer++; // Increment amplitude timer.
  if (abs(HALF_SAMPLE_VALUE - ADCH) > maxAmp){
    maxAmp = abs(HALF_SAMPLE_VALUE-ADCH);
  }
  if (ampTimer == 1000){
    ampTimer = 0;
    checkMaxAmp = maxAmp;
    maxAmp = 0;
  }
  
}

void reset(){   // Clear out some variables.
  index = 0;    // Reset index.
  noMatch = 0;  // Reset match counter.
  maxSlope = 0; // Reset slope.
}


void setLeds(String segments) {
  if (LED_DISPLAY_TYPE == COMMON_CATHODE) {
    // Decode led pattern and switch on/off the leds.
    digitalWrite(LED3,  segments[0] ==  '0' ? LOW : HIGH);  // low
    digitalWrite(LED4,  segments[1] ==  '0' ? LOW : HIGH);  // ok
    digitalWrite(LED5,  segments[2] ==  '0' ? LOW : HIGH);  // high
    digitalWrite(LEDE,  segments[3] ==  '0' ? LOW : HIGH);
    digitalWrite(LEDD,  segments[4] ==  '0' ? LOW : HIGH);
    digitalWrite(LEDC,  segments[5] ==  '0' ? LOW : HIGH);
    digitalWrite(LEDDP, segments[6] ==  '0' ? LOW : HIGH);
    digitalWrite(LEDB,  segments[7] ==  '0' ? LOW : HIGH);
    digitalWrite(LEDA,  segments[8] ==  '0' ? LOW : HIGH);
    digitalWrite(LEDF,  segments[9] ==  '0' ? LOW : HIGH);
    digitalWrite(LEDG,  segments[10] == '0' ? LOW : HIGH);
  } else {
    digitalWrite(LED3,  segments[0] ==  '1' ? LOW : HIGH);  // low
    digitalWrite(LED4,  segments[1] ==  '1' ? LOW : HIGH);  // ok
    digitalWrite(LED5,  segments[2] ==  '1' ? LOW : HIGH);  // high
    digitalWrite(LEDE,  segments[3] ==  '1' ? LOW : HIGH);
    digitalWrite(LEDD,  segments[4] ==  '1' ? LOW : HIGH);
    digitalWrite(LEDC,  segments[5] ==  '1' ? LOW : HIGH);
    digitalWrite(LEDDP, segments[6] ==  '1' ? LOW : HIGH);
    digitalWrite(LEDB,  segments[7] ==  '1' ? LOW : HIGH);
    digitalWrite(LEDA,  segments[8] ==  '1' ? LOW : HIGH);
    digitalWrite(LEDF,  segments[9] ==  '1' ? LOW : HIGH);
    digitalWrite(LEDG,  segments[10] == '1' ? LOW : HIGH);
  }
}

#define NR_COLUMNS 14

const int frequencyTable[] = {
  // Note, this table contains frequency values multiplied by 10 
  // so that one decimal value is included.
  // A4 which in an equal tempered scale is at 440 Hz, is represented by 4400 in the table.

  // A4 = 440 Hz
  // Allowed range = 6.0 cents

  158,  162,  317,  325,  635,  651,  1270, 1303, 2541, 2607, 5083, 5214, 10167,  10428,  // C
  162,  164,  325,  328,  651,  656,  1303, 1312, 2607, 2625, 5214, 5250, 10428,  10501,  
  164,  168,  328,  336,  656,  673,  1312, 1346, 2625, 2693, 5250, 5385, 10501,  10771,  
  168,  172,  336,  345,  673,  690,  1346, 1381, 2692, 2762, 5385, 5524, 0,  0,  // C#
  172,  173,  345,  347,  690,  695,  1381, 1390, 2762, 2781, 5524, 5562, 0,  0,  
  173,  178,  347,  356,  695,  713,  1390, 1426, 2781, 2853, 5562, 5706, 0,  0,  
  178,  182,  356,  365,  713,  731,  1426, 1463, 2853, 2926, 5706, 5853, 0,  0,  // D
  182,  184,  365,  368,  731,  736,  1463, 1473, 2926, 2946, 5853, 5893, 0,  0,  
  184,  188,  368,  377,  736,  755,  1473, 1511, 2946, 3022, 5893, 6045, 0,  0,  
  189,  193,  377,  387,  755,  775,  1511, 1550, 3022, 3100, 6045, 6201, 0,  0,  // D#
  193,  195,  387,  390,  775,  780,  1550, 1561, 3100, 3122, 6201, 6244, 0,  0,  
  195,  200,  390,  400,  780,  800,  1561, 1601, 3122, 3202, 6244, 6404, 0,  0,  
  200,  205,  400,  410,  800,  821,  1601, 1642, 3202, 3284, 6404, 6569, 0,  0,  // E
  205,  206,  410,  413,  821,  827,  1642, 1653, 3284, 3307, 6569, 6615, 0,  0,  
  206,  212,  413,  424,  827,  848,  1653, 1696, 3307, 3392, 6615, 6785, 0,  0,  
  212,  217,  424,  435,  848,  870,  1696, 1740, 3392, 3480, 6785, 6960, 0,  0,  // F
  217,  219,  435,  438,  870,  876,  1740, 1752, 3480, 3504, 6960, 7008, 0,  0,  
  219,  224,  438,  449,  876,  898,  1752, 1797, 3504, 3594, 7008, 7189, 0,  0,  
  224,  230,  449,  460,  898,  921,  1797, 1843, 3594, 3687, 7189, 7374, 0,  0,  // F#
  230,  232,  460,  464,  921,  928,  1843, 1856, 3687, 3712, 7374, 7425, 0,  0,  
  232,  238,  464,  476,  928,  952,  1856, 1904, 3712, 3808, 7425, 7616, 0,  0,  
  238,  244,  476,  488,  952,  976,  1904, 1953, 3808, 3906, 7616, 7812, 0,  0,  // G
  244,  245,  488,  491,  976,  983,  1953, 1966, 3906, 3933, 7812, 7867, 0,  0,  
  245,  252,  491,  504,  983,  1008, 1966, 2017, 3933, 4034, 7867, 8069, 0,  0,  
  252,  258,  504,  517,  1008, 1034, 2017, 2069, 4034, 4138, 8069, 8277, 0,  0,  // G#
  258,  260,  517,  520,  1034, 1041, 2069, 2083, 4138, 4167, 8277, 8334, 0,  0,  
  260,  267,  520,  534,  1041, 1068, 2083, 2137, 4167, 4274, 8334, 8549, 0,  0,  
  267,  274,  534,  548,  1068, 1096, 2137, 2192, 4274, 4384, 8549, 8769, 0,  0,  // A
  274,  276,  548,  551,  1096, 1103, 2192, 2207, 4384, 4415, 8769, 8830, 0,  0,  
  276,  283,  551,  566,  1103, 1132, 2207, 2264, 4415, 4528, 8830, 9057, 0,  0,  
  283,  290,  566,  580,  1132, 1161, 2264, 2322, 4528, 4645, 9057, 9291, 0,  0,  // A#
  290,  292,  580,  584,  1161, 1169, 2322, 2338, 4645, 4677, 9291, 9355, 0,  0,  
  292,  299,  584,  599,  1169, 1199, 2338, 2399, 4677, 4798, 9355, 9596, 0,  0,  
  299,  307,  599,  615,  1199, 1230, 2399, 2460, 4798, 4921, 9596, 9843, 0,  0,  // B
  307,  309,  615,  619,  1230, 1239, 2460, 2478, 4921, 4955, 9843, 9912, 0,  0,  
  309,  317,  619,  635,  1239, 1270, 2478, 2541, 4955, 5083, 9912, 10167,  0,  0 
};

void testNote(int tableIndex, String pattern) {
  boolean condition = false;
  // Find an interval of frequencies which includes the measured frequency.
  for (int i = tableIndex * NR_COLUMNS; i < tableIndex * NR_COLUMNS + NR_COLUMNS; i += 2) {
    if ((frequencyTable[i] > 0) && (frequencyTable[i] < frequency) && (frequency < frequencyTable[i + 1])) {
      condition = true;
      break; // Exit the loop as soon as there is a match.
    }
  }
  // If any of the conditions turns out to be true,
  // switch on the corresponding LEDs.
  if (condition == true) setLeds(pattern);
}

void testLedsIndividually(int delayTime) {
  setLeds(F("10000000000"));
  Serial.println(F("1"));
  delay(delayTime);
  setLeds(F("01000000000"));
  delay(delayTime);
  setLeds(F("00100000000"));
  Serial.println(F("2"));
  delay(delayTime);
  setLeds(F("00010000000"));
  Serial.println(F("3"));
  delay(delayTime);
  setLeds(F("00001000000"));
  Serial.println(F("4"));
  delay(delayTime);
  setLeds(F("00000100000"));
  Serial.println(F("5"));
  delay(delayTime);
  setLeds(F("00000010000"));
  Serial.println(F("6"));
  delay(delayTime);
  setLeds(F("00000001000"));
  Serial.println(F("7"));
  delay(delayTime);
  setLeds(F("00000000100"));
  Serial.println(F("8"));
  delay(delayTime);
  setLeds(F("00000000010"));
  Serial.println(F("9"));
  delay(delayTime);
  setLeds(F("00000000001"));
  Serial.println(F("10"));
  delay(delayTime);
  // Clear all leds of 9 segment display.
  setLeds(F("00000000000"));
  delay(delayTime);
  
  // Test clipping Led
  digitalWrite(CLIPPING_LED, HIGH);
  delay(delayTime);
  digitalWrite(CLIPPING_LED, LOW);
  delay(delayTime);
}

void testMusicalChars(int delayTime) {
  setLeds(F("00011000110")); // C
  delay(delayTime);
  setLeds(F("00000000000"));
  delay(delayTime);
  setLeds(F("00011010110")); // C#
  delay(delayTime);
  setLeds(F("00000000000"));
  delay(delayTime);
  setLeds(F("00011101001")); // D
  delay(delayTime);
  setLeds(F("00000000000"));
  delay(delayTime);
  setLeds(F("00011111001")); // D#
  delay(delayTime);
  setLeds(F("00000000000"));
  delay(delayTime);
  setLeds(F("00011000111")); // E
  delay(delayTime);
  setLeds(F("00000000000"));
  delay(delayTime);
  setLeds(F("00010000111")); // F0
  delay(delayTime);
  setLeds(F("00000000000"));
  delay(delayTime);
  setLeds(F("00010010111")); // F0#
  delay(delayTime);
  setLeds(F("00000000000"));
  delay(delayTime);
  setLeds(F("00001101111")); // G0
  delay(delayTime);
  setLeds(F("00000000000"));
  delay(delayTime);
  setLeds(F("00001111111")); // G0#
  delay(delayTime);
  setLeds(F("00000000000"));
  delay(delayTime);
  setLeds(F("00010101111")); // A0
  delay(delayTime);
  setLeds(F("00000000000"));
  delay(delayTime);
  setLeds(F("00010111111")); // A0#
  delay(delayTime);
  setLeds(F("00000000000"));
  delay(delayTime);
  setLeds(F("00011101111")); // B0   
  delay(delayTime);
  setLeds(F("00000000000"));
  delay(delayTime);  
}

void setup() {
  pinMode(LED3, OUTPUT);  // low
  pinMode(LED4, OUTPUT);  // ok
  pinMode(LED5, OUTPUT);  // high

  pinMode(LEDE, OUTPUT);
  pinMode(LEDD, OUTPUT);
  pinMode(LEDC, OUTPUT);
  pinMode(LEDDP, OUTPUT);
  pinMode(LEDB, OUTPUT);
  pinMode(LEDA, OUTPUT);
  pinMode(LEDF, OUTPUT);
  pinMode(LEDG, OUTPUT);

  Serial.begin(9600);

  cli(); // Disable interrupts.

  // Set up continuous sampling of analog pin 0.

  // Clear ADCSRA and ADCSRB registers.
  ADCSRA = 0;
  ADCSRB = 0;

  ADMUX |= (1 << REFS0); // Set reference voltage.
  ADMUX |= (1 << ADLAR); // Left align the ADC valu e- so we can read highest 8 bits from ADCH register only

  ADCSRA |= (1 << ADPS2) | (1 << ADPS0); // Set ADC clock with 32 prescaler -> 16mHz / 32 = 500kHz.
  ADCSRA |= (1 << ADATE); // Enable auto trigger.
  ADCSRA |= (1 << ADIE);  // Enable interrupts when measurement complete.
  ADCSRA |= (1 << ADEN);  // Enable ADC.
  ADCSRA |= (1 << ADSC);  // Start ADC measurements.

  sei(); // Enable interrupts.
  // Run a test of all leds.
#ifndef LED_TEST
  testLedsIndividually(50);
  testMusicalChars(50);
  testLedsIndividually(50);
  testMusicalChars(50);
#endif
}

#ifdef LED_TEST
void loop() {  
  testLedsIndividually(500);
  testMusicalChars(500);
}
#else
void loop() {

  frequency = TIMER_RATE_10 / period; // Timer rate with an extra zero/period.


/*
 * Seven-Segment Display: segment names
 *    A
 *  F   B
 *    G
 *  E   C
 *    D   dp
 */

  if ((frequency > 0) && (frequency < 158)) {
    /*
     *    --
     *  |    |
     *     
     *  |    |
     *    --
     */
    //         lohEDC.BAFG
    setLeds(F("00011101110"));
  }

  // C
  /*
   *    --
   *  |  
   *     
   *  | 
   *    --
   */
  //              lohEDC.BAFG
  testNote( 0, F("10011000110"));
  testNote( 1, F("01011000110"));
  testNote( 2, F("00111000110"));

  // C#
  /*
   *    --
   *  |  
   *     
   *  | 
   *    --   *
   */
  //              lohEDC.BAFG
  testNote( 3, F("10011010110"));
  testNote( 4, F("01011010110"));
  testNote( 5, F("00111010110"));

  // D
  /*
   *    
   *       |
   *    --
   *  |    |
   *    --
   */
  //              lohEDC.BAFG
  testNote( 6, F("10011101001"));
  testNote( 7, F("01011101001"));
  testNote( 8, F("00111101001"));

  // D#
  /*
   *    
   *       |
   *    --
   *  |    |
   *    --   *
   */
  //              lohEDC.BAFG
  testNote( 9, F("10011111001"));
  testNote(10, F("01011111001"));
  testNote(11, F("00111111001"));

  // E
  /*
   *    --
   *  |     
   *    --
   *  |     
   *    --   
   */
  //              lohEDC.BAFG
  testNote(12, F("10011000111"));
  testNote(13, F("01011000111"));
  testNote(14, F("00111000111"));

  // F
  /*
   *    --
   *  |     
   *    --
   *  |     
   *         
   */
  //              lohEDC.BAFG
  testNote(15, F("10010000111"));
  testNote(16, F("01010000111"));
  testNote(17, F("00110000111"));

  // F#
  /*
   *    --
   *  |     
   *    --
   *  |     
   *         *
   */
  //              lohEDC.BAFG
  testNote(18, F("10010010111"));
  testNote(19, F("01010010111"));
  testNote(20, F("00110010111"));

  // G
  /*
   *    --
   *  |    |
   *    --
   *       |
   *    --   
   */
  //              lohEDC.BAFG
  testNote(21, F("10001101111"));
  testNote(22, F("01001101111"));
  testNote(23, F("00101101111"));

  // G#
  /*
   *    --
   *  |    |
   *    --
   *       |
   *    --   *
   */
  //              lohEDC.BAFG
  testNote(24, F("10001111111"));
  testNote(25, F("01001111111"));
  testNote(26, F("00101111111"));

  // A
  /*
   *    --
   *  |    |
   *    --
   *  |    |
   *         
   */
  //              lohEDC.BAFG
  testNote(27, F("10010101111"));
  testNote(28, F("01010101111"));
  testNote(29, F("00110101111"));

  // A#
  /*
   *    --
   *  |    |
   *    --
   *  |    |
   *         *
   */
  //              lohEDC.BAFG
  testNote(30, F("10010111111"));
  testNote(31, F("01010111111"));
  testNote(32, F("00110111111"));

  // B
  /*
   *    --
   *  |    |
   *    --
   *  |    |
   *    --  
   */
  //              lohEDC.BAFG
  testNote(33, F("10011101111"));
  testNote(34, F("01011101111"));
  testNote(35, F("00111101111"));

  // TOO HIGH FOR NOW

  if ((frequency > 10180) && (frequency < 100000))
  {
    /*
     *      
     *        
     *      
     *        
     *         *
     */
    //         lohEDC.BAFG
    setLeds(F("00000010000"));
  }

  delay(70);
//  Serial.print(frequency / 10);
//  Serial.println(F("Hz"));
}
#endif
