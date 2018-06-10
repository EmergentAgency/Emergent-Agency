/**
 * File: TouchtoneArduino.ino
 *
 * Description: Reading conductance of skin and sending it to the PC.
 * Also dimming AC lights based on skin reading.
 * 
 * Copyright: 2018 Chris Linder
 */

#include <TimerOne.h>                    

// Setting for using serial for debugging or communicating with the PC
bool bUseSerialForDebugging = true;

// The numbers of dimmers in this setup
#define NUM_DIMMERS 2

// Pin defines
#define SENSOR_PIN A8    // Input pin to read from
#define LED_OUT_PIN 10   // LED output pin to visualize current reading
#define PSSR1_PIN 4      // Dimmer 1
#define PSSR2_PIN 5      // Dimmer 2
#define ZERO_CROSS_PIN 2 // Zero cross pin

// PowerSSR Tails connected to digital pins 
static const int pssrPins[] = {PSSR1_PIN, PSSR2_PIN};

// Current sensor value
int g_iSensorValue = 0;

// Smoothied float value of the sensor
float g_fSmoothedSensor = 0;

// Lookup map used to linearize LED brightness
static const unsigned char exp_map[256]={
  0,  0,  1,  1,  1,  2,  2,  2,  3,  3,  3,  4,  4,  4,  5,  5,
  5,  6,  6,  6,  7,  7,  7,  8,  8,  8,  9,  9,  9,  10, 10, 10,
  11, 11, 11, 12, 12, 12, 13, 13, 13, 14, 14, 14, 15, 15, 15, 16,
  16, 16, 17, 17, 17, 18, 18, 18, 19, 19, 19, 20, 20, 20, 21, 21, 
  21, 22, 22, 22, 23, 23, 23, 24, 24, 24, 25, 25, 25, 26, 26, 26, 
  27, 27, 27, 28, 28, 28, 29, 29, 29, 30, 30, 30, 31, 31, 31, 32, 
  32, 32, 33, 33, 33, 34, 34, 34, 35, 35, 35, 36, 37, 37, 37, 38, 
  38, 38, 39, 39, 39, 40, 40, 40, 41, 41, 41, 42, 42, 42, 43, 43,
  43, 44, 44, 44, 45, 45, 45, 46, 46, 46, 47, 47, 47, 48, 48, 48, 
  49, 49, 49, 50, 50, 50, 51, 51, 51, 52, 52, 52, 53, 53, 53, 54,
  54, 54, 55, 55, 55, 56, 56, 56, 57, 57, 57, 58, 58, 58, 59, 59, 
  59, 60, 60, 60, 61, 61, 61, 62, 62, 62, 63, 63, 63, 64, 64, 64, 
  65, 66, 67, 69, 70, 72, 74, 75, 77, 79, 80, 82, 84, 86, 88, 90,
  91, 94, 96, 98, 100,102,104,107,109,111,114,116,119,122,124,127,
  130,133,136,139,142,145,148,151,155,158,161,165,169,172,176,180,
  184,188,192,196,201,205,210,214,219,224,229,234,239,244,250,255
};


// Dimmer vars

// Variable to use as a counter between zero crossing events. Runs from 0 to 127.
volatile int g_iDimmerCounter = 0;

// Flag set by the interupt when a zero cross on the AC power occurs.
volatile boolean g_bZeroCross = false;

// Default dimming level (0-127)  0 = off, 127 = on
int g_iDimmerBrightness[NUM_DIMMERS];

// This is 66 microseconds.  AC power runs at 60 hz and crosses zero twice in a since cycle.  Our brightness runs from 0 to 127 so there are 128 different
// brightness levels in a single half cycle.  This means our counter should count to 128 twice (256) every full power cycle.
// 1,000,000 microseconds / 60 / 256 = 65.1 microseconds
// And we rounded up to 66 because that ensures that we don't over count.
#define FREQ_STEP 66



