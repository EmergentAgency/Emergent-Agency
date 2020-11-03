// ScienceClouds

// LED setup
#include <WS2812Serial.h>
#define USE_WS2812SERIAL
//#define FASTLED_ALLOW_INTERRUPTS 1
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

// Radar setup
#include <avr/io.h>
#include <avr/interrupt.h>



// LED vars
#define DATA_PIN 17

#define NUM_LEDS_PER_STRIP 250 // 70 is the max the 150 led strip in the bottles seems to work past...
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

//// Sign wave
//AudioSynthWaveform    waveform1;
//AudioOutputI2S        i2s1;
//AudioConnection       patchCord1(waveform1, 0, i2s1, 0);
//AudioConnection       patchCord2(waveform1, 0, i2s1, 1);
//AudioControlSGTL5000  sgtl5000_1;

//Bounce button0 = Bounce(0, 15);
//Bounce button1 = Bounce(1, 15);
//Bounce button2 = Bounce(2, 15);
//
//int count=1;
//int a1history=0, a2history=0, a3history=0;

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


// Radar

// Interupt pin for motion sensor
#define INTERUPT_PIN 14

// The motion sensor indicates the speed of the motion detected by oscillating its output pin.
// The faster the pulses, the faster the motion.  The detect the speed, we are counting the
// number of microseconds between rising pulses using a hardware interupt pin.  The interupt
// is called each time a rising pulse is detect and we save off the between the new pulse and
// the last pulse.  All the other code for calculating speed is done outside of the interupt
// because the interupt code needs to be as light weight at possible so it returns execution
// to the main code as quickly as possible.

// This is the last period detected
volatile unsigned long g_iLastPeriodMicro = 0;

// This is the last time the interupt was called
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
	//FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS).setCorrection( TypicalLEDStrip );
	//FastLED.setBrightness( BRIGHTNESS );
	//FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
	
	// This works for the Teensy 4.0
	//FastLED.addLeds<NUM_STRIPS, WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS_PER_STRIP);
	
	// WS2812Serial
	LEDS.addLeds<WS2812SERIAL, DATA_PIN, BRG>(leds, NUM_LEDS_PER_STRIP);


	//gPal = CRGBPalette16(CRGB(64,32,32), CRGB(192,128,64), CRGB(255,192,96), CRGB(255,255,128));
	//gPal = CRGBPalette16(CRGB(64,0,0), CRGB(92,32,0), CRGB(255,64,0), CRGB(255,128,0));
	//gPal = CRGBPalette16(CRGB(8,0,0), CRGB(92,32,0), CRGB(255,64,0), CRGB(255,128,0));
	//gPal = CRGBPalette16(CRGB(0,0,0), CRGB(16,8,0), CRGB(128,32,0), CRGB(255,128,0));
	//gPal = CRGBPalette16(CHSV(0,0,0), CHSV(30,255,16), CHSV(15,255,128), CHSV(30,255,255));
	//gPal = CRGBPalette16(CHSV(0,0,0), CHSV(30,255,16), CHSV(30,255,128), CHSV(30,255,255));
	//gPal = CRGBPalette16(CHSV(0,0,0), CHSV(30,255,16), CHSV(30,255,128), CHSV(30,255,255));
	
	
	// Audio 
	
	//// Sine wave
	//AudioMemory(10);
	//pinMode(0, INPUT_PULLUP);
	//pinMode(1, INPUT_PULLUP);
	//pinMode(2, INPUT_PULLUP);
	//Serial.begin(115200);
	//sgtl5000_1.enable();
	//sgtl5000_1.volume(0.3);
	//waveform1.begin(WAVEFORM_SINE);
	//delay(1000);
	//button0.update();
	//button1.update();
	//button2.update();
	//a1history = analogRead(A1);
	//a2history = analogRead(A2);
	//a3history = analogRead(A3);
	
	// Wave file
	AudioMemory(8);
	sgtl5000_1.enable();
	sgtl5000_1.volume(0.5);
	SPI.setMOSI(SDCARD_MOSI_PIN);
	SPI.setSCK(SDCARD_SCK_PIN);
	if (!(SD.begin(SDCARD_CS_PIN)))
	{
		while (1)
		{
			Serial.println("Unable to access the SD card");
			delay(500);
		}
	}
	delay(1000);

	
	// Radar
	// Attach an Interupt to INTERUPT_PIN for timing period of motion detector input
	pinMode(INTERUPT_PIN, INPUT);
	//attachInterrupt(INTERUPT_PIN, MotionDetectorPulse, RISING);
	attachInterrupt(digitalPinToInterrupt(INTERUPT_PIN), MotionDetectorPulse, RISING);   
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
	// TEMP_CL - Testing x-band
	//int raw_sensor = analogRead(INTERUPT_PIN);
	//Serial.print(micros());
	//Serial.print(",");
	//Serial.println(raw_sensor);
	//
	//// TEMP_CL
	//delay(1);
	//return;
	
	
	//// TEMP_CL
	//for( int j = 0; j < NUM_LEDS; ++j)
	//{
	//	leds[j] = ColorFromPalette( gPal, random8(0, 240));
	//}
	//FastLED.show(); // display this frame
	//FastLED.delay(1000 / FRAMES_PER_SECOND);
	//return;

	
	// Audio
	
	// Sine wave
	//Serial.print("Beep #");
	//Serial.println(count);
	//count = count + 1;
	//waveform1.frequency(440 + heat[0]);
	//waveform1.amplitude(0.9);
	//wait(250);
	//waveform1.amplitude(0);
	//wait(250);
	
	// Wave file
	if (playSdWav1.isPlaying() == false)
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
		
		// Wave file
		sgtl5000_1.volume(g_fSpeedRatio / 2.0 + 0.05);

		
		
		//LEDs
		
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
			
			// Sine way
			//// Add audio for test
			//if(j == 2) 
			//{
			//	waveform1.frequency(440 + lerpHeat * 2);
			//	waveform1.amplitude(g_fSpeedRatio);
			//}
		

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


// TEMP_CL Sine wave
//void wait(unsigned int milliseconds)
//{
//  elapsedMillis msec=0;
//
//  while (msec <= milliseconds) {
//    button0.update();
//    button1.update();
//    button2.update();
//    if (button0.fallingEdge()) Serial.println("Button (pin 0) Press");
//    if (button1.fallingEdge()) Serial.println("Button (pin 1) Press");
//    if (button2.fallingEdge()) Serial.println("Button (pin 2) Press");
//    if (button0.risingEdge()) Serial.println("Button (pin 0) Release");
//    if (button1.risingEdge()) Serial.println("Button (pin 1) Release");
//    if (button2.risingEdge()) Serial.println("Button (pin 2) Release");
//    int a1 = analogRead(A1);
//    int a2 = analogRead(A2);
//    int a3 = analogRead(A3);
//    if (a1 > a1history + 50 || a1 < a1history - 50) {
//      Serial.print("Knob (pin A1) = ");
//      Serial.println(a1);
//      a1history = a1;
//    }
//    if (a2 > a2history + 50 || a2 < a2history - 50) {
//      Serial.print("Knob (pin A2) = ");
//      Serial.println(a2);
//      a2history = a2;
//    }
//    if (a3 > a3history + 50 || a3 < a3history - 50) {
//      Serial.print("Knob (pin A3) = ");
//      Serial.println(a3);
//      a3history = a3;
//    }
//  }
//}



// Interupt called to get time between pulses
void MotionDetectorPulse()
{
	unsigned long iCurTimeMicro = micros();
	
	// HACK - For some reason the interupt is getting called twice is
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
// between this call and the the last interupt.  This is to ensure that if we
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


