/**
 * File: InputGraph.pde
 *
 * Description: Graph serial input in several different ways to help figure out how to detect interesting touch events.
 *
 * Copyright: 2015 Chris Linder
 */
 
import processing.serial.*;



static int MAX_NUM_SAMPLES = 30;

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

	ProcessingNode m_oNodeInput;
	boolean m_bDerivative;
	float m_fExponentialSmoothingWeight;
	int m_iAvgSmoothingNumSamples;
	float m_fScalingFactor;

	float m_fInput;
	float m_fProcessingVar;
	float m_fExponentialSmoothedInput;
	float m_fAvgSmoothedInput;
	float[] m_afAvgBuffer;
	int m_iBufferIndex;
	float m_fOutput;
}



// The serial port
Serial g_port;

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

// Min and max thresholds
float g_fMinInputDetectThreshold = 0.02;
float g_fMaxInputDetectThreshold = 0.05;

// Class to generate music
MusicGenerator g_oMusicGen;



void setup ()
{
	// set the window size:
	size(1200, 800);        
 
	// List all the available serial ports
	println(Serial.list());

	// I know that the first port in the serial list on my mac
	// is always my  Arduino, so I open Serial.list()[0].
	// Open whatever port is the one you're using.
	g_port = new Serial(this, Serial.list()[0], 9600);

	// set inital background:
	background(0);

	// Setup processing nodes
	g_oNode0 = new ProcessingNode(null,     false, 0.5, 4, 1.0); // Input
	g_oNode1 = new ProcessingNode(g_oNode0, true,  0.7, 3, 0.5); // 1st serivative
	g_oNode2 = new ProcessingNode(g_oNode1, true,  0.0, 0, 0.2); // 2nd Derivative

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
	}
	if(iReadByte < 0)
	{
		return;
	}

	// Get raw input and then adjust it with a power function
	//g_fRawInput = pow(float(iReadByte) / 255.0, 2.0); 
	g_fRawInput = pow(float(iReadByte) / 255.0, 1.0); 

	// Get delta time
	long iLastTimeMS = g_iTimeMS;
	g_iTimeMS = millis();
	int iDeltaTimeMS = int(g_iTimeMS - iLastTimeMS);

	// Update nodes
	g_oNode0.UpdateFromInput(iDeltaTimeMS, g_fRawInput);
	g_oNode1.Update(iDeltaTimeMS);
	g_oNode2.Update(iDeltaTimeMS);

	// Simple detection for note on
	float fSmoothedInput = g_oNode0.GetOutput();
	float fFinalOutput = g_oNode1.GetOutput();
	float fDetectOnThreshold = g_fMinInputDetectThreshold * (1.0 - fSmoothedInput) + g_fMaxInputDetectThreshold * fSmoothedInput;
	boolean bOldDetectOn = g_bDetectOn;
	g_bDetectOn = fFinalOutput > fDetectOnThreshold;
	boolean bTriggerOn = !bOldDetectOn && g_bDetectOn;
	if(bTriggerOn)
 	{
		stroke(255,255,255);
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
	g_oMusicGen.Update(g_iTimeMS, fSmoothedInput, bTriggerOn, bTriggerOff);

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
		g_iPosX++;
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
		println("Node 0 ExponentialSmoothingWeight=" + fNewSmoothing);
		g_oNode0.AdjustSmoothing(fNewSmoothing, g_oNode0.GetAvgSmoothingNumSamples());
	}
	else if (key == 'w' || key == 's')
	{
		int iInc = (key == 'w') ? 1 : -1;
		int iNewSamples = ClampI(g_oNode0.GetAvgSmoothingNumSamples() + iInc, 0, MAX_NUM_SAMPLES-1);
		println("Node 0 AvgSmoothingNumSamples=" + iNewSamples);
		g_oNode0.AdjustSmoothing(g_oNode0.GetExponentialSmoothingWeight(), iNewSamples);
	}

	// Node 1
	else if (key == 'e' || key == 'd')
	{
		float fInc = (key == 'e') ? 0.05 : -0.05;
		float fNewSmoothing = ClampF(g_oNode1.GetExponentialSmoothingWeight() + fInc, 0, 1);
		println("Node 1 ExponentialSmoothingWeight=" + fNewSmoothing);
		g_oNode1.AdjustSmoothing(fNewSmoothing, g_oNode1.GetAvgSmoothingNumSamples());
	}
	else if (key == 'r' || key == 'f')
	{
		int iInc = (key == 'r') ? 1 : -1;
		int iNewSamples = ClampI(g_oNode1.GetAvgSmoothingNumSamples() + iInc, 0, MAX_NUM_SAMPLES-1);
		println("Node 1 AvgSmoothingNumSamples=" + iNewSamples);
		g_oNode1.AdjustSmoothing(g_oNode1.GetExponentialSmoothingWeight(), iNewSamples);
	}

	// Node2
	else if (key == 't' || key == 'g')
	{
		float fInc = (key == 't') ? 0.05 : -0.05;
		float fNewSmoothing = ClampF(g_oNode2.GetExponentialSmoothingWeight() + fInc, 0, 1);
		println("Node 2 ExponentialSmoothingWeight=" + fNewSmoothing);
		g_oNode2.AdjustSmoothing(fNewSmoothing, g_oNode2.GetAvgSmoothingNumSamples());
	}
	else if (key == 'y' || key == 'h')
	{
		int iInc = (key == 'y') ? 1 : -1;
		int iNewSamples = ClampI(g_oNode2.GetAvgSmoothingNumSamples() + iInc, 0, MAX_NUM_SAMPLES-1);
		println("Node 2 AvgSmoothingNumSamples=" + iNewSamples);
		g_oNode2.AdjustSmoothing(g_oNode2.GetExponentialSmoothingWeight(), iNewSamples);
	}

	// Detect thresholds
	else if (key == 'u' || key == 'j')
	{
		float fInc = (key == 'u') ? 0.01 : -0.01;
		g_fMinInputDetectThreshold = ClampF(g_fMinInputDetectThreshold + fInc, 0, 1);
		println("Node 0 g_fMinInputDetectThreshold=" + g_fMinInputDetectThreshold);
	}
	else if (key == 'i' || key == 'k')
	{
		float fInc = (key == 'i') ? 0.01 : -0.01;
		g_fMaxInputDetectThreshold = ClampF(g_fMaxInputDetectThreshold + fInc, 0, 1);
		println("Node 0 g_fMaxInputDetectThreshold=" + g_fMaxInputDetectThreshold);
	}

}