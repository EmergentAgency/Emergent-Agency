// ScienceClouds.ino
//
// copyright 2020, Christopher Linder
//
// Works with:
// * Arduino 1.8.12
// * Teensy 4.0
// * FastLED version 3.003.003


// LED  includes
#include <WS2812Serial.h>
#define USE_WS2812SERIAL
#include <FastLED.h>

// Audio includes
#include <Audio.h>
#include <Wire.h>
#include <SD.h>
#include <SPI.h>
#include <SerialFlash.h>
#include <Bounce.h>

// Radar includes
#include <avr/io.h>
#include <avr/interrupt.h>


// LED
#define DATA_PIN 17

#define NUM_LEDS 500 
#define MAX_HEAT 240 // Don't go above 240
#define FRAMES_PER_SECOND 120
#define NUM_INTERP_FRAMES 50

CRGB leds[NUM_LEDS];
byte heat[NUM_LEDS];
byte last_heat[NUM_LEDS]; // Used for interp between heat frames

// Fire colors
DEFINE_GRADIENT_PALETTE( heatmap_gp ) {
  0,     8,  2,  0,   
50,     64, 16,  0,   
100,   128, 64,  8,   
200,   255,128, 64,   
240,   255,200,100, 
255,   255,255,255 }; //junk value
#define COOLING_MIN  5
#define COOLING_MAX  15
#define SPARKING 130
#define SPARK_HEAT_MIN 50
#define SPARK_HEAT_MAX 100
#define SPARK_WIDTH 2

CRGBPalette16 gPal = heatmap_gp;


// Audio

// Wave file
AudioPlaySdWav           playSdWav1;
AudioOutputI2S           i2s1;
AudioConnection          patchCord1(playSdWav1, 0, i2s1, 0);
AudioConnection          patchCord2(playSdWav1, 1, i2s1, 1);
AudioControlSGTL5000     sgtl5000_1;

//// Use these with the Teensy 3.0 Audio Shield
//#define SDCARD_CS_PIN    10
//#define SDCARD_MOSI_PIN  7
//#define SDCARD_SCK_PIN   14

// Use these with the Teensy 4.0 Audio Shield
#define SDCARD_CS_PIN    10
#define SDCARD_MOSI_PIN  11
#define SDCARD_SCK_PIN   13

// True of the audio system and the SD card were initialized successfully
bool g_bAudioInitialized;


// Radar

// Interrupt pin for motion sensor
#define INTERRUPT_PIN 14

// The motion sensor indicates the speed of the motion detected by oscillating its output pin.
// The faster the pulses, the faster the motion.  The detect the speed, we are counting the
// number of microseconds between rising pulses using a hardware interrupt pin.  The interrupt
// is called each time a rising pulse is detect and we save off the between the new pulse and
// the last pulse.  All the other code for calculating speed is done outside of the interrupt
// because the interrupt code needs to be as light weight at possible so it returns execution
// to the main code as quickly as possible.

// This is the last period detected
volatile unsigned long g_iLastPeriodMicro = 0;

// This is the last time the interrupt was called
volatile unsigned long g_iLastTimeMicro = 0;

// TEMP_CL - Testing x-band
volatile unsigned long g_iPulseCount = 0;

// Tuning - The min speed in meters per second to respond to.  Any motion at or below this will be 
// considered no motion at all.
float g_fMinSpeed = 0.02;

// Tuning The max speed in meters per second.  All motion above this speed will be treated like this speed
float g_fMaxSpeed = 0.10;

// Tuning - This is the speed smoothing factor (0, 1.0].  Low values mean more smoothing while a value of 1 means 
// no smoothing at all.  This value depends on the loop speed so if anything changes the loop speed,
// the amount of smoothing will change (see LOOP_DELAY_MS).
//static float fNewSpeedWeight = 0.15;
float g_fNewSpeedWeight = 0.02;

// Tuning - The exponent to apply to the linear 0.0 to 1.0 value from the sensor.  This allows the sensitivity curve
// to be adjusted in a non-linear fashion.
float g_fInputExponent = 0.8;

