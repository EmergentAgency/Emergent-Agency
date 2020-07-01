// BarLights.ino
//
// copyright 2020, Christopher Linder
//
// Works with:
// * Arduino 1.8.12
// * Teensyduino 1.52
// * Teensy 3.2
// * FastLED version 3.003.001



// Helps with long strings of LEDs
#define FASTLED_ALLOW_INTERRUPTS 0
#ifdef __AVR__
  #include <avr/power.h>
#endif

// Fast LED lib
#include <FastLED.h>

// Use Serial to print out debug statements
#define USE_SERIAL_FOR_DEBUGGING true

// LED data pin
#define DATA_PIN 7

// Number of LEDs
#define NUM_LEDS 120

// Input pins for touch 
#define SENSOR_PIN_CONTROL A3    // Touch pin to turn the system on and off and adjust things
#define SENSOR_PIN_TOUCH   A1    // Touchtone pin

// Global tuning for touch
#define MIN_SENSOR_VALUE 90
#define MIN_INPUT_VALUE 10
#define MIN_CONTROL_ON 300

// Global tuning for LED heat effect
#define MAX_HEAT 240 // Don't go above 240
#define FRAMES_PER_SECOND 120
#define NUM_INTERP_FRAMES 20
#define COOLING_MIN  5
#define COOLING_MAX  15
#define SPARKING 130
#define SPARK_HEAT_MIN 50
#define SPARK_HEAT_MAX 100
#define SPARK_WIDTH 2

// LEDs
CRGB g_aLeds[NUM_LEDS];

// Heat vars
byte g_ayHeat[NUM_LEDS];
byte g_ayLastHeat[NUM_LEDS]; // Used for interp between heat frames

//// Bar light colors - 1 - more white
//DEFINE_GRADIENT_PALETTE( g_oHeatGP ) {
//  0,     8,  0,  2,   
//50,     64,  0, 16,   
//100,   128,  8, 64,   
//200,   255, 64,128,   
//240,   255,100,200, 
//255,   255,255,255 }; //junk value

// Bar light colors - 2 - more amber
DEFINE_GRADIENT_PALETTE( g_oHeatGP ) {
  0,     8,  0,  2,   
50,     64,  0, 16,   
100,   128,  0, 64,   
200,   255,  0,128,   
240,   255,  0,200, 
255,   255,  0,255 }; //junk value

//// R (amber) channel on white LEDs
//DEFINE_GRADIENT_PALETTE( g_oHeatGP ) {
//  0,     8,  0,  0,   
//50,     64,  0,  0,   
//100,   128,  0,  0,   
//200,   255,  0,  0,   
//240,   255,  0,  0, 
//255,   255,  0,  0 }; //junk value

//// G (cool white) channel on white LEDs
//DEFINE_GRADIENT_PALETTE( g_oHeatGP ) {
//  0,     0,  8,  0,   
//50,      0, 64,  0,   
//100,     0,128,  0,   
//200,     0,255,  0,   
//240,     0,255,  0, 
//255,     0,255,  0 }; //junk value

//// B (warm white) channel on white LEDs
//DEFINE_GRADIENT_PALETTE( g_oHeatGP ) {
//  0,     0,  0,  8,   
//50,      0,  0, 64,   
//100,     0,  0,128,   
//200,     0,  0,255,   
//240,     0,  0,255, 
//255,     0,  0,255 }; //junk value

// Heat palette
CRGBPalette256 g_oPalHeat = g_oHeatGP;


// Cool white gradiant
DEFINE_GRADIENT_PALETTE( g_oTouchGP ) {
  0,     0,  0,  0,   
50,      0, 32,  0,   
100,     0,128,  0,   
200,     0,255,  0,   
240,     0,255,  0, 
255,   255,255,255 }; //junk value

// Touch palette
CRGBPalette256 g_oPalTouch = g_oTouchGP;


// Touch values

//// Tuning vars for touch light
//#define TOUCH_TRIGGER_FRAMES 10
//#define MIN_TOUCH_BRIGHTNESS 0.3
////#define DEAFULT_BRIGHTNESS_LIGHT_1 50
//#define TOUCH_SMOOTH_ATTACK 0.80
//#define TOUCH_SMOOTH_DECAY 0.95
//
//// Tuning vars for singal processing
//static float g_fNode0Exp=0.0;
//static int   g_iNode0Avg=2;
//static float g_fNode1Exp=0.3;
//static int   g_iNode1Avg=10;
//static float g_fDetectThreshold=0.4;



float g_fTouchInput = 0;
bool g_bLedsOn = true;
bool g_bLastControlTouched = false;

// Smoothied float value of the sensor
float g_fSmoothedSensor = 0;



