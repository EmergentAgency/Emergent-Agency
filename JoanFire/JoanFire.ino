/**
 * File: JoanFire.ino
 *
 * Description: Fire control for JOAN via two SNES controllers and USB
 *
 * Copyright: 2016 Chris Linder
 */

// Library includes
#include <SNESpaduino.h>

// Constants

// Tuning
#define TICK_TIME_IN_MS 20
#define SERIAL_TIMEOUT_TICKS 100
#define FLAME_COOLDOWN_TICKS 3

// Controller 0
#define CONTROLLER_0_CLOCK_PIN 12
#define CONTROLLER_0_LATCH_PIN 11
#define CONTROLLER_0_DATA_PIN 10

// Controller 1
#define CONTROLLER_1_CLOCK_PIN 9
#define CONTROLLER_1_LATCH_PIN 8
#define CONTROLLER_1_DATA_PIN 7

#define NUM_FLAMES 4

// Solenoid control pins
static const int FIRE_PINS[] = {2, 3, 4, 5};

// Fire bits
static const byte FLAME_STATE_ON[] = {0x01, 0x02, 0x04, 0x08};

static const byte RAPID_FIRE_SEQ_LEFT[] = {
	FLAME_STATE_ON[0], FLAME_STATE_ON[0], FLAME_STATE_ON[0],
	0, 0, 0,
	FLAME_STATE_ON[3], FLAME_STATE_ON[3], FLAME_STATE_ON[3],
	0, 0, 0,
};

static const byte RAPID_FIRE_SEQ_RIGHT[] = {
	FLAME_STATE_ON[1], FLAME_STATE_ON[1], FLAME_STATE_ON[1],
	0, 0, 0,
	FLAME_STATE_ON[2], FLAME_STATE_ON[2], FLAME_STATE_ON[2],
	0, 0, 0,
};


// Global vars

// SNESpaduino instances (latch, clock, data)
SNESpaduino g_pad0(CONTROLLER_0_LATCH_PIN, CONTROLLER_0_CLOCK_PIN, CONTROLLER_0_DATA_PIN);
SNESpaduino g_pad1(CONTROLLER_1_LATCH_PIN, CONTROLLER_1_CLOCK_PIN, CONTROLLER_1_DATA_PIN);

// Button state for both controllers
uint16_t g_iOldButtons0;
uint16_t g_iOldButtons1;
uint16_t g_iButtons0;
uint16_t g_iButtons1;

// Fire states
byte g_ySerialFireState;
byte g_yPad0FireState;
byte g_yPad1FireState;
byte g_yPatternFireState;
byte g_yDesiredFireState;
byte g_yCurrentFireState;
int g_bRapidFireCounterLeft;
int g_bRapidFireCounterRight;

// Programmable pattern
#define PROG_BUFFER_SIZE 1024
byte g_ayProgPattern[PROG_BUFFER_SIZE];
int g_iPatternLength;
int g_iActiveProgPadIndex; // -1 if not actively programming
boolean g_bRunningProg;
int g_iCurPatternIndex;
bool g_bRecordedAnythingInPattern;

// Cooldowns
int g_aiFlameChangeCooldown[] = {0, 0, 0, 0};

// Serial timeout counter
unsigned int g_uSerialTimeoutCounter;

