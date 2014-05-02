/**
 * File: WornLightArduino.ino
 *
 * Description: Program for controlling lights worn on the body.
 *
 * Copyright: 2014 Chris Linder
 */

#include <EEPROM.h>

// Use Serial to print out debug statements
static bool USE_SERIAL_FOR_DEBUGGING = true;

// Number of LEDs
static const int NUM_LEDS = 5;

// The address in EEPROM for various saved values
//static const int EEPROM_ADDR_NODE_INDEX = 0;

// The current data version of the EEPROM data.  Each time the format changes, increment this
static const int CUR_EEPROM_DATA_VERSION = 1;

// Output pin mapping
static const int pins[NUM_LEDS] = {14, 15, 26, 25, 16};

// Input pin for music syncing
static const int MUSIC_SYNC_PIN = 19;

// This is the number of init loops to code does to test LEDs
static const int NUM_INIT_LOOPS = 2;

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

// Light sequence deef
static const int MAX_SEQ_SIZE = 64;
struct Sequence
{
	int m_iNumNotes;
	int m_iNumMeasures;
	int m_aiPattern[MAX_SEQ_SIZE];
	int m_iMax;
	int m_iFadeRate;
	int m_aiOutValues[NUM_LEDS];
	int m_iLastNoteIndex;

	Sequence() : m_iNumNotes(4), m_iNumMeasures(1), m_iMax(255), m_iFadeRate(1), m_iLastNoteIndex(-1)
	{
		memset(m_aiPattern, 0, MAX_SEQ_SIZE);
		memset(m_aiOutValues, 0, NUM_LEDS);
	}
};

// Sequences
static const int NUM_SEQ = 2;
struct Sequence g_aSeq[NUM_SEQ];

// Measure time in ms
static const int g_MeasureTimeInMS = 3200;

// The note index in the sequence in the last loop
int g_iLastNoteIndexInSeq;

// Total LED brightness
int g_aiLEDValue[NUM_LEDS];

// Time offset used to let user sync to music
unsigned long g_iMusicTimeOffset;

void setup()
{
	if(USE_SERIAL_FOR_DEBUGGING)
	{
		// setup debugging serial port (over USB cable) at 9600 bps
		Serial.begin(9600);
		Serial.println("WornLight - Setup");
	}

	// Read in settings from EPROM
	ReadSettingsFromEEPROM();

	// Init LEDs
	memset(g_aiLEDValue, 0, NUM_LEDS);

	// Init music sync
	g_iMusicTimeOffset = 0;

	// Init sequences
	// A
	g_aSeq[0].m_iNumNotes = 16;
	g_aSeq[0].m_iNumMeasures = 4;
	int aiSeqA[MAX_SEQ_SIZE] = {3,1,2,0,4,2,1,0,3,1,2,0,4,2,1,-1};
	memcpy(g_aSeq[0].m_aiPattern, aiSeqA, MAX_SEQ_SIZE);
	g_aSeq[0].m_iMax = 255;
	g_aSeq[0].m_iFadeRate = 1;
	// B
	g_aSeq[1].m_iNumNotes = 32;
	g_aSeq[1].m_iNumMeasures = 1;
	int aiSeqB[MAX_SEQ_SIZE] = {0,1,2,3,4,3,2,1,0,1,2,3,4,3,2,1,0,1,2,3,4,3,2,1,0,1,2,3,4,3,2,1};
	memcpy(g_aSeq[1].m_aiPattern, aiSeqB, MAX_SEQ_SIZE);
	g_aSeq[1].m_iMax = 128;
	g_aSeq[1].m_iFadeRate = 4;

    // setup LED pins for output
    for(int i = 0; i < NUM_LEDS; i++)
    {
        // turn on pins for output to test an LED
        pinMode(pins[i], OUTPUT);   
    }

	// Setup input pin
	pinMode(MUSIC_SYNC_PIN, INPUT_PULLUP);

 	// Do a simple startup sequence to test LEDs
	for(int nTest = 0; nTest < NUM_INIT_LOOPS; nTest++)
	{
		// For some reason "for" loops here weren't compiling so I just wrote it all out...
		digitalWrite(pins[0], HIGH); 
		digitalWrite(pins[1], HIGH); 
		digitalWrite(pins[2], HIGH); 
		digitalWrite(pins[3], HIGH); 
		digitalWrite(pins[4], HIGH);
		delay(500);
		digitalWrite(pins[0], LOW); 
		digitalWrite(pins[1], LOW); 
		digitalWrite(pins[2], LOW); 
		digitalWrite(pins[3], LOW); 
		digitalWrite(pins[4], LOW); 
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



void UpdateSeq(unsigned long iCurTime, struct Sequence &seq)
{
	// Get current note
	unsigned long iCurTimeInSeq = iCurTime % (g_MeasureTimeInMS *seq.m_iNumMeasures);
	int iCurNoteIndexInSeq = (iCurTimeInSeq *seq.m_iNumNotes) / (g_MeasureTimeInMS *seq.m_iNumMeasures);

	// Check if we just advanced to a new note
	bool bNewNote = false;
	if(iCurNoteIndexInSeq !=seq.m_iLastNoteIndex)
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
		seq.m_aiOutValues[seq.m_aiPattern[iCurNoteIndexInSeq]] = seq.m_iMax;
	}
}



void loop()
{
	// Get the raw current time (after all the sensor and LED stuff)
	unsigned long iRawTime = millis();

	// Check for music resync
	bool bMusicSyncDown = !digitalRead(MUSIC_SYNC_PIN);
	if(bMusicSyncDown)
	{
		g_iMusicTimeOffset = iRawTime;
	}

	// Set current time now that our offest is adjusted
	unsigned long iCurTime = iRawTime - g_iMusicTimeOffset;

	// Updates sequences
    for(int i = 0; i < NUM_SEQ; i++)
    {
		UpdateSeq(iCurTime, g_aSeq[i]);
	}

	// Display the light values
    for(int nLED = 0; nLED < NUM_LEDS; nLED++)
    {
		g_aiLEDValue[nLED] = 0;

		for(int nSeq = 0; nSeq < NUM_SEQ; nSeq++)
		{
			int iOutValue = g_aSeq[nSeq].m_aiOutValues[nLED];
			if(iOutValue > 0)
			{
				if(iOutValue > g_aiLEDValue[nLED])
				{
					g_aiLEDValue[nLED] = iOutValue;
				}

				g_aSeq[nSeq].m_aiOutValues[nLED] -= g_aSeq[nSeq].m_iFadeRate;
			}
		}
			
		if(g_aiLEDValue[nLED] > 0)
		{
			// set the LED brightness
			analogWrite(pins[nLED], exp_map[g_aiLEDValue[nLED]]);
		}
		else
		{
			digitalWrite(pins[nLED], LOW); // Turn off LED
		}
	}

	// Maybe a delay would be nice?
	delay(5);
}

