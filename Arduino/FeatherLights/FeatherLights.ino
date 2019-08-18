/**
 * File: FeatherLights.ino
 *
 * Description: Motion reactive NeoPixels for Priceless beach stage with Calli.
 * Used FastLED
 * Based on Somatone which was based on EaMidiNodesNeoPixel which was based on 
 * EaMidiNodesRS485 but uses an array of NeoPixels instead of individual LEDs.
 *
 * Copyright: 2019 Chris Linder
 */
 
#define FASTLED_ALLOW_INTERRUPTS 0
#ifdef __AVR__
  #include <avr/power.h>
#endif
#include <FastLED.h>
#include <EEPROM.h>

// Use Serial to print out debug statements
static bool USE_SERIAL_FOR_DEBUGGING = true;

// Number of nodes
#define NUM_NODES 7

// The index for this node.  If it is < 0, use the node index stored in
// EEPROM.  If >= 0, store that node index in the EEPROM of this node.
int g_iNodeIndex = -1;

// Set this to 'true' is this is being run with a short strip of LEDs for home testing.  'false' for the full setup.
#define SHORT_LED_STRIP false

// Number of LEDs
#if SHORT_LED_STRIP
#define NUM_NEO_PIXELS 10
#else
#define NUM_NEO_PIXELS 300
#endif

// The address in EEPROM for various saved values
static const int EEPROM_ADDR_NODE_INDEX = 0;
static const int EEPROM_ADDR_DATA_VERSION = 21;
static const int EEPROM_ADDR_MIN_SPEED = 22;
static const int EEPROM_ADDR_MAX_SPEED = 24;
static const int EEPROM_ADDR_NEW_SPEED_WEIGHT = 26;
static const int EEPROM_ADDR_INPUT_EXPONENT = 28;

// The current data version of the EEPROM data.  Each time the format changes, increment this
static const int CUR_EEPROM_DATA_VERSION = 2;

// If this is true, we use RS-485 to send signals.
static const bool USE_RS485 = true;



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

// Clamp function - int
int ClampI(int iVal, int iMin, int iMax)
{
	if(iVal < iMin)
	{
		iVal = iMin;
	}
	else if(iVal > iMax)
	{
		iVal = iMax;
	}

	return iVal;
}



// FastLEDs

// Allotted time in microsecond for LED processing.  Frequently the LEDs code will take
// different amounts time to run.  This causes changes in the input smoothing code
//
#define ALLOTTED_LED_TIME_MICRO 8000

// Data pin for Adafruit_NeoPixel
#define LED_DATA_PIN 40

// Create led and head arrays
CRGB g_LEDs[NUM_NEO_PIXELS];
byte g_ayHeat[NUM_NEO_PIXELS];
byte g_ayHeat2[NUM_NEO_PIXELS];
// TEMP_CL i think I'm running out of memory
byte g_ayPrevHeat[NUM_NEO_PIXELS]; // Used for frame blending
byte g_ayLast_heat[NUM_NEO_PIXELS]; // Used for interp between heat frames

// Trying more yellow
// Fire color - bright for club
DEFINE_GRADIENT_PALETTE( g_Heatmap_gp ) {
//  0,     8,  2,  0,   
//50,     64, 16,  0,   
//100,   128, 64,  8,   
//200,   255,128, 64,   
//240,   255,200,100, 
//255,   255,255,255 }; //junk value
//  0,    4,  0,  0,   
//50,     70, 25,  0,   
//100,   128, 64,  8,   
//200,   255,128, 64,   
//240,   255,200, 80, 
//255,   255,255,255 }; //junk value
  0,     0,  4,  8,   
 50,     0, 25, 70,   
100,    96, 64,128,   
140,   100,100,200,   
240,   255, 80,210, 
255,   255,255,255 }; //junk value

DEFINE_GRADIENT_PALETTE( g_Heatmap2_gp ) {
  0,     0,  0,  0,   
50,     70, 25,  0,   
100,   128, 64,  4,   
200,   255,128,  8,   
240,   255,200, 16, 
255,   255,255,255 }; //junk value

// Create palette var
//CRGBPalette16 g_Pal = g_Heatmap_gp;
CRGBPalette256 g_Pal = g_Heatmap_gp;
CRGBPalette256 g_Pal2 = g_Heatmap2_gp;


#define MAX_HEAT 240 // Don't go above 240
#define ADJUST_CONTRAST_ON_MOVEMENT false
#define MODE 3


// Set slowest intermp num frames 
#define MAX_INTERP_FRAMES 60
#define MIN_INTERP_FRAMES 10

// Current interp frame
int g_iCurInterpFrame = 0;



// Used for delta time
unsigned long g_iLastTimeMS;

// Distance (as ratio from start 0.0 to end 1.0) that it takes to go from 0.0 intensity to 1.0 intensity 
#define FADE_DISTANCE 1.5



// Interupt pin for motion sensor
static const int INT_PIN = 19; // INT7 (interupt 7)

// This is the number of init loops to code does to test LEDs
static const int NUM_INIT_LOOPS = 2;

// Recieve data start byte and end byte
static const int START_RCV_BYTE = 240; // Binary = 11110000 (this also translates to node 7 sending 16 (out of 32) as a motion value).
static const int END_RCV_BYTE = 241; // Binary = 11110001 (this also translates to node 7 sending 17 (out of 32) as a motion value).

// Recieve data timeout counter.  Use this to avoid getting stuck waiting for data.
static const int RCV_TIMEOUT_MAX_COUNT = 1000;

// The hardware serial port used for communication (either RS-485 or XBee)
HardwareSerial Uart = HardwareSerial();

// Hardware serial settings
static const int COM_BAUD_RATE = 9600;

// RS-485 specific settings
static const int MICRO_SECOND_DELAY_POST_WRITE_RS485 = 1000000 * 1 / (COM_BAUD_RATE/10) * 2; // formula from http://www.gammon.com.au/forum/?id=11428
static const int RS485_ENABLE_WRITE_PIN = 22;

