/**
 * File: TouchtonePC.pde
 *
 * Description: PC code for Touchtone
 *
 * Copyright: 2018 Chris Linder
 */
 
import processing.serial.*;

static boolean LOOK_FOR_JOAN_SERIAL = true;
static int JOAN_FIRE_PATTERN = 0x09;
static float JOAN_FIRE_THRESHOLD = 0.3f;

static int MAX_NUM_SAMPLES = 30;

// Settings to cut and paste from output
//static float g_fNode0Exp=0.0;
//static int   g_iNode0Avg=5;
//static float g_fNode1Exp=0.0;
//static int   g_iNode1Avg=5;
//static float g_fNode2Exp=0.0;
//static int   g_iNode2Avg=5;
//static float g_fMinInput=0.07;
//static float g_fMaxInput=0.14;

//static float g_fNode0Exp=0.0;
//static int   g_iNode0Avg=5;
//static float g_fNode1Exp=0.1;
//static int   g_iNode1Avg=6;
//static float g_fNode2Exp=0.0;
//static int   g_iNode2Avg=5;
//static float g_fMinInput=0.16000001;
//static float g_fMaxInput=0.23000005;

//// With a capacitor
//static float g_fNode0Exp=0.0;
//static int   g_iNode0Avg=2;
//static float g_fNode1Exp=0.3;
//static int   g_iNode1Avg=4;
//static float g_fNode2Exp=0.0;
//static int   g_iNode2Avg=5;
//static float g_fMinInput=0.8;
//static float g_fMaxInput=0.6;

//// No capacitor 
//static float g_fNode0Exp=0.0;
//static int   g_iNode0Avg=2;
//static float g_fNode1Exp=0.3;
//static int   g_iNode1Avg=10;
//static float g_fNode2Exp=0.0;
//static int   g_iNode2Avg=5;
//static float g_fMinInput=0.5;
//static float g_fMaxInput=0.3;

// No capacitor w/ person
static float g_fNode0Exp=0.0;
static int   g_iNode0Avg=2;
static float g_fNode1Exp=0.3;
static int   g_iNode1Avg=10;
static float g_fNode2Exp=0.0;
static int   g_iNode2Avg=5;
static float g_fMinInput=0.4;
static float g_fMaxInput=0.4;

//// No ground attached 
//static float g_fNode0Exp=0.5499999;
//static int   g_iNode0Avg=9;
//static float g_fNode1Exp=0.0;
//static int   g_iNode1Avg=15;
//static float g_fNode2Exp=0.0;
//static int   g_iNode2Avg=5;
//static float g_fMinInput=0.4000001;
//static float g_fMaxInput=0.15;

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



class ProcessingNode
{
	ProcessingNode(ProcessingNode oNodeInput, boolean bDerivative, float fExponentialSmoothingWeight, int iAvgSmoothingNumSamples, float fScalingFactor)
	{
		m_oNodeInput = oNodeInput;
		m_bDerivative = bDerivative;
		m_fExponentialSmoothingWeight = fExponentialSmoothingWeight;
		m_iAvgSmoothingNumSamples = iAvgSmoothingNumSamples;
		m_fScalingFactor = fScalingFactor;

		m_afPastBuffer = new float[MAX_NUM_SAMPLES];
		m_iPastBufferIndex = 0;

		m_afAvgBuffer = new float[MAX_NUM_SAMPLES];
		m_iBufferIndex = 0;
	}

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

		// Past buffer - very much like avg buffer but not variable in number of samples
		m_iPastBufferIndex = (m_iPastBufferIndex + 1) % MAX_NUM_SAMPLES;
		m_afPastBuffer[m_iPastBufferIndex] = m_fExponentialSmoothedInput;

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
		UpdateFromInput(iDeltaMS, m_oNodeInput.GetOutput());
	}

	float GetOutput()
	{
		return m_fOutput;
	}

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

	ProcessingNode m_oNodeInput;
	boolean m_bDerivative;
	float m_fExponentialSmoothingWeight;
	int m_iAvgSmoothingNumSamples;
	float m_fScalingFactor;

	float m_fInput;
	float m_fProcessingVar;
	float m_fExponentialSmoothedInput;
	float m_fAvgSmoothedInput;

	float[] m_afPastBuffer;
	int m_iPastBufferIndex;

	float[] m_afAvgBuffer;
	int m_iBufferIndex;
	float m_fOutput;
}