// This is the outout speed ratio [0, 1.0].  It is based on the speed read from the motion detector
// and g_fMinSpeed, g_fMaxSpeed, g_fNewSpeedWeight.
float g_fSpeedRatio;



// Clamp function
float Clamp(float fVal, float fMin, float fMax)
{
	if(fVal < fMin)
	{
		fVal = fMin;
	}
	else if(fVal > fMax)
	{
		fVal = fMax;
	}

	return fVal;
}



void setup()
{
	// LEDs
	
	// WS2812Serial and FastLED
	LEDS.addLeds<WS2812SERIAL, DATA_PIN, BRG>(leds, NUM_LEDS);
	
	
	// Audio 
		
	// Wave file
	g_bAudioInitialized = false;
	SPI.setMOSI(SDCARD_MOSI_PIN);
	SPI.setSCK(SDCARD_SCK_PIN);
	if (!(SD.begin(SDCARD_CS_PIN)))
	{
		Serial.print("Unable to access the SD card. Audio not initialized.");
	}
	else
	{
		AudioMemory(8);
		sgtl5000_1.enable();
		sgtl5000_1.volume(0.5);
		Serial.print("Audio and SD card successfully initialized.");
		g_bAudioInitialized = true;
	}
	delay(1000);

	
	// Radar
	
	// Attach an Interrupt to INTERRUPT_PIN for timing period of motion detector input
	pinMode(INTERRUPT_PIN, INPUT);
	attachInterrupt(digitalPinToInterrupt(INTERRUPT_PIN), MotionDetectorPulse, RISING);   
}


void update_speed() 
{
	// Get the cur
	float fCurSpeed = GetCurSpeed();

	// Clamp the range of the speed
	fCurSpeed = Clamp(fCurSpeed, g_fMinSpeed, g_fMaxSpeed);

	// Calculate the new speed ratio
	float fNewSpeedRatio = (fCurSpeed - g_fMinSpeed) / (g_fMaxSpeed - g_fMinSpeed);

	// TEMP_CL - try this AFTER the smoothing..
	//// Apply the input exponent to change the input curve
	//Serial.print("TEMP_CL fNewSpeedRatio pre exponent = ");
	//Serial.print(fNewSpeedRatio);
	//fNewSpeedRatio = pow(fNewSpeedRatio, fInputExponent);
	//Serial.print(" fNewSpeedRatio post exponent = ");
	//Serial.println(fNewSpeedRatio);

	// Calculate the smoothed speed ratio
	g_fSpeedRatio = fNewSpeedRatio * g_fNewSpeedWeight + g_fSpeedRatio * (1.0 - g_fNewSpeedWeight);

	// Apply the input exponent to change the input curve.  This is the final output.
	float fDisplaySpeedRatio = pow(g_fSpeedRatio, g_fInputExponent);
	
	// Debug output
	Serial.print(micros());
	Serial.print(" - TEMP_CL fNewSpeedRatio=");
	Serial.print(fNewSpeedRatio);
	Serial.print(" fCurSpeed=");
	Serial.print(fCurSpeed);
	Serial.print(" g_fSpeedRatio=");
	Serial.print(g_fSpeedRatio);
	Serial.print(" fDisplaySpeedRatio=");
	Serial.println(fDisplaySpeedRatio);
}



void loop()
{	
	// Make sure wav file is playing
	if (g_bAudioInitialized && playSdWav1.isPlaying() == false)
	{
		Serial.println("Start playing");
		//playSdWav1.play("SDTEST2.WAV");
		//playSdWav1.play("elaborate_thunder-Mike_Koenig-1877244752.WAV");
		playSdWav1.play("thunder.wav");
		delay(10); // wait for library to parse WAV info
	}


	// Add entropy to random number generator; we use a lot of it.
	// TEMP_CL random16_add_entropy( random());

	memcpy(last_heat, heat, NUM_LEDS);
	UpdateHeat(); // run simulation frame, using palette colors
  
	for(int i = 0; i < NUM_INTERP_FRAMES; ++i)
	{
		// Radar
		update_speed();
		
		// Adjust sound volume based on motion
		if(g_bAudioInitialized) {
			sgtl5000_1.volume(g_fSpeedRatio / 2.0 + 0.05);
		}

		//LEDs
		fract8 fLerp = i*256/NUM_INTERP_FRAMES;
		byte lerpHeat;
		for( int j = 0; j < NUM_LEDS; ++j)
		{
			// Fade between last and cur heat values
			lerpHeat = lerp8by8(last_heat[j], heat[j], fLerp);

			// Scale heat
			lerpHeat = scale8(lerpHeat, MAX_HEAT);

			leds[j] = ColorFromPalette( gPal, lerpHeat);
		}
		FastLED.show(); // display this frame
		
		// Delay a consistent amount of time so this code basically works the same on different chips
		//FastLED.delay(1000 / FRAMES_PER_SECOND); // This doesnt seem to work on the Teensy 4.0
		delay(1000 / FRAMES_PER_SECOND);
	}
}



