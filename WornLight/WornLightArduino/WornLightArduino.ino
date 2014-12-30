/**
 * File: WornLightArduino.ino
 *
 * Description: Program for controlling lights worn on the body.
 *
 * Copyright: 2014 Chris Linder
 */

#include <EEPROM.h>
#include <Adafruit_NeoPixel.h>

// Use Serial to print out debug statements
static bool USE_SERIAL_FOR_DEBUGGING = true;

// Number of LEDs
#define NUM_LEDS 45

// The address in EEPROM for various saved values
//static const int EEPROM_ADDR_NODE_INDEX = 0;

// The current data version of the EEPROM data.  Each time the format changes, increment this
#define CUR_EEPROM_DATA_VERSION 1

// Data pin for Adafruit_NeoPixel
#define LED_DATA_PIN 10

// Create an instance of the Adafruit_NeoPixel class
Adafruit_NeoPixel g_Leds = Adafruit_NeoPixel(NUM_LEDS, LED_DATA_PIN, NEO_GRB + NEO_KHZ800);

// Input pin for music syncing
#define MUSIC_SYNC_PIN 15

// The music sync / rhythm tap button is down
bool g_bMusicSyncDown;

// Input pins for controls
#define BUTTON_A_PIN 5
#define BUTTON_B_PIN 4
#define BUTTON_C_PIN 3
#define BUTTON_D_PIN 2

// Button states
bool g_bButtonADown = false;
bool g_bButtonBDown = false;
bool g_bButtonCDown = false;
bool g_bButtonDDown = false;

// This is the number of init loops to code does to test LEDs
#define NUM_INIT_LOOPS 2

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

// Color class
class Color
{
public:
	Color() : m_yR(0), m_yG(0), m_yB(0)
	{
	};

	Color(byte yR, byte yG, byte yB) : m_yR(yR), m_yG(yG), m_yB(yB)
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

	byte m_yR;
	byte m_yG;
	byte m_yB;
};

// Light sequence deef
#define MAX_SEQ_SIZE 128 // 64?
struct Sequence
{
	int m_bActive;
	int m_iNumNotes;
	int m_iNumMeasures;
	int* m_aiPattern;
	Color m_oOnColor;
	Color m_oFadeColor;
	Color m_aoOutColors[NUM_LEDS];
	int m_iLastNoteIndex;

	Sequence() : m_bActive(false), m_iNumNotes(4), m_iNumMeasures(1), m_oOnColor(255,255,255), m_oFadeColor(1,1,1), m_iLastNoteIndex(-1)
	{
		memset(m_aoOutColors, 0, NUM_LEDS * sizeof(Color));
	}
};

// Hard coded patterns
////static int aiSeq0[] = {23,31,42,20,24,32,21,40,23,31,25,20,34,42,21,-1,-2};
//static int aiSeq0[] = {54,40,55,47,42,50,45,46,44,51,43,49,48,52,53,41,-2};
//static int aiSeq1[] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,-2};
////static int aiSeq2[] = {59,58,57,56,55,54,53,52,51,50,49,48,47,46,45,44,43,42,41,40,39,38,37,36,35,34,33,32,31,30,29,28,27,26,25,24,23,22,21,20,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0,59,58,57,56,55,54,53,52,51,50,49,48,47,46,45,44,43,42,41,40,39,38,37,36,35,34,33,32,31,30,29,28,27,26,25,24,23,22,21,20,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0,-2};
//static int aiSeq2[] = {20,25,30,35,-2};

// Eighth notes (32)
//// random (odds)
static int aiSeq0[] = {27,17,21,25,13,23,17,11,15,21,5,9,7,15,17,11,25,9,19,23,5,21,9,21,7,13,17,9,25,15,7,13,-2};
// Cylon sweep
//static int aiSeq0[] = {3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,26,25,24,23,22,21,20,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,-2};