// The motion sensor indicates the speed of the motion detected by oscillating its output pin.
// The faster the pulses, the faster the motion.  The detect the speed, we are counting the
// number of microseconds between rising pulses using a hardware interupt pin.  The interupt
// is called each time a rising pulse is detect and we save off the between the new pulse and
// the last pulse.  All the other code for calculating speed is done outside of teh interupt
// because the interupt code needs to be as light weight at possible so it returns execution
// to the main code as quickly as possible.

// This is the last period detected
volatile unsigned long g_iLastPeriodMicro = 0;

// This is the last time the interupt was called
volatile unsigned long g_iLastTimeMicro = 0;

// The number of tuning vars there are per node.  These are sent from the PC program.
static int NUM_TUNING_VARS = 4;

// Tuning - The min speed in meters per second to respond to.  Any motion at or below this will be 
// considered no motion at all.
float fMinSpeed = 0.02;

// Tuning The max speed in meters per second.  All motion above this speed will be treated like this speed
float fMaxSpeed = 0.22;

// Tuning - This is the speed smoothing factor (0, 1.0].  Low values mean more smoothing while a value of 1 means 
// no smoothing at all.  This value depends on the loop speed so if anything changes the loop speed,
// the amount of smoothing will change (see LOOP_DELAY_MS).
//static float fNewSpeedWeight = 0.15;
float fNewSpeedWeight = 0.05;

// Tuning - The exponent to apply to the linear 0.0 to 1.0 value from the sensor.  This allows the sensitivity curve
// to be adjusted in a non-linear fashion.
float fInputExponent = 1.0;

// This is the outout speed ratio [0, 1.0].  It is based on the speed read from the motion detector
// and fMinSpeed, fMaxSpeed, fNewSpeedWeight.
float g_fSpeedRatio;

// Settings needed for timing out nodes in case some disconnect, break, or a signal is lost.
// These could probably be lower but right now I want to bias towards waiting more to make sure
// all communication works as opposed to having things still be fast when nodes are removed.
// This means running a partial node setup isn't a really good idea.
static const int NODE_COM_TIMEOUT_MS = 30; 
static const int PC_COM_TIMEOUT_MS = 70;
int g_iNextNodeIndex = 0;
unsigned long g_iLastReceiveTime = 0;



void DebugLog(const char* sLog)
{
	if(USE_SERIAL_FOR_DEBUGGING)
	{
		Serial.println(sLog);
	}
}

void DebugLog(const char* sLog, float f)
{
	if(USE_SERIAL_FOR_DEBUGGING)
	{
		Serial.print(sLog);
		Serial.println(f);
	}
}

void DebugLog(const char* sLog, int i)
{
	if(USE_SERIAL_FOR_DEBUGGING)
	{
		Serial.print(sLog);
		Serial.println(i);
	}
}



void setup()
{
	if(USE_SERIAL_FOR_DEBUGGING)
	{
		// setup debugging serial port (over USB cable) at 9600 bps
		Serial.begin(9600);
		Serial.println("EAMidiNodes - Setup");
	}

	// setup Uart
	Uart.begin(COM_BAUD_RATE);
	if(USE_RS485)
	{
		pinMode(RS485_ENABLE_WRITE_PIN, OUTPUT);
		digitalWrite(RS485_ENABLE_WRITE_PIN, LOW); 
	}

	// Deal with the index for this node.
	// If g_iNodeIndex is < 0, use the node index stored in
	// EEPROM.  If >= 0, store that node index in the EEPROM of this node.
	if(g_iNodeIndex < 0)
	{
		g_iNodeIndex = EEPROM.read(EEPROM_ADDR_NODE_INDEX);
		if(g_iNodeIndex == 255)
		{
			if(USE_SERIAL_FOR_DEBUGGING)
			{
				Serial.println("EAMidiNodes - tried to read node index from EEPROM but failed to get valid value.  Using 0.");
			}
			g_iNodeIndex = 0;
		}
	}
	else
	{
		EEPROMWrite(EEPROM_ADDR_NODE_INDEX, g_iNodeIndex);
	}

	// Read in settings from EPROM
	ReadSettingsFromEEPROM();


	// Init NeoPixel
	FastLED.addLeds<NEOPIXEL, LED_DATA_PIN>(g_LEDs, NUM_NEO_PIXELS);

 	// Do a simple startup sequence to test LEDs
	static const int iNumLEDsToSkip = NUM_NEO_PIXELS > 10 ? 3 : 1;
	for(int nTest = 0; nTest < NUM_INIT_LOOPS; nTest++)
	{
		// Neo pixels
		int nLED;
		for(nLED = 0; nLED < NUM_NEO_PIXELS/iNumLEDsToSkip; nLED++)
		{
			g_LEDs[nLED*iNumLEDsToSkip] = CRGB::White;
		}
		FastLED.show();
		delay(500);
		for(nLED = g_iNodeIndex; nLED < NUM_NEO_PIXELS/iNumLEDsToSkip; nLED++)
		{
			g_LEDs[nLED*iNumLEDsToSkip] = CRGB::Black;
		}
		FastLED.show();

		delay(500);
	}

	// Setup interupt
	pinMode(INT_PIN, INPUT);
	attachInterrupt(INT_PIN, MotionDetectorPulse, RISING);   // Attach an Interupt to INT_PIN for timing period of motion detector input
}



