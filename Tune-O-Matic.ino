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

// Febr. 2024 
// Made some changes to the frequency table following comments by D. Haillant
// Added the option to choose a small b or a large B for the corresponding note.
// Added an option to use a LED to show clipping or signal strength.
// Got rid of some compiler warnings.

/*

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

*/

#include <LibPrintf.h>

const int COMMON_CATHODE = 1;
const int COMMON_ANODE   = 2;
//
// SET YOUR DISPLAY TYPE HERE
//
const int LED_DISPLAY_TYPE = COMMON_CATHODE;

// Uncomment the following #define if you want to check the 7-segment display
// 3 signalling LEDs and clipping LED continuously for testing purposes.
// Leave it commented out if you want to run the tuner.

//#define LED_TEST

// When connecting and LED to pin 13, clipping is shown as an on/off value
// When connected to pin 11, the amplitude of the signal is shown as a pwm-value.

#define PWM_SIGNAL_LED
#define DETECTION_THRESHOLD 15

#ifdef PWM_SIGNAL_LED
  int SIGNAL_LED = 11;
  #define AMP_FACTOR 2
  #define NR_OF_SAMPLES 20 // Nr samples for computing of mean sample value.
#else // LED shows clipping of input signal.
  int SIGNAL_LED = 13;
#endif

//
// Display a large character of a small character for B.
//
//#define LARGE_B

// DO NOT CHANGE ANYTHING BELOW THIS LINE
//-------------------------------------------------------------------------------------------

// LED OUTPUT PINS
int LED3 = 18;
int LED4 = 19;
int LED5 = 17;


// 9 segment display output pins;

int LEDE  = 2;
int LEDD  = 3;
int LEDC  = 4;
int LEDDP = 5;
int LEDB  = 9;
int LEDA  = 8;
int LEDF  = 7;
int LEDG  = 6;



// Data storage variables.
int newData = 0;
int prevData = 0;

// Freq variables.
unsigned int period;
int frequency;

#define HALF_SAMPLE_VALUE 127
#define TIMER_RATE 38462
#define TIMER_RATE_10 TIMER_RATE * 10

// Data storage variables.
unsigned int time = 0;   // Keeps time and sends values to store in timer[] occasionally.
#define BUFFER_SIZE 10
int timer[BUFFER_SIZE];  // Storage for timing of events.
int slope[BUFFER_SIZE];  // Storage for slope of events.
unsigned int totalTimer; // Used to calculate period.
byte index   = 0; // Current storage index.
int maxSlope = 0; // Used to calculate max slope as trigger point.
int newSlope;     // Storage for incoming slope data.

// Variables for deciding whether you have a match.
#define MAX_NO_MATCH_VALUE 9
int noMatch =   0; // Counts how many non-matches you've received to reset variables if it's been too long.
int slopeTol =  3; // Slope tolerance - adjust this if you need.
int timerTol = 10; // Timer tolerance - adjust this if you need.

// Variables for amp detection.
int ampTimer = 0;
int maxAmp = 0;
int checkMaxAmp;
long clippingTimer = 0;

// Clipping indicator variables.
boolean clipping = true;
#define CLIPPING_TIME 5 * TIMER_RATE // This should amount to 2 seconds.

