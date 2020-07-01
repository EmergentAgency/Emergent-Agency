// BarLights.ino
//
// copyright 2020, Christopher Linder
//
// Works with:
// * Arduino 1.8.12
// * Teensyduino 1.52
// * Teensy 3.2
// * FastLED version 3.003.001



// Helps with long strings of LEDs
#define FASTLED_ALLOW_INTERRUPTS 0
#ifdef __AVR__
  #include <avr/power.h>
#endif

// Fast LED lib
#include <FastLED.h>

// Use Serial to print out debug statements
#define USE_SERIAL_FOR_DEBUGGING true

// LED data pin
#define DATA_PIN 7

// Number of LEDs
#define NUM_LEDS 120

// Input pins for touch 
#define SENSOR_PIN_CONTROL A3    // Touch pin to turn the system on and off and adjust things
#define SENSOR_PIN_TOUCH   A1    // Touchtone pin

// Global tuning for touch
#define MIN_SENSOR_VALUE 90
#define MIN_INPUT_VALUE 10
#define MIN_CONTROL_ON 300

// Global tuning for LED heat effect
#define MAX_HEAT 240 // Don't go above 240
#define FRAMES_PER_SECOND 120
#define NUM_INTERP_FRAMES 20
#define COOLING_MIN  5
#define COOLING_MAX  15
#define SPARKING 130
#define SPARK_HEAT_MIN 50
#define SPARK_HEAT_MAX 100
#define SPARK_WIDTH 2

// LEDs
CRGB g_aLeds[NUM_LEDS];

// Heat vars
byte g_ayHeat[NUM_LEDS];
byte g_ayLastHeat[NUM_LEDS]; // Used for interp between heat frames

//// Bar light colors - 1 - more white
//DEFINE_GRADIENT_PALETTE( g_oHeatGP ) {
//  0,     8,  0,  2,   
//50,     64,  0, 16,   
//100,   128,  8, 64,   
//200,   255, 64,128,   
//240,   255,100,200, 
//255,   255,255,255 }; //junk value

// Bar light colors - 2 - more amber
DEFINE_GRADIENT_PALETTE( g_oHeatGP ) {
  0,     8,  0,  2,   
50,     64,  0, 16,   
100,   128,  0, 64,   
200,   255,  0,128,   
240,   255,  0,200, 
255,   255,  0,255 }; //junk value

//// R (amber) channel on white LEDs
//DEFINE_GRADIENT_PALETTE( g_oHeatGP ) {
//  0,     8,  0,  0,   
//50,     64,  0,  0,   
//100,   128,  0,  0,   
//200,   255,  0,  0,   
//240,   255,  0,  0, 
//255,   255,  0,  0 }; //junk value

//// G (cool white) channel on white LEDs
//DEFINE_GRADIENT_PALETTE( g_oHeatGP ) {
//  0,     0,  8,  0,   
//50,      0, 64,  0,   
//100,     0,128,  0,   
//200,     0,255,  0,   
//240,     0,255,  0, 
//255,     0,255,  0 }; //junk value

//// B (warm white) channel on white LEDs
//DEFINE_GRADIENT_PALETTE( g_oHeatGP ) {
//  0,     0,  0,  8,   
//50,      0,  0, 64,   
//100,     0,  0,128,   
//200,     0,  0,255,   
//240,     0,  0,255, 
//255,     0,  0,255 }; //junk value

// Heat palette
CRGBPalette256 g_oPalHeat = g_oHeatGP;


// Cool white gradiant
DEFINE_GRADIENT_PALETTE( g_oTouchGP ) {
  0,     0,  0,  0,   
50,      0, 32,  0,   
100,     0,128,  0,   
200,     0,255,  0,   
240,     0,255,  0, 
255,   255,255,255 }; //junk value

// Touch palette
CRGBPalette256 g_oPalTouch = g_oTouchGP;


// Touch values

// Heat array for quick touches
byte g_ayTouchHeat[NUM_LEDS];

// Tuning vars for touch light
#define TOUCH_TRIGGER_FRAMES 10
#define MIN_TOUCH_BRIGHTNESS 0.3
//#define DEAFULT_BRIGHTNESS_LIGHT_1 50
#define TOUCH_SMOOTH_ATTACK 0.80
#define TOUCH_SMOOTH_DECAY 0.95