// TEMP_CL - Add PC singal processing nodes from "InputGraph" program
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

  // TEMP_CL
  //float m_afPastBuffer[MAX_NUM_SAMPLES];
  //int m_iPastBufferIndex;

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

  /* - TEMP_CL - only used on PC to live tune values
  float GetExponentialSmoothingWeight()
  {
    return m_fExponentialSmoothingWeight;
  }

  int GetAvgSmoothingNumSamples()
  {
    return m_iAvgSmoothingNumSamples;
  }

  void AdjustSmoothing(float fExponentialSmoothingWeight, int iAvgSmoothingNumSamples)
  {
    m_fExponentialSmoothingWeight = fExponentialSmoothingWeight;
    m_iAvgSmoothingNumSamples = iAvgSmoothingNumSamples;
  }
  */

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

    // TENP_CL
    //// Past buffer - very much like avg buffer but not variable in number of samples
    // = (m_iPastBufferIndex + 1) % MAX_NUM_SAMPLES;
    //m_afPastBuffer[m_iPastBufferIndex] = m_fExponentialSmoothedInput;

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

/* TEMP_CL - I don't think we need this
  // 0 = new, 1 = one sample is the past
  float GetOutputInPast(int iHistoryIndex)
  {
    int iPastIndex = m_iPastBufferIndex - iHistoryIndex;
    while(iPastIndex < 0)
    {
      iPastIndex += MAX_NUM_SAMPLES;
    }
    return m_afPastBuffer[iPastIndex];
  }
  */
};



// Tuning vars for singal processing
static float g_fNode0Exp=0.0;
static int   g_iNode0Avg=2;
static float g_fNode1Exp=0.3;
static int   g_iNode1Avg=10;
static float g_fDetectThreshold=0.4;

// Signal processing node setup
ProcessingNode g_oNode0(NULL,     false, g_fNode0Exp, g_iNode0Avg, 1.0); // Input
ProcessingNode g_oNode1(&g_oNode0, true,  g_fNode1Exp, g_iNode1Avg, 0.6); // 1st serivative

bool g_bDetectOn = false;
float g_fTriggerVelocity = 0.0;

// Max of the smoothed input so far
float g_fMaxSmoothedInput = 0.1;

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
  }

  if(!g_bDetectOn)
  {
    g_fTriggerVelocity = 0;
  }
}



void setup()
{
	// setup debugging serial port (over USB cable) at 9600 bps
	Serial.begin(9600);

	if(bUseSerialForDebugging)
	{
		Serial.println("Touchtone - Setup");
	}

	// Setting the sensor pin as INPUT produces weird results so we're not doing that.
  // Weird results means that if the input level exceeds something around 512 then the analogRead
  // gets "stuck" and keeps returning a high value despite being grounded. This doesn't happen
  // if the pin mode is not set.
  // Also weird is that setting the pin to INPUT_PULLUP once and then not setting the next code push
  // Seem to have helped reduce the noise on the pin. I really really don't understand analog...
  //pinMode(SENSOR_PIN, INPUT);
  //pinMode(SENSOR_PIN, INPUT_PULLUP);

	// LED output pin
	pinMode(LED_OUT_PIN, OUTPUT);
	digitalWrite(LED_OUT_PIN, LOW);


	// Dimmer setup

	// Set PSSR pins as output and set initial brightness levels
  for(int i = 0; i < NUM_DIMMERS; ++i) 
  {
    pinMode(pssrPins[i], OUTPUT);
    g_iDimmerBrightness[i] = 0;
  }

	// Attach an Interupt to digital pin
  pinMode(ZERO_CROSS_PIN, INPUT);
  attachInterrupt(ZERO_CROSS_PIN, ZeroCrossDetect, RISING);

	// Init timer
	Timer1.initialize(FREQ_STEP);
	Timer1.attachInterrupt(DimmerCheck, FREQ_STEP);


}


