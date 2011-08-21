/*
 Bloom Serial Code
 
 This code reads a byte in via serial communication and then sets the state of 5 pin
 based on the byte it received.  This pins are intended to drive the effects on Toxic Bloom.
 
 Chris Linder - 2011
 */

// define output pins
const int mainFlamePin       = 5;   
const int effect1Pin         = 2;
const int effect2Pin         = 3;
const int effect3Pin         = 4;
const int adjustableFlamePin = 10;

// define map for 0-15 input values
const int adjustableFlameMap[] = {180, 178, 175, 173, 170, 168, 165, 163,
                                  150, 130, 110,  90,  70,  50,  25,   0};

// solenoid state
bool mainFlame;
bool effect1;
bool effect2;
bool effect3;
byte adjustableFlame;

void setup()
{
  // initialize the serial communication:
  Serial.begin(9600);
  // initialize the ledPin as an output:
  pinMode(mainFlamePin, OUTPUT);
  pinMode(effect1Pin, OUTPUT);
  pinMode(effect2Pin, OUTPUT);
  pinMode(effect3Pin, OUTPUT);
  pinMode(adjustableFlamePin, OUTPUT);
  
  
}

void loop()
{
  byte inStates;

  // check if data has been sent from the computer:
  if (Serial.available())
  {
    // read the most recent byte (which will be from 0 to 255):
    inStates = Serial.read();
    
    // read solenoid state from serial byte
    mainFlame = inStates & 0x01;
    effect1   = inStates & 0x02;
    effect2   = inStates & 0x04;
    effect3   = inStates & 0x08;
    byte adjustableFlameIn = inStates >> 4;
    adjustableFlame = adjustableFlameMap[adjustableFlameIn];
    
    // set output pins based on state
    digitalWrite(mainFlamePin, mainFlame ? HIGH : LOW);
    digitalWrite(effect1Pin,   effect1   ? HIGH : LOW);
    digitalWrite(effect2Pin,   effect2   ? HIGH : LOW);
    digitalWrite(effect3Pin,   effect3   ? HIGH : LOW);
    analogWrite(adjustableFlamePin, adjustableFlame);
  }
}