// Tuning vars for singal processing
static float g_fNode0Exp=0.0;
static int   g_iNode0Avg=2;
static float g_fNode1Exp=0.3;
static int   g_iNode1Avg=10;
static float g_fDetectThreshold=0.4;

// PC singal processing nodes from "InputGraph" program
#define MAX_NUM_SAMPLES 30

class ProcessingNode
{
  protected:
  
  class ProcessingNode* m_pNodeInput;
  boolean m_bDerivative;
  float m_fExponentialSmoothingWeight;
  int m_iAvgSmoothingNumSamples;
  float m_fScalingFactor;

  float m_fInput;
  float m_fProcessingVar;
  float m_fExponentialSmoothedInput;
  float m_fAvgSmoothedInput;

  float m_afAvgBuffer[MAX_NUM_SAMPLES];
  int m_iBufferIndex;
  float m_fOutput;
  
  public:
  
  ProcessingNode(class ProcessingNode* pNodeInput, boolean bDerivative, float fExponentialSmoothingWeight, int iAvgSmoothingNumSamples, float fScalingFactor)
  {
    m_pNodeInput = pNodeInput;
    m_bDerivative = bDerivative;
    m_fExponentialSmoothingWeight = fExponentialSmoothingWeight;
    m_iAvgSmoothingNumSamples = iAvgSmoothingNumSamples;
    m_fScalingFactor = fScalingFactor;

    m_iBufferIndex = 0;
  }

  void UpdateFromInput(int iDeltaTimeMS, float fInput)
  {
    if(m_bDerivative)
    {
      float fOldInput = m_fInput;
      m_fInput = fInput;
      m_fProcessingVar = (m_fInput - fOldInput) / iDeltaTimeMS * 1000.0;
    }
    else
    {
      m_fProcessingVar = fInput;
    }

    // Smooth the input using a Exponential moving average
    if(m_fExponentialSmoothingWeight > 0)
    {
      m_fExponentialSmoothedInput = m_fProcessingVar * (1.0 - m_fExponentialSmoothingWeight) + m_fExponentialSmoothedInput * m_fExponentialSmoothingWeight;
    }
    else
    {
      m_fExponentialSmoothedInput = m_fProcessingVar;
    }

    // Smooth the input using the last n samples (which have already been smoothed with the Exponential smoothing)
    if(m_iAvgSmoothingNumSamples > 1)
    {
      m_iBufferIndex = (m_iBufferIndex + 1) % m_iAvgSmoothingNumSamples;
      m_afAvgBuffer[m_iBufferIndex] = m_fExponentialSmoothedInput;
      m_fAvgSmoothedInput = 0;
      for(int i = 0; i < m_iAvgSmoothingNumSamples; ++i)
      {
        m_fAvgSmoothedInput += m_afAvgBuffer[i];
      }
      m_fAvgSmoothedInput /= m_iAvgSmoothingNumSamples;
    }
    else
    {
      m_fAvgSmoothedInput = m_fExponentialSmoothedInput;
    }

    // The output is the result of both types of smoothing scaled by the scaling factor
    m_fOutput = m_fAvgSmoothedInput * m_fScalingFactor;
  }

  void Update(int iDeltaMS)
  {
    UpdateFromInput(iDeltaMS, m_pNodeInput->GetOutput());
  }

  float GetOutput()
  {
    return m_fOutput;
  }
};

// Signal processing node setup
ProcessingNode g_oNode0(NULL,     false, g_fNode0Exp, g_iNode0Avg, 1.0); // Input
ProcessingNode g_oNode1(&g_oNode0, true,  g_fNode1Exp, g_iNode1Avg, 0.6); // 1st serivative

// Touch vars
bool g_bDetectOn = false; // If we are actively detecting a touch. This can be TRUE for multiple frames in a row.
bool g_bTriggerNow = false; // This is only true for the first frame of detecting a touch.
float g_fTriggerVelocity = 0.0; // The velocity (0.0-1.0) of the last touch
float g_fMaxSmoothedInput = 0.1; // Max of the smoothed input so far which is used to set g_fTriggerVelocity