// Half notes (8)
//static int aiSeq1[] = {17,15,12,9,26,6,8,24,-2};
//static int aiSeq1[] = {16,14,11,8,25,5,7,23,-2};
//static int aiSeq1[] = {2,6,10,14,18,22,20,12,-2};
//static int aiSeq1[] = {-1,16,-1,18,-1,20,-1,22,-1,24,-1,22,-1,20,-1,18,-2};
static int aiSeq1[] = {-1,28,-1,28,-2};

// Quarter notes (16)
//static int aiSeq2[] = {16,21,14,19,11,5,8,4,25,22,5,24,7,18,23,6,-2};
//static int aiSeq2[] = {2,4,6,8,10,12,14,16,18,20,22,24,20,16,12,8,-2};
//static int aiSeq2[] = {4,16,6,18,8,20,10,22,12,24,10,22,8,20,6,18,-2};
//static int aiSeq2[] = {27,27,27,27,-2};
//static int aiSeq2[] = {33,33,33,33,-2};
static int aiSeq2[] = {33,36,33,38,33,31,33,35,-2};

// Quarter notes 2
//static int aiSeq3[] = {36,36,36,36,-2};

// Fire test
//static int aiSeq3[] = {34,37,38,40,42,31,35,36,32,41,39,33,42,36,40,37,31,41,38,33,34,35,32,39,-2};
static int aiSeq3[] = {36,32,35,33,34,33,36,35,34,32,33,35,36,32,34,32,36,34,35,33,36,32,35,33,34,33,-2};

// TEMP_CL
static int iRotateTicks = 0;

// Sequences
#define NUM_SEQ 4
struct Sequence g_aSeq[NUM_SEQ];

// Measure time in ms - at some point make this defined by user input beat tapping
int g_MeasureTimeInMS = 3010;

// The note index in the sequence in the last loop
int g_iLastNoteIndexInSeq;

// Total LED brightness
Color g_aoLEDColors[NUM_LEDS];

// Time offset used to let user sync to music
unsigned long g_iMusicTimeOffset;

// The last time time in MS
unsigned long g_iLastSyncTapTimeMS;

// The number of taps we've counted so far for beat matching
int g_iNumTapsForRhythm;

// The time we started tapping to beat match
unsigned long g_iTappingStartTimeMS;

// Different states based on input
bool g_bNewLights = true;
bool g_bBackgroundLights = true;
bool g_bBackgroundLightsPulse = true;
bool g_bSeqLights = true;
int g_iColorMapping = 0;