void setup()
{
	Serial.begin(9600);
    
    // Init fire control pins (pins are OUTPUT and HIGH to turn on fire and INPUT to turn off fire)
    for(int i = 0; i < NUM_FLAMES; i++)
    {
		pinMode(FIRE_PINS[i], INPUT);
	}
    
    // Init button states
    g_iOldButtons0 = 0;
    g_iOldButtons1 = 0;
    g_iButtons0 = 0;
    g_iButtons1 = 0;
    
    // Init fire states
    g_ySerialFireState = 0;
    g_yPad0FireState = 0;
    g_yPad1FireState = 0;
	g_yPatternFireState = 0;
    g_yDesiredFireState = 0;
    g_yCurrentFireState = 0;
	g_bRapidFireCounterLeft = 0;
	g_bRapidFireCounterRight = 0;
  
    // Init serial timeout counter
    g_uSerialTimeoutCounter = 0;

	// Init pattern
	g_iPatternLength = 0;
	g_iActiveProgPadIndex = -1;
	g_bRunningProg = false;
	g_iCurPatternIndex = 0;
	g_bRecordedAnythingInPattern = false;
}


 
byte GetFireStatefromButtons(uint16_t iOldButtons, uint16_t iNewButtons)
{
	byte yFireStateOut = 0;
    
    // If you press start, fire all the flames
    if(iNewButtons & BTN_START)
    {
        return 0x0F;
    }
    
    // Buttons
    yFireStateOut |= (iNewButtons & BTN_Y) ? FLAME_STATE_ON[0] : 0;
    yFireStateOut |= (iNewButtons & BTN_B) ? FLAME_STATE_ON[1] : 0;
    yFireStateOut |= (iNewButtons & BTN_X) ? FLAME_STATE_ON[2] : 0;
    yFireStateOut |= (iNewButtons & BTN_A) ? FLAME_STATE_ON[3] : 0;
    
    // D-Pad
    yFireStateOut |= (iNewButtons & BTN_LEFT)  ? FLAME_STATE_ON[0] : 0;
    yFireStateOut |= (iNewButtons & BTN_UP)    ? FLAME_STATE_ON[1] : 0;
    yFireStateOut |= (iNewButtons & BTN_DOWN)  ? FLAME_STATE_ON[2] : 0;
    yFireStateOut |= (iNewButtons & BTN_RIGHT) ? FLAME_STATE_ON[3] : 0;

	// Button presses start the sequences over
	if((iNewButtons & BTN_L) && !(iOldButtons & BTN_L))
	{
		g_bRapidFireCounterLeft = 0;
	}
	if((iNewButtons & BTN_R) && !(iOldButtons & BTN_R))
	{
		g_bRapidFireCounterRight = 0;
	}

	// Run the sequences if the button is pressed
	if(iNewButtons & BTN_L)
	{
		yFireStateOut |= RAPID_FIRE_SEQ_LEFT[g_bRapidFireCounterLeft];
	}
	if(iNewButtons & BTN_R)
	{
		yFireStateOut |= RAPID_FIRE_SEQ_RIGHT[g_bRapidFireCounterRight];
	}

    return yFireStateOut;
}



