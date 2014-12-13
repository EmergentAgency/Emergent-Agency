/**
 * File: EaMidiNodesNeoPixel.pde
 *
 * Description: Arduino program for each node that collects motion signals and transmits them to the PC.
 * This version is focused on using the RS485 protocol to communicate along a shared pair of wires.
 * The same logic can also be used for the XBees.
 * 
 * This variant is based on EaMidiNodesRS485 but uses and array of NeoPixels instead of individual LEDs.
 *
 * Copyright: 2014 Chris Linder
 */

#include <EEPROM.h>
#include <Adafruit_NeoPixel.h>

// Use Serial to print out debug statements
static bool USE_SERIAL_FOR_DEBUGGING = true;

// Number of nodes
static int NUM_NODES = 7;

// The index for this node.  If it is < 0, use the node index stored in
// EEPROM.  If >= 0, store that node index in the EEPROM of this node.
int g_iNodeIndex = -1;

// The address in EEPROM for various saved values
static const int EEPROM_ADDR_NODE_INDEX = 0;
static const int EEPROM_ADDR_DATA_VERSION = 21;
static const int EEPROM_ADDR_MIN_SPEED = 22;
static const int EEPROM_ADDR_MAX_SPEED = 24;
static const int EEPROM_ADDR_NEW_SPEED_WEIGHT = 26;
static const int EEPROM_ADDR_INPUT_EXPONENT = 28;

// The current data version of the EEPROM data.  Each time the format changes, increment this
static const int CUR_EEPROM_DATA_VERSION = 2;

// If this is true, we use Xbee to send signals.  Right now, these doesn't turn on anything special but it might at some point.
static const bool USE_XBEE = false;

// If this is true, we use RS-485 to send signals.
static const bool USE_RS485 = true;



// OLD LED system

// The numbers of LEDs this node has
static const int NUM_LEDS_PER_NODE = 5;

// Output pin mapping
static const int pins[NUM_LEDS_PER_NODE] = {14, 15, 26, 25, 16};



// NeoPixel system

// Color class
class Color
{
public:
	//Color() : m_yPlaceholder(0), m_yR(0), m_yG(0), m_yB(0)
	Color() : m_yR(0), m_yG(0), m_yB(0)
	{
	};

	//Color(byte yR, byte yG, byte yB) : m_yPlaceholder(0), m_yR(yR), m_yG(yG), m_yB(yB)
	Color(byte yR, byte yG, byte yB) : m_yR(yR), m_yG(yG), m_yB(yB)
	{
	};

	//Color(uint32_t c) : m_yPlaceholder(0), m_yR( (c&0xFF0000) >> 16 ), m_yG( (c&0x00FF00) >> 8 ), m_yB( c&0x0000FF )
	Color(uint32_t c) : m_yR( (c&0xFF0000) >> 16 ), m_yG( (c&0x00FF00) >> 8 ), m_yB( c&0x0000FF )
	{
	};

	void Add(Color oColor)
	{
		m_yR += oColor.m_yR;
		m_yG += oColor.m_yG;
		m_yB += oColor.m_yB;
	}

	void AddClamped(Color oColor)
	{
		m_yR = ClampI(m_yR + oColor.m_yR, 0, 255);
		m_yG = ClampI(m_yG + oColor.m_yG, 0, 255);
		m_yB = ClampI(m_yB + oColor.m_yB, 0, 255);
	}

	void Sub(Color oColor)
	{
		m_yR -= oColor.m_yR;
		m_yG -= oColor.m_yG;
		m_yB -= oColor.m_yB;
	}

	void SubClamped(Color oColor)
	{
		m_yR = ClampI(m_yR - oColor.m_yR, 0, 255);
		m_yG = ClampI(m_yG - oColor.m_yG, 0, 255);
		m_yB = ClampI(m_yB - oColor.m_yB, 0, 255);
	}