// Touch light vars
float g_fTouchBrightness = 0.0; 
float g_iTriggerCountdown = 0;
float g_fTargetTouchBrightness = 0;
float g_fTouchInput = 0;
bool g_bLedsOn = true;
bool g_bLastControlTouched = false;

// Current sensor value
int g_iSensorValue = 0;

// Smoothied float value of the sensor
float g_fSmoothedSensor = 0;



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



// This reads the input nodes and sets g_bDetectOn, g_bTriggerNow, g_fTriggerVelocity, and g_fMaxSmoothedInput
void ProcessSignal()
{
  float fRawInput = float(g_iSensorValue) / 1024.0; 

  // TEMP_CL - This doesn't match the main loop time right now but I'm just keeping the old value from the PC program for now
  int iDeltaTimeMS = 10; // TEMP_CL - we might actually get better more consistent results assuming a fixed timestep as opposed to having two loops (the microcontroller and the PC) that sometimes don't sync well.
  
  // Update nodes
  g_oNode0.UpdateFromInput(iDeltaTimeMS, fRawInput);
  g_oNode1.Update(iDeltaTimeMS);
  
  // Simple detection for note on
  float fSmoothedInput = g_oNode0.GetOutput();
  float fSmoothedFirstDeriv = g_oNode1.GetOutput();
  
  g_bTriggerNow = false;
  boolean bOldDetectOn = g_bDetectOn;
  g_bDetectOn = fSmoothedFirstDeriv > g_fDetectThreshold;
  if(!bOldDetectOn && g_bDetectOn)
  {
    // TODO - Find some way to recalibrate this over time as opposed to restarting the app
    if(fSmoothedInput > g_fMaxSmoothedInput)
    {
      g_fMaxSmoothedInput = fSmoothedInput;
    }
  
    g_fTriggerVelocity = ClampF(fSmoothedInput / (g_fMaxSmoothedInput * 0.9), 0.0, 1.0);
    g_bTriggerNow = true;
  }
}



void setup()
{
	// Works with Teensy 3.2
	FastLED.addLeds<NEOPIXEL, DATA_PIN>(g_aLeds, NUM_LEDS);
	
	// Works with Teensy 4.0? Nope... :(
	//FastLED.addLeds<1, SK6812, DATA_PIN, GRB>(g_aLeds, NUM_LEDS);
}



void AddSpark(byte ayHeat[], int iMin = 0, int iMax = NUM_LEDS, int iWidth = SPARK_WIDTH, byte ySparkHeatMin = SPARK_HEAT_MIN, byte ySparkHeatMax = SPARK_HEAT_MAX) 
{
	int y = random8(iMin, iMax - iWidth);

	byte yNewHeat = random8(ySparkHeatMin, ySparkHeatMax);
	for(int j = 0; j < iWidth; ++j)
	{
		ayHeat[y+j] = qadd8( ayHeat[y+j],  yNewHeat);
	}
}



void UpdateHeat(byte ayHeat[], bool bAddSparks)
{
	// Cool down every cell a little
	for(int i = 0; i < NUM_LEDS; i++)
	{
		ayHeat[i] = qsub8(ayHeat[i],  random8(COOLING_MIN, COOLING_MAX));
	}
	 
	// Heat from each cell drifts out
	for(int i = 1; i < NUM_LEDS-1; i++)
	{
		ayHeat[i] = (ayHeat[i]*3 + ayHeat[i-1]*2 + ayHeat[i+1]*2) / 7;
	}
	
	// Randomly ignite new 'sparks' of heat - per 10 leds
	if(bAddSparks)
	{
		for(int i = 0; i < NUM_LEDS / 10; i++)
		{
			if(random8() < SPARKING )
			{
				// TEMP_CL
				//int y = random8(0, NUM_LEDS - SPARK_WIDTH);
                //
				//byte yNewHeat = random8(SPARK_HEAT_MIN, SPARK_HEAT_MAX);
				//for(int j = 0; j < SPARK_WIDTH; ++j)
				//{
				//	ayHeat[y+j] = qadd8( ayHeat[y+j],  yNewHeat);
				//}
				
				AddSpark(ayHeat);
			}
		}
	}
}