void setup()
{
	if(USE_SERIAL_FOR_DEBUGGING)
	{
		// setup debugging serial port (over USB cable) at 9600 bps
		Serial.begin(9600);
		Serial.println("WornLight - Setup");
	}

	// Flash the on board LEDs as a test
	int RXLED = 17;  // The RX LED has a defined Arduino pin
	pinMode(RXLED, OUTPUT);  // Set RX LED as an output
	for(int i = 0; i < 5; i++)
	{
		digitalWrite(RXLED, LOW);   // set the LED on
		TXLED0; //TX LED is not tied to a normally controlled pin
		delay(200);
		digitalWrite(RXLED, HIGH);    // set the LED off
		TXLED1;
		delay(200);
	}
	TXLED0;

	// Read in settings from EPROM
	ReadSettingsFromEEPROM();

	// Init LED values
	memset(g_aoLEDColors, 0, NUM_LEDS * sizeof(Color));

	// Init music sync
	g_iMusicTimeOffset = 0;

	// Init sequences
	int nSeq = 0;
	int nPattern = 0;

	// 0
	g_aSeq[nSeq].m_bActive = true;
	g_aSeq[nSeq].m_iNumMeasures = 4;
	g_aSeq[nSeq].m_aiPattern = aiSeq0;
	for(g_aSeq[nSeq].m_iNumNotes = 0; g_aSeq[nSeq].m_iNumNotes < MAX_SEQ_SIZE; g_aSeq[nSeq].m_iNumNotes++)
	{
		if(g_aSeq[nSeq].m_aiPattern[g_aSeq[nSeq].m_iNumNotes] == -2)
			break;
	}
	g_aSeq[nSeq].m_oOnColor = Color(222,100,100);
	g_aSeq[nSeq].m_oFadeColor = Color(6,9,9);
	nSeq++;

	// 1
	g_aSeq[nSeq].m_bActive = true;
	g_aSeq[nSeq].m_iNumMeasures = 1;
	g_aSeq[nSeq].m_aiPattern = aiSeq1;
	for(g_aSeq[nSeq].m_iNumNotes = 0; g_aSeq[nSeq].m_iNumNotes < MAX_SEQ_SIZE; g_aSeq[nSeq].m_iNumNotes++)
	{
		if(g_aSeq[nSeq].m_aiPattern[g_aSeq[nSeq].m_iNumNotes] == -2)
			break;
	}
	g_aSeq[nSeq].m_oOnColor = Color(255,255,255);
	g_aSeq[nSeq].m_oFadeColor = Color(6,6,6);
	nSeq++;

	// 2
	g_aSeq[nSeq].m_bActive = true;
	g_aSeq[nSeq].m_iNumMeasures = 1;
	g_aSeq[nSeq].m_aiPattern = aiSeq2;
	for(g_aSeq[nSeq].m_iNumNotes = 0; g_aSeq[nSeq].m_iNumNotes < MAX_SEQ_SIZE; g_aSeq[nSeq].m_iNumNotes++)
	{
		if(g_aSeq[nSeq].m_aiPattern[g_aSeq[nSeq].m_iNumNotes] == -2)
			break;
	}
	g_aSeq[nSeq].m_oOnColor = Color(222,100,100);
	g_aSeq[nSeq].m_oFadeColor = Color(6,9,9);
	nSeq++;

	// 3
	g_aSeq[nSeq].m_bActive = true;
	g_aSeq[nSeq].m_iNumMeasures = 1;
	g_aSeq[nSeq].m_aiPattern = aiSeq3;
	for(g_aSeq[nSeq].m_iNumNotes = 0; g_aSeq[nSeq].m_iNumNotes < MAX_SEQ_SIZE; g_aSeq[nSeq].m_iNumNotes++)
	{
		if(g_aSeq[nSeq].m_aiPattern[g_aSeq[nSeq].m_iNumNotes] == -2)
			break;
	}
	g_aSeq[nSeq].m_oOnColor = Color(255,220,150);
	g_aSeq[nSeq].m_oFadeColor = Color(2,3,4);
	nSeq++;



	// Init NeoPixel
	g_Leds.begin();  // Call this to start up the LED strip.
	clearLEDs();     // This function, defined below, turns all LEDs off...
	g_Leds.show();   // ...but the LEDs don't actually update until you call this.

	// Setup input pins
	pinMode(MUSIC_SYNC_PIN, INPUT_PULLUP);
	pinMode(BUTTON_A_PIN, INPUT_PULLUP);
	pinMode(BUTTON_B_PIN, INPUT_PULLUP);
	pinMode(BUTTON_C_PIN, INPUT_PULLUP);
	pinMode(BUTTON_D_PIN, INPUT_PULLUP);

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
}



// Sets all LEDs to off, but DOES NOT update the display;
// call show() to actually turn them off after this.
void clearLEDs()
{
	for (int i = 0; i < NUM_LEDS; i++)
	{
		g_Leds.setPixelColor(i, 0);
	}
}



void ReadSettingsFromEEPROM()
{
	//int ySavedDataVersion = EEPROM.read(EEPROM_ADDR_DATA_VERSION);

	if(USE_SERIAL_FOR_DEBUGGING)
	{
		Serial.println("Settings from EEPROM:");
		//Serial.print("fMinSpeed=");
		//Serial.println(fMinSpeed);
	}
}



// Clamp function - float
float ClampF(float fVal, float fMin, float fMax)
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