	void Lighten(Color oColor)
	{
		if(oColor.m_yR > m_yR) m_yR = oColor.m_yR;
		if(oColor.m_yG > m_yG) m_yG = oColor.m_yG;
		if(oColor.m_yB > m_yB) m_yB = oColor.m_yB;
	}

	void Mult(float fMult)
	{
		m_yR = byte(float(m_yR) * fMult);
		m_yG = byte(float(m_yG) * fMult);
		m_yB = byte(float(m_yB) * fMult);
	}

	//byte m_yPlaceholder; // This allows for easy conversation to uint32_t color type
	byte m_yG;
	byte m_yR;
	byte m_yB;
};

// Number of LEDs
#define NUM_LEDS 100

// Data pin for Adafruit_NeoPixel
#define LED_DATA_PIN 40

// Create an instance of the Adafruit_NeoPixel class
Adafruit_NeoPixel g_Leds = Adafruit_NeoPixel(NUM_LEDS, LED_DATA_PIN, NEO_GRB + NEO_KHZ800);

// LED frame buffer
Color g_aoLEDOut[NUM_LEDS];

// Dimming is done as floats but color values are ints.  This allows for smooth dimming between frames;
float g_fDimRemainder;

// Number of light "points" 
#define NUM_SPRITES 15

// -1.0 is first LED, 1.0 is last LED.  Positions can be past ends.
float g_afSpritePositions[NUM_SPRITES];

// Velocity scale
float g_afSpriteVelcale[NUM_SPRITES];

// Total life of the sprites in MS (counts up)
float g_afSpriteLifeInMS[NUM_SPRITES];

// Max lifetime
#define MAX_LIFETIME_IN_MS 4000

// Used for delta time
unsigned long g_iLastTimeMS;

// Intensity color ramp
#define NUM_INTENSITY_POINTS 4
static const float g_afIntensityPoint[NUM_INTENSITY_POINTS]={0.0, 0.333, 0.666, 1.0};
//static const Color g_acIntensityColor[NUM_INTENSITY_POINTS]={Color(64,0,0), Color(128,64,0), Color(255,128,64), Color(255,255,128)};
//static const Color g_acIntensityColor[NUM_INTENSITY_POINTS]={Color(64,0,0), Color(96,64,32), Color(255,192,128), Color(255,255,192)};
//static const Color g_acIntensityColor[NUM_INTENSITY_POINTS]={Color(64,0,0), Color(96,32,0), Color(255,192,128), Color(255,255,192)};
//static const Color g_acIntensityColor[NUM_INTENSITY_POINTS]={Color(64,0,0), Color(96,32,0), Color(255,192,128), Color(255,255,255)};
//static const Color g_acIntensityColor[NUM_INTENSITY_POINTS]={Color(64,0,0), Color(96,32,0), Color(220,192,128), Color(220,220,220)};

// More consistent brightness
//static const Color g_acIntensityColor[NUM_INTENSITY_POINTS]={Color(96,0,0), Color(96,32,0), Color(220,192,128), Color(220,220,220)};

// Test full power
static const Color g_acIntensityColor[NUM_INTENSITY_POINTS]={Color(96,0,0), Color(96,32,0), Color(220,192,128), Color(255,255,255)};

// Create a fast lookup table for intensity color to save computation time
#define INTENSITY_LOOKUP_SAMPLES 200
#define MAX_INTENSITY_LOOKUP 199
Color g_acIntensityLookup[INTENSITY_LOOKUP_SAMPLES];

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