void ReadSettingsFromEEPROM()
{
	int ySavedDataVersion = EEPROM.read(EEPROM_ADDR_DATA_VERSION);
	int ySavedMinSpeedU = EEPROM.read(EEPROM_ADDR_MIN_SPEED);
	int ySavedMinSpeedL = EEPROM.read(EEPROM_ADDR_MIN_SPEED+1);
	int ySavedMaxSpeedU = EEPROM.read(EEPROM_ADDR_MAX_SPEED);
	int ySavedMaxSpeedL = EEPROM.read(EEPROM_ADDR_MAX_SPEED+1);
	int ySavedNewSpeedWeightU = EEPROM.read(EEPROM_ADDR_NEW_SPEED_WEIGHT);
	int ySavedNewSpeedWeightL = EEPROM.read(EEPROM_ADDR_NEW_SPEED_WEIGHT+1);
	int ySavedInputExponentU = EEPROM.read(EEPROM_ADDR_INPUT_EXPONENT);
	int ySavedInputExponentL = EEPROM.read(EEPROM_ADDR_INPUT_EXPONENT+1);
	if( ySavedDataVersion != CUR_EEPROM_DATA_VERSION )
	{
		// Invalid saved settings
		if(USE_SERIAL_FOR_DEBUGGING)
		{
			Serial.print("EAMidiNodes - tried to read settings from EEPROM but failed.  Read in data version ");
			Serial.println(ySavedDataVersion);
		}
	}
	else
	{
		if(USE_SERIAL_FOR_DEBUGGING)
		{
			Serial.print("EAMidiNodes - Read in data version ");
			Serial.println(ySavedDataVersion);
			Serial.println(ySavedMinSpeedU);
			Serial.println(ySavedMinSpeedL);
			Serial.println(ySavedMaxSpeedU);
			Serial.println(ySavedMaxSpeedL);
			Serial.println(ySavedNewSpeedWeightU);
			Serial.println(ySavedNewSpeedWeightL);
			Serial.println(ySavedInputExponentU);
			Serial.println(ySavedInputExponentL);
		}

		// Update fMinSpeed. This has a range from 0.0 to 2.0 stored 2 bytes
		int iNewMinSpeed = (ySavedMinSpeedU << 8) + ySavedMinSpeedL;
		fMinSpeed = iNewMinSpeed / 65535.0 * 2.0;

		// Update fMaxSpeed. This has a range from 0.0 to 2.0 stored 2 bytes
		int iNewMaxSpeed = (ySavedMaxSpeedU << 8) + ySavedMaxSpeedL;
		fMaxSpeed = iNewMaxSpeed / 65535.0 * 2.0;

		// Update fNewSpeedWeight with new value. This has a range from 0.0 to 1.0 stored 2 bytes 
		int iNewWeight = (ySavedNewSpeedWeightU << 8) + ySavedNewSpeedWeightL;
		fNewSpeedWeight = iNewWeight / 65535.0;

		// Update fInputExponent with new value. This has a range from 0.0 to 1.0 stored 2 bytes 
		int iInputExponent = (ySavedInputExponentU << 8) + ySavedInputExponentL;
		fInputExponent = iInputExponent / 65535.0 * 5.0;

		if(USE_SERIAL_FOR_DEBUGGING)
		{
			Serial.println("Settings from EEPROM:");
			Serial.print("fMinSpeed=");
			Serial.println(fMinSpeed);
			Serial.print("fMaxSpeed=");
			Serial.println(fMaxSpeed);
			Serial.print("fNewSpeedWeight*10=");
			Serial.println(fNewSpeedWeight*10);
			Serial.print("fInputExponent*10=");
			Serial.println(fInputExponent*10);
		}
	}
}


