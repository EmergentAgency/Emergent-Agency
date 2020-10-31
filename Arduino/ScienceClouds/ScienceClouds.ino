// ScienceClouds

// LED setup
//#define FASTLED_ALLOW_INTERRUPTS 0
//#ifdef __AVR__
//  #include <avr/power.h>
//#endif
#include <FastLED.h>

// Audio setup
#include <Audio.h>
#include <Wire.h>
#include <SD.h>
#include <SPI.h>
#include <SerialFlash.h>
#include <Bounce.h>

// LED vars
#define DATA_PIN 9

#define NUM_LEDS_PER_STRIP 23 // 70 is the max the 150 led strip in the bottles seems to work past...
#define MAX_HEAT 240 // Don't go above 240
#define FRAMES_PER_SECOND 120
#define NUM_INTERP_FRAMES 50

#define NUM_STRIPS 1
#define NUM_LEDS   NUM_LEDS_PER_STRIP  

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


// Audio vars
AudioSynthWaveform    waveform1;
AudioOutputI2S        i2s1;
AudioConnection       patchCord1(waveform1, 0, i2s1, 0);
AudioConnection       patchCord2(waveform1, 0, i2s1, 1);
AudioControlSGTL5000  sgtl5000_1;

Bounce button0 = Bounce(0, 15);
Bounce button1 = Bounce(1, 15);
Bounce button2 = Bounce(2, 15);

int count=1;
int a1history=0, a2history=0, a3history=0;



void setup()
{
	//FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS).setCorrection( TypicalLEDStrip );
	//FastLED.setBrightness( BRIGHTNESS );
	//FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
	
	// This works for the Teensy 4.0
	FastLED.addLeds<NUM_STRIPS, WS2812B, DATA_PIN,GRB>(leds, NUM_LEDS_PER_STRIP);

	//gPal = CRGBPalette16(CRGB(64,32,32), CRGB(192,128,64), CRGB(255,192,96), CRGB(255,255,128));
	//gPal = CRGBPalette16(CRGB(64,0,0), CRGB(92,32,0), CRGB(255,64,0), CRGB(255,128,0));
	//gPal = CRGBPalette16(CRGB(8,0,0), CRGB(92,32,0), CRGB(255,64,0), CRGB(255,128,0));
	//gPal = CRGBPalette16(CRGB(0,0,0), CRGB(16,8,0), CRGB(128,32,0), CRGB(255,128,0));
	//gPal = CRGBPalette16(CHSV(0,0,0), CHSV(30,255,16), CHSV(15,255,128), CHSV(30,255,255));
	//gPal = CRGBPalette16(CHSV(0,0,0), CHSV(30,255,16), CHSV(30,255,128), CHSV(30,255,255));
	//gPal = CRGBPalette16(CHSV(0,0,0), CHSV(30,255,16), CHSV(30,255,128), CHSV(30,255,255));
	
	// Audio 
	AudioMemory(10);
	pinMode(0, INPUT_PULLUP);
	pinMode(1, INPUT_PULLUP);
	pinMode(2, INPUT_PULLUP);
	Serial.begin(115200);
	sgtl5000_1.enable();
	sgtl5000_1.volume(0.3);
	waveform1.begin(WAVEFORM_SINE);
	delay(1000);
	button0.update();
	button1.update();
	button2.update();
	a1history = analogRead(A1);
	a2history = analogRead(A2);
	a3history = analogRead(A3);
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

	//Serial.print("Beep #");
	//Serial.println(count);
	//count = count + 1;
	//waveform1.frequency(440 + heat[0]);
	waveform1.amplitude(0.9);
	//wait(250);
	//waveform1.amplitude(0);
	//wait(250);



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
			
			// Add audio for test
			if(j == 2) 
			{
				waveform1.frequency(440 + lerpHeat * 2);
			}
		

		}

		FastLED.show(); // display this frame
		//FastLED.delay(1000 / FRAMES_PER_SECOND); // This doesnt seem to work on the Teensy 4.0
		delay(1000 / FRAMES_PER_SECOND);
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



void wait(unsigned int milliseconds)
{
  elapsedMillis msec=0;

  while (msec <= milliseconds) {
    button0.update();
    button1.update();
    button2.update();
    if (button0.fallingEdge()) Serial.println("Button (pin 0) Press");
    if (button1.fallingEdge()) Serial.println("Button (pin 1) Press");
    if (button2.fallingEdge()) Serial.println("Button (pin 2) Press");
    if (button0.risingEdge()) Serial.println("Button (pin 0) Release");
    if (button1.risingEdge()) Serial.println("Button (pin 1) Release");
    if (button2.risingEdge()) Serial.println("Button (pin 2) Release");
    int a1 = analogRead(A1);
    int a2 = analogRead(A2);
    int a3 = analogRead(A3);
    if (a1 > a1history + 50 || a1 < a1history - 50) {
      Serial.print("Knob (pin A1) = ");
      Serial.println(a1);
      a1history = a1;
    }
    if (a2 > a2history + 50 || a2 < a2history - 50) {
      Serial.print("Knob (pin A2) = ");
      Serial.println(a2);
      a2history = a2;
    }
    if (a3 > a3history + 50 || a3 < a3history - 50) {
      Serial.print("Knob (pin A3) = ");
      Serial.println(a3);
      a3history = a3;
    }
  }
}

