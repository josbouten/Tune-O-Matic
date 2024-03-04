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

// Feb. Mar. 2024
// - new frequencies (calculated from script, tuned for A4=440Hz, with 6 cents tolerance)
// - modification of display chars (reduced memory usage and simplification)
// - added exponential weighted moving average calculation for the period, and increased precision

/*
 * This code reads an analog signal, finds the rising edge, measure the period between each rising edge,
 * compute the frequency, compares that frequency to the table of in-tune frequencies and finally displays
 * the corresponding note and if it's in tune or not.
 * 
 * Compatible hardware:
 * - https://www.lookmumnocomputer.com/projects#/1222-performance-vco
 * - https://github.com/MyModularJourney/Tuna
 * - https://www.davidhaillant.com/category/electronic-projects/utility-modules/tuner/
 * 
 * 
 * input DC biased (+2.5V) signal on pin A0
 * 
 * output one 7-segment display on pins D2 to D9 (see below)
 * output 3 LEDs on pins A3, A4 and A5 for
 */

/*
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

*/

//#define DEBUG

#define COMMON_CATHODE 1
#define COMMON_ANODE   2
//
// SET YOUR DISPLAY TYPE HERE
//
#define LED_DISPLAY_TYPE COMMON_ANODE


// DO NOT CHANGE ANYTHING BELOW THIS LINE
//-------------------------------------------------------------------------------------------
// "target" LED output pins. Active HIGH (as Common Cathode)
#define LED_FLAT_PIN    18    // A4 (flat)
#define LED_INTUNE_PIN  19    // A5 (in-tune)
#define LED_SHARP_PIN   17    // A3 (sharp)

// 9 segment display output pins;
#define LEDE  2
#define LEDD  3
#define LEDC  4
#define LEDDP 5
#define LEDB  9
#define LEDA  8
#define LEDF  7
#define LEDG  6

// #define CLIPPING_LED 13 // not used?

// Data storage variables.
byte newData  = 0;
byte prevData = 0;

// Freq variables.
uint16_t period = 0;
uint32_t averaged_period = 0;

uint16_t frequency = 0;

#define HALF_SAMPLE_VALUE 127

/* 
 * TIMER_RATE is ADC clock (set by prescaler) divided by number of cycles for each conversion: 
 * 500kHz / 13 = 38461,538461538
 * 
 * As we only want integers, but with better precision, TIMER_RATE is int((500kHz / 13) x 10)
 * To correct the 10x TIMER_RATE_10 is equal to TIMER_RATE
 */

#define TIMER_RATE 384615
#define TIMER_RATE_10 TIMER_RATE

// Data storage variables.
unsigned int time = 0;    // Keeps time and sends values to store in timer[] occasionally.
#define BUFFER_SIZE 10
int timer[BUFFER_SIZE];   // Storage for timing of events.
int slope[BUFFER_SIZE];   // Storage for slope of events.
unsigned int totalTimer;  // Used to calculate period.
byte index = 0;           // Current storage index.
int maxSlope = 0;         // Used to calculate max slope as trigger point.
int newSlope;             // Storage for incoming slope data.

// Variables for decided whether you have a match.
#define MAX_NO_MATCH_VALUE 9
byte noMatch = 0;         // Counts how many non-matches you've received to reset variables if it's been too long.
byte slopeTol = 100;      // Slope tolerance - adjust this if you need.    *** 100 required here for dealing with square-ish signals ***
int timerTol = 10;        // Timer tolerance - adjust this if you need.

// Variables for amp detection.
unsigned int ampTimer = 0;
byte maxAmp = 0;
byte checkMaxAmp;
byte ampThreshold = 30;   // Raise if you have a very noisy signal.
long clippingTimer = 0;

// Clipping indicator variables.
boolean clipping = true;
#define CLIPPING_TIME 5 * TIMER_RATE // This should amount to 2 seconds.

#define write_pin12_low PORTB &= B11101111
#define write_pin12_high PORTB |= B00010000