void UpdateSeq(unsigned long iCurTime, struct Sequence &seq)
{
	// Get current note
	unsigned long iCurTimeInSeq = iCurTime % (g_MeasureTimeInMS * seq.m_iNumMeasures);
	unsigned long iCurNoteIndexInSeq = (iCurTimeInSeq * seq.m_iNumNotes) / (g_MeasureTimeInMS * seq.m_iNumMeasures);

	// Check if we just advanced to a new note
	bool bNewNote = false;
	if(iCurNoteIndexInSeq != seq.m_iLastNoteIndex)
	{
		bNewNote = true;
		if(USE_SERIAL_FOR_DEBUGGING)
		{
			Serial.print("iCurNoteIndexInSeq="); Serial.println(iCurNoteIndexInSeq);
		}
	}

	// Assign g_iLastNoteIndexInSeq
	seq.m_iLastNoteIndex = iCurNoteIndexInSeq;

	// If we have a new note in the sequence, display it on the correct LED
	if(bNewNote && seq.m_aiPattern[iCurNoteIndexInSeq] >= 0)
	{
		if(USE_SERIAL_FOR_DEBUGGING)
		{
			Serial.print("seq.m_aiPattern[iCurNoteIndexInSeq]="); Serial.println(seq.m_aiPattern[iCurNoteIndexInSeq]);
		}
		seq.m_aoOutColors[seq.m_aiPattern[iCurNoteIndexInSeq]] = seq.m_oOnColor;
	}
}

// TEMP_CL - Fire
void UpdateSeqFire(unsigned long iCurTime, struct Sequence &seq)
{
	//// Get current note
	//unsigned long iCurTimeInSeq = iCurTime % (g_MeasureTimeInMS * seq.m_iNumMeasures);
	//unsigned long iCurNoteIndexInSeq = (iCurTimeInSeq * seq.m_iNumNotes) / (g_MeasureTimeInMS * seq.m_iNumMeasures);

	//// Check if we just advanced to a new note
	//bool bNewNote = false;
	//if(iCurNoteIndexInSeq != seq.m_iLastNoteIndex)
	//{
	//	bNewNote = true;
	//	if(USE_SERIAL_FOR_DEBUGGING)
	//	{
	//		Serial.print("iCurNoteIndexInSeq="); Serial.println(iCurNoteIndexInSeq);
	//	}
	//}

	//// Assign g_iLastNoteIndexInSeq
	//seq.m_iLastNoteIndex = iCurNoteIndexInSeq;

	//// If we have a new note in the sequence, display it on the correct LED
	//if(bNewNote && seq.m_aiPattern[iCurNoteIndexInSeq] >= 0)

	int iMinLEDIndex = 1;
	int iMaxLEDIndex = 43;

	//// rotate
	//iRotateTicks++;
	//if((iRotateTicks % 10) == 0)
	//{
	//	byte yFirstValueR = seq.m_ayOutValuesR[iMinLEDIndex];
	//	for(int nLED = iMinLEDIndex; nLED < iMaxLEDIndex-1; nLED++)
	//	{
	//		seq.m_ayOutValuesR[nLED] = seq.m_ayOutValuesR[nLED+1];
	//	}
	//	seq.m_ayOutValuesR[iMaxLEDIndex-1] = yFirstValueR;

	//	byte yFirstValueG = seq.m_ayOutValuesG[iMinLEDIndex];
	//	for(int nLED = iMinLEDIndex; nLED < iMaxLEDIndex-1; nLED++)
	//	{
	//		seq.m_ayOutValuesG[nLED] = seq.m_ayOutValuesG[nLED+1];
	//	}
	//	seq.m_ayOutValuesR[iMaxLEDIndex-1] = yFirstValueG;

	//	byte yFirstValueB = seq.m_ayOutValuesB[iMinLEDIndex];
	//	for(int nLED = iMinLEDIndex; nLED < iMaxLEDIndex-1; nLED++)
	//	{
	//		seq.m_ayOutValuesB[nLED] = seq.m_ayOutValuesB[nLED+1];
	//	}
	//	seq.m_ayOutValuesR[iMaxLEDIndex-1] = yFirstValueB;
	//}

	static const int iNumFirePoints = 8;
	static int aFirePoints[iNumFirePoints];
	static int iCyclesTillNewPoints = 0;
	static const int iFirePointsRefreshCycles = 5;

	if(iCyclesTillNewPoints <= 0)
	{
		for(int i = 0; i < iNumFirePoints; i++)
		{
			aFirePoints[i] = random(iMinLEDIndex,iMaxLEDIndex);
		}

		iCyclesTillNewPoints = iFirePointsRefreshCycles;
	}
	iCyclesTillNewPoints--;

	if(true)
	{
		// TEMP_CL - don't use the pattern just pick some random LEDs in the range from 32-36
		// also add to what is there, don't overwrite
		for(int i = 0; i < iNumFirePoints; i++)
		{
			int nLED = aFirePoints[i];
			seq.m_aoOutColors[nLED].m_yR = ClampI(seq.m_oOnColor.m_yR / 20 + seq.m_aoOutColors[nLED].m_yR, 0, seq.m_oOnColor.m_yR);
			seq.m_aoOutColors[nLED].m_yG = ClampI(seq.m_oOnColor.m_yG / 20 + seq.m_aoOutColors[nLED].m_yG, 0, seq.m_oOnColor.m_yG);
			seq.m_aoOutColors[nLED].m_yB = ClampI(seq.m_oOnColor.m_yB / 20 + seq.m_aoOutColors[nLED].m_yB, 0, seq.m_oOnColor.m_yB);
		}
	}

	// Fade
    for(int nLED = iMinLEDIndex; nLED < iMaxLEDIndex; nLED++)
    {
		seq.m_aoOutColors[nLED].SubClamped(seq.m_oFadeColor);
	}
}