// Lookup map used to linearize LED brightness
static const unsigned char exp_map[256]={
  0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,3,
  3,3,3,3,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,4,4,4,5,5,5,
  5,5,5,5,5,6,6,6,6,6,6,6,7,7,7,7,7,7,8,8,8,8,8,8,9,9,
  9,9,10,10,10,10,10,11,11,11,11,12,12,12,13,13,13,13,
  14,14,14,15,15,15,16,16,16,17,17,18,18,18,19,19,20,
  20,20,21,21,22,22,23,23,24,24,25,26,26,27,27,28,29,
  29,30,31,31,32,33,33,34,35,36,36,37,38,39,40,41,42,
  42,43,44,45,46,47,48,50,51,52,53,54,55,57,58,59,60,
  62,63,64,66,67,69,70,72,74,75,77,79,80,82,84,86,88,
  90,91,94,96,98,100,102,104,107,109,111,114,116,119,
  122,124,127,130,133,136,139,142,145,148,151,155,158,
  161,165,169,172,176,180,184,188,192,196,201,205,210,
  214,219,224,229,234,239,244,250,255
};

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



void DebugLog(char* sLog)
{
	if(USE_SERIAL_FOR_DEBUGGING)
	{
		Serial.println(sLog);
	}
}

void DebugLog(char* sLog, float f)
{
	if(USE_SERIAL_FOR_DEBUGGING)
	{
		Serial.print(sLog);
		Serial.println(f);
	}
}