void loop()
{
    // Update serial timeout counter (it's ok if this wraps if we haven't heard from serial in a long time)
    g_uSerialTimeoutCounter++;

    // Read flame state from serial
    while (Serial.available() > 0)
    {
        byte yIncomingByte = Serial.read();
        g_ySerialFireState = yIncomingByte & 0x0F;
        g_uSerialTimeoutCounter = 0;
    }
    
    // If we haven't heard from serial in a while, set the serial fire states to off
    if(g_uSerialTimeoutCounter > SERIAL_TIMEOUT_TICKS)
    {
        g_ySerialFireState = 0;
    }
    
    // Get button states
    g_iOldButtons0 = g_iButtons0;
    g_iOldButtons1 = g_iButtons0;
    g_iButtons0 = g_pad0.getButtons();
    g_iButtons1 = g_pad1.getButtons();
    
    // Update fire state based on buttons
    g_yPad0FireState = GetFireStatefromButtons(g_iOldButtons0, g_iButtons0);
    g_yPad1FireState = GetFireStatefromButtons(g_iOldButtons1, g_iButtons1);

	// Update sequences
	g_bRapidFireCounterLeft++;
	if(g_bRapidFireCounterLeft >= (int)sizeof(RAPID_FIRE_SEQ_LEFT))
	{
		g_bRapidFireCounterLeft = 0;
	}
	g_bRapidFireCounterRight++;
	if(g_bRapidFireCounterRight >= (int)sizeof(RAPID_FIRE_SEQ_RIGHT))
	{
		g_bRapidFireCounterRight = 0;
	}


	// Pattern

	// First check to see if anyone wants to start programming
	if(!g_bRunningProg)
	{
		g_yPatternFireState = 0;

		if(g_iActiveProgPadIndex < 0)
		{
			if(g_iButtons0 & BTN_SELECT)
			{
				g_iActiveProgPadIndex = 0;
				g_iPatternLength = 0;
			}
			else if(g_iButtons1 & BTN_SELECT)
			{
				g_iActiveProgPadIndex = 1;
				g_iPatternLength = 0;
			}
		}
		else
		{
			if( (g_iActiveProgPadIndex == 0 && !(g_iButtons0 & BTN_SELECT)) ||
				(g_iActiveProgPadIndex == 1 && !(g_iButtons1 & BTN_SELECT)) )
			{
				g_iActiveProgPadIndex = -1;
				g_bRunningProg = true;
				g_iCurPatternIndex = 0;
			}
		}

		if(g_iActiveProgPadIndex == 0)
		{
			g_ayProgPattern[g_iPatternLength] = g_yPad0FireState;
			g_iPatternLength = (g_iPatternLength + 1) % PROG_BUFFER_SIZE;
			g_bRecordedAnythingInPattern = g_bRecordedAnythingInPattern || g_yPad0FireState != 0;
		}
		else if(g_iActiveProgPadIndex == 1)
		{
			g_ayProgPattern[g_iPatternLength] = g_yPad1FireState;
			g_iPatternLength = (g_iPatternLength + 1) % PROG_BUFFER_SIZE;
			g_bRecordedAnythingInPattern = g_bRecordedAnythingInPattern || g_yPad0FireState != 0;
		}
	}
	// If we're running the program
	else if(g_iPatternLength > 0)
	{
		g_yPatternFireState = g_ayProgPattern[g_iCurPatternIndex];
		g_iCurPatternIndex = (g_iCurPatternIndex + 1) % g_iPatternLength;

		if( (g_iButtons0 & BTN_SELECT) || (g_iButtons1 & BTN_SELECT) )
		{
			// This will immediately fall through to starting to program again which is fine.
			// If the user taps the button it is just programming a very short empty loop.
			g_bRunningProg = false;
		}
	}
	// If we somehow ended up entering a 0 length pattern (which can happen if the pattern wraps PROG_BUFFER_SIZE just as you stop programming), turn it off
	else
	{
		g_bRunningProg = false;
		g_yPatternFireState = 0;
	}

    // Aggregate fire state
    byte yOldFireState = g_yCurrentFireState;
    g_yDesiredFireState = g_ySerialFireState | g_yPad0FireState | g_yPad1FireState | g_yPatternFireState;

    // Send fire state to output pins (pins are OUTPUT and HIGH to turn on fire and INPUT to turn off fire)
    for(int i = 0; i < NUM_FLAMES; i++)
    {
		if(g_aiFlameChangeCooldown[i] > 0)
		{
			g_aiFlameChangeCooldown[i]--;
		}

		if(g_aiFlameChangeCooldown[i] <= 0)
		{
			if((g_yDesiredFireState & FLAME_STATE_ON[i]) != (g_yCurrentFireState & FLAME_STATE_ON[i]))
			{
				// Write state to pin
				if(g_yDesiredFireState & FLAME_STATE_ON[i])
				{
					pinMode(FIRE_PINS[i], OUTPUT);
					digitalWrite(FIRE_PINS[i], HIGH);
				}
				else
				{
					digitalWrite(FIRE_PINS[i], LOW);
					pinMode(FIRE_PINS[i], INPUT);
				}

				// Update the state and start a new cooldown 
				g_yCurrentFireState = (g_yCurrentFireState & ~FLAME_STATE_ON[i]) | (g_yDesiredFireState & FLAME_STATE_ON[i]);
				g_aiFlameChangeCooldown[i] = FLAME_COOLDOWN_TICKS;
			}
		}
    }

    // Write current fire state to serial
    if(g_yCurrentFireState != yOldFireState)
    {
        Serial.print((g_yCurrentFireState & FLAME_STATE_ON[0]) ? "1" : "0");
        Serial.print((g_yCurrentFireState & FLAME_STATE_ON[1]) ? "1" : "0");
        Serial.print((g_yCurrentFireState & FLAME_STATE_ON[2]) ? "1" : "0");
        Serial.print((g_yCurrentFireState & FLAME_STATE_ON[3]) ? "1" : "0");
        Serial.println();
    }

	// Wait a bit
	delay(TICK_TIME_IN_MS);
}
