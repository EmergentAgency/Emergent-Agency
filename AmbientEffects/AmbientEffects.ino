#define FASTLED_ALLOW_INTERRUPTS 0
#ifdef __AVR__
  #include <avr/power.h>
#endif
#include <FastLED.h>

#define DATA_PIN 8

#define NUM_LEDS 150 // 70 is the max the 150 led strip in the bottles seems to work past...
#define MAX_HEAT 240 // Don't go above 240
#define FRAMES_PER_SECOND 120
#define NUM_INTERP_FRAMES 50

CRGB leds[NUM_LEDS];
byte heat[NUM_LEDS];
// TEMP_CL i think I'm running out of memory
//byte prev_heat[NUM_LEDS]; // Used for frame blending
byte last_heat[NUM_LEDS]; // Used for interp between heat frames


// Trying more yellow
// Fire color - bright for club
DEFINE_GRADIENT_PALETTE( heatmap_gp ) {
  0,     8,  2,  0,   
50,     64, 16,  0,   
100,   128, 64,  8,   
200,   255,128, 64,   
240,   255,200,100, 
255,   255,255,255 }; //junk value
#define COOLING_MIN  5
#define COOLING_MAX  15
//#define SPARKING 200
//#define SPARK_HEAT_MIN 40
//#define SPARK_HEAT_MAX 60
#define SPARKING 130
#define SPARK_HEAT_MIN 50
#define SPARK_HEAT_MAX 100
#define SPARK_WIDTH 2




// Looks good when you can see the LEDs but too red and white when in bottles
//// Fire color - bright for club
//DEFINE_GRADIENT_PALETTE( heatmap_gp ) {
//  0,     4,  0,  0,   
//50,     64,  8,  0,   
//100,    80, 30,  0,   
//200,   255,128, 64,   
//240,   255,200,150, 
//255,   255,255,255 }; //junk value
//#define COOLING_MIN  5
//#define COOLING_MAX  15
////#define SPARKING 200
////#define SPARK_HEAT_MIN 40
////#define SPARK_HEAT_MAX 60
//#define SPARKING 130
//#define SPARK_HEAT_MIN 50
//#define SPARK_HEAT_MAX 100
//#define SPARK_WIDTH 2

//// Dinner 1 - All colors are GRB!
//DEFINE_GRADIENT_PALETTE( heatmap_gp ) {
//  0,   0,   3,   0, 
//50,    1,   8,   0,
//100,   5,   30,  0,
//200,   25, 40, 20,
//240,   50,  64,  40,
//255,   0,0,0 }; //junk value
//#define COOLING_MIN  5
//#define COOLING_MAX  15
//#define SPARKING 150
//#define SPARK_HEAT_MIN 40
//#define SPARK_HEAT_MAX 60


//// Dinner 2
//DEFINE_GRADIENT_PALETTE( heatmap_gp ) {
//  0,   0,   5,   0, 
//50,    2,   10,  0,
//100,   5,   20,  0,
//200,   20,  40,  5,
//240,   30,  50,  10,
//255,   0,0,0 }; //junk value
//#define COOLING_MIN  5
//#define COOLING_MAX  10
//#define SPARKING 160
//#define SPARK_HEAT_MIN 40
//#define SPARK_HEAT_MAX 50

//// Dinner 2 GRB
//DEFINE_GRADIENT_PALETTE( heatmap_gp ) {
//  0,   0,   0,   5, 
//50,    0,   2,  10,
//100,   0,   5,  20,
//200,   5,   10,  30,
//240,   10,  50,  50,
//255,   0,0,0 }; //junk value
//#define COOLING_MIN  5
//#define COOLING_MAX  10
//#define SPARKING 160
//#define SPARK_HEAT_MIN 40
//#define SPARK_HEAT_MAX 50



CRGBPalette16 gPal = heatmap_gp;