void DebugLog(char* sLog, int i)
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

	// Old LED system
    // setup LED pins for output
    for(int i = 0; i < NUM_LEDS_PER_NODE; i++)
    {
        // turn on pins for output to test an LED
        pinMode(pins[i], OUTPUT);   
    }

	// Init LED values
	memset(g_aoLEDOut, 0, NUM_LEDS * sizeof(Color));

	// Init intensity lookup
	for (int i = 0; i < INTENSITY_LOOKUP_SAMPLES; i++)
	{
		g_acIntensityLookup[i] = GetIntensityColor((float)i / (float)(MAX_INTENSITY_LOOKUP));
	}

	// Init NeoPixel
	g_Leds.begin();  // Call this to start up the LED strip.
	for (int i = 0; i < NUM_LEDS; i++)
	{
		g_Leds.setPixelColor(i, 0);
	}
	g_Leds.show();   // ...but the LEDs don't actually update until you call this.

	// Init sprites
	for(int i = 0; i < NUM_SPRITES; i++)
	{
		g_afSpriteLifeInMS[i] = MAX_LIFETIME_IN_MS / NUM_SPRITES * i;
		g_afSpritePositions[i] = random(2000) / 1000.0 - 1.0;
		g_afSpritePositions[i] /= 2.0;
		g_afSpriteVelcale[i] = (random(2) == 0) ? -1.0 : 1.0;
	}

 	// Do a simple startup sequence to test LEDs
	for(int nTest = 0; nTest < NUM_INIT_LOOPS; nTest++)
	{
		int nLED;
		for(nLED = 0; nLED < NUM_LEDS; nLED+=3)
		{
			g_Leds.setPixelColor(nLED, 0xFFFFFF);
		}
		g_Leds.show();
		delay(500);
		for(nLED = 0; nLED < NUM_LEDS; nLED++)
		{
			g_Leds.setPixelColor(nLED, 0);
		}
		g_Leds.show();
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

		// TEMP_CL - HACK for overriding internal settings for test
		iNewMinSpeed = 0.01;
		fMaxSpeed = 0.4;
		fNewSpeedWeight = 0.02;
		fInputExponent = 1.0;

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
	for(int i = 0; i < 2; i++)
	{
		// TEMP_CL - write NeoPixel version
		//for(int j = NUM_LEDS_PER_NODE-1; j >= 0; j--)
		//{
		//	analogWrite(pins[j], 255);
		//	delay(100);
		//	digitalWrite(pins[j], LOW);
		//}
		//delay(200);
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



// 0.0 <= fPos <= 1.0 
void ShowLight(float fPos, class Color c)
{
	float fLEDPos = fPos * (NUM_LEDS - 1);

	int nLED1 = int(fLEDPos);

	// TEMP_CL
	g_aoLEDOut[nLED1].Lighten(c);
	return;

	int nLED2 = nLED1 + 1;

	float fLightValue1 = 1.0 - (fLEDPos - nLED1);
	float fLightValue2 = 1.0 - fLightValue1;

	//if(USE_SERIAL_FOR_DEBUGGING)
	//{
	//	Serial.print("fLEDPos=");
	//	Serial.println(fLEDPos);
	//	Serial.print("nLED1=");
	//	Serial.println(nLED1);
	//	Serial.print("fLightValue1=");
	//	Serial.println(fLightValue1);
	//	Serial.print("fLightValue2=");
	//	Serial.println(fLightValue2);
	//}

	Color c1 = c;
	c1.Mult(fLightValue1);
	//g_aoLEDOut[nLED1] = c1;
	g_aoLEDOut[nLED1].Lighten(c1);
	//g_Leds.setPixelColor(nLED1, c1.m_yR, c1.m_yG, c1.m_yB);

	if(nLED2 < NUM_LEDS)
	{
		Color c2 = c;
		c2.Mult(fLightValue2);
		//g_aoLEDOut[nLED2] = c2;
		g_aoLEDOut[nLED2].Lighten(c2);
		//g_Leds.setPixelColor(nLED1, c2.m_yR, c2.m_yG, c2.m_yB);
	}
}

Color GetIntensityColor(float fIntensity)
{
	Color cOut;

	if(fIntensity <= 0.01)
	{
		cOut = g_acIntensityColor[0];
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
		cOut = g_acIntensityColor[iLowInterpPoint];
		cOut.Mult(1.0 - fInterp);
		Color c2 = g_acIntensityColor[iHighInterpPoint];
		c2.Mult(fInterp);
		cOut.Add(c2);
	}

	return cOut;
}

void DisplayMovementSpeed(float fDisplaySpeedRatio, int iDeltaTimeMS)
{
	// Profiling time vars in microseconds
	unsigned long iStartTime;
	unsigned long iEndTime;

	iStartTime = micros(); // TEMP_CL


	//// Old system
	//iStartTime = micros(); // TEMP_CL
 //   if(fDisplaySpeedRatio > 0)
 //   {
	//	for(int nLED = 0; nLED < NUM_LEDS_PER_NODE; nLED++)
	//	{
	//		float fSubRangeMin = nLED / 5.0;
	//		float fSubRangeMax = (nLED + 1.0) / 5.0;
	//		float fSubRangeRatio = (fDisplaySpeedRatio - fSubRangeMin) / (fSubRangeMax - fSubRangeMin);
	//		fSubRangeRatio = Clamp(fSubRangeRatio, 0.0, 1.0);
	//		int iLEDbrightness = fSubRangeRatio * 255;

	//		analogWrite(pins[nLED], exp_map[iLEDbrightness]);   // set the LED brightness
	//	}
 //    }
 //   else
 //   {
	//	for(int nLEDOff = 0; nLEDOff < NUM_LEDS_PER_NODE; nLEDOff++)
	//	{
	//		digitalWrite(pins[nLEDOff], LOW); // Turn off LED
	//	}
 //   }
	//iEndTime = micros(); // TEMP_CL
	//Serial.print("Old LED total time = "); // TEMP_CL
	//Serial.println(iEndTime-iStartTime); // TEMP_CL



	//// All LEDs same color
	//Color cIntensityColor = GetIntensityColor(fDisplaySpeedRatio);
	//for(int nLED = 0; nLED < NUM_LEDS; nLED++)
	//{
	//	g_aoLEDOut[nLED] = cIntensityColor;
	//}
	//delay(4);



	//// Dimming
	//static const float kfDimPerSec = 800.0;
	//float fDimThisFrame = kfDimPerSec / 1000.0 * iDeltaTimeMS + g_fDimRemainder;
	//int iDimThisFrame = int(fDimThisFrame);
	//g_fDimRemainder = fDimThisFrame - iDimThisFrame;
	//for(int i = 0; i < NUM_LEDS; i++)
	//{
	//	g_aoLEDOut[i].SubClamped(Color(iDimThisFrame, iDimThisFrame, iDimThisFrame));
	//}

	//// Sprites
	//for(int i = 0; i < NUM_SPRITES; i++)
	//{
	//	// Update positions - velocity is based on distance from center
	//	//float fVelocityFactor = (float)iRawTimeMS / 60000.0;
	//	//float fVelocityFactor = (sin(iRawTimeMS/5000) + 1.0) * 1.5;
	//	//float fVelocityFactor = fDisplaySpeedRatio * 0.8;
	//	float fVelocityFactor = fDisplaySpeedRatio * fDisplaySpeedRatio * 1.5;

	//	//float fVel = g_afSpritePositions[i] * fVelocityFactor;
	//	float fVel = g_afSpriteVelcale[i] * fVelocityFactor;

	//	float fNewPos = g_afSpritePositions[i] + fVel * iDeltaTimeMS / 1000.0;
	//	g_afSpritePositions[i] = fNewPos;

	//	// Update lifetimes
	//	g_afSpriteLifeInMS[i] += iDeltaTimeMS;
	//	if(g_afSpriteLifeInMS[i] > MAX_LIFETIME_IN_MS || g_afSpritePositions[i] > 1.0 || g_afSpritePositions[i] < -1.0)
	//	{
	//		g_afSpriteLifeInMS[i] = 0;
	//		g_afSpritePositions[i] = random(100,2000) / 2500.0;
	//		if(random(2) == 0)
	//		{
	//			g_afSpritePositions[i] *= -1;
	//		}
	//	}

	//	int nLED = int((g_afSpritePositions[i] + 1.0) / 2.0 * NUM_LEDS);
	//	if(nLED >= 0 && nLED < NUM_LEDS)
	//	{
	//		Color c = GetIntensityColor(fDisplaySpeedRatio);
	//		if(g_afSpriteLifeInMS[i] < MAX_LIFETIME_IN_MS/4)
	//		{
	//			c.Mult(g_afSpriteLifeInMS[i] / (MAX_LIFETIME_IN_MS/4));
	//		}
	//		else if(g_afSpriteLifeInMS[i] > MAX_LIFETIME_IN_MS * 3 / 4)
	//		{
	//			c.Mult(1.0 - (g_afSpriteLifeInMS[i] - MAX_LIFETIME_IN_MS * 3 / 4) / (MAX_LIFETIME_IN_MS/4));
	//		}

	//		ShowLight((g_afSpritePositions[i] + 1.0) / 2.0, c);
	//	}
	//}



	//// Show exact value
	////ShowLight(1.0 - fDisplaySpeedRatio, Color(255,128,100));
	//iStartTime = micros(); // TEMP_CL
	//ShowLight(1.0 - fDisplaySpeedRatio, Color(random(256),0,random(256)));
	//iEndTime = micros(); // TEMP_CL
	////Serial.print("ShowLight time = "); // TEMP_CL
	////Serial.println(iEndTime-iStartTime); // TEMP_CL



	// TEMP_CL - testing
	//fDisplaySpeedRatio = 0.5;



	// Full string color fade from center

	//iStartTime = micros(); // TEMP_CL

	//static const int iNumFadeLEDs = 30;
	static const int iNumFadeLEDs = 100;

	static const int iHalfNumLEDS = iNumFadeLEDs / 2;
	static const int iLEDOffset = NUM_LEDS / 2;

	float fMinIntensityPos = fDisplaySpeedRatio * (1.0 + FADE_DISTANCE);
	float fMaxIntensityPos = fMinIntensityPos - FADE_DISTANCE;

	Color cMinIntensity = g_acIntensityLookup[0];
	Color cMaxIntensity = g_acIntensityLookup[MAX_INTENSITY_LOOKUP];

	//for(int nLED = 0; nLED < iHalfNumLEDS; nLED++)
	//{
	//	float fLEDPos = (float)nLED / (float)(iHalfNumLEDS-1);
	//	if(fLEDPos <= fMaxIntensityPos)
	//	{
	//		g_aoLEDOut[nLED+iLEDOffset] = cMaxIntensity;
	//	}
	//	else if(fLEDPos >= fMinIntensityPos)
	//	{
	//		g_aoLEDOut[nLED+iLEDOffset] = cMinIntensity;
	//	}
	//	else
	//	{
	//		float fRatio = (fMinIntensityPos - fLEDPos) / FADE_DISTANCE;
	//		g_aoLEDOut[nLED+iLEDOffset] = g_acIntensityLookup[int(fRatio * MAX_INTENSITY_LOOKUP)];
	//	}
	//}

	int iMinIntensityPos = ClampI(int(fMinIntensityPos * (iHalfNumLEDS-1)), 0, iHalfNumLEDS);
	int iMaxIntensityPos = int(fMaxIntensityPos * (iHalfNumLEDS-1));
	int iFadeDist = int(FADE_DISTANCE * (iHalfNumLEDS-1));

	//DebugLog("fDisplaySpeedRatio=", fDisplaySpeedRatio);
	//DebugLog("iMinIntensityPos=", iMinIntensityPos);
	//DebugLog("iMaxIntensityPos=", iMaxIntensityPos);
	//DebugLog("iFadeDist=", iFadeDist);

	int nLED = 0;
	for(; nLED < iMaxIntensityPos; nLED++)
	{
		g_aoLEDOut[nLED+iLEDOffset] = cMaxIntensity;
	}
	for(; nLED < iMinIntensityPos; nLED++)
	{
		float fRatio = (nLED - iMaxIntensityPos) / iFadeDist;
		g_aoLEDOut[nLED+iLEDOffset] = g_acIntensityLookup[int(fRatio * MAX_INTENSITY_LOOKUP)];
	}
	for(; nLED < iHalfNumLEDS; nLED++)
	{
		g_aoLEDOut[nLED+iLEDOffset] = cMinIntensity;
	}

	// Fill in other half
	for(int nLED = 0; nLED < iHalfNumLEDS; nLED++)
	{
		g_aoLEDOut[iLEDOffset-1 - nLED] = g_aoLEDOut[iLEDOffset + nLED];
	}

	iEndTime = micros(); // TEMP_CL
	DebugLog("calc intensity time = ", int(iEndTime-iStartTime)); // TEMP_CL

	//delay(4);



	// Set final LED values
	iStartTime = micros(); // TEMP_CL
	for(int i = 0; i < NUM_LEDS; i++)
	{
		//g_Leds.setPixelColor(i, g_aoLEDOut[i].m_yR, g_aoLEDOut[i].m_yG, g_aoLEDOut[i].m_yB);
		g_Leds.setPixelColor(i, exp_map[g_aoLEDOut[i].m_yR], exp_map[g_aoLEDOut[i].m_yG], exp_map[g_aoLEDOut[i].m_yB]);
	}
	iEndTime = micros(); // TEMP_CL
	DebugLog("setPixel time = ", int(iEndTime-iStartTime)); // TEMP_CL

	//iStartTime = micros(); // TEMP_CL
	//g_Leds.setBrightness(80); // Dim for now to use less power
	g_Leds.setBrightness(255);
	g_Leds.show();
	//iEndTime = micros(); // TEMP_CL
	//Serial.print("g_Leds.show = "); // TEMP_CL
	//Serial.println(iEndTime-iStartTime); // TEMP_CL


	//iEndTime = micros(); // TEMP_CL
	//DebugLog("total LED time = ",int(iEndTime-iStartTime)); // TEMP_CL
}



void loop()
{
	// Profiling time vars in microseconds
	unsigned long iStartTime;
	unsigned long iEndTime;

	iStartTime = micros(); // TEMP_CL

	// Get the cur
	float fCurSpeed = GetCurSpeed();

	// DEBUG
	//// Print the speed returned
	//if(USE_SERIAL_FOR_DEBUGGING)
	//{
	//	Serial.print("Current speed = ");
	//	Serial.println(fCurSpeed);
	//}

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
	//Serial.print("TEMP_CL fNewSpeedRatio =");
	//Serial.print(fNewSpeedRatio);
	//Serial.print(" g_fSpeedRatio=");
	//Serial.print(g_fSpeedRatio);
	//Serial.print(" fDisplaySpeedRatio=");
	//Serial.println(fDisplaySpeedRatio);

	// Get delta time.  TODO TEMP_CL - figure out how this different from the CurTime below.
	// Maybe that time needs to be really really acurate and doing all the LED stuff in the middle
	// will mess it up?
	unsigned long iCurTimeMS = millis();
	int iDeltaTimeMS = iCurTimeMS - g_iLastTimeMS;
	g_iLastTimeMS = iCurTimeMS;

	// Use the speed ratio to set the brightness of the LEDs
	DisplayMovementSpeed(fDisplaySpeedRatio, iDeltaTimeMS);

	// Construct the byte to send that contains our node ID
	// and the current speed.  We are compressing our speed ratio
	// to 5 bits (32 values) so that it fits in a single byte.
	// We are also never sending a byte of 0 so make speed run from 1 to 31, not 0 to 31
	byte ySendByte = g_iNodeIndex << 5;
	ySendByte = ySendByte | (int(fDisplaySpeedRatio * 30 + 0.5) + 1);

	// DEBUG
	// We don't do this check anymore because we only have 7 nodes 
	// and the node index of the START_RCV_BYTE and END_RCV_BYTE
	// refer to the 8th node so they will never get sent.
	//// Make sure send byte isn't one of our signal chars
	//if(ySendByte == START_RCV_BYTE || ySendByte == END_RCV_BYTE)
	//{
	//	ySendByte++;
	//}

	// Get the current time (after all the sensor and LED stuff)
	unsigned long iCurTime = millis();

	// DEBUG
	// Send non-sensor data.  Just send the last 5 bits of the time for now.
	//int iTestSendValue = iCurTime & 31;
	//if(iTestSendValue == 0)
	//{
	//	iTestSendValue++;
	//}
	//ySendByte = (g_iNodeIndex << 5) | iTestSendValue;

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
			if(USE_SERIAL_FOR_DEBUGGING)
			{
				Serial.print("Got more more than a single byte in a loop:");
				Serial.println(iNumReadBytes);
			}
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
				Serial.print(iCurTime);
				Serial.println(" ############# Ignoring 0 byte!");
				continue;
			}

			// Do some error checking that can likely be commented out in a final version
			// The PC "node" index is NUM_NODES, so accept that as a valid value too.
			if( iIncomingNodeIndex == g_iNodeIndex ||
				iIncomingNodeIndex > NUM_NODES )
			{
				if(USE_SERIAL_FOR_DEBUGGING)
				{
					Serial.print(iCurTime);
					Serial.println(" ############# Got invlaid node index!");
				}
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
		int iTimeout = (g_iNextNodeIndex == NUM_NODES) ? PC_COM_TIMEOUT_MS : NODE_COM_TIMEOUT_MS;
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
			delay(1); // TEMP_CL - short extra delay just to be safe
			digitalWrite (RS485_ENABLE_WRITE_PIN, HIGH);  // enable sending
		}
		Uart.write(ySendByte);
		if(USE_RS485)
		{
			Uart.flush();
			delay(1); // TEMP_CL - short extra delay just to be safe
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

	iEndTime = micros(); // TEMP_CL
	Serial.print("Total loop time = "); // TEMP_CL
	Serial.println(iEndTime-iStartTime); // TEMP_CL

}