// The serial port
Serial g_port;

// The serial port for Joan if it's attached
Serial g_portJoan; 

// If we found an extra serial port for JOAN
boolean g_bFoundJoan;

// The current cannon state of Joan
int g_iJoanCannonState;

// horizontal position of the graph
int g_iPosX = 1;         
 
// Last time in MS
long g_iTimeMS = 0;

// Raw input
float g_fRawInput = 0;

// Processing nodes
ProcessingNode g_oNode0;
ProcessingNode g_oNode1;
ProcessingNode g_oNode2;

// Number of graphs to show
int g_iNumGraphs = 3;
 
// If we are currently detecting an active touch
boolean g_bDetectOn = false;
boolean g_bDetectOff = false;

// Vars relating to trigger
boolean g_bUseTriggerVelocity = true;
boolean g_bStartTrigger = false;
float g_fPreTriggerValue = 0.0;
int g_iTriggerVelocity = 0;
boolean g_bSendTrigger = false;

// Min and max thresholds
float g_fMinInputDetectThreshold = g_fMinInput;
float g_fMaxInputDetectThreshold = g_fMaxInput;

// Max of the smoothed input so far
float g_fMaxSmoothedInput = 0.1;

// Class to generate music
MusicGenerator g_oMusicGen;



void setup ()
{
	// set the window size:
	size(1600, 1000);        
 
	// List all the available serial ports
	println(Serial.list());

	// Different machines have different port indices

  // PandaStation
  int iTouchtonePort = 1;
  int iJoanPort = 0;
  
  //// Minitone
  //int iTouchtonePort = 1;
  //int iJoanPort = 2;
  
  //// Work laptop
  //int iTouchtonePort = 1;
  //int iJoanPort = 0;  
  
  g_port = new Serial(this, Serial.list()[iTouchtonePort], 9600);
        
  if(LOOK_FOR_JOAN_SERIAL && Serial.list().length >= 2)
  {
    g_portJoan = new Serial(this, Serial.list()[iJoanPort], 9600);
    g_bFoundJoan = true;
  }

	// set inital background:
	background(0);

	// Setup processing nodes
	g_oNode0 = new ProcessingNode(null,     false, g_fNode0Exp, g_iNode0Avg, 1.0); // Input
	g_oNode1 = new ProcessingNode(g_oNode0, true,  g_fNode1Exp, g_iNode1Avg, 0.6); // 1st serivative
	g_oNode2 = new ProcessingNode(g_oNode1, true,  g_fNode2Exp, g_iNode2Avg, 0.2); // 2nd Derivative

	// Setup music generator
	g_oMusicGen = new MusicGenerator();
}