ISR(ADC_vect) {       // When new ADC value ready.
  PORTB &= B11101111; // Set pin 12 low.
  prevData = newData; // Store previous value.
  newData = ADCH;     // Get value from A0.
  if (prevData < HALF_SAMPLE_VALUE && newData >= HALF_SAMPLE_VALUE){ // If increasing and crossing midpoint
    newSlope = newData - prevData; // Calculate slope
    if (abs(newSlope - maxSlope) < slopeTol){ // If slopes are ==
      // Record new data and reset time.
      slope[index] = newSlope;
      timer[index] = time;
      time = 0;
      if (index == 0){      // New max slope just reset.
        PORTB |= B00010000; // Set pin 12 high.
        noMatch = 0;
        index++;            // Increment index.
      }
      else {
        if (abs(timer[0] - timer[index]) < timerTol && abs(slope[0] - newSlope) < slopeTol){ //if timer duration and slopes match
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
    }
    else {
      if (newSlope > maxSlope){ // If new slope is much larger than max slope.
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
  }

  
  if (newData == 0 || newData == 128){ // If clipping 
    #ifndef PWM_SIGNAL_LED      
      PORTB |= B00100000; // Set pin 13 high, i.e. turn on clipping indicator led.
    #endif  
    clipping = true; // Currently clipping.   
  }
  

  time++; // Increment timer at rate of 38.5kHz
  clippingTimer++;
  if (clippingTimer > CLIPPING_TIME) {
    #ifndef PWM_SIGNAL_LED
      PORTB &= B11011111; // Set pin 13 low, i.e. turn off clipping indicator led.
    #endif
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
  index    = 0; // Reset index.
  noMatch  = 0; // Reset match counter.
  maxSlope = 0; // Reset slope.
}


void setLeds(String segments) {
  // bit# vs led counting from left to right [0...10]
  // ===========
  // 0 left led
  // 1 center led
  // 2 right led
  // segment display
  // ===============
  // 7    -
  // 8   | | 6
  // 9    -
  // 2   | | 4
  // 3/5  - .
  if (LED_DISPLAY_TYPE == COMMON_CATHODE) {
    // Decode led pattern and switch on/off the leds.
    digitalWrite(LED3,  segments[0] ==  '0' ? LOW : HIGH);
    digitalWrite(LED4,  segments[1] ==  '0' ? LOW : HIGH);
    digitalWrite(LED5,  segments[2] ==  '0' ? LOW : HIGH);
    digitalWrite(LEDE,  segments[3] ==  '0' ? LOW : HIGH);
    digitalWrite(LEDD,  segments[4] ==  '0' ? LOW : HIGH);
    digitalWrite(LEDC,  segments[5] ==  '0' ? LOW : HIGH);
    digitalWrite(LEDDP, segments[6] ==  '0' ? LOW : HIGH);
    digitalWrite(LEDB,  segments[7] ==  '0' ? LOW : HIGH);
    digitalWrite(LEDA,  segments[8] ==  '0' ? LOW : HIGH);
    digitalWrite(LEDF,  segments[9] ==  '0' ? LOW : HIGH);
    digitalWrite(LEDG,  segments[10] == '0' ? LOW : HIGH);
  } else {
    digitalWrite(LED3,  segments[0] ==  '1' ? LOW : HIGH);
    digitalWrite(LED4,  segments[1] ==  '1' ? LOW : HIGH);
    digitalWrite(LED5,  segments[2] ==  '1' ? LOW : HIGH);
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
   58, 162, 317, 325,  635,  651, 1270, 1303, 2541, 2606, 5083,  5213, 10167, 10428, // C$
  162, 163, 325, 328,  651,  656, 1303, 1312, 2606, 2625, 5213,  5250, 10428, 10501,
  163, 168, 328, 336,  656,  672, 1312, 1345, 2625, 2692, 5250,  5385, 10501, 10771,
  168, 172, 336, 344,  672,  689, 1345, 1380, 2692, 2761, 5385,  5523, 10770, 11046, // C#
  172, 173, 344, 347,  689,  694, 1380, 1389, 2761, 2780, 5523,  5562, 11046, 11124,
  173, 177, 347, 356,  694,  713, 1389, 1426, 2780, 2852, 5562,  5705, 11124, 11410,
  177, 182, 356, 365,  713,  731, 1426, 1462, 2852, 2925, 5705,  5852, 11410, 11704, // D
  182, 183, 365, 368,  731,  736, 1462, 1473, 2925, 2946, 5852,  5893, 11704, 11786,
  183, 188, 368, 376,  736,  754, 1473, 1510, 2946, 3022, 5893,  6044, 11786, 12088,
  188, 193, 376, 386,  754,  774, 1510, 1549, 3022, 3100, 6044,  6200, 12088, 12400, // D#
  193, 194, 386, 389,  774,  779, 1549, 1560, 3100, 3121, 6200,  6243, 12400, 12486,
  194, 200, 389, 400,  779,  800, 1560, 1601, 3121, 3202, 6243,  6404, 12486, 12808,
  200, 205, 400, 410,  800,  821, 1601, 1642, 3202, 3284, 6404,  6569, 12808, 13138, // E
  205, 206, 410, 413,  821,  826, 1642, 1653, 3284, 3307, 6569,  6614, 13138, 13228,
  206, 211, 413, 423,  826,  848, 1653, 1696, 3307, 3392, 6614,  6785, 13228, 13570,
  211, 217, 423, 434,  848,  869, 1696, 1739, 3392, 3479, 6785,  6959, 13570, 13918, // F#
  217, 218, 434, 437,  869,  876, 1739, 1752, 3479, 3504, 6959,  7008, 13918, 14016,
  218, 224, 437, 448,  876,  897, 1752, 1796, 3504, 3593, 7008,  7188, 14016, 14376,
  224, 230, 448, 460,  897,  920, 1796, 1842, 3593, 3686, 7188,  7373, 14376, 14746, // F#
  230, 231, 460, 463,  920,  927, 1842, 1855, 3686, 3711, 7373,  7424, 14746, 14848,
  231, 237, 463, 475,  927,  951, 1855, 1903, 3711, 3807, 7424,  7615, 14848, 15230,
  237, 243, 475, 487,  951,  975, 1903, 1952, 3807, 3905, 7615,  7811, 15230, 15622, // G#
  243, 244, 487, 490,  975,  982, 1952, 1965, 3905, 3932, 7811,  7866, 15622, 15732,
  244, 251, 490, 504,  982, 1008, 1965, 2016, 3932, 4034, 7866,  8069, 15732, 16138,
  251, 258, 504, 517, 1008, 1034, 2016, 2068, 4034, 4138, 8069,  8277, 16138, 16554, // G#
  258, 259, 517, 520, 1034, 1041, 2068, 2083, 4138, 4167, 8277,  8334, 16554, 16668,
  259, 267, 520, 534, 1041, 1068, 2083, 2137, 4167, 4274, 8334,  8549, 16668, 17098,
  267, 274, 534, 548, 1068, 1096, 2137, 2192, 4274, 4385, 8549,  8769, 17098, 17538, // A
  274, 275, 548, 551, 1096, 1103, 2192, 2207, 4385, 4415, 8769,  8830, 17538, 17660,
  275, 282, 551, 565, 1103, 1131, 2207, 2263, 4415, 4528, 8830,  9057, 17660, 18114,
  282, 289, 565, 579, 1131, 1160, 2263, 2321, 4528, 4644, 9057,  9290, 18114, 18580, // A#
  289, 292, 579, 584, 1160, 1169, 2321, 2338, 4644, 4677, 9290,  9355, 18580, 18710,
  292, 299, 584, 599, 1169, 1198, 2338, 2398, 4677, 4797, 9355,  9595, 18710, 19190,
  299, 306, 599, 614, 1198, 1229, 2398, 2460, 4797, 4921, 9595,  9842, 19190, 19684, // B
  306, 309, 614, 619, 1229, 1238, 2460, 2477, 4921, 4956, 9842,  9911, 19684, 19822,
  309, 317, 619, 635, 1238, 1270, 2477, 2541, 4956, 5083, 9911, 10167, 19822, 20334,
};

void testNote(int tableIndex, String pattern) {
  boolean condition = false;
  // Find an interval of frequencies which includes the measured frequency.
  for (int i = tableIndex * NR_COLUMNS; i < tableIndex * NR_COLUMNS + NR_COLUMNS; i += 2) {
    if ((frequencyTable[i] < frequency) && (frequency < frequencyTable[i + 1])) {
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
  printf("1\t");
  delay(delayTime);
  setLeds(F("01000000000"));
  delay(delayTime);
  setLeds(F("00100000000"));
  printf("2\t");
  delay(delayTime);
  setLeds(F("00010000000"));
  printf("3\t");
  delay(delayTime);
  setLeds(F("00001000000"));
  printf("4\t");
  delay(delayTime);
  setLeds(F("00000100000"));
  printf("5\t");
  delay(delayTime);
  setLeds(F("00000010000"));
  printf("6\t");
  delay(delayTime);
  setLeds(F("00000001000"));
  printf("7\t");
  delay(delayTime);
  setLeds(F("00000000100"));
  printf("8\t");
  delay(delayTime);
  setLeds(F("00000000010"));
  printf("9\t");
  delay(delayTime);
  setLeds(F("00000000001"));
  printf("10\n");
  delay(delayTime);
  // Clear all leds of 9 segment display.
  setLeds(F("00000000000"));
  delay(delayTime);

  // Test clipping Led
  #ifdef PWM_SIGNAL_LED
    for (int pwm = 0; pwm < 1024; pwm += 10) {
      analogWrite(SIGNAL_LED, pwm);
      delay(10);
    }
    for (int pwm = 1024; pwm > 0; pwm -= 10) {
      analogWrite(SIGNAL_LED, pwm);
      delay(10);
    }
  #else
    digitalWrite(SIGNAL_LED, HIGH);
    delay(delayTime);
    digitalWrite(SIGNAL_LED, LOW);
    delay(delayTime);
  #endif
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
  setLeds(F("00010000111")); // F
  delay(delayTime);
  setLeds(F("00000000000"));
  delay(delayTime);
  setLeds(F("00010010111")); // F#
  delay(delayTime);
  setLeds(F("00000000000"));
  delay(delayTime);
  setLeds(F("00001101111")); // G
  delay(delayTime);
  setLeds(F("00000000000"));
  delay(delayTime);
  setLeds(F("00001111111")); // G#
  delay(delayTime);
  setLeds(F("00000000000"));
  delay(delayTime);
  setLeds(F("00010101111")); // A
  delay(delayTime);
  setLeds(F("00000000000"));
  delay(delayTime);
  setLeds(F("00010111111")); // A#
  delay(delayTime);
  setLeds(F("00000000000"));
  delay(delayTime);
  #ifdef LARGE_B
    setLeds(F("00011101111")); // B
    delay(delayTime);
  #else  
    setLeds(F("00011100011")); // b
    delay(delayTime);
  #endif
  setLeds(F("00000000000"));
  delay(delayTime);  
}

void setup() {
  pinMode(LED3, OUTPUT);
  pinMode(LED4, OUTPUT);
  pinMode(LED5, OUTPUT);

  pinMode(LEDE, OUTPUT);
  pinMode(LEDD, OUTPUT);
  pinMode(LEDC, OUTPUT);
  pinMode(LEDDP, OUTPUT);
  pinMode(LEDB, OUTPUT);
  pinMode(LEDA, OUTPUT);
  pinMode(LEDF, OUTPUT);
  pinMode(LEDG, OUTPUT);
  pinMode(SIGNAL_LED, OUTPUT);

  Serial.begin(230400);

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
    static float mean = 100.0;
    static int dataBuffer[NR_OF_SAMPLES];
    static int index = 0;
    int sum;
  
    frequency = TIMER_RATE_10 / period; // Timer rate with an extra zero/period.
    if ((frequency > 0) && (frequency < 158)) {
      setLeds(F("00011101110"));
    }
  
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
  
    #ifdef LARGE_B   
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
    #else // small B
      /*
       *    
       *  |
       *    --
       *  |    |
       *    --  
       */  
      testNote(33, F("10011100011")); 
      testNote(34, F("01011100011")); // b
      testNote(35, F("00111100011"));
   #endif
    
  
    // TOO HIGH FOR NOW
    if ((frequency > 10180) && (frequency < 20334))
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
    // Signal when the input amplitude is approximately high enough for estimating the note value.
    #ifdef PWM_SIGNAL_LED
      dataBuffer[index] = newData;
      index++;
      if (index == NR_OF_SAMPLES) {
        index = 0;
      }
      sum = 0;
      for (int i = 0; i < NR_OF_SAMPLES; i++) {
        sum += dataBuffer[i];
      }
      mean = sum / NR_OF_SAMPLES;
      if (abs(newData - mean) > DETECTION_THRESHOLD) {
        analogWrite(SIGNAL_LED, AMP_FACTOR * (abs(newData - int(mean)) - DETECTION_THRESHOLD));
      } else {
        analogWrite(SIGNAL_LED, 0);
      }
    #endif
    delay(70);
  }
#endif
