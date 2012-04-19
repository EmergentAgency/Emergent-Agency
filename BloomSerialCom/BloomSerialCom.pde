/*
 Bloom Serial Code
 
 This code reads a byte in via serial communication and then sets the state of 5 pin
 based on the byte it received.  This pins are intended to drive the effects on Toxic Bloom.
 
 Chris Linder - 2011
 */

// define output pins
const int mainFlamePin       = 6;   
const int effect1Pin         = 3;
const int effect2Pin         = 4;
const int effect3Pin         = 5;
const int adjustableFlamePin = 10;

// Not enough fire - goes otu
// define map for 0-15 input values
//const int adjustableFlameMap[] = {180, 178, 175, 173, 170, 168, 165, 163,
//                                  150, 130, 110,  90,  70,  50,  25,   0};

// define map for 0-15 input values
const int adjustableFlameMap[] = {130, 120, 110, 100,  90,  80,  70,  60,
                                   50,  40,  30,  20,  15,  10,   5,   0};
                                  
const int timeInMillisBeforeTurnOffPins = 1000;

const int noInputFlameValue = 15;

// solenoid state
bool mainFlame;
bool effect1;
bool effect2;
bool effect3;
byte adjustableFlame;

bool bInNoInputMode;

unsigned long lastSerialInputTime;

void enterNoInputMode()
{
  digitalWrite(mainFlamePin, LOW);
  digitalWrite(effect1Pin,   LOW);
  digitalWrite(effect2Pin,   LOW);
  digitalWrite(effect3Pin,   LOW);
  analogWrite(adjustableFlamePin, adjustableFlameMap[noInputFlameValue]); // keep adjustable flame effects partly on to might main effect 
  bInNoInputMode = true;
}

void exitNoInputMode()
{
  bInNoInputMode = false;
}

void setup()
{
  // initialize the serial communication:
  Serial.begin(9600);
  // initialize the pins as an output:
  pinMode(mainFlamePin, OUTPUT);
  pinMode(effect1Pin, OUTPUT);
  pinMode(effect2Pin, OUTPUT);
  pinMode(effect3Pin, OUTPUT);
  pinMode(adjustableFlamePin, OUTPUT);
}

void loop()
{
  byte inStates;
  
  // check to see if we haven't gotten input in a while and if so turn off the output pins
  if(!bInNoInputMode)
  {
    unsigned long curTime = millis();
    if(curTime - lastSerialInputTime > timeInMillisBeforeTurnOffPins)
    {
      enterNoInputMode();
    }
  }

  // check if data has been sent from the computer:
  if (Serial.available())
  {
    // record last serial time
    lastSerialInputTime = millis();
    
    // if the output pins were off, turn them back on
    if(bInNoInputMode)
    {
      exitNoInputMode();
    }

    // read the most recent byte (which will be from 0 to 255):
    inStates = Serial.read();
    
    // read solenoid state from serial byte
    mainFlame = inStates & 0x01;
    effect1   = inStates & 0x02;
    effect2   = inStates & 0x04;
    effect3   = inStates & 0x08;
    byte adjustableFlameIn = inStates >> 4;
    adjustableFlame = adjustableFlameMap[adjustableFlameIn];
 
    // TEMP_CL - debugging adjustable flame
    //Serial.write(inStates);
    //mainFlame = 0;
    //effect1 = 0;
    //effect2 = 0;
    //effect3 = 0;
    //adjustableFlame = inStates - 32;
    
    // set output pins based on state
    digitalWrite(mainFlamePin, mainFlame ? HIGH : LOW);
    digitalWrite(effect1Pin,   effect1   ? HIGH : LOW);
    digitalWrite(effect2Pin,   effect2   ? HIGH : LOW);
    digitalWrite(effect3Pin,   effect3   ? HIGH : LOW);
    analogWrite(adjustableFlamePin, adjustableFlame);
  }
}