void draw ()
{
	// Get the lastest raw input
	int iReadByte = -1;
	while(g_port.available() > 0)
	{
		iReadByte = g_port.read();

		// Get raw input and then adjust it with a power function
		//g_fRawInput = pow(float(iReadByte) / 255.0, 2.0); 
		g_fRawInput = pow(float(iReadByte) / 255.0, 1.0); 

		// Get delta time
		long iLastTimeMS = g_iTimeMS;
		g_iTimeMS = millis();
		//int iDeltaTimeMS = int(g_iTimeMS - iLastTimeMS);
		int iDeltaTimeMS = 10; // TEMP_CL - we might actually get better more consistent results assuming a fixed timestep as opposed to having two loops (the microcontroller and the PC) that sometimes don't sync well.

		// Update nodes
		g_oNode0.UpdateFromInput(iDeltaTimeMS, g_fRawInput);
		g_oNode1.Update(iDeltaTimeMS);
		g_oNode2.Update(iDeltaTimeMS);

		// Simple detection for note on
		float fSmoothedInput = g_oNode0.GetOutput();
		float fFinalOutput = g_oNode1.GetOutput();
		float fDetectOnThreshold = g_fMinInputDetectThreshold * (1.0 - fSmoothedInput) + g_fMaxInputDetectThreshold * fSmoothedInput;
		if(!g_bStartTrigger || !g_bUseTriggerVelocity)
		{
			boolean bOldDetectOn = g_bDetectOn;
			g_bDetectOn = fFinalOutput > fDetectOnThreshold;
			boolean bTriggerOn = !bOldDetectOn && g_bDetectOn;
			if(bTriggerOn)
 			{
				// Save that we detected a trigger if we're using velocity, otherwise, just send it
				if(g_bUseTriggerVelocity)
				{
					g_bStartTrigger = true;
					g_fPreTriggerValue = g_oNode0.GetOutputInPast(1);
				}
				else
				{
					g_iTriggerVelocity = 64;
					g_bSendTrigger = true;
				}
			}
		}
		else if(g_bStartTrigger)
		{
			// TODO - Find some way to recalibrate this over time as opposed to restarting the app
			if(fSmoothedInput > g_fMaxSmoothedInput)
			{
				g_fMaxSmoothedInput = fSmoothedInput;
			}

			float fCurValue = fSmoothedInput;
			//float fTriggerDelta = fCurValue - g_fPreTriggerValue;
			float fTriggerDelta = fCurValue; // Try this for now because the previous is clearly wrong for second notes.  This is wrong too but might sound better
			float fTriggerVelocity = ClampF(fTriggerDelta / (g_fMaxSmoothedInput * 0.9), 0.0, 1.0);
			fTriggerVelocity = pow(fTriggerVelocity, 1.0);
			g_iTriggerVelocity = int(fTriggerVelocity * 127);
			println("TEMP_CL g_iTriggerVelocity=" + g_iTriggerVelocity);
			g_bStartTrigger = false;
			g_bSendTrigger = true;
		}

		if(g_bSendTrigger)
		{
			stroke(g_iTriggerVelocity + 127, g_iTriggerVelocity + 127, g_iTriggerVelocity + 127);
			line(g_iPosX, 0, g_iPosX, height);
		}

		// Simple detection for note off
		float fDetectOffThreshold = -fDetectOnThreshold;
		boolean bOldDetectOff = g_bDetectOff;
		g_bDetectOff = fFinalOutput < fDetectOffThreshold;
		boolean bTriggerOff = !bOldDetectOff && g_bDetectOff;
		if(bTriggerOff)
 		{
			stroke(255,0,0);
			line(g_iPosX, 0, g_iPosX, height);
		}

		// Update music gen
		g_oMusicGen.Update(g_iTimeMS, fSmoothedInput, g_bSendTrigger, bTriggerOff, g_iTriggerVelocity);
		g_bSendTrigger = false;

		// Graph data
		GraphValue(0, g_oNode0.GetOutput());
		GraphValue(1, g_oNode1.GetOutput() + 0.5);
		GraphValue(2, g_oNode2.GetOutput() + 0.5);

		// Advance x pos for all graphs
		// at the edge of the screen, go back to the beginning:
		if (g_iPosX >= width)
		{
			g_iPosX = 0;
			background(0); 
		} 
		else
		{
			// increment the horizontal position:
			// TEMP_CL g_iPosX++;
			g_iPosX += 2;
		}

    // Joan fire code
    if(g_bFoundJoan)
    {
      int iLastCannonState = g_iJoanCannonState;
      g_iJoanCannonState = 0;
      if(fSmoothedInput > JOAN_FIRE_THRESHOLD)
      {
        g_iJoanCannonState = JOAN_FIRE_PATTERN;
      }
      
      if(g_iJoanCannonState != iLastCannonState)
      {
        g_portJoan.write(g_iJoanCannonState);
      }
    }
	}
}

 

void GraphValue(int iGraphIndex, float fValue)
{
	int iPosYMin = iGraphIndex * height / g_iNumGraphs;
	int iPosYMax = (iGraphIndex+1) * height / g_iNumGraphs;

	float fValueY = iPosYMax - fValue * (iPosYMax-iPosYMin);
	if(fValueY < iPosYMin)
	{
		fValueY = iPosYMin;
	}
	else if(fValueY > iPosYMax)
	{
		fValueY = iPosYMax;
	}

	// draw the line:
	if(g_bDetectOn)
	{
		stroke(255,128,255);
	}
	else if(g_bDetectOff)
	{
		stroke(255,128,128);
	}
	else
	{
		stroke(127,34,255);
	}
	line(g_iPosX, iPosYMax, g_iPosX, fValueY);
}