void UpdateFromTouchInput()
{
	// Turn lights off and on based on control pin
	bool bControlTouched = analogRead(SENSOR_PIN_CONTROL) > MIN_CONTROL_ON;
	if(bControlTouched & !g_bLastControlTouched)
	{
		if(g_bLedsOn) 
		{
			g_bLedsOn = false;
		}
		else
		{
			g_bLedsOn = true;
		}
	}
	g_bLastControlTouched = bControlTouched;
	
	// Read raw touch input value
	g_iSensorValue = analogRead(SENSOR_PIN_TOUCH);

	// Sometimes this system has noise, particularlly if people are near the touch lights
	// and are only holding the sensor read handled (as opposed to the postive voltage handle).
	// This helps removed that noise.
	if(g_iSensorValue < MIN_SENSOR_VALUE)
	{
		g_iSensorValue = 0;
	}
	else
	{
		g_iSensorValue = g_iSensorValue - MIN_SENSOR_VALUE;
	}
	
	// Process the raw sensor value
	ProcessSignal();

	// Show touches
	if(g_bTriggerNow)
	{
		if(USE_SERIAL_FOR_DEBUGGING)
		{
			Serial.print("TOUCH!!!!!!!!!!!!!!!!!!!!!!!!!  ");
			Serial.print("g_fTriggerVelocity=");
			Serial.println(g_fTriggerVelocity);
		}
		
		AddSpark(g_ayTouchHeat, NUM_LEDS/2, NUM_LEDS, 10, 255, 255);
	}
	
	// Take raw sensor value and turn it unto a useful input value called fInput
	float fSensor = 0;
	if(g_iSensorValue > MIN_INPUT_VALUE)
	{
		fSensor = g_iSensorValue / 1000.0;
		if(fSensor > 1.0)
		{
			fSensor = 1.0;
		}
	}
	float fSmoothAmount = 0.8;
	g_fSmoothedSensor = g_fSmoothedSensor * fSmoothAmount + fSensor * (1 - fSmoothAmount);
	g_fTouchInput = pow(g_fSmoothedSensor, 1.5);
	
	if(USE_SERIAL_FOR_DEBUGGING && false) 
	{
		Serial.print("g_fTouchInput=");
		Serial.println(g_fTouchInput);
	}
}



void loop()
{
	// Save off the last heat value to interp between
	memcpy(g_ayLastHeat, g_ayHeat, NUM_LEDS);

	// Update the head simulation 
	UpdateHeat(g_ayHeat, true);
  
	for(int i = 0; i < NUM_INTERP_FRAMES; ++i)
	{
		// Update global values based on touch input
		UpdateFromTouchInput();

		// Update the head simulation 
		UpdateHeat(g_ayTouchHeat, false);
 		
		// If the LEDs are off, we don't need to do anything else.
		if(!g_bLedsOn)
		{
			FastLED.setBrightness(0);
		}
		else
		{
			// If the LEDs aren't off, we should adjust the brightness
			FastLED.setBrightness(191 + g_fTouchInput * 64);
			//FastLED.setBrightness(64 + g_fTouchInput * 191);
			//FastLED.setBrightness(192);

			// Lerp between the target frames
			fract8 fLerp = i * 256 / NUM_INTERP_FRAMES;
			byte yLerpHeat;
			for(int j = 0; j < NUM_LEDS; ++j)
			{
				// Fade between last and cur heat values
				yLerpHeat = lerp8by8(g_ayLastHeat[j], g_ayHeat[j], fLerp);
				
				// Add based on touch
				yLerpHeat = qadd8(yLerpHeat, g_fTouchInput * 128);

				// Scale heat
				yLerpHeat = scale8(yLerpHeat, MAX_HEAT);
				
				// Get the heat color
				CRGB color0 = ColorFromPalette(g_oPalHeat, yLerpHeat);
				
				// Get the touch color
				//CRGB color1 = ColorFromPalette(g_oPalTouch, g_fTouchInput * MAX_HEAT);
				CRGB color1 = ColorFromPalette(g_oPalTouch, scale8(g_ayTouchHeat[j], MAX_HEAT));
				
				// Blend them together
				g_aLeds[j] = color0 + color1;
			}
		}

		// Update the LEDs
		FastLED.show();
		
		// Keep a regular framerate
		FastLED.delay(1000 / FRAMES_PER_SECOND);
	}
}