void setup()
{
	//FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS).setCorrection( TypicalLEDStrip );
	//FastLED.setBrightness( BRIGHTNESS );
	FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);

	//gPal = CRGBPalette16(CRGB(64,32,32), CRGB(192,128,64), CRGB(255,192,96), CRGB(255,255,128));
	//gPal = CRGBPalette16(CRGB(64,0,0), CRGB(92,32,0), CRGB(255,64,0), CRGB(255,128,0));
	//gPal = CRGBPalette16(CRGB(8,0,0), CRGB(92,32,0), CRGB(255,64,0), CRGB(255,128,0));
	//gPal = CRGBPalette16(CRGB(0,0,0), CRGB(16,8,0), CRGB(128,32,0), CRGB(255,128,0));
	//gPal = CRGBPalette16(CHSV(0,0,0), CHSV(30,255,16), CHSV(15,255,128), CHSV(30,255,255));
	//gPal = CRGBPalette16(CHSV(0,0,0), CHSV(30,255,16), CHSV(30,255,128), CHSV(30,255,255));
	//gPal = CRGBPalette16(CHSV(0,0,0), CHSV(30,255,16), CHSV(30,255,128), CHSV(30,255,255));
}



void loop()
{
	//// TEMP_CL
	//for( int j = 0; j < NUM_LEDS; ++j)
	//{
	//	leds[j] = ColorFromPalette( gPal, random8(0, 240));
	//}
	//FastLED.show(); // display this frame
	//FastLED.delay(1000 / FRAMES_PER_SECOND);
	//return;




	// Add entropy to random number generator; we use a lot of it.
	// TEMP_CL random16_add_entropy( random());

	memcpy(last_heat, heat, NUM_LEDS);
	UpdateHeat(); // run simulation frame, using palette colors
  
	for(int i = 0; i < NUM_INTERP_FRAMES; ++i)
	{
		//byte yHeatAdd = 64;

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
		FastLED.delay(1000 / FRAMES_PER_SECOND);
	}
}



void UpdateHeat()
{
	//// TEMP_CL - just do random
	//for( int i = 0; i < NUM_LEDS; i++)
	//{
	//	heat[i] = random8(0, 255);
	//}
	//return;
  

	// Cool down every cell a little
	for(int i = 0; i < NUM_LEDS; i++)
	{
		heat[i] = qsub8( heat[i],  random8(COOLING_MIN, COOLING_MAX));
	}
	
	// Save a copy for bluring
 	// TEMP_CL i think I'm running out of memory
	//memcpy(prev_heat, heat, NUM_LEDS);
 
	// Heat from each cell drifts out
	//// TEMP_CL - assume there are only 3 for now
	//heat[0] = (heat[0]*3 + heat[1]*2 + heat[2]) / 6;
	//heat[1] = (heat[1]*3 + heat[1]*2 + heat[2]*2) / 7;
	//heat[2] = (heat[2]*3 + heat[1]*2 + heat[0]) / 6;
	for(int i = 1; i < NUM_LEDS-1; i++)
	{
		// TEMP_CL i think I'm running out of memory
		//heat[i] = (prev_heat[i]*3 + prev_heat[i-1]*2 + prev_heat[i+1]*2) / 7;
		heat[i] = (heat[i]*3 + heat[i-1]*2 + heat[i+1]*2) / 7;
	}
	
	// Randomly ignite new 'sparks' of heat - per 10 leds
	for(int i = 0; i < NUM_LEDS / 10; i++) {
		if( random8() < SPARKING )
		{
			int y = random8(0, NUM_LEDS - SPARK_WIDTH);

			byte newHeat = random8(SPARK_HEAT_MIN, SPARK_HEAT_MAX);
			for(int j = 0; j < SPARK_WIDTH; ++j) {
				heat[y+j] = qadd8( heat[y+j],  newHeat);
			}
			//byte halfHeat = newHeat >> 1;
			//heat[y-1] = qadd8( heat[y], halfHeat );
			//heat[y+1] = qadd8( heat[y], halfHeat );
		}
	}
}