void loop()
{
	// Read the current sensor value
  g_iSensorValue = analogRead(SENSOR_PIN); 

	if(bUseSerialForDebugging)
	{
		Serial.print("g_iSensorValue=");
		Serial.println(g_iSensorValue);
	}

  // Process the raw sensor value
  ProcessSignal();

  // TEMP_CL - show trigger for now
  if(g_bDetectOn)
  {
    if(bUseSerialForDebugging)
    {
      Serial.print("TOUCH!!!!!!!!!!!!!!!!!!!!!!!!!  ");
      Serial.print("g_fTriggerVelocity=");
      Serial.println(g_fTriggerVelocity);
    }
  }

	// Take raw sensor value and turn it unto a useful input value called fInput
	float fSensor = 0;
	if(g_iSensorValue > 100)
	{
		fSensor = g_iSensorValue / 1000.0;
		if(fSensor > 1.0)
		{
			fSensor = 1.0;
		}
	}
	float fSmoothAmount = 0.8;
	g_fSmoothedSensor = g_fSmoothedSensor * fSmoothAmount + fSensor * (1 - fSmoothAmount);
	float fInput = pow(g_fSmoothedSensor, 1.5);
 
  if(bUseSerialForDebugging)
  {
    Serial.print("fInput=");
    Serial.println(fInput);
  }

	// Set dimmer brightness
  g_iDimmerBrightness[0] = 0;
  g_iDimmerBrightness[1] = 0;
	if(fInput > 0.05)
	{
    g_iDimmerBrightness[0] = fInput * 107 + 20; // Incandecent lights don't turn on right away so we need to start above 0

    // TEMP_CL
    /*
    if(fInput < 0.5) {
      g_iDimmerBrightness[1] = fInput * 50 + 20; // Incandecent lights don't turn on right away so we need to start above 0
    }
    else {
      g_iDimmerBrightness[1] = (1.0-fInput) * 50 + 20; // Incandecent lights don't turn on right away so we need to start above 0
    }
    */
 	}
  float g_iDefaultBrightnessLight1 = 50; // TEMP_CL
  g_iDimmerBrightness[1] = g_iDefaultBrightnessLight1 + g_fTriggerVelocity * (127-g_iDefaultBrightnessLight1);

	if(bUseSerialForDebugging)
	{
		Serial.print("g_iDimmerBrightness=");
		Serial.println(g_iDimmerBrightness[0]);
	}

	// The output LED is based on fInput not g_iDimmerBrightness
	int iLEDValue = fInput * 255;

	// Adjust brightness to be more linear
	iLEDValue = exp_map[iLEDValue];

  // Write out the LED value
	analogWrite(LED_OUT_PIN, iLEDValue);

	// Send the raw sensor value to the PC Touchtone client. Cut off the buttom bits so it fits in a byte.
  if(!bUseSerialForDebugging)
  {
	  byte ySendByte = g_iSensorValue/4;
	  Serial.write(ySendByte);
  }

	// Delay a bit before reading again
	// Oddly if this is too short the analog read seems have more noise near zero.
	// Both 5 and 10 have worked well in the past
	delay(5);
}


// This function will fire the triac in the PSSR Tail at the proper time based on g_iDimmerBrightness
void DimmerCheck()
{
	// If we cross zero, reset the counter of how far along we are to the next zero cross.
  // The counter runs from 0 to 127.
	if(g_bZeroCross)
	{
    g_iDimmerCounter = 0;  
    g_bZeroCross = false;
	}

  // Loop through all dimmers
  for(int i = 0; i < NUM_DIMMERS; ++i) 
  {
    // If too much time has passed (g_iDimmerCounter >= 120), don't fire the tail at all. This is because if we fire too late,
    // it will get stuck on the whole next cycle and cause a flash in brightness.
    // The brighter the light should be, the sooner we should fire the tail to turn on for the rest of this cycle
		if( g_iDimmerCounter < 120)
		{
		  if (g_iDimmerCounter >= (127-g_iDimmerBrightness[i])) 
  		{
  			//These values will fire the PSSR Tail. 
  			digitalWrite(pssrPins[i], HIGH);
  		}
    }
    else {
      // Turn the tail off before the next zero cross. Even though we're setting the control pin low,
      // the tail will stay on till the next zero cross.
      digitalWrite(pssrPins[i], LOW);
    }
  }

  // Increment the counter
  ++g_iDimmerCounter;
}


void ZeroCrossDetect() 
{
	// Set the boolean to true to tell our dimming function that a zero cross has occured.
  // This will be reset in the dimming function
	g_bZeroCross = true;
} 