void keyPressed()
{
	// Node 0
	if (key == 'q' || key == 'a')
	{
		float fInc = (key == 'q') ? 0.05 : -0.05;
		float fNewSmoothing = ClampF(g_oNode0.GetExponentialSmoothingWeight() + fInc, 0, 1);
		g_oNode0.AdjustSmoothing(fNewSmoothing, g_oNode0.GetAvgSmoothingNumSamples());
	}
	else if (key == 'w' || key == 's')
	{
		int iInc = (key == 'w') ? 1 : -1;
		int iNewSamples = ClampI(g_oNode0.GetAvgSmoothingNumSamples() + iInc, 0, MAX_NUM_SAMPLES-1);
		g_oNode0.AdjustSmoothing(g_oNode0.GetExponentialSmoothingWeight(), iNewSamples);
	}

	// Node 1
	else if (key == 'e' || key == 'd')
	{
		float fInc = (key == 'e') ? 0.05 : -0.05;
		float fNewSmoothing = ClampF(g_oNode1.GetExponentialSmoothingWeight() + fInc, 0, 1);
		g_oNode1.AdjustSmoothing(fNewSmoothing, g_oNode1.GetAvgSmoothingNumSamples());
	}
	else if (key == 'r' || key == 'f')
	{
		int iInc = (key == 'r') ? 1 : -1;
		int iNewSamples = ClampI(g_oNode1.GetAvgSmoothingNumSamples() + iInc, 0, MAX_NUM_SAMPLES-1);
		g_oNode1.AdjustSmoothing(g_oNode1.GetExponentialSmoothingWeight(), iNewSamples);
	}

	// Node2
	else if (key == 't' || key == 'g')
	{
		float fInc = (key == 't') ? 0.05 : -0.05;
		float fNewSmoothing = ClampF(g_oNode2.GetExponentialSmoothingWeight() + fInc, 0, 1);
		g_oNode2.AdjustSmoothing(fNewSmoothing, g_oNode2.GetAvgSmoothingNumSamples());
	}
	else if (key == 'y' || key == 'h')
	{
		int iInc = (key == 'y') ? 1 : -1;
		int iNewSamples = ClampI(g_oNode2.GetAvgSmoothingNumSamples() + iInc, 0, MAX_NUM_SAMPLES-1);
		g_oNode2.AdjustSmoothing(g_oNode2.GetExponentialSmoothingWeight(), iNewSamples);
	}

	// Detect thresholds
	else if (key == 'u' || key == 'j')
	{
		float fInc = (key == 'u') ? 0.01 : -0.01;
		g_fMinInputDetectThreshold = ClampF(g_fMinInputDetectThreshold + fInc, 0, 1);
	}
	else if (key == 'i' || key == 'k')
	{
		float fInc = (key == 'i') ? 0.01 : -0.01;
		g_fMaxInputDetectThreshold = ClampF(g_fMaxInputDetectThreshold + fInc, 0, 1);
	}

	// Recording mode toggle
	else if(key == ' ')
	{
		g_oMusicGen.ToggleRecording();
	}

	println("Settings:");
	println("static float g_fNode0Exp=" + g_oNode0.GetExponentialSmoothingWeight() + ";");
	println("static int   g_iNode0Avg=" + g_oNode0.GetAvgSmoothingNumSamples() + ";");
	println("static float g_fNode1Exp=" + g_oNode1.GetExponentialSmoothingWeight() + ";");
	println("static int   g_iNode1Avg=" + g_oNode1.GetAvgSmoothingNumSamples() + ";");
	println("static float g_fNode2Exp=" + g_oNode2.GetExponentialSmoothingWeight() + ";");
	println("static int   g_iNode2Avg=" + g_oNode2.GetAvgSmoothingNumSamples() + ";");
	println("static float g_fMinInput=" + g_fMinInputDetectThreshold + ";");
	println("static float g_fMaxInput=" + g_fMaxInputDetectThreshold + ";");
}