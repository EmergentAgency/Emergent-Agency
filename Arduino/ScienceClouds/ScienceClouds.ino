// ScienceClouds.ino
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

// Audio includes
#include <Audio.h>
#include <Wire.h>
#include <SD.h>
#include <SPI.h>
#include <SerialFlash.h>
#include <Bounce.h>

// Radar includes
#include <avr/io.h>
#include <avr/interrupt.h>

// EEPROM includes
#include <EEPROM.h>


// LED
#define DATA_PIN 17

#define NUM_LEDS 1000 
#define MAX_HEAT 255
#define FRAMES_PER_SECOND 120

CRGB g_aLeds[NUM_LEDS];
byte g_ayHeat[NUM_LEDS];
byte g_ayLastHeat[NUM_LEDS]; // Used for interp between heat frames

// Define the color gradient
#define COLOR_GRAD_SIZE 20
byte g_ayColorGrad[] = {
	  0,     0,  0, 24,   
	 64,    64, 16,  0,   
	128,   128, 50,  8,   
	192,   255,128, 64,   
	255,   255,255,255
};
byte g_ayColorGradSaved[COLOR_GRAD_SIZE];
byte g_ayColorGradDefault[COLOR_GRAD_SIZE];

CRGBPalette256 g_oPalHeat;

#define COOLING_MIN  8
#define COOLING_MAX  22
#define SPARKING 130
#define SPARK_HEAT_MIN 75
#define SPARK_HEAT_MAX 150
#define SPARK_WIDTH 2

int g_iLedSpacing = 1;

// Color tuning

byte g_yBaseHeatMax = 96;
byte g_yBaseHeatMaxSaved = g_yBaseHeatMax;
byte g_yBaseHeatMaxDefault = g_yBaseHeatMax;

float g_fMotionHeadMult = 3.0;
float g_fMotionHeadMultSaved = g_fMotionHeadMult;
float g_fMotionHeadMultDefault = g_fMotionHeadMult;

byte g_yMotionHeatAdd = 230;
byte g_yMotionHeatAddSaved = g_yMotionHeatAdd;
byte g_yMotionHeatAddDefault = g_yMotionHeatAdd;

int g_iNumInterpFrames = 20;
int g_iNumInterpFramesSaved = g_iNumInterpFrames;
int g_iNumInterpFramesDefault = g_iNumInterpFrames;


// Audio

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

// True of the audio system and the SD card were initialized successfully
bool g_bAudioInitialized;


// Radar

// Interrupt pin for motion sensor
#define INTERRUPT_PIN 14

// The motion sensor indicates the speed of the motion detected by oscillating its output pin.
// The faster the pulses, the faster the motion.  The detect the speed, we are counting the
// number of microseconds between rising pulses using a hardware interrupt pin.  The interrupt
// is called each time a rising pulse is detect and we save off the between the new pulse and
// the last pulse.  All the other code for calculating speed is done outside of the interrupt
// because the interrupt code needs to be as light weight at possible so it returns execution
// to the main code as quickly as possible.

// This is the last period detected
volatile unsigned long g_iLastPeriodMicro = 0;

// This is the last time the interrupt was called
volatile unsigned long g_iLastTimeMicro = 0;

// TEMP_CL - Testing x-band
volatile unsigned long g_iPulseCount = 0;

// Tuning - The min speed in meters per second to respond to.  Any motion at or below this will be 
// considered no motion at all.
float g_fMinSpeed = 0.02;
float g_fMinSpeedSaved = g_fMinSpeed;
float g_fMinSpeedDefault = g_fMinSpeed;

// Tuning The max speed in meters per second.  All motion above this speed will be treated like this speed
float g_fMaxSpeed = 0.45;
float g_fMaxSpeedSaved = g_fMaxSpeed;
float g_fMaxSpeedDefault = g_fMaxSpeed;

// Tuning - This is the speed smoothing factor (0, 1.0].  Low values mean more smoothing while a value of 1 means 
// no smoothing at all.  This value depends on the loop speed so if anything changes the loop speed,
// the amount of smoothing will change (see LOOP_DELAY_MS).
//static float fNewSpeedWeight = 0.15;
float g_fNewSpeedWeight = 0.03;
float g_fNewSpeedWeightSaved = g_fNewSpeedWeight;
float g_fNewSpeedWeightDefault = g_fNewSpeedWeight;

// Tuning - The exponent to apply to the linear 0.0 to 1.0 value from the sensor.  This allows the sensitivity curve
// to be adjusted in a non-linear fashion.
float g_fInputExponent = 1.2;
float g_fInputExponentSaved = g_fInputExponent;
float g_fInputExponentDefault = g_fInputExponent;

// This is the raw speed ratio [0, 1.0].  It is based on the speed read from the motion detector
// and g_fMinSpeed, g_fMaxSpeed, g_fNewSpeedWeight.
float g_fRawSpeedRatio;

// This is the outout speed ratio [0, 1.0] after being adjusted with g_fInputExponent 
float g_fSpeedRatio;

// TEMP_CL
int g_iPulsesSinceLastTick = 0;

// Reading data from serial port

char g_acSerialInputBuffer [256];
int g_iSerialInputBufferPos = 0;


// EEPROM saved settigs

#define EEPROM_VERSION 2
#define EEPROM_ADDR_VER 0
#define EEPROM_ADDR_COLORS 2
#define EEPROM_ADDR_TUNING 100 // Make sure this is far enough along for larger color arrays



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
	// Setup serial
	Serial.begin(9600);
	
	
	// LEDs
	
	// WS2812Serial and FastLED
	LEDS.addLeds<WS2812SERIAL, DATA_PIN, BRG>(g_aLeds, NUM_LEDS);
	
	// Define color gradiant
	g_oPalHeat.loadDynamicGradientPalette(g_ayColorGrad);
	
	
	// Audio 
		
	// Wave file
	g_bAudioInitialized = false;
	SPI.setMOSI(SDCARD_MOSI_PIN);
	SPI.setSCK(SDCARD_SCK_PIN);
	if (!(SD.begin(SDCARD_CS_PIN)))
	{
		Serial.println("Unable to access the SD card. Audio not initialized.");
	}
	else
	{
		AudioMemory(8);
		sgtl5000_1.enable();
		sgtl5000_1.volume(0.5);
		Serial.print("Audio and SD card successfully initialized.");
		g_bAudioInitialized = true;
	}
	delay(1000);

	
	// Radar
	
	// Attach an Interrupt to INTERRUPT_PIN for timing period of motion detector input
	pinMode(INTERRUPT_PIN, INPUT);
	attachInterrupt(digitalPinToInterrupt(INTERRUPT_PIN), MotionDetectorPulse, CHANGE);   
	
	
	// EEPROM saved settigs
	
	// Save off the saved and default copies
	memcpy(g_ayColorGradSaved, g_ayColorGrad, COLOR_GRAD_SIZE);
	memcpy(g_ayColorGradDefault, g_ayColorGrad, COLOR_GRAD_SIZE);
	
	// Load from memory
	// If the load doesn't work, it's because there wasn't a save found
	// or the version has changed. Either way, take the current defaults
	// and save them.
	if(!load_eeprom_to_current_settings())
	{
		save_current_settings_to_eeprom();
	}
}


void update_speed() 
{
	// Get the cur
	float fRawSpeed = GetCurSpeed();

	// Clamp the range of the speed
	float fCurSpeed = Clamp(fRawSpeed, g_fMinSpeed, g_fMaxSpeed);

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
	if(isnan(g_fRawSpeedRatio))
	{
		g_fRawSpeedRatio = fNewSpeedRatio;
	}
	else
	{
		g_fRawSpeedRatio = fNewSpeedRatio * g_fNewSpeedWeight + g_fRawSpeedRatio * (1.0 - g_fNewSpeedWeight);
	}

	// Apply the input exponent to change the input curve.  This is the final output.
	g_fSpeedRatio = pow(g_fRawSpeedRatio, g_fInputExponent);
	
	// TEMP_CL
	float fPulsesSinceLastTick = g_iPulsesSinceLastTick / 10.0;
	g_iPulsesSinceLastTick = 0;
	
	// Send current status over the serial port either for debugging or talking to tuner program
	Serial.print("STATUS - ");
	Serial.print(micros());
	Serial.print(" -");
	Serial.print(" fRawSpeed=");
	Serial.print(fRawSpeed);
	Serial.print(" fCurSpeed=");
	Serial.print(fCurSpeed);
	Serial.print(" fNewSpeedRatio=");
	Serial.print(fNewSpeedRatio);
	Serial.print(" g_fRawSpeedRatio=");
	Serial.print(g_fRawSpeedRatio);
	Serial.print(" g_fSpeedRatio=");
	Serial.print(g_fSpeedRatio);
	Serial.print(" fPulsesSinceLastTick=");
	Serial.print(fPulsesSinceLastTick);
	Serial.println("");
}


void send_tuning_vars()
{
	// Send the tuning variables over the serial port
	Serial.print("TUNING -");
	Serial.print(" g_fMinSpeed=");
	Serial.print(g_fMinSpeed);
	Serial.print(" g_fMaxSpeed=");
	Serial.print(g_fMaxSpeed);
	Serial.print(" g_fNewSpeedWeight=");
	Serial.print(g_fNewSpeedWeight);
	Serial.print(" g_fInputExponent=");
	Serial.print(g_fInputExponent);
	Serial.print(" g_yBaseHeatMax=");
	Serial.print(g_yBaseHeatMax);
	Serial.print(" g_fMotionHeadMult=");
	Serial.print(g_fMotionHeadMult);
	Serial.print(" g_yMotionHeatAdd=");
	Serial.print(g_yMotionHeatAdd);
	Serial.print(" g_iNumInterpFrames=");
	Serial.print(g_iNumInterpFrames);
	Serial.println("");
}

void send_tuning_vars_saved()
{
	// Send the tuning variables over the serial port
	Serial.print("TUNING_SAVED -");
	Serial.print(" g_fMinSpeed=");
	Serial.print(g_fMinSpeedSaved);
	Serial.print(" g_fMaxSpeed=");
	Serial.print(g_fMaxSpeedSaved);
	Serial.print(" g_fNewSpeedWeight=");
	Serial.print(g_fNewSpeedWeightSaved);
	Serial.print(" g_fInputExponent=");
	Serial.print(g_fInputExponentSaved);
	Serial.print(" g_yBaseHeatMax=");
	Serial.print(g_yBaseHeatMaxSaved);
	Serial.print(" g_fMotionHeadMult=");
	Serial.print(g_fMotionHeadMultSaved);
	Serial.print(" g_yMotionHeatAdd=");
	Serial.print(g_yMotionHeatAddSaved);
	Serial.print(" g_iNumInterpFrames=");
	Serial.print(g_iNumInterpFramesSaved);
	Serial.println("");
}


void send_color_gradient()
{
	// Send the tuning variables over the serial port
	Serial.print("COLORS - ");
	for(int i = 0; i < COLOR_GRAD_SIZE; ++i) 
	{
		Serial.print(g_ayColorGrad[i]);
		if(i != COLOR_GRAD_SIZE - 1)
		{
			Serial.print(",");
		}
	}
	Serial.println("");
}

void send_color_gradient_saved()
{
	// Send the tuning variables over the serial port
	Serial.print("COLORS_SAVED - ");
	for(int i = 0; i < COLOR_GRAD_SIZE; ++i) 
	{
		Serial.print(g_ayColorGradSaved[i]);
		if(i != COLOR_GRAD_SIZE - 1)
		{
			Serial.print(",");
		}
	}
	Serial.println("");
}


void save_current_settings_to_eeprom()
{
	// First copy to the saved vars
	memcpy(g_ayColorGradSaved, g_ayColorGrad, COLOR_GRAD_SIZE);
	g_fMinSpeedSaved = g_fMinSpeed;
	g_fMaxSpeedSaved = g_fMaxSpeed;
	g_fNewSpeedWeightSaved = g_fNewSpeedWeight;
	g_fInputExponentSaved = g_fInputExponent;
	g_yBaseHeatMaxSaved = g_yBaseHeatMax;
	g_fMotionHeadMultSaved = g_fMotionHeadMult;
	g_yMotionHeatAddSaved = g_yMotionHeatAdd;
	g_iNumInterpFramesSaved = g_iNumInterpFrames;
	
	// Write the current version out first
	EEPROM.write(EEPROM_ADDR_VER, EEPROM_VERSION);
	
	// Write out the color array
	EEPROM.put(EEPROM_ADDR_COLORS, g_ayColorGradSaved);
	
	// Write out all the tuning vars
	int iCurTuningAddr = EEPROM_ADDR_TUNING;
	EEPROM.put(iCurTuningAddr, g_fMinSpeedSaved);
	iCurTuningAddr += sizeof(float);
	EEPROM.put(iCurTuningAddr, g_fMaxSpeedSaved);
	iCurTuningAddr += sizeof(float);
	EEPROM.put(iCurTuningAddr, g_fNewSpeedWeightSaved);
	iCurTuningAddr += sizeof(float);
	EEPROM.put(iCurTuningAddr, g_fInputExponentSaved);
	iCurTuningAddr += sizeof(float);
	EEPROM.put(iCurTuningAddr, g_yBaseHeatMaxSaved);
	iCurTuningAddr += sizeof(float);
	EEPROM.put(iCurTuningAddr, g_fMotionHeadMultSaved);
	iCurTuningAddr += sizeof(float);
	EEPROM.put(iCurTuningAddr, g_yMotionHeatAddSaved);
	iCurTuningAddr += sizeof(float);
	EEPROM.put(iCurTuningAddr, g_iNumInterpFramesSaved);
	iCurTuningAddr += sizeof(float);
}

bool load_eeprom_to_current_settings()
{
	if(load_eeprom())
	{
		memcpy(g_ayColorGrad, g_ayColorGradSaved, COLOR_GRAD_SIZE);
		g_oPalHeat.loadDynamicGradientPalette(g_ayColorGrad);

		g_fMinSpeed = g_fMinSpeedSaved;
		g_fMaxSpeed = g_fMaxSpeedSaved;
		g_fNewSpeedWeight = g_fNewSpeedWeightSaved;
		g_fInputExponent = g_fInputExponentSaved;
		g_yBaseHeatMax = g_yBaseHeatMaxSaved;
		g_fMotionHeadMult = g_fMotionHeadMultSaved;
		g_yMotionHeatAdd = g_yMotionHeatAddSaved;
		g_iNumInterpFrames = g_iNumInterpFramesSaved;
		
		return true;
	}
	
	return false;
}

bool load_eeprom()
{
	// Get the saved version.
	// If it doesn't match the saved version, don't load anything else.
	byte yCurVer = EEPROM.read(EEPROM_ADDR_VER);
	if(yCurVer != EEPROM_VERSION)
	{
		Serial.println("Wrong EEPROM version!");
		Serial.print("Got ");
		Serial.print(yCurVer);
		Serial.print(" when expecting ");
		Serial.println(EEPROM_VERSION);
		
		return false;
	}
	
	// Get the color array
	EEPROM.get(EEPROM_ADDR_COLORS, g_ayColorGradSaved);
	
	// Get all the tuning vars
	int iCurTuningAddr = EEPROM_ADDR_TUNING;
	EEPROM.get(iCurTuningAddr, g_fMinSpeedSaved);
	iCurTuningAddr += sizeof(float);
	EEPROM.get(iCurTuningAddr, g_fMaxSpeedSaved);
	iCurTuningAddr += sizeof(float);
	EEPROM.get(iCurTuningAddr, g_fNewSpeedWeightSaved);
	iCurTuningAddr += sizeof(float);
	EEPROM.get(iCurTuningAddr, g_fInputExponentSaved);
	iCurTuningAddr += sizeof(float);
	EEPROM.get(iCurTuningAddr, g_yBaseHeatMaxSaved);
	iCurTuningAddr += sizeof(float);
	EEPROM.get(iCurTuningAddr, g_fMotionHeadMultSaved);
	iCurTuningAddr += sizeof(float);
	EEPROM.get(iCurTuningAddr, g_yMotionHeatAddSaved);
	iCurTuningAddr += sizeof(float);
	EEPROM.get(iCurTuningAddr, g_iNumInterpFramesSaved);
	iCurTuningAddr += sizeof(float);
	
	return true;
}

void restore_defaults_to_current_settings()
{
	memcpy(g_ayColorGrad, g_ayColorGradDefault, COLOR_GRAD_SIZE);
	g_oPalHeat.loadDynamicGradientPalette(g_ayColorGrad);

	g_fMinSpeed = g_fMinSpeedDefault;
	g_fMaxSpeed = g_fMaxSpeedDefault;
	g_fNewSpeedWeight = g_fNewSpeedWeightDefault;
	g_fInputExponent = g_fInputExponentDefault;
	g_yBaseHeatMax = g_yBaseHeatMaxDefault;
	g_fMotionHeadMult = g_fMotionHeadMultDefault;
	g_yMotionHeatAdd = g_yMotionHeatAddDefault;
	g_iNumInterpFrames = g_iNumInterpFramesDefault;

}


void check_serial()
{
	static const char REQUEST_TUNING[] = "REQUEST_TUNING";
	static const char REQUEST_COLORS[] = "REQUEST_COLORS";
	static const char REQUEST_TUNING_SAVED[] = "REQUEST_TUNING_SAVED";
	static const char REQUEST_COLORS_SAVED[] = "REQUEST_COLORS_SAVED";
	static const char SAVE_CURRENT[] = "SAVE_CURRENT";
	static const char OVERWRITE_CURRENT_WITH_SAVED[] = "OVERWRITE_CURRENT_WITH_SAVED";
	static const char RESTORE_DEFAULTS[] = "RESTORE_DEFAULTS";
	static const char NEW_TUNING[] = "NEW_TUNING";
	static size_t NEW_TUNING_LEN = strlen(NEW_TUNING);
	static const char NEW_COLOR_GRAD[] = "NEW_COLOR_GRAD";
	static size_t NEW_COLOR_GRAD_LEN = strlen(NEW_COLOR_GRAD);
	
	// Check to see if tuning variables have been requested
	while(Serial.available() > 0)
	{
        char c = Serial.read();
		//Serial.print("TEMP_CL - got char:");
		//Serial.println(c);
        if ((c == '\n') || (g_iSerialInputBufferPos == sizeof(g_acSerialInputBuffer)-1))
		{
			g_acSerialInputBuffer[g_iSerialInputBufferPos] = '\0';
            g_iSerialInputBufferPos = 0;
			
			Serial.print("Got serial input:");
			Serial.println(g_acSerialInputBuffer);
			Serial.flush();
			
			if(strcmp(g_acSerialInputBuffer, REQUEST_TUNING) == 0)
			{
				send_tuning_vars();
			}
			else if(strcmp(g_acSerialInputBuffer, REQUEST_COLORS) == 0)
			{
				send_color_gradient();
			}
			if(strcmp(g_acSerialInputBuffer, REQUEST_TUNING_SAVED) == 0)
			{
				send_tuning_vars_saved();
			}
			else if(strcmp(g_acSerialInputBuffer, REQUEST_COLORS_SAVED) == 0)
			{
				send_color_gradient_saved();
			}
			if(strcmp(g_acSerialInputBuffer, SAVE_CURRENT) == 0)
			{
				save_current_settings_to_eeprom();
				load_eeprom_to_current_settings();
				send_tuning_vars();
				send_color_gradient();
				send_tuning_vars_saved();
				send_color_gradient_saved();
			}
			else if(strcmp(g_acSerialInputBuffer, OVERWRITE_CURRENT_WITH_SAVED) == 0)
			{
				load_eeprom_to_current_settings();
				send_tuning_vars();
				send_color_gradient();
				send_tuning_vars_saved();
				send_color_gradient_saved();
			}
			else if(strcmp(g_acSerialInputBuffer, RESTORE_DEFAULTS) == 0)
			{
				restore_defaults_to_current_settings();
				send_tuning_vars();
				send_color_gradient();
				send_tuning_vars_saved();
				send_color_gradient_saved();
			}
			// Example = "NEW_TUNING MaxSpeed=0.25"
			else if(strncmp(g_acSerialInputBuffer, NEW_TUNING, NEW_TUNING_LEN) == 0)
			{
				strtok(g_acSerialInputBuffer, " "); // Read off the NEW_TUNING tag
				char *pTuningKey = NULL;
				char *pTuningValue = NULL;
				
				if((pTuningKey = strtok(NULL, "=")) != NULL && (pTuningValue = strtok(NULL, " ")) != NULL)
				{
					Serial.println(pTuningKey);
					Serial.flush();
					Serial.println(pTuningValue);
					Serial.flush();
					
					// Test if valid float
					char *pEndPtr;
					float fValue = strtof(pTuningValue, &pEndPtr);
					bool bValidFloat = *pEndPtr == '\0';
					if(!bValidFloat)
					{
						Serial.print("Invalid float format! ");
						Serial.println(pTuningValue);
					}
					else 
					{
						bool bGotNewValue = false;
						if(strcmp(pTuningKey, "MinSpeed") == 0 && bValidFloat)
						{
							g_fMinSpeed = fValue;
							bGotNewValue = true;
						}
						else if(strcmp(pTuningKey, "MaxSpeed") == 0 && bValidFloat)
						{
							g_fMaxSpeed = fValue;
							bGotNewValue = true;
						}
						else if(strcmp(pTuningKey, "NewSpeedWeight") == 0 && bValidFloat)
						{
							g_fNewSpeedWeight = fValue;
							bGotNewValue = true;
						}
						else if(strcmp(pTuningKey, "InputExponent") == 0 && bValidFloat)
						{
							g_fInputExponent = fValue;
							bGotNewValue = true;
						}
						else if(strcmp(pTuningKey, "BaseHeatMax") == 0 && bValidFloat)
						{
							g_yBaseHeatMax = fValue;
							bGotNewValue = true;
						}
						else if(strcmp(pTuningKey, "MotionHeadMult") == 0 && bValidFloat)
						{
							g_fMotionHeadMult = fValue;
							bGotNewValue = true;
						}
						else if(strcmp(pTuningKey, "MotionHeatAdd") == 0 && bValidFloat)
						{
							g_yMotionHeatAdd = fValue;
							bGotNewValue = true;
						}
						else if(strcmp(pTuningKey, "NumInterpFrames") == 0 && bValidFloat)
						{
							g_iNumInterpFrames = fValue;
							bGotNewValue = true;
						}
						
						if(bGotNewValue) 
						{
							Serial.print("Got new tunning: ");
							Serial.print(pTuningKey);
							Serial.print("=");
							Serial.println(fValue);
						}
						else
						{
							Serial.print("Unrecognised tuning var: ");
							Serial.println(pTuningKey);
						}
					}
				}
				else
				{
					Serial.println("Invalid tuning format! Should be 'NEW_TUNING MaxSpeed=0.25'.");
				}
			}
			// Color gradiant
			else if(strncmp(g_acSerialInputBuffer, NEW_COLOR_GRAD, NEW_COLOR_GRAD_LEN) == 0)
			{
				strtok(g_acSerialInputBuffer, " "); // Read off the NEW_TUNING tag
				char *pTuningKey = NULL;
				
				if((pTuningKey = strtok(NULL, "=")) != NULL)
				{
					Serial.println(pTuningKey);
					Serial.flush();
					
					char *pTuningValue = NULL;
					bool bGotValidColorGrad = true;
					byte ayColorGrad[COLOR_GRAD_SIZE];
					for(int i = 0; i < COLOR_GRAD_SIZE; ++i) 
					{
						pTuningValue = strtok(NULL, ",");
						if(pTuningValue == NULL)
						{
							Serial.print("Invalid color gradiant format - Didn't get ");
							Serial.print(COLOR_GRAD_SIZE);
							Serial.println(" bytes");
							bGotValidColorGrad = false;
							break;
						}
						
						// Test if valid byte
						char *pEndPtr;
						long int iValue = strtol(pTuningValue, &pEndPtr, 10);
						bool bValidInt = *pEndPtr == '\0';
						if(!bValidInt)
						{
							Serial.println("Invalid color gradiant format - Couldn't parse number");
							bGotValidColorGrad = false;
							break;
						}
						bool bValidByte = iValue >= 0 && iValue <= 255;
						if(!bValidByte)
						{
							Serial.println("Invalid color gradiant format - Data out of range");
							bGotValidColorGrad = false;
							break;
						}
						
						ayColorGrad[i] = (byte)iValue;
					}
					
					// Sanity check values
					if(bGotValidColorGrad &&
					   (ayColorGrad[0] != 0 ||
					    ayColorGrad[COLOR_GRAD_SIZE - 4] != 255))
					{
						Serial.println("Invalid color gradiant format - Bad first or last entry");
						bGotValidColorGrad = false;
					}
					
					if(bGotValidColorGrad)
					{
						bool bGotNewValue = false;
						if(strcmp(pTuningKey, "PalHeat") == 0)
						{
							memcpy(g_ayColorGrad, ayColorGrad, COLOR_GRAD_SIZE);
							g_oPalHeat.loadDynamicGradientPalette(g_ayColorGrad);
							bGotNewValue = true;
						}
						
						if(bGotNewValue) 
						{
							Serial.print("Got new color gradiant for: ");
							Serial.println(pTuningKey);
						}
						else
						{
							Serial.print("Unrecognised tuning var: ");
							Serial.println(pTuningKey);
						}
					}
				}
				else
				{
					Serial.println("Invalid color gradiant format - Should be:");
					Serial.println("NEW_COLOR_GRAD PalHeat=0,0,0,8,50,32,8,4,100,128,64,32,200,255,200,64,255,255,255,255");
				}
			}
		}
		else 
		{
			g_acSerialInputBuffer[g_iSerialInputBufferPos++] = c;
		}
	}
}



void loop()
{	
	// Make sure wav file is playing
	if(g_bAudioInitialized && playSdWav1.isPlaying() == false)
	{
		Serial.println("Start playing");
		//playSdWav1.play("SDTEST2.WAV");
		//playSdWav1.play("elaborate_thunder-Mike_Koenig-1877244752.WAV");
		playSdWav1.play("thunder.wav");
		delay(10); // wait for library to parse WAV info
	}
	


	// Add entropy to random number generator; we use a lot of it.
	// TEMP_CL random16_add_entropy( random());

	memcpy(g_ayLastHeat, g_ayHeat, NUM_LEDS);
	UpdateHeat(); // run simulation frame, using palette colors
  
	for(int i = 0; i < g_iNumInterpFrames; ++i)
	{
		// Serial input
		check_serial();
		
		// Radar
		update_speed();
		
		// Adjust sound volume based on motion
		if(g_bAudioInitialized) {
			sgtl5000_1.volume(g_fSpeedRatio / 2.0 + 0.05);
		}

		
		//LEDs

		// Lerp between the target frames
		fract8 fLerp = i * 256 / g_iNumInterpFrames;
		byte yLerpHeat;
		for(int j = 0; j < NUM_LEDS / g_iLedSpacing; ++j)
		{
			// Fade between last and cur heat values
			yLerpHeat = lerp8by8(g_ayLastHeat[j], g_ayHeat[j], fLerp);
			
			// TEMP_CL - Scale heat
			yLerpHeat = scale8(yLerpHeat, g_yBaseHeatMax);

			// Account for touch
			yLerpHeat = byte(Clamp(yLerpHeat * (g_fSpeedRatio * (g_fMotionHeadMult-1.0) + 1.0), 0, 255));
			yLerpHeat = qadd8(yLerpHeat, g_fSpeedRatio * g_yMotionHeatAdd);

			// Scale heat
			yLerpHeat = scale8(yLerpHeat, MAX_HEAT);
			
			// Get the heat color
			g_aLeds[j*g_iLedSpacing] = ColorFromPalette(g_oPalHeat, yLerpHeat);
						
		}
		FastLED.show(); // display this frame
		
		// Delay a consistent amount of time so this code basically works the same on different chips
		//FastLED.delay(1000 / FRAMES_PER_SECOND); // This doesnt seem to work on the Teensy 4.0
		delay(1000 / FRAMES_PER_SECOND);
	}
}