// Interupt called to get time between pulses
void MotionDetectorPulse()
{
	unsigned long iCurTimeMicro = micros();
	g_iLastPeriodMicro = iCurTimeMicro - g_iLastTimeMicro;
	g_iLastTimeMicro = iCurTimeMicro;
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



void EEPROMWrite(int iAddr, byte yValue)
{
	int yCurValue = EEPROM.read(iAddr);
	if(yCurValue != (int)yValue)
	{
		EEPROM.write(iAddr, yValue);
		if(USE_SERIAL_FOR_DEBUGGING)
		{
			Serial.println("WROTE TO EEPROM! The EEPROM is now slightly closer to death.");
		}
	}
}



bool ReceiveTuningMessage()
{
	if(USE_SERIAL_FOR_DEBUGGING)
	{
		Serial.println("##### ReceiveTuningMessage ######");
	}

	// Once we have the start of a message, delay until we have it all
	int iTimeoutCounter = 0;
	int iNumExpectedBytes = NUM_TUNING_VARS * 2 * NUM_NODES + 1; // 3 tuning vars * 2 bytes per var * number of nodes + the end byte
	while(Uart.available() < iNumExpectedBytes && iTimeoutCounter < RCV_TIMEOUT_MAX_COUNT)
	{
		iTimeoutCounter++;
		delay(1);
	}

	// Return false if we hit the timeout max
	if(iTimeoutCounter >= RCV_TIMEOUT_MAX_COUNT)
	{
		if(USE_SERIAL_FOR_DEBUGGING)
		{
			Serial.print("ReceiveTuningMessage timeout: iNumExpectedBytes=");
			Serial.print(iNumExpectedBytes);
			Serial.print(" available=");
			Serial.println(Uart.available());
		}

		return false;
	}

	// Read data
	int iNewMinSpeedU = -1;
	int iNewMinSpeedL = -1;
	int iNewMaxSpeedU = -1;
	int iNewMaxSpeedL = -1;
	int iNewWeightU = -1;
	int iNewWeightL = -1;
	int iNewInputExponentU = -1;
	int iNewInputExponentL = -1;
	for(int i = 0; i < NUM_NODES; i++)
	{
		// If this is our node, read in the values and save them
		if(i == g_iNodeIndex)
		{
			iNewMinSpeedU      = Uart.read();
			iNewMinSpeedL      = Uart.read();
			iNewMaxSpeedU      = Uart.read();
			iNewMaxSpeedL      = Uart.read();
			iNewWeightU        = Uart.read();
			iNewWeightL        = Uart.read();
			iNewInputExponentU = Uart.read();
			iNewInputExponentL = Uart.read();
		}
		// Otherwise, just read and ignore the results
		else
		{
			Uart.read();
			Uart.read();
			Uart.read();
			Uart.read();
			Uart.read();
			Uart.read();
			Uart.read();
			Uart.read();
		}
	}

	// Return false if we don't hit the correct end byte
	int iEndByte = Uart.read();
	if(iEndByte != END_RCV_BYTE)
	{
		if(USE_SERIAL_FOR_DEBUGGING)
		{
			Serial.print("Didn't get valid End Byte. iEndByte=");
			Serial.println(iEndByte);
		}

		return false;
	}

	// Debug saying we got a valid message
	// This works for both the NeoPixels and the LEDs
	for(int nLED = 0; nLED < NUM_NEO_PIXELS; nLED++)
	{
		g_LEDs[nLED] = CRGB::Black;
	}
	FastLED.show();
	for(int i = 0; i < 2; i++)
	{
		for(int j = NUM_NEO_PIXELS - 1; j >= 0; j--)
		{
			g_LEDs[j] = CRGB::Grey;
			FastLED.show();
			delay(10);
		}
		delay(200);
	}

	if(USE_SERIAL_FOR_DEBUGGING)
	{
		Serial.println("Get new settings from network:");
		Serial.println(iNewMinSpeedU);
		Serial.println(iNewMinSpeedL);
		Serial.println(iNewMaxSpeedU);
		Serial.println(iNewMaxSpeedL);
		Serial.println(iNewWeightU);
		Serial.println(iNewWeightL);
		Serial.println(iNewInputExponentU);
		Serial.println(iNewInputExponentL);
	}

	// Save out settings to EEPROM
	EEPROMWrite(EEPROM_ADDR_DATA_VERSION, CUR_EEPROM_DATA_VERSION);
	EEPROMWrite(EEPROM_ADDR_MIN_SPEED,   iNewMinSpeedU);
	EEPROMWrite(EEPROM_ADDR_MIN_SPEED+1, iNewMinSpeedL);
	EEPROMWrite(EEPROM_ADDR_MAX_SPEED,   iNewMaxSpeedU);
	EEPROMWrite(EEPROM_ADDR_MAX_SPEED+1, iNewMaxSpeedL);
	EEPROMWrite(EEPROM_ADDR_NEW_SPEED_WEIGHT,   iNewWeightU);
	EEPROMWrite(EEPROM_ADDR_NEW_SPEED_WEIGHT+1, iNewWeightL);
	EEPROMWrite(EEPROM_ADDR_INPUT_EXPONENT,   iNewInputExponentU);
	EEPROMWrite(EEPROM_ADDR_INPUT_EXPONENT+1, iNewInputExponentL);

	// Read data just written to EEPROM as new settings
	ReadSettingsFromEEPROM();

	return true;
}



/* TEMP_CL - Removing old neopixel code
class Color GetIntensityColor(float fIntensity)
{
	Color cOut;

	if(fIntensity <= 0.01)
	{
		cOut = g_acIntensityColors[g_iNodeIndex][0];
	}
	else
	{
		int iHighInterpPoint = 1;
		for(iHighInterpPoint = 1; iHighInterpPoint < NUM_INTENSITY_POINTS; iHighInterpPoint++)
		{
			if(fIntensity <= g_afIntensityPoint[iHighInterpPoint])
			{
				break;
			}
		}
		int iLowInterpPoint = iHighInterpPoint-1;
		float fInterp = (fIntensity - g_afIntensityPoint[iLowInterpPoint]) / (g_afIntensityPoint[iHighInterpPoint] - g_afIntensityPoint[iLowInterpPoint]);
		cOut = g_acIntensityColors[g_iNodeIndex][iLowInterpPoint];
		cOut.Mult(1.0 - fInterp);
		Color c2 = g_acIntensityColors[g_iNodeIndex][iHighInterpPoint];
		c2.Mult(fInterp);
		cOut.Add(c2);
	}

	return cOut;
}
*/



//#define COOLING_MIN  5
//#define COOLING_MAX  15
//#define SPARKING 130
//#define SPARK_HEAT_MIN 50
//#define SPARK_HEAT_MAX 100
//#define SPARK_WIDTH 2

void UpdateHeat0(float fDisplaySpeedRatio, int iDeltaTimeMS)
{
	static const int COOLING_MIN = 5;
	static const int COOLING_MAX = 15;
	static const int SPARKING = 130;
	static const int SPARK_HEAT_MIN = 50;
	static const int SPARK_HEAT_MAX = 100;
	static const int SPARK_WIDTH = 2;
	
	//// TEMP_CL - just do random
	//for( int i = 0; i < NUM_NEO_PIXELS; i++)
	//{
	//	g_ayHeat[i] = random8(0, 255);
	//}
	//return;
  

	// Cool down every cell a little
	for(int i = 0; i < NUM_NEO_PIXELS; i++)
	{
		g_ayHeat[i] = qsub8( g_ayHeat[i],  random8(COOLING_MIN, COOLING_MAX));
	}
	
	// Save a copy for bluring
 	// TEMP_CL i think I'm running out of memory
	//memcpy(prev_heat, heat, NUM_NEO_PIXELS);
 
	// Heat from each cell drifts out
	//// TEMP_CL - assume there are only 3 for now
	//g_ayHeat[0] = (g_ayHeat[0]*3 + g_ayHeat[1]*2 + g_ayHeat[2]) / 6;
	//g_ayHeat[1] = (g_ayHeat[1]*3 + g_ayHeat[1]*2 + g_ayHeat[2]*2) / 7;
	//g_ayHeat[2] = (g_ayHeat[2]*3 + g_ayHeat[1]*2 + g_ayHeat[0]) / 6;
	for(int i = 1; i < NUM_NEO_PIXELS-1; i++)
	{
		// TEMP_CL i think I'm running out of memory
		//g_ayHeat[i] = (prev_heat[i]*3 + prev_heat[i-1]*2 + prev_heat[i+1]*2) / 7;
		g_ayHeat[i] = (g_ayHeat[i]*3 + g_ayHeat[i-1]*2 + g_ayHeat[i+1]*2) / 7;
	}
	
	// Randomly ignite new 'sparks' of heat - per 10 leds
	for(int i = 0; i < NUM_NEO_PIXELS / 10; i++) {
		if( random8() < SPARKING )
		{
			int y = random16(0, NUM_NEO_PIXELS - SPARK_WIDTH);

			byte newHeat = random8(SPARK_HEAT_MIN, SPARK_HEAT_MAX);
			for(int j = 0; j < SPARK_WIDTH; ++j) {
				g_ayHeat[y+j] = qadd8( g_ayHeat[y+j],  newHeat);
			}
			//byte halfHeat = newHeat >> 1;
			//g_ayHeat[y-1] = qadd8( g_ayHeat[y], halfHeat );
			//g_ayHeat[y+1] = qadd8( g_ayHeat[y], halfHeat );
		}
	}
}



void UpdateHeat1(float fDisplaySpeedRatio, int iDeltaTimeMS)
{	
	static const int SPARK_WIDTH = 2;
	
	// Save a copy for bluring
 	// TEMP_CL i think I'm running out of memory
	memcpy(g_ayPrevHeat, g_ayHeat, NUM_NEO_PIXELS);
 
	// Heat from each cell drifts out
	#define BLUR_HALF_WIDTH 2
	//static const fract8 yBlurScale[] = {16, 32, 159, 32, 16};
	//static const fract8 yBlurScale[] = {8, 32, 112, 64, 32};
	//static const fract8 yBlurScale[] = {7, 30, 100, 60, 30};
	static const fract8 yBlurScale[] = {7, 30, 60, 100, 30};
	for(int i = BLUR_HALF_WIDTH; i < NUM_NEO_PIXELS-BLUR_HALF_WIDTH; i++)
	{
		g_ayHeat[i] = 
			scale8(g_ayPrevHeat[i-2], yBlurScale[0]) + 
			scale8(g_ayPrevHeat[i-1], yBlurScale[1]) +
			scale8(g_ayPrevHeat[i-0], yBlurScale[2]) +
			scale8(g_ayPrevHeat[i+1], yBlurScale[3]) +
			scale8(g_ayPrevHeat[i+2], yBlurScale[4]);
	}
	
	// Ignite new 'sparks' of heat - per 100 leds
	#define MAX_SPARKS_PER_SEC 20.0
	#define MIN_SPARKS_PER_SEC 0.5
	byte yNewSparkHeat = 120 + byte(120 * fDisplaySpeedRatio);
	//float fSParksPerSecond = (fDisplaySpeedRatio * MAX_SPARKS_PER_SEC * NUM_NEO_PIXELS / 100.0) + 0.5; // TEMP_CL - fix
	float fSParksPerSecond = (fDisplaySpeedRatio * (MAX_SPARKS_PER_SEC - MIN_SPARKS_PER_SEC) + MIN_SPARKS_PER_SEC) *
							 NUM_NEO_PIXELS / 100.0;
	int iSparkTimeMS = int(1000 / fSParksPerSecond);
	static int iTimeSinceSparkMS = 0;
	iTimeSinceSparkMS += iDeltaTimeMS;
	while(iTimeSinceSparkMS > iSparkTimeMS)
	{
		iTimeSinceSparkMS -= iSparkTimeMS;
		
		int y = random16(BLUR_HALF_WIDTH, NUM_NEO_PIXELS - SPARK_WIDTH - BLUR_HALF_WIDTH);

		byte newHeat = yNewSparkHeat;
		for(int j = 0; j < SPARK_WIDTH; ++j) {
			g_ayHeat[y+j] = qadd8( g_ayHeat[y+j],  newHeat);
		}

	}
}



void UpdateHeat2(float fDisplaySpeedRatio, int iDeltaTimeMS)
{	
	//// Save a copy for bluring
 	//// TEMP_CL i think I'm running out of memory
	//memcpy(g_ayPrevHeat, g_ayHeat, NUM_NEO_PIXELS);
    //
	//// Heat from each cell drifts out
	//#define BLUR_HALF_WIDTH 2
	////static const fract8 yBlurScale[] = {16, 32, 159, 32, 16};
	////static const fract8 yBlurScale[] = {8, 32, 112, 64, 32};
	////static const fract8 yBlurScale[] = {7, 30, 100, 60, 30};
	//static const fract8 yBlurScale[] = {7, 30, 60, 100, 30};
	//for(int i = BLUR_HALF_WIDTH; i < NUM_NEO_PIXELS-BLUR_HALF_WIDTH; i++)
	//{
	//	g_ayHeat[i] = 
	//		scale8(g_ayPrevHeat[i-2], yBlurScale[0]) + 
	//		scale8(g_ayPrevHeat[i-1], yBlurScale[1]) +
	//		scale8(g_ayPrevHeat[i-0], yBlurScale[2]) +
	//		scale8(g_ayPrevHeat[i+1], yBlurScale[3]) +
	//		scale8(g_ayPrevHeat[i+2], yBlurScale[4]);
	//}
	
	// Increase brightness for everything based on fDisplaySpeedRatio
	//byte ySpeedAdd = byte(230 * fDisplaySpeedRatio) + 10;
	byte ySpeedAdd = byte(240 * fDisplaySpeedRatio);
	for(int i = 0; i < NUM_NEO_PIXELS; i++)
	{
		g_ayHeat[i] = ySpeedAdd;
	}
	
	//// Ignite new 'sparks' of heat - per 100 leds
	//#define MAX_SPARKS_PER_SEC 20.0
	//byte yNewSparkHeat = 120 + byte(120 * fDisplaySpeedRatio);
	//float fSParksPerSecond = (fDisplaySpeedRatio * MAX_SPARKS_PER_SEC * NUM_NEO_PIXELS / 100.0) + 0.5;
	//int iSparkTimeMS = int(1000 / fSParksPerSecond);
	//static int iTimeSinceSparkMS = 0;
	//iTimeSinceSparkMS += iDeltaTimeMS;
	//while(iTimeSinceSparkMS > iSparkTimeMS)
	//{
	//	iTimeSinceSparkMS -= iSparkTimeMS;
	//	
	//	int y = random8(BLUR_HALF_WIDTH, NUM_NEO_PIXELS - SPARK_WIDTH - BLUR_HALF_WIDTH);
    //
	//	byte newHeat = yNewSparkHeat;
	//	for(int j = 0; j < SPARK_WIDTH; ++j) {
	//		g_ayHeat[y+j] = qadd8( g_ayHeat[y+j],  newHeat);
	//	}
    //
	//}
}



void UpdateHeat3(float fDisplaySpeedRatio, int iDeltaTimeMS)
{
	static const int COOLING_MIN = 5;
	static const int COOLING_MAX = 15;
	static const int SPARKING = 100;
	static const int SPARK_HEAT_MIN = 50;
	static const int SPARK_HEAT_MAX = 100;
	static const int SPARK_WIDTH = 3;
	
	//// TEMP_CL - just do random
	//for( int i = 0; i < NUM_NEO_PIXELS; i++)
	//{
	//	g_ayHeat[i] = random8(0, 255);
	//}
	//return;
  

	// Cool down every cell a little
	for(int i = 0; i < NUM_NEO_PIXELS; i++)
	{
		g_ayHeat[i] = qsub8( g_ayHeat[i],  random8(COOLING_MIN, COOLING_MAX));
	}
	
	// Save a copy for bluring
 	// TEMP_CL i think I'm running out of memory
	//memcpy(prev_heat, heat, NUM_NEO_PIXELS);
 
	// Heat from each cell drifts out
	//// TEMP_CL - assume there are only 3 for now
	//g_ayHeat[0] = (g_ayHeat[0]*3 + g_ayHeat[1]*2 + g_ayHeat[2]) / 6;
	//g_ayHeat[1] = (g_ayHeat[1]*3 + g_ayHeat[1]*2 + g_ayHeat[2]*2) / 7;
	//g_ayHeat[2] = (g_ayHeat[2]*3 + g_ayHeat[1]*2 + g_ayHeat[0]) / 6;
	for(int i = 1; i < NUM_NEO_PIXELS-1; i++)
	{
		// TEMP_CL i think I'm running out of memory
		//g_ayHeat[i] = (prev_heat[i]*3 + prev_heat[i-1]*2 + prev_heat[i+1]*2) / 7;
		g_ayHeat[i] = (g_ayHeat[i]*3 + g_ayHeat[i-1]*2 + g_ayHeat[i+1]*2) / 7;
	}
	
	// Randomly ignite new 'sparks' of heat - per 10 leds
	for(int i = 0; i < NUM_NEO_PIXELS / 10; i++) {
		if( random8() < SPARKING )
		{
			int y = random16(0, NUM_NEO_PIXELS - SPARK_WIDTH);

			byte newHeat = random8(SPARK_HEAT_MIN, SPARK_HEAT_MAX);
			for(int j = 0; j < SPARK_WIDTH; ++j) {
				g_ayHeat[y+j] = qadd8( g_ayHeat[y+j],  newHeat);
			}
			//byte halfHeat = newHeat >> 1;
			//g_ayHeat[y-1] = qadd8( g_ayHeat[y], halfHeat );
			//g_ayHeat[y+1] = qadd8( g_ayHeat[y], halfHeat );
		}
	}
}


#define NUM_SPARKS 20
void UpdateHeat4(float fDisplaySpeedRatio, int iDeltaTimeMS)
{
	//static const float fSpeedMin = 0.75;
	//static const float fSpeedMax = 2.5;
	//static const float fSpeedDiff = fSpeedMax - fSpeedMin;
	//static const float fSpeeds[5] = {0.5, 0.75, 1.0, 1.25, 1.5};
	//static const float fSpeeds[5] = {1.5, 1.0, 1.0, 2, 2};
	static const float fSpeeds[5] = {1.0, 1.5, 2.0, 2.5, 3.0};
	static const int iFade = 50;
	
	static float afPositions[NUM_SPARKS];
//	= {NUM_NEO_PIXELS, NUM_NEO_PIXELS, NUM_NEO_PIXELS, NUM_NEO_PIXELS};
	static float afSpeeds[NUM_SPARKS];
	
	// Fade
	for(int i = 0; i < NUM_NEO_PIXELS; i++)
	{
		g_ayHeat2[i] = qsub8( g_ayHeat2[i],  iFade);
	}
	
	// Spawn new sparks based on movement
	static int iNextSpark = 0;
	//if(random8() / 255.0 < (fDisplaySpeedRatio - 0.25) * 0.1)
	static int iNextSparkCountdown = 100;
	if(fDisplaySpeedRatio > 0.75)
	{
		iNextSparkCountdown -= int(fDisplaySpeedRatio * 10);
	}
	if(iNextSparkCountdown < 0)
	{
		iNextSparkCountdown = 100;
		afPositions[iNextSpark] = 0;
		//afSpeeds[iNextSpark] = random8(0, 255) / 255.0 * fSpeedDiff + fSpeedMin;
		afSpeeds[iNextSpark] = fSpeeds[random8(5)];
		iNextSpark = (iNextSpark + 1) % NUM_SPARKS;
	}
	
	// Move
	int iPos;
	for(int i = 0; i < NUM_SPARKS; i++)
	{
		if(afPositions[i] < NUM_NEO_PIXELS)
		{		
			afPositions[i] = afPositions[i] + afSpeeds[i];
		}
		iPos = (int)afPositions[i];
		if(iPos < NUM_NEO_PIXELS)
		{	
			//byte yNewBrightness = 240;
			byte yNewBrightness = random8(40,240);
			g_ayHeat2[iPos] = yNewBrightness;
			g_ayHeat2[iPos-1] = yNewBrightness;
		}
	}
}



void DisplayMovementSpeed(float fDisplaySpeedRatio, int iDeltaTimeMS)
{
	// Time vars in microseconds
	unsigned long iStartTime;
	unsigned long iEndTime;
	iStartTime = micros();
	
	//fDisplaySpeedRatio = 1.0; // TEMP_CL
	

	//// Basic - map speed to color
	//for(int i = 0; i < NUM_NEO_PIXELS; i++)
	//{
	//	byte yPaletteValue = byte(fDisplaySpeedRatio * 240);
	//	g_LEDs[i] = ColorFromPalette( g_Pal, yPaletteValue);
	//}
    //
	//// Write final values to LEDs
	//FastLED.show();

	
	// Update frame count
	g_iCurInterpFrame++;
	
	// Get number of interp frames from speed
	int iNumFrames = MAX_INTERP_FRAMES;
	if(MODE == 0) 
	{
		iNumFrames = MAX_INTERP_FRAMES - int(pow(fDisplaySpeedRatio,0.5) * (MAX_INTERP_FRAMES - MIN_INTERP_FRAMES)) + MIN_INTERP_FRAMES;
	}
	else if(MODE == 1)
	{
		iNumFrames = 10; 
	}
	else if(MODE == 2 )
	{
		iNumFrames = 1; 
	}
	if( MODE == 3) 
	{
		iNumFrames = 
			MAX_INTERP_FRAMES - 
			int(pow(fDisplaySpeedRatio,0.35) * (MAX_INTERP_FRAMES - 2)) +
			2;
	}
	if( MODE == 4) 
	{
		//iNumFrames = 1; 
		iNumFrames = 
			MAX_INTERP_FRAMES - 
			int(pow(fDisplaySpeedRatio,0.35) * (MAX_INTERP_FRAMES - 2)) +
			2;
		UpdateHeat4(fDisplaySpeedRatio, iDeltaTimeMS * iNumFrames);
	}
	
	// If we're past the number of frames we should have, start a new interp
	if(g_iCurInterpFrame >= iNumFrames)
	{
		// Save off last heat to interp from
		memcpy(g_ayLast_heat, g_ayHeat, NUM_NEO_PIXELS);
		
		// Get a new frame to interp to
		if(MODE == 0) 
		{
			UpdateHeat0(fDisplaySpeedRatio, iDeltaTimeMS * iNumFrames);
		}
		else if(MODE == 1)
		{
			UpdateHeat1(fDisplaySpeedRatio, iDeltaTimeMS * iNumFrames);
		}
		else if(MODE == 2)
		{
			UpdateHeat2(fDisplaySpeedRatio, iDeltaTimeMS * iNumFrames);
		}
		else if(MODE == 3)
		{
			UpdateHeat3(fDisplaySpeedRatio, iDeltaTimeMS * iNumFrames);
		}
		else if(MODE == 4)
		{
			UpdateHeat3(fDisplaySpeedRatio, iDeltaTimeMS * iNumFrames);
		}
		
		// Restart interp frame counter
		g_iCurInterpFrame = 0;
	}
  
	byte ySpeedAdd = 0;
	byte yBrightness = 0;
	if(MODE == 3)
	{
		ySpeedAdd = byte(32 * fDisplaySpeedRatio);
		yBrightness = byte(225 * fDisplaySpeedRatio) + 30;
	}
	else if(MODE == 4)
	{
		ySpeedAdd = byte(32 * fDisplaySpeedRatio);
		yBrightness = byte(100 * fDisplaySpeedRatio) + 30;
	}
	
	if(MODE == 2) 
	{
		for( int j = 0; j < NUM_NEO_PIXELS; ++j)
		{			
			g_LEDs[j] = ColorFromPalette( g_Pal, g_ayHeat[j]);
		}
	}
	else
	{
		// Do interp
		fract8 fLerp = g_iCurInterpFrame*256/iNumFrames;
		byte lerpHeat;
		for( int j = 0; j < NUM_NEO_PIXELS; ++j)
		{
			// Fade between last and cur heat values
			lerpHeat = lerp8by8(g_ayLast_heat[j], g_ayHeat[j], fLerp);

			// Scale heat
			lerpHeat = scale8(lerpHeat, MAX_HEAT);
			
			if(ADJUST_CONTRAST_ON_MOVEMENT)
			{
				// Try adding contrast based on speed
				int iMid = MAX_HEAT / 5;
				lerpHeat = ClampI((int(lerpHeat) - iMid) * (fDisplaySpeedRatio * 8 + 1.0) + iMid, 0, MAX_HEAT);
			}
			
			if(MODE == 3) 
			{
				//byte yOutHeat = qadd8(lerpHeat, ySpeedAdd);
				byte yOutHeat = ClampI(lerpHeat + ySpeedAdd, 0, 240);
				g_LEDs[j] = ColorFromPalette( g_Pal, yOutHeat, yBrightness);
			}
			else if(MODE == 4) 
			{
				//byte yOutHeat = qadd8(lerpHeat, ySpeedAdd);
				byte yOutHeat = ClampI(lerpHeat + ySpeedAdd, 0, 240);
				CRGB color0 = ColorFromPalette( g_Pal, yOutHeat, yBrightness);
				//CRGB color0 = ColorFromPalette( g_Pal, 0);
				CRGB color1 = ColorFromPalette( g_Pal2, g_ayHeat2[j]);
				g_LEDs[j] = color0 + color1;
				//g_LEDs[j] = ColorFromPalette( g_Pal2, g_ayHeat2[j]);
			}
			else
			{
				g_LEDs[j] = ColorFromPalette( g_Pal, lerpHeat);
			}
		}
	}

	FastLED.show(); // display this frame

	
	// Make this take a consistent amount of time so the frame rate dependent
	// smoothing code runs cleanly.
	iEndTime = micros();
	int iMicrosToDelay = ALLOTTED_LED_TIME_MICRO - (iEndTime-iStartTime);
	if(iMicrosToDelay > 0)
	{
		delayMicroseconds(iMicrosToDelay);
	}

	//// TEMP_CL - profile
	//iEndTime = micros();
	//DebugLog("Total LED time = ",int(iEndTime-iStartTime));
}



void loop()
{
	// Profiling time vars in microseconds
	unsigned long iStartTime;
	unsigned long iEndTime;
	iStartTime = micros();

	// Get the cur
	float fCurSpeed = GetCurSpeed();

	// Clamp the range of the speed
	fCurSpeed = Clamp(fCurSpeed, fMinSpeed, fMaxSpeed);

	// Calculate the new speed ratio
	float fNewSpeedRatio = (fCurSpeed - fMinSpeed) / (fMaxSpeed - fMinSpeed);

	// TEMP_CL - try this AFTER the smoothing..
	//// Apply the input exponent to change the input curve
	//Serial.print("TEMP_CL fNewSpeedRatio pre exponent = ");
	//Serial.print(fNewSpeedRatio);
	//fNewSpeedRatio = pow(fNewSpeedRatio, fInputExponent);
	//Serial.print(" fNewSpeedRatio post exponent = ");
	//Serial.println(fNewSpeedRatio);

	// Calculate the smoothed speed ratio
	g_fSpeedRatio = fNewSpeedRatio * fNewSpeedWeight + g_fSpeedRatio * (1.0 - fNewSpeedWeight);

	// Apply the input exponent to change the input curve.  This is the final output.
	float fDisplaySpeedRatio = pow(g_fSpeedRatio, fInputExponent);
	Serial.print("TEMP_CL fNewSpeedRatio =");
	Serial.print(fNewSpeedRatio);
	Serial.print(" g_fSpeedRatio=");
	Serial.print(g_fSpeedRatio);
	Serial.print(" fDisplaySpeedRatio=");
	Serial.println(fDisplaySpeedRatio);

	// Get delta time.  TODO TEMP_CL - figure out how this different from the CurTime below.
	// Maybe that time needs to be really really acurate and doing all the LED stuff in the middle
	// will mess it up?
	unsigned long iCurTimeMS = millis();
	int iDeltaTimeMS = iCurTimeMS - g_iLastTimeMS;
	g_iLastTimeMS = iCurTimeMS;

	// TEMP_CL
	if(iDeltaTimeMS > 50)
	{
		DebugLog("TEMP_CL - HITCH --------------------------------------------------");
	}

	// Use the speed ratio to set the brightness of the LEDs
	DisplayMovementSpeed(fDisplaySpeedRatio, iDeltaTimeMS);

	/* TEMP_CL - not sure we need this now...
	
	// Construct the byte to send that contains our node ID
	// and the current speed.  We are compressing our speed ratio
	// to 5 bits (32 values) so that it fits in a single byte.
	// We are also never sending a byte of 0 so make speed run from 1 to 31, not 0 to 31
	byte ySendByte = g_iNodeIndex << 5;
	ySendByte = ySendByte | (int(fDisplaySpeedRatio * 30 + 0.5) + 1);

	// Get the current time (after all the sensor and LED stuff)
	unsigned long iCurTime = millis();

	// Listen for other nodes talking
	int iNumReadBytes = 0;
	while(Uart.available() > 0)
	{
		// Get the byte 
		int iReadByte = Uart.read();
		iNumReadBytes++;

		// Really, I don't think we should be reading more than one each loop... but maybe not?
		if(iNumReadBytes > 1)
		{
			DebugLog("Got more more than a single byte in a loop:", iNumReadBytes);
		}

		// If we got a new start signal, start trying to read the packet
		if(iReadByte == START_RCV_BYTE)
		{
			ReceiveTuningMessage();
		}
		else
		{
			// Get the node index of the sender
			int iIncomingNodeIndex = iReadByte >> 5;

			if(USE_SERIAL_FOR_DEBUGGING)
			{
				Serial.print(iCurTime);
				Serial.print(" Got data ");
				Serial.print(iReadByte);
				Serial.print(" from node ");
				Serial.print(iIncomingNodeIndex);

				int iMotion = (iReadByte & 31) << 3;
				Serial.print(" with value ");
				Serial.println(iMotion);
			}

			// Throw out 0 bytes
			if(iReadByte == 0)
			{
				DebugLog(" ############# Ignoring 0 byte!");
				continue;
			}

			// Do some error checking that can likely be commented out in a final version
			// The PC "node" index is NUM_NODES, so accept that as a valid value too.
			if( iIncomingNodeIndex == g_iNodeIndex ||
				iIncomingNodeIndex > NUM_NODES )
			{
				DebugLog(" ############# Got invlaid node index!");
			}
			else
			{
				// Save the next node index
				g_iNextNodeIndex = (iIncomingNodeIndex + 1) % (NUM_NODES + 1); // The PC gets a chance to talk also and has a node index of NUM_NODES
			}
		
			// Save off the time we got this message so we can time out if a node isn't sending.
			g_iLastReceiveTime = iCurTime;
		}
	}

	if(iNumReadBytes == 0)
	{
		unsigned int iTimeout = (g_iNextNodeIndex == NUM_NODES) ? PC_COM_TIMEOUT_MS : NODE_COM_TIMEOUT_MS;
		// Check for a timeout.
		if(iCurTime - g_iLastReceiveTime > iTimeout)
		{
			if(USE_SERIAL_FOR_DEBUGGING)
			{
				Serial.print(iCurTime);
				Serial.print(" Timeout! g_iNextNodeIndex=");
				Serial.print(g_iNextNodeIndex);
				Serial.print(" g_iLastReceiveTime=");
				Serial.print(g_iLastReceiveTime);
				Serial.println("");
			}

			g_iNextNodeIndex = (g_iNextNodeIndex + 1) % (NUM_NODES + 1); // The PC gets a chance to talk also and has a node index of NUM_NODES
			g_iLastReceiveTime = iCurTime;
		}
	}

	// If we are supposed to send next, do that
	if(g_iNodeIndex == g_iNextNodeIndex)
	{
		if(USE_SERIAL_FOR_DEBUGGING)
		{
			Serial.print(iCurTime);
			Serial.print(" About to write data g_iNodeIndex=");
			Serial.print(g_iNodeIndex);
			Serial.print(" value=");
			Serial.print(ySendByte & 31);
			Serial.println("");
		}

		if(USE_RS485)
		{
			delay(1); // short extra delay just to be safe
			digitalWrite (RS485_ENABLE_WRITE_PIN, HIGH);  // enable sending
		}
		Uart.write(ySendByte);
		if(USE_RS485)
		{
			Uart.flush();
			delay(1); // short extra delay just to be safe
			digitalWrite (RS485_ENABLE_WRITE_PIN, LOW);  // disable sending  

		}

		// Update next node now that we have sent
		g_iNextNodeIndex = (g_iNextNodeIndex + 1) % (NUM_NODES + 1); // The PC gets a chance to talk also and has a node index of NUM_NODES

		// Also reset the time or else we will timeout too early
		g_iLastReceiveTime = iCurTime;

		if(USE_SERIAL_FOR_DEBUGGING)
		{
			Serial.print(iCurTime);
			Serial.print(" Just wrote data g_iNodeIndex=");
			Serial.println(g_iNodeIndex);
		}
	}
	*/

	//// TEMP_CL - Profiling
	iEndTime = micros();
	DebugLog("Total loop time = ", int(iEndTime-iStartTime));
}