void UpdateHeat()
{
	// Cool down every cell a little
	for(int i = 0; i < NUM_LEDS; i++)
	{
		heat[i] = qsub8( heat[i],  random8(COOLING_MIN, COOLING_MAX));
	}
	
	// Heat from each cell drifts out
	for(int i = 1; i < NUM_LEDS-1; i++)
	{
		heat[i] = (heat[i]*3 + heat[i-1]*2 + heat[i+1]*2) / 7;
	}
	
	// Randomly ignite new 'sparks' of heat - per 10 leds
	for(int i = 0; i < NUM_LEDS / 10; i++) {
		if( random8() < SPARKING )
		{
			int y = random16(0, NUM_LEDS - SPARK_WIDTH);

			byte newHeat = random8(SPARK_HEAT_MIN, SPARK_HEAT_MAX);
			for(int j = 0; j < SPARK_WIDTH; ++j) {
				heat[y+j] = qadd8( heat[y+j],  newHeat);
			}
		}
	}
}



// Interrupt called to get time between pulses
void MotionDetectorPulse()
{
	unsigned long iCurTimeMicro = micros();
	
	// HACK - For some reason the interrupt is getting called twice is
	// in very quick succession. This prevents the timing from being 
	// effected by that.
	unsigned long iPeriod = iCurTimeMicro - g_iLastTimeMicro;
	if(iPeriod > 100) 
	{
		g_iLastPeriodMicro = iPeriod;
		g_iLastTimeMicro = iCurTimeMicro;
	}
}



// Returns the number of micro seconds for the last period (time between pulses)
// for the motion sensor.  If the time between this call and
// the last inpterupt call is greater than the last period , we return the time
// between this call and the the last interrupt.  This is to ensure that if we
// abruptly go from fast motion to slow motion, that this function will not smoothly
// transition to slow motion.
unsigned long GetLastPeriodMicro()
{
	unsigned long iCurTimeMicro = micros();
	unsigned long iThisPeriodMicro = iCurTimeMicro - g_iLastTimeMicro;
	if(iThisPeriodMicro < g_iLastPeriodMicro)
		return g_iLastPeriodMicro;
	else
		return iThisPeriodMicro;
}



// Returns the current speed of the detected motion in meters per second.
// It is calculated with the following formula (pulled from the X-Band website)
// http://www.parallax.com/Portals/0/Downloads/docs/prod/sens/32213-X-BandMotionDetector-v1.1.pdf
//
// Resulting frequency for speed detection:
//
// Fd = 2V(Ft/c)cos(theta)
//
// Where: 
// Fd = Difference frequency (sometimes referred to as Doppler frequency) 
// V = Velocity of the target 
// Ft = Transmit frequency (10.525 GHz)
// c = Speed of light at 3 x 10^8 m/s
// theta = Motion direction angle deviation from perpendicular to the antenna PCB (Figure 1)
//
// Thus and object moving at 0.2 ms/2 =
// 2*0.2*(10.525*10^9/(3*10^8)) = 14.0333
float GetCurSpeed()
{
	float iLastPeriod = GetLastPeriodMicro();
	float fFreq = 1.0 / (iLastPeriod / 1000000.0);
	float fSpeed = fFreq / 70.1666; // 70.1666 = (2 * Ft/c) - Ignore angle (cos(theta)) because we have no way of knowing it
	return fSpeed;
}


