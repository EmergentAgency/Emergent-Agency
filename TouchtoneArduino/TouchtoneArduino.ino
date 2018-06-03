/**
 * File: TouchtoneArduino.ino
 *
 * Description: Reading conductance of skin and sending it to the PC
 * 
 * Copyright: 2018 Chris Linder
 */

#include <TimerOne.h>                    

// Setting for using serial for debugging or communicating with the PC
bool bUseSerialForDebugging = false;

// Input pin to read from
#define SENSOR_PIN A8

// Current sensor value
int g_iSensorValue = 0;

// LED output pin to visualize current reading
#define LED_OUT_PIN 10

// Lookup map used to linearize LED brightness
static const unsigned char exp_map[256]={
  0,  0,  1,  1,  1,  2,  2,  2,  3,  3,  3,  4,  4,  4,  5,  5,
  5,  6,  6,  6,  7,  7,  7,  8,  8,  8,  9,  9,  9,  10, 10, 10,
  11, 11, 11, 12, 12, 12, 13, 13, 13, 14, 14, 14, 15, 15, 15, 16,
  16, 16, 17, 17, 17, 18, 18, 18, 19, 19, 19, 20, 20, 20, 21, 21, 
  21, 22, 22, 22, 23, 23, 23, 24, 24, 24, 25, 25, 25, 26, 26, 26, 
  27, 27, 27, 28, 28, 28, 29, 29, 29, 30, 30, 30, 31, 31, 31, 32, 
  32, 32, 33, 33, 33, 34, 34, 34, 35, 35, 35, 36, 37, 37, 37, 38, 
  38, 38, 39, 39, 39, 40, 40, 40, 41, 41, 41, 42, 42, 42, 43, 43,
  43, 44, 44, 44, 45, 45, 45, 46, 46, 46, 47, 47, 47, 48, 48, 48, 
  49, 49, 49, 50, 50, 50, 51, 51, 51, 52, 52, 52, 53, 53, 53, 54,
  54, 54, 55, 55, 55, 56, 56, 56, 57, 57, 57, 58, 58, 58, 59, 59, 
  59, 60, 60, 60, 61, 61, 61, 62, 62, 62, 63, 63, 63, 64, 64, 64, 
  65, 66, 67, 69, 70, 72, 74, 75, 77, 79, 80, 82, 84, 86, 88, 90,
  91, 94, 96, 98, 100,102,104,107,109,111,114,116,119,122,124,127,
  130,133,136,139,142,145,148,151,155,158,161,165,169,172,176,180,
  184,188,192,196,201,205,210,214,219,224,229,234,239,244,250,255
};


// Dimmer vars

// Variable to use as a counter
volatile int g_iDimmerCounter = 0;

// Boolean to store a "switch" to tell us if we have crossed zero
volatile boolean g_bZeroCross = false;

// PowerSSR Tail connected to digital pin 4
#define PSSR1 4

// Default dimming level (0-127)  0 = off, 127 = on
int g_iDimmerBrightness = 32;

// This 66 microseconds.  AC power runs at 60 hz and crosses zero twice in a since cycle.  Our brightness runs from 0 to 127 so there are 128 different
// brightness levels in a single half cycle.  This means our counter should count to 128 twice (256) every full power cycle.
// 1,000,000 microseconds / 60 / 256 = 65.1 microseconds
// And we rounded up to 66 because that ensures that we don't over count.
int g_iFreqStep = 66;

// smoothing
float g_fSmoothedBrightness = 0;

// Last activity time in milliseconds
unsigned long g_iLastActivityTimeMS = 0;

// Time at which to engage the inactvity / attract mode
unsigned long g_iTimeTillAttactModeMS = 10000;

// True if we are in attract mode
bool g_bAttractModeActive = false;

// Attract mode single sin pulse period in seconds - This is randomize for each pulse
float g_fAttractPeriodSec = 0.5;

// Attract mode period min / max
float g_fAttackPeriodSecMin = 0.2;
float g_fAttackPeriodSecMax = 10.0;

// Start time in MS of attract pulse
unsigned long g_iAttractPeriodPulseStartTimeMS = 0;

// Attract mode peak brightness - This is randomized each pulse
float g_iAttractBrightness = 64;

// Attract mode peak brightness min / max
float g_iAttractBrightnessMin = 64;
float g_iAttractBrightnessMax = 127;

// Attrack mode pulse repeats
int g_iAttractRepeats = 1;

// Attract mode repeats min / max
int g_iAttractRepeatsMin = 2;
int g_iAttractRepeatsMax = 4;



void setup()
{
	// setup debugging serial port (over USB cable) at 9600 bps
	Serial.begin(9600);

	if(bUseSerialForDebugging)
	{
		Serial.println("Touchtone - Setup");
	}

	// Setting the sensor pin as INPUT produces weird results so we're not doing that.
  // Weird results means that if the input level exceeds something around 512 then the analogRead
  // gets "stuck" and keeps returning a high value despite being grounded. This doesn't happen
  // if the pin mode is not set.
  // Also weird is that setting the pin to INPUT_PULLUP once and then not setting the next code push
  // Seem to have helped reduce the noise on the pin. I really really don't understand analog...
  //pinMode(SENSOR_PIN, INPUT);
  //pinMode(SENSOR_PIN, INPUT_PULLUP);


	// LED output pin
	pinMode(LED_OUT_PIN, OUTPUT);
	digitalWrite(LED_OUT_PIN, LOW);

/*
	// Dimmer setup

	// Set SSR1 pin as output
	pinMode(PSSR1, OUTPUT);

	// Attach an Interupt to digital pin 2 (interupt 0),
	attachInterrupt(0, ZeroCrossDetect, RISING);

	// Init timer
	Timer1.initialize(g_iFreqStep);
	Timer1.attachInterrupt(DimmerCheck, g_iFreqStep);
 */
}