void setup()
{
	// Works with Teensy 3.2
	FastLED.addLeds<NEOPIXEL, DATA_PIN>(g_aLeds, NUM_LEDS);
	
	// Works with Teensy 4.0? Nope... :(
	//FastLED.addLeds<1, SK6812, DATA_PIN, GRB>(g_aLeds, NUM_LEDS);
}


void UpdateFromTouchInput()
{
	// Turn lights off and on based on control pin
	bool bControlTouched = analogRead(SENSOR_PIN_CONTROL) > MIN_CONTROL_ON;
	if(bControlTouched & !g_bLastControlTouched)
	{
		if(g_bLedsOn) 
		{
			g_bLedsOn = false;
		}
		else
		{
			g_bLedsOn = true;
		}
	}
	g_bLastControlTouched = bControlTouched;
	
	// Read raw touch input value
	int iSensorValue = analogRead(SENSOR_PIN_TOUCH);

	// Sometimes this system has noise, particularlly if people are near the touch lights
	// and are only holding the sensor read handled (as opposed to the postive voltage handle).
	// This helps removed that noise.
	if(iSensorValue < MIN_SENSOR_VALUE)
	{
		iSensorValue = 0;
	}
	else
	{
		iSensorValue = iSensorValue - MIN_SENSOR_VALUE;
	}
	
	// Take raw sensor value and turn it unto a useful input value called fInput
	float fSensor = 0;
	if(iSensorValue > MIN_INPUT_VALUE)
	{
		fSensor = iSensorValue / 1000.0;
		if(fSensor > 1.0)
		{
			fSensor = 1.0;
		}
	}
	float fSmoothAmount = 0.8;
	g_fSmoothedSensor = g_fSmoothedSensor * fSmoothAmount + fSensor * (1 - fSmoothAmount);
	g_fTouchInput = pow(g_fSmoothedSensor, 1.5);
	
	if(USE_SERIAL_FOR_DEBUGGING)
	{
		Serial.print("g_fTouchInput=");
		Serial.println(g_fTouchInput);
	}
}



void loop()
{
	// Save off the last heat value to interp between
	memcpy(g_ayLastHeat, g_ayHeat, NUM_LEDS);

	// Update the head simulation 
	UpdateHeat(g_ayHeat, true);
  
	for(int i = 0; i < NUM_INTERP_FRAMES; ++i)
	{
		// Update global values based on touch input
		UpdateFromTouchInput();
		
		// If the LEDs are off, we don't need to do anything else.
		if(!g_bLedsOn)
		{
			FastLED.setBrightness(0);
		}
		else
		{
			// If the LEDs aren't off, we should adjust the brightness
			//FastLED.setBrightness(127 + fInput * 128);
			FastLED.setBrightness(192);

			// Lerp between the target frames
			fract8 fLerp = i * 256 / NUM_INTERP_FRAMES;
			byte yLerpHeat;
			for(int j = 0; j < NUM_LEDS; ++j)
			{
				// Fade between last and cur heat values
				yLerpHeat = lerp8by8(g_ayLastHeat[j], g_ayHeat[j], fLerp);

				// Scale heat
				yLerpHeat = scale8(yLerpHeat, MAX_HEAT);
				
				// Get the heat color
				CRGB color0 = ColorFromPalette(g_oPalHeat, yLerpHeat);
				
				// Get the touch color
				CRGB color1 = ColorFromPalette(g_oPalTouch, g_fTouchInput * MAX_HEAT);
				
				// Blend them together
				g_aLeds[j] = color0 + color1;
			}
		}

		// Update the LEDs
		FastLED.show();
		
		// Keep a regular framerate
		FastLED.delay(1000 / FRAMES_PER_SECOND);
	}
}



void UpdateHeat(byte ayHeat[], bool bAddSparks)
{
	// Cool down every cell a little
	for(int i = 0; i < NUM_LEDS; i++)
	{
		ayHeat[i] = qsub8(ayHeat[i],  random8(COOLING_MIN, COOLING_MAX));
	}
	 
	// Heat from each cell drifts out
	for(int i = 1; i < NUM_LEDS-1; i++)
	{
		ayHeat[i] = (ayHeat[i]*3 + ayHeat[i-1]*2 + ayHeat[i+1]*2) / 7;
	}
	
	// Randomly ignite new 'sparks' of heat - per 10 leds
	if(bAddSparks)
	{
		for(int i = 0; i < NUM_LEDS / 10; i++)
		{
			if(random8() < SPARKING )
			{
				int y = random8(0, NUM_LEDS - SPARK_WIDTH);

				byte yNewHeat = random8(SPARK_HEAT_MIN, SPARK_HEAT_MAX);
				for(int j = 0; j < SPARK_WIDTH; ++j) {
					ayHeat[y+j] = qadd8( ayHeat[y+j],  yNewHeat);
				}
			}
		}
	}
}
