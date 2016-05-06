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
#define SERIAL_TIMEOUT_TICKS 50

// Controller 0
#define CONTROLLER_0_CLOCK_PIN 15
#define CONTROLLER_0_LATCH_PIN 14
#define CONTROLLER_0_DATA_PIN 13

// Controller 1
#define CONTROLLER_1_CLOCK_PIN 12
#define CONTROLLER_1_LATCH_PIN 11
#define CONTROLLER_1_DATA_PIN 10

// Solenoid control pins
#define FIRE_0_PIN 8 // Far left
#define FIRE_1_PIN 7 // Mid left
#define FIRE_2_PIN 6 // Mid right
#define FIRE_3_PIN 5 // Far right

// Fire bits
#define FLAME_0_On 0x01
#define FLAME_1_On 0x02
#define FLAME_2_On 0x04
#define FLAME_3_On 0x08



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
byte g_yFireState;

// Serial timeout counter
unsigned int g_uSerialTimeoutCounter;

void setup()
{
	Serial.begin(9600);
    
    // Init fire control pins
    pinMode(FIRE_0_PIN, OUTPUT);
    pinMode(FIRE_1_PIN, OUTPUT);
    pinMode(FIRE_2_PIN, OUTPUT);
    pinMode(FIRE_3_PIN, OUTPUT);
    
    // Init button states
    g_iOldButtons0 = 0;
    g_iOldButtons1 = 0;
    g_iButtons0 = 0;
    g_iButtons1 = 0;
    
    // Init fire states
    g_ySerialFireState = 0;
    g_yPad0FireState = 0;
    g_yPad1FireState = 0;
    g_yFireState = 0;
    
    // Init serial timeout counter
    g_uSerialTimeoutCounter = 0;
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
    yFireStateOut |= (iNewButtons & BTN_Y) ? FLAME_0_On : 0;
    yFireStateOut |= (iNewButtons & BTN_B) ? FLAME_1_On : 0;
    yFireStateOut |= (iNewButtons & BTN_X) ? FLAME_2_On : 0;
    yFireStateOut |= (iNewButtons & BTN_A) ? FLAME_3_On : 0;
    
    // D-Pad
    yFireStateOut |= (iNewButtons & BTN_LEFT)  ? FLAME_0_On : 0;
    yFireStateOut |= (iNewButtons & BTN_UP)    ? FLAME_1_On : 0;
    yFireStateOut |= (iNewButtons & BTN_DOWN)  ? FLAME_2_On : 0;
    yFireStateOut |= (iNewButtons & BTN_RIGHT) ? FLAME_3_On : 0;

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
    
    // Aggregate fire state
    byte yOldFireState = g_yFireState;
    g_yFireState = g_ySerialFireState | g_yPad0FireState | g_yPad1FireState;

    // Send fire state to output pins
    digitalWrite(FIRE_0_PIN, (g_yFireState & FLAME_0_On) ? HIGH : LOW);
    digitalWrite(FIRE_1_PIN, (g_yFireState & FLAME_1_On) ? HIGH : LOW);
    digitalWrite(FIRE_2_PIN, (g_yFireState & FLAME_2_On) ? HIGH : LOW);
    digitalWrite(FIRE_3_PIN, (g_yFireState & FLAME_3_On) ? HIGH : LOW);

    // Write current fire state to serial
    if(g_yFireState != yOldFireState)
    {
        Serial.print((g_yFireState & FLAME_0_On) ? "1" : "0");
        Serial.print((g_yFireState & FLAME_1_On) ? "1" : "0");
        Serial.print((g_yFireState & FLAME_2_On) ? "1" : "0");
        Serial.print((g_yFireState & FLAME_3_On) ? "1" : "0");
        Serial.println();
    }

	// Wait a bit
	delay(TICK_TIME_IN_MS);
}