void loop()
{
	// Read the current sensor value
  // TEMP_CL analogRead(SENSOR_PIN); 
  g_iSensorValue = analogRead(SENSOR_PIN); 

	// Write the current sensor value to serial
	if(bUseSerialForDebugging)
	{
		Serial.print("g_iSensorValue=");
		Serial.println(g_iSensorValue);
	}

	// Take raw sensor value and turn it unto a useful input value called fInput
	float fBrightness = 0;
	if(g_iSensorValue > 100)
	{
		fBrightness = g_iSensorValue / 1000.0;
		if(fBrightness > 1.0)
		{
			fBrightness = 1.0;
		}
	}
	float fSmoothAmount = 0.8;
	g_fSmoothedBrightness = g_fSmoothedBrightness * fSmoothAmount + fBrightness * (1 - fSmoothAmount);
	float fInput = pow(g_fSmoothedBrightness, 1.5);
 
  // Write fInput
  if(bUseSerialForDebugging)
  {
    Serial.print("fInput=");
    Serial.println(fInput);
  }

	// Set dimmer brightness
	g_iDimmerBrightness = 0;
	if(fInput > 0.05)
	{
		g_iDimmerBrightness = fInput * 107 + 20; // Incandecent lights don't turn on right away so we need to start above 0
	}

	// Write the current sensor value to serial
	if(bUseSerialForDebugging)
	{
		Serial.print("g_iDimmerBrightness=");
		Serial.println(g_iDimmerBrightness);
	}

/*
	// Override g_iDimmerBrightness if there hasn't been activity for a while.  This will not effect the LEDS or the data sent to the computer
	unsigned long iCurTimeMS = millis();
	if(g_iDimmerBrightness > 0)
	{
		g_iLastActivityTimeMS = iCurTimeMS;
		g_bAttractModeActive = false;
	}
	if(!g_bAttractModeActive && iCurTimeMS - g_iLastActivityTimeMS > g_iTimeTillAttactModeMS)
	{
		g_bAttractModeActive = true;
		g_iAttractPeriodPulseStartTimeMS = iCurTimeMS;
	}
	if(g_bAttractModeActive)
	{
		float fPeriodRatio = (float(iCurTimeMS - g_iAttractPeriodPulseStartTimeMS) / 1000.0) / g_fAttractPeriodSec;
		if(fPeriodRatio > g_iAttractRepeats)
		{
			g_iAttractPeriodPulseStartTimeMS = iCurTimeMS;
			fPeriodRatio = 0;
			g_fAttractPeriodSec = g_fAttackPeriodSecMin + random(g_fAttackPeriodSecMax - g_fAttackPeriodSecMin + 1);
			g_iAttractBrightness = g_iAttractBrightnessMin + random(g_iAttractBrightnessMax - g_iAttractBrightnessMin + 1);
			g_iAttractRepeats = g_iAttractRepeatsMin + random(g_iAttractRepeatsMax - g_iAttractRepeatsMin + 1);
		}
		g_iDimmerBrightness = 1.0 - (cos(2 * 3.14 * fPeriodRatio) + 0.0) / 2.0 * g_iAttractBrightness; // TEMP_CL - This is wrong and I don't know why the +0.0 works (it should be +1.0) but it does so I'm leaving it.
	}
 */

	// The output LED is based on fInput not g_iDimmerBrightness
	int iLEDValue = fInput * 255;

	// Adjust brightness to be more linear
	iLEDValue = exp_map[iLEDValue];

  // Write out the LED value
	analogWrite(LED_OUT_PIN, iLEDValue);

	// Send the raw sensor value to the PC Touchtone client. Cut off the buttom bits so it fits in a byte.
  if(!bUseSerialForDebugging)
  {
	  byte ySendByte = g_iSensorValue/4;
	  Serial.write(ySendByte);
  }

	// Delay a bit before reading again
	// Oddly if this is too short the analog read seems to read non-zero...  um, what?
	// Both 5 and 10 have worked well in the past
	delay(5);
}

/*
// This function will fire the triac at the proper time
void DimmerCheck()
{
	// First check to make sure the zero-cross has happened else do nothing
	if(g_bZeroCross)
	{
		if( g_iDimmerCounter < 120 && // If too much time has passed, don't fire the tail at all. This is because if we fire too late, it will get stuck on the whole next cycle and cause a flash in brightness
			g_iDimmerCounter >= (127-g_iDimmerBrightness)) // The brighter the light should be, the sooner we should fire the tail to turn on the rest of this cycle
		{
			//These values will fire the PSSR Tail.  When the tail fires, it will stay on till the next zero cross.
			//delayMicroseconds(100); // I'm not sure why there was this delay in the example code so I'm taking it out because it made the tail fire late sometimes which caused the light to flicker
			digitalWrite(PSSR1, HIGH);
			delayMicroseconds(50);
			digitalWrite(PSSR1, LOW);

			// Reset the accumulator
			g_iDimmerCounter = 0;                         

			// Reset the zero_cross so it may be turned on again at the next zero_cross_detect    
			g_bZeroCross = false;                
		}
		else
		{
			// If the dimming value has not been reached, increment the counter
			g_iDimmerCounter++;
		}
	}
}


void ZeroCrossDetect() 
{
	// set the boolean to true to tell our dimming function that a zero cross has occured
	g_bZeroCross = true;
	g_iDimmerCounter = 0;
} 
*/