ISR(ADC_vect) {       // When new ADC value ready.
  write_pin12_low; // Set pin 12 low.
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
        write_pin12_high; // Set pin 12 high.
        noMatch = 0;
        index++; // Increment index.
      }
      else if (abs(timer[0] - timer[index]) < timerTol && abs(slope[0] - newSlope) < slopeTol){ //if timer duration and slopes match
        // Sum timer values.
        totalTimer = 0;
        for (byte i = 0; i < index; i++){
          totalTimer += timer[i];
        }

        period = totalTimer * 100 ; // Set period. and increase resolution

        // moving average
        averaged_period = period + averaged_period - ((averaged_period - 8) >> 4);
        // remember to divide by 16 (>> 4)
        
        // Reset new zero index values to compare with.
        timer[0] = timer[index];
        slope[0] = slope[index];
        index = 1; // Set index to 1.
        write_pin12_high; // Set pin 12 high.
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

/*
// Stripped down code, works for any (simple) signal, including square-ish signals, but readings are ~2% high...


byte new_sample = 0;
byte saved_sample = 0;

uint16_t counter = 0;

ISR(ADC_vect) {       // When new ADC value ready.
  write_pin12_low;

  new_sample = ADCH;
  if (saved_sample < HALF_SAMPLE_VALUE && new_sample >= HALF_SAMPLE_VALUE)     // if we just crossed midpoint while rising
  {
    period = (counter * 100) + 0;

    // moving average
    averaged_period = period + averaged_period - ((averaged_period - 8) >> 4);
    // (remember to divide by 16 (>> 4))
   
    counter = 0;
    write_pin12_high;
    
  }
  else
  {
    counter++;
  }
  saved_sample = new_sample;
}
*/





void reset(){   // Clear out some variables.
  index = 0;    // Reset index.
  noMatch = 0;  // Reset match counter.
  maxSlope = 0; // Reset slope.
}




#define NR_COLUMNS 14

const uint16_t frequencyTable[] = {
  // Note, this table contains frequency values multiplied by 10 
  // so that one decimal value is included.
  // A4 which in an equal tempered scale is at 440 Hz, is represented by 4400 in the table.

  // A4 = 440 Hz
  // Allowed range = 6.0 cents

  // 504 x 2 bytes = 1008 bytes

  158,  162,  317,  325,  635,  651,  1270, 1303, 2541, 2607, 5083, 5214,  10167,  10428,  // C
  162,  164,  325,  328,  651,  656,  1303, 1312, 2607, 2625, 5214, 5250,  10428,  10501,  
  164,  168,  328,  336,  656,  673,  1312, 1346, 2625, 2693, 5250, 5385,  10501,  10771,  
  168,  172,  336,  345,  673,  690,  1346, 1381, 2692, 2762, 5385, 5524,  0,  0,  // C#
  172,  173,  345,  347,  690,  695,  1381, 1390, 2762, 2781, 5524, 5562,  0,  0,  
  173,  178,  347,  356,  695,  713,  1390, 1426, 2781, 2853, 5562, 5706,  0,  0,  
  178,  182,  356,  365,  713,  731,  1426, 1463, 2853, 2926, 5706, 5853,  0,  0,  // D
  182,  184,  365,  368,  731,  736,  1463, 1473, 2926, 2946, 5853, 5893,  0,  0,  
  184,  188,  368,  377,  736,  755,  1473, 1511, 2946, 3022, 5893, 6045,  0,  0,  
  189,  193,  377,  387,  755,  775,  1511, 1550, 3022, 3100, 6045, 6201,  0,  0,  // D#
  193,  195,  387,  390,  775,  780,  1550, 1561, 3100, 3122, 6201, 6244,  0,  0,  
  195,  200,  390,  400,  780,  800,  1561, 1601, 3122, 3202, 6244, 6404,  0,  0,  
  200,  205,  400,  410,  800,  821,  1601, 1642, 3202, 3284, 6404, 6569,  0,  0,  // E
  205,  206,  410,  413,  821,  827,  1642, 1653, 3284, 3307, 6569, 6615,  0,  0,  
  206,  212,  413,  424,  827,  848,  1653, 1696, 3307, 3392, 6615, 6785,  0,  0,  
  212,  217,  424,  435,  848,  870,  1696, 1740, 3392, 3480, 6785, 6960,  0,  0,  // F
  217,  219,  435,  438,  870,  876,  1740, 1752, 3480, 3504, 6960, 7008,  0,  0,  
  219,  224,  438,  449,  876,  898,  1752, 1797, 3504, 3594, 7008, 7189,  0,  0,  
  224,  230,  449,  460,  898,  921,  1797, 1843, 3594, 3687, 7189, 7374,  0,  0,  // F#
  230,  232,  460,  464,  921,  928,  1843, 1856, 3687, 3712, 7374, 7425,  0,  0,  
  232,  238,  464,  476,  928,  952,  1856, 1904, 3712, 3808, 7425, 7616,  0,  0,  
  238,  244,  476,  488,  952,  976,  1904, 1953, 3808, 3906, 7616, 7812,  0,  0,  // G
  244,  245,  488,  491,  976,  983,  1953, 1966, 3906, 3933, 7812, 7867,  0,  0,  
  245,  252,  491,  504,  983,  1008, 1966, 2017, 3933, 4034, 7867, 8069,  0,  0,  
  252,  258,  504,  517,  1008, 1034, 2017, 2069, 4034, 4138, 8069, 8277,  0,  0,  // G#
  258,  260,  517,  520,  1034, 1041, 2069, 2083, 4138, 4167, 8277, 8334,  0,  0,  
  260,  267,  520,  534,  1041, 1068, 2083, 2137, 4167, 4274, 8334, 8549,  0,  0,  
  267,  274,  534,  548,  1068, 1096, 2137, 2192, 4274, 4384, 8549, 8769,  0,  0,  // A
  274,  276,  548,  551,  1096, 1103, 2192, 2207, 4384, 4415, 8769, 8830,  0,  0,  
  276,  283,  551,  566,  1103, 1132, 2207, 2264, 4415, 4528, 8830, 9057,  0,  0,  
  283,  290,  566,  580,  1132, 1161, 2264, 2322, 4528, 4645, 9057, 9291,  0,  0,  // A#
  290,  292,  580,  584,  1161, 1169, 2322, 2338, 4645, 4677, 9291, 9355,  0,  0,  
  292,  299,  584,  599,  1169, 1199, 2338, 2399, 4677, 4798, 9355, 9596,  0,  0,  
  299,  307,  599,  615,  1199, 1230, 2399, 2460, 4798, 4921, 9596, 9843,  0,  0,  // B
  307,  309,  615,  619,  1230, 1239, 2460, 2478, 4921, 4955, 9843, 9912,  0,  0,  
  309,  317,  619,  635,  1239, 1270, 2478, 2541, 4955, 5083, 9912, 10167, 0,  0 
};

void testNote(int tableIndex, uint8_t LEDs, uint8_t digit) {
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
  if (condition == true) setLeds(LEDs, digit);
}




#define MAX_CHARS 25

#define CHAR_ZERO  0
#define CHAR_ONE   1
#define CHAR_TWO   2
#define CHAR_THREE 3
#define CHAR_FOUR  4
#define CHAR_FIVE  5
#define CHAR_SIX   6
#define CHAR_SEVEN 7
#define CHAR_EIGHT 8
#define CHAR_NINE  9
#define CHAR_OFF   10
#define CHAR_C     11
#define CHAR_CS    12
#define CHAR_D     13
#define CHAR_DS    14
#define CHAR_E     15
#define CHAR_F     16
#define CHAR_FS    17
#define CHAR_G     18
#define CHAR_GS    19
#define CHAR_A     20
#define CHAR_AS    21
#define CHAR_B     22
#define CHAR_NEG   23

#define MAX_CHARS 24



/*
 * Seven-Segment Display: segment names
 *    A
 *  F   B
 *    G
 *  E   C
 *    D   dp
 */

const uint8_t led_digits[] = {
  //EDC.BAFG

  0b11101110,     // 0 CHAR_ZERO
  0b00101000,     // 1 CHAR_ONE
  0b11001101,     // 2 CHAR_TWO
  0b01101101,     // 3 CHAR_THREE
  0b00101011,     // 4 CHAR_FOUR
  0b01100111,     // 5 CHAR_FIVE
  0b11100111,     // 6 CHAR_SIX
  0b00101100,     // 7 CHAR_SEVEN
  0b11101111,     // 8 CHAR_EIGHT
  0b01101111,     // 9 CHAR_NINE

  0b00000000,     // all off CHAR_OFF

  0b11000110,     // C  CHAR_C
  0b11010110,     // C# CHAR_CS
  0b11101001,     // D  CHAR_D
  0b11111001,     // D# CHAR_DS
  0b11000111,     // E  CHAR_E
  0b10000111,     // F  CHAR_F
  0b10010111,     // F# CHAR_FS
  #ifdef LARGE_G
    0b01101111,   // G  CHAR_G  ("G" shaped)
    0b01111111,   // G# CHAR_GS ("G" shaped)
  #else
    0b11100110,   // G  CHAR_G  ("g" shaped)
    0b11110110,   // G# CHAR_GS ("g" shaped)
  #endif
  0b10101111,     // A  CHAR_A
  0b10111111,     // A# CHAR_AS
  #ifdef LARGE_B
    0b11101111,   // B  CHAR_B ("8" shaped)
  #else
    0b11100011,   // B  CHAR_B ("b" shaped)
  #endif

  0b00000001      // - CHAR_NEG
};

// definitions for the 3 target LEDs
#define LED_FLAT    4
#define LED_IN_TUNE 2
#define LED_SHARP   1
#define LED_NONE    0

void test_digits(int delayTime)
{
  setLeds(LED_FLAT,    led_digits[CHAR_OFF]);
  delay(delayTime);
  setLeds(LED_IN_TUNE, led_digits[CHAR_OFF]);
  delay(delayTime);
  setLeds(LED_SHARP,   led_digits[CHAR_OFF]);
  delay(delayTime);
  
  for (uint8_t i = 0; i < MAX_CHARS; i++)
  {
    setLeds(0, led_digits[i]);
    delay(delayTime);
  }
}


void setLeds(uint8_t LEDs, uint8_t digit)
// LEDs contains the 3 bits indicating the 3 "target" LEDs
// bit 0 is "sharp", bit 1 is "in-tune" and bit 2 is "flat"
// digit describes how to draw the character on the 7-segment display
// it's a byte, and each bit is a segment, where 0 = segment is OFF, 1 = segment is ON
{
  digitalWrite(LED_FLAT_PIN,    LEDs & (1 << 2) ? HIGH : LOW);  // flat
  digitalWrite(LED_INTUNE_PIN,  LEDs & (1 << 1) ? HIGH : LOW);  // in-tune
  digitalWrite(LED_SHARP_PIN,   LEDs & (1 << 0) ? HIGH : LOW);  // sharp

  if (LED_DISPLAY_TYPE == COMMON_CATHODE) {
    // Decode led pattern and switch on/off the leds.          mask
    digitalWrite(LEDE,  digit & (1 << 7) ? LOW : HIGH);     // 10000000
    digitalWrite(LEDD,  digit & (1 << 6) ? LOW : HIGH);     // 01000000
    digitalWrite(LEDC,  digit & (1 << 5) ? LOW : HIGH);     // 00100000
    digitalWrite(LEDDP, digit & (1 << 4) ? LOW : HIGH);     // 00010000
    digitalWrite(LEDB,  digit & (1 << 3) ? LOW : HIGH);     // 00001000
    digitalWrite(LEDA,  digit & (1 << 2) ? LOW : HIGH);     // 00000100
    digitalWrite(LEDF,  digit & (1 << 1) ? LOW : HIGH);     // 00000010
    digitalWrite(LEDG,  digit & (1 << 0) ? LOW : HIGH);     // 00000001
  } else {
    digitalWrite(LEDE,  digit & (1 << 7) ? LOW : HIGH);
    digitalWrite(LEDD,  digit & (1 << 6) ? LOW : HIGH);
    digitalWrite(LEDC,  digit & (1 << 5) ? LOW : HIGH);
    digitalWrite(LEDDP, digit & (1 << 4) ? LOW : HIGH);
    digitalWrite(LEDB,  digit & (1 << 3) ? LOW : HIGH);
    digitalWrite(LEDA,  digit & (1 << 2) ? LOW : HIGH);
    digitalWrite(LEDF,  digit & (1 << 1) ? LOW : HIGH);
    digitalWrite(LEDG,  digit & (1 << 0) ? LOW : HIGH);
  }
}


void setup() {
  // target LEDs
  pinMode(LED_FLAT_PIN,  OUTPUT);  // flat
  pinMode(LED_INTUNE_PIN,  OUTPUT);  // in-tune
  pinMode(LED_SHARP_PIN,  OUTPUT);  // sharp

  // 7-segment display
  pinMode(LEDE,  OUTPUT);
  pinMode(LEDD,  OUTPUT);
  pinMode(LEDC,  OUTPUT);
  pinMode(LEDDP, OUTPUT);
  pinMode(LEDB,  OUTPUT);
  pinMode(LEDA,  OUTPUT);
  pinMode(LEDF,  OUTPUT);
  pinMode(LEDG,  OUTPUT);



#ifdef DEBUG
  Serial.begin(9600);

  test_digits(200);  // run some LEDs showcase

  Serial.println(F("Tune-O-Matic"));
  Serial.print(F("Lowest freq.:"));
  Serial.println(frequencyTable[0]);
  Serial.print(F("Lowest freq.:"));
  Serial.println(frequencyTable[0]);
#endif
  
  cli(); // Disable interrupts.

  // Set up continuous sampling of analog pin 0.

  // Clear ADCSRA and ADCSRB registers.
  ADCSRA = 0;
  ADCSRB = 0;

  ADMUX |= (1 << REFS0); // Set reference voltage.
  ADMUX |= (1 << ADLAR); // Left align the ADC value - so we can read highest 8 bits from ADCH register only

  ADCSRA |= (1 << ADPS2) | (1 << ADPS0); // Set ADC clock with 32 prescaler -> 16mHz / 32 = 500kHz.
  ADCSRA |= (1 << ADATE); // Enable auto trigger.
  ADCSRA |= (1 << ADIE);  // Enable interrupts when measurement complete.
  ADCSRA |= (1 << ADEN);  // Enable ADC.
  ADCSRA |= (1 << ADSC);  // Start ADC measurements.

  sei(); // Enable interrupts.
}


void loop() {
  frequency = (TIMER_RATE_10 * 100) / (averaged_period >> 4); // Timer rate with an extra zero/period.

  if (frequency < frequencyTable[0]) {
    // do not test for frequencies and display "0"
    setLeds(LED_NONE, led_digits[CHAR_ZERO]);
  }

  // C
  testNote( 0, LED_FLAT,    led_digits[CHAR_C]);
  testNote( 1, LED_IN_TUNE, led_digits[CHAR_C]);
  testNote( 2, LED_SHARP,   led_digits[CHAR_C]);

  // C#
  testNote( 3, LED_FLAT,    led_digits[CHAR_CS]);
  testNote( 4, LED_IN_TUNE, led_digits[CHAR_CS]);
  testNote( 5, LED_SHARP,   led_digits[CHAR_CS]);

  // D
  testNote( 6, LED_FLAT,    led_digits[CHAR_D]);
  testNote( 7, LED_IN_TUNE, led_digits[CHAR_D]);
  testNote( 8, LED_SHARP,   led_digits[CHAR_D]);

  // D#
  testNote( 9,  LED_FLAT,    led_digits[CHAR_DS]);
  testNote( 10, LED_IN_TUNE, led_digits[CHAR_DS]);
  testNote( 11, LED_SHARP,   led_digits[CHAR_DS]);

  // E
  testNote(12, LED_FLAT,    led_digits[CHAR_E]);
  testNote(13, LED_IN_TUNE, led_digits[CHAR_E]);
  testNote(14, LED_SHARP,   led_digits[CHAR_E]);

  // F
  testNote(15, LED_FLAT,    led_digits[CHAR_F]);
  testNote(16, LED_IN_TUNE, led_digits[CHAR_F]);
  testNote(17, LED_SHARP,   led_digits[CHAR_F]);

  // F#
  testNote(18, LED_FLAT,    led_digits[CHAR_FS]);
  testNote(19, LED_IN_TUNE, led_digits[CHAR_FS]);
  testNote(20, LED_SHARP,   led_digits[CHAR_FS]);

  // G
  testNote(21, LED_FLAT,    led_digits[CHAR_G]);
  testNote(22, LED_IN_TUNE, led_digits[CHAR_G]);
  testNote(23, LED_SHARP,   led_digits[CHAR_G]);

  // G#
  testNote(24, LED_FLAT,    led_digits[CHAR_GS]);
  testNote(25, LED_IN_TUNE, led_digits[CHAR_GS]);
  testNote(26, LED_SHARP,   led_digits[CHAR_GS]);

  // A
  testNote(27, LED_FLAT,    led_digits[CHAR_A]);
  testNote(28, LED_IN_TUNE, led_digits[CHAR_A]);
  testNote(29, LED_SHARP,   led_digits[CHAR_A]);

  // A#
  testNote(30, LED_FLAT,    led_digits[CHAR_AS]);
  testNote(31, LED_IN_TUNE, led_digits[CHAR_AS]);
  testNote(32, LED_SHARP,   led_digits[CHAR_AS]);

  // B
  testNote(33, LED_FLAT,    led_digits[CHAR_B]);
  testNote(34, LED_IN_TUNE, led_digits[CHAR_B]);
  testNote(35, LED_SHARP,   led_digits[CHAR_B]);

  // TOO HIGH FOR NOW
  if (frequency > 10180)
  {
    // do not test for frequencies and display "-"
    setLeds(LED_NONE, led_digits[CHAR_NEG]);
  }

#ifdef DEBUG
  Serial.print(averaged_period >> 4);
  Serial.print(F(" "));
  Serial.print(frequency);
  Serial.println(F(" dHz"));
#endif
}