void UpdateHeat()
{
	// Cool down every cell a little
	for(int i = 0; i < NUM_LEDS; i++)
	{
		g_ayHeat[i] = qsub8( g_ayHeat[i],  random8(COOLING_MIN, COOLING_MAX));
	}
	
	// Heat from each cell drifts out
	for(int i = 1; i < NUM_LEDS-1; i++)
	{
		g_ayHeat[i] = (g_ayHeat[i]*3 + g_ayHeat[i-1]*2 + g_ayHeat[i+1]*2) / 7;
	}
	
	// Randomly ignite new 'sparks' of heat - per 10 leds
	for(int i = 0; i < NUM_LEDS / 10; i++) {
		if( random8() < SPARKING )
		{
			int y = random16(0, NUM_LEDS - SPARK_WIDTH);

			byte newHeat = random8(SPARK_HEAT_MIN, SPARK_HEAT_MAX);
			for(int j = 0; j < SPARK_WIDTH; ++j) {
				g_ayHeat[y+j] = qadd8( g_ayHeat[y+j],  newHeat);
			}
		}
	}
}



// Interrupt called to get time between pulses
void MotionDetectorPulse()
{
	unsigned long iCurTimeMicro = micros();
	
	// HACK - For some reason the interrupt is getting called twice is
	// in very quick succession. This prevents the timing from being 
	// effected by that.
	unsigned long iPeriod = iCurTimeMicro - g_iLastTimeMicro;
	if(iPeriod > 100) 
	{
		g_iLastPeriodMicro = (iPeriod + g_iLastPeriodMicro) / 2;
		g_iLastTimeMicro = iCurTimeMicro;
	
		// TEMP_CL
		++g_iPulsesSinceLastTick;
	}
}



// Returns the number of micro seconds for the last period (time between pulses)
// for the motion sensor.  If the time between this call and
// the last inpterupt call is greater than the last period , we return the time
// between this call and the the last interrupt.  This is to ensure that if we
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