void loop()
{
	// Get the raw current time (after all the sensor and LED stuff)
	unsigned long iRawTime = millis();

	// Get button presses
	bool g_bButtonADownOld = g_bButtonADown;
	bool g_bButtonBDownOld = g_bButtonBDown;
	bool g_bButtonCDownOld = g_bButtonCDown;
	bool g_bButtonDDownOld = g_bButtonDDown;
	g_bButtonADown = !digitalRead(BUTTON_A_PIN);
	g_bButtonBDown = !digitalRead(BUTTON_B_PIN);
	g_bButtonCDown = !digitalRead(BUTTON_C_PIN);
	g_bButtonDDown = !digitalRead(BUTTON_D_PIN);
	bool bButtonAPressed = g_bButtonADown && !g_bButtonADownOld;
	bool bButtonBPressed = g_bButtonBDown && !g_bButtonBDownOld;
	bool bButtonCPressed = g_bButtonCDown && !g_bButtonCDownOld;
	bool bButtonDPressed = g_bButtonDDown && !g_bButtonDDownOld;

	// Change state based on in button pressed
	if(bButtonAPressed) g_bBackgroundLights      = !g_bBackgroundLights;
	if(bButtonBPressed) g_bBackgroundLightsPulse = !g_bBackgroundLightsPulse;
	// TEMP_CL if(bButtonCPressed) g_bNewLights             = !g_bNewLights;
	if(bButtonCPressed) g_iColorMapping = (g_iColorMapping + 1) % 7;
	if(bButtonDPressed) g_bSeqLights             = !g_bSeqLights;

	// Check for sync / rhythm tap
	bool bOldMusicSyncDown = g_bMusicSyncDown;
	g_bMusicSyncDown = !digitalRead(MUSIC_SYNC_PIN);
	bool bFirstTap = false; // TEMP_CL
	if(g_bMusicSyncDown && !bOldMusicSyncDown)
	{
		unsigned long iTimeSinceLastSyncTapMS = iRawTime - g_iLastSyncTapTimeMS;

		// If we just get one tap after no recent taps we are either resyncing the beat but not trying to define a new one,
		// or starting to tap out a new beat.  Either way, restart time and start counting taps.
		if(iTimeSinceLastSyncTapMS > g_MeasureTimeInMS)
		{
			g_iMusicTimeOffset = iRawTime;
			g_iNumTapsForRhythm = 0;
			g_iTappingStartTimeMS = iRawTime;

			bFirstTap = true;
		}
		// If we are tapping regularly, we are trying to redefine the existing beat
		else
		{
			// Increment the beat counter
			g_iNumTapsForRhythm++;

			// Get new measure time (assuming we are tapping quater notes and are in 4/4 time)
			g_MeasureTimeInMS = (iRawTime - g_iTappingStartTimeMS) / g_iNumTapsForRhythm * 4;
		}

		g_iLastSyncTapTimeMS = iRawTime;
	}

	// Set current time now that our offest is adjusted
	unsigned long iCurTime = iRawTime - g_iMusicTimeOffset;

	// Updates sequences
	if(g_bNewLights)
	{
		for(int i = 0; i < NUM_SEQ; i++)
		{
			if(g_aSeq[i].m_bActive)
			{
				if(i == 3)
				{
					UpdateSeqFire(iCurTime, g_aSeq[i]);
				}
				else if(g_bSeqLights || i == 1)
				{
					UpdateSeq(iCurTime, g_aSeq[i]);
				}
			}
		}
	}

	// Display the light values
    for(int nLED = 0; nLED < NUM_LEDS; nLED++)
    {
		g_aoLEDColors[nLED] = Color(0,0,0);

		for(int nSeq = 0; nSeq < NUM_SEQ; nSeq++)
		{
			if(nSeq == 3 && !g_bBackgroundLights)
			{
				continue;
			}

			Color oOutColor = g_aSeq[nSeq].m_aoOutColors[nLED];

			if(nSeq == 3)
			{
				if(g_bBackgroundLightsPulse)
					oOutColor.Mult( 0.5 * (float)g_aSeq[1].m_aoOutColors[28].m_yR / 255.0); // TEMP_CL - fix this hardcoding
				else
					oOutColor.Mult(0.5);
			}

			g_aoLEDColors[nLED].Lighten(oOutColor);

			if(nSeq != 3)
			{
				g_aSeq[nSeq].m_aoOutColors[nLED].SubClamped(g_aSeq[nSeq].m_oFadeColor);
			}
		}

		//// TEMP_CL
		//if(nLED < 10 && g_bMusicSyncDown && !bOldMusicSyncDown)
		//{
		//	if(bFirstTap)
		//	{
		//		g_Leds.setPixelColor(nLED, 255, 255, 255);
		//	}
		//	else
		//	{
		//		g_Leds.setPixelColor(nLED, 255, 0, 0);
		//	}
		//}
	
		if(g_aoLEDColors[nLED].m_yR > 0 || g_aoLEDColors[nLED].m_yG > 0 || g_aoLEDColors[nLED].m_yB > 0)
		{
			// Remap brightness values
			byte yR = exp_map[g_aoLEDColors[nLED].m_yR];
			byte yG = exp_map[g_aoLEDColors[nLED].m_yG];
			byte yB = exp_map[g_aoLEDColors[nLED].m_yB];

			// Set pixel color based on current setting of color mapping
			switch(g_iColorMapping)
			{
				case 0: g_Leds.setPixelColor(nLED, yR, yG, yB); break;
				case 1: g_Leds.setPixelColor(nLED, yR, yB, yG); break;
				case 2: g_Leds.setPixelColor(nLED, yG, yR, yB); break;
				case 3: g_Leds.setPixelColor(nLED, yG, yB, yR); break;
				case 4: g_Leds.setPixelColor(nLED, yB, yR, yG); break;
				case 5: g_Leds.setPixelColor(nLED, yB, yG, yR); break;
				case 6: g_Leds.setPixelColor(nLED, yR, yR, yR); break; // White
			}
		}
		else
		{
			// Turn off LED
			g_Leds.setPixelColor(nLED, 0);
		}
	}
	g_Leds.show();

	// Maybe a delay would be nice?
	delay(10);
}

