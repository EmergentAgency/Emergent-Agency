// TreeLights.ino
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

// LED
#define DATA_PIN 17
#define NUM_LEDS 510 
#define NUM_INTERP_FRAMES 20

CRGB g_aLeds[NUM_LEDS];
CRGB g_aNewLeds[NUM_LEDS];
CRGB g_aLastLeds[NUM_LEDS];



void setup()
{
	// Setup serial
	Serial.begin(9600);
	
	// LEDs
	// WS2812Serial and FastLED
	LEDS.addLeds<WS2812SERIAL, DATA_PIN, BRG>(g_aLeds, NUM_LEDS);
}



void loop()
{	
	memcpy(g_aLastLeds, g_aNewLeds, NUM_LEDS * sizeof(CRGB));
	for(int i = 0; i < NUM_LEDS; ++i)
	{
		if(random8() >= 128)
		{
			byte yRand8 = random8(); 
			g_aNewLeds[i] = CRGB (yRand8, yRand8, yRand8/2);
		}
		else
		{
			g_aNewLeds[i] = CRGB (0, 0, random8());
		}
	}
	int iInterpFrames = 100;
	for(int i = 0; i < iInterpFrames; ++i)
	{
		fract8 fLerp = i * 255 / iInterpFrames;
		for(int i = 0; i < NUM_LEDS; ++i)
		{
			// Pick random color
			g_aLeds[i] = g_aLastLeds[i].lerp8(g_aNewLeds[i], fLerp);
		}
		FastLED.show(); 	
		delay(10);
	}
		
	// Delay a consistent amount of time so this code basically works the same on different chips
	//FastLED.delay(1000 / FRAMES_PER_SECOND); // This doesnt seem to work on the Teensy 4.0
	delay(1);
}




