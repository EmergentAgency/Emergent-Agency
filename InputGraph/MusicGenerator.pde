/**
 * File: MusicGenerator.pde
 *
 * Description: Takes touch input and generates MIDI
 *
 * Copyright: 2015 Chris Linder
 */

import rwmidi.*;

//// Synth
//static String MIDI_DEVICE_NAME = "LoopBe";
//static int CHANNEL_HIGH = 1;
//static int CHANNEL_LOW = 2;
//static int CONTROLLER_CHANNEL = 10;
//static int CONTROLLER_INDEX = 1;
//static boolean SEND_EXTRA_NOTE_OFF = false;

// Velocity
static String MIDI_DEVICE_NAME = "LoopBe";
static int CHANNEL_HIGH = 1;
static int CHANNEL_LOW = 10; // skip this for now
static int CONTROLLER_CHANNEL = 10;
static int CONTROLLER_INDEX = 2; // skip this for now
static boolean SEND_EXTRA_NOTE_OFF = false;

//// Xiao Xiao's piano
//static String MIDI_DEVICE_NAME = "E-MU";
//static int CHANNEL_HIGH = 0;
//static int CHANNEL_LOW = 0;
//static int CONTROLLER_CHANNEL = 10;
//static int CONTROLLER_INDEX = 1;
//static boolean SEND_EXTRA_NOTE_OFF = true;



//int g_aiNoteSet[] = {53,55,57,60,62,65,67,69,72};
//int g_iNumSetNotes = 9;

int g_aiNoteSet[] = {60,62,65,67,69};
int g_iNumSetNotes = 5;

int[][] g_aiCordSet = {
	{64,67,72},
	{65,69,74},
	{69,72,77},
	{71,74,79},
	{72,76,81},
};
int g_iNumCordNotes = 3;

// TEMP_CL static float NOTE_OFF_THRESHOLD = 0.02;
static float NOTE_OFF_THRESHOLD = 0.05;
static int MAX_NOTES = 5;
static int OCTAVE = 12;
static int TRANSPOSE = -12;


class MusicGenerator
{
	MusicGenerator()
	{
		// List valid MIDI output devices and look for LoopBe.
		int iLoopBeMidiIndex = -1;
		println("Available MIDI ouput devices:");
		for(int i = 0; i < RWMidi.getOutputDeviceNames().length; i++)
		{
			println("MIDI output device name " + i + " = " + RWMidi.getOutputDeviceNames()[i]);
			if(RWMidi.getOutputDeviceNames()[i].startsWith(MIDI_DEVICE_NAME))
			{
				iLoopBeMidiIndex = i;
				println("Found device!");
			}
		}

		// Check to see if we initialized out MIDI device
		m_bInitialized = iLoopBeMidiIndex >= 0;

		if(!m_bInitialized)
		{
			println("ERROR - Couldn't find LoopBe MIDI device. No MIDI will be generated.");
		}
		else
		{
			// Pick the correct MIDI device.
			m_oMidiOut = RWMidi.getOutputDevices()[iLoopBeMidiIndex].createOutput();
		}

		// Channel is 1 indexed in Reason and 0 index here so channel 0 = channel 1 there.
		// Controllers are 0 indexed in both places.

		// Reason
		m_iMidiNoteChannelHigh = CHANNEL_HIGH;
		m_iMidiNoteChannelLow = CHANNEL_LOW;
		m_iMidiControllerChannel = CONTROLLER_CHANNEL;
		m_iMidiControllerIndex = CONTROLLER_INDEX;

		m_aiNotesHigh = new int[MAX_NOTES];
		m_aiCordIndices = new int[MAX_NOTES];

		fMaxInput = 0.25;
		fMinInput = 0.75;
	}

	void Update(long iCurTimeMS, float fInput, boolean bNoteOnEvent, boolean bNoteOffEvent, int iNoteVelocity)
	{
		if(!m_bInitialized)
		{
			return;
		}

		// If we ever fall below this threshold, turn off all notes and turn off controller
		//println("TEMP_CL fInput=" + fInput + " m_iNumNotesHigh=" + m_iNumNotesHigh + " iNoteVelocity=" + iNoteVelocity);
		if(fInput < NOTE_OFF_THRESHOLD && m_iNumNotesHigh > 0)
		{
			println("TEMP_CL - kill all input");

			// Turn off high notes
			for(int i = 0; i <  m_iNumNotesHigh; ++i)
			{
				m_oMidiOut.sendNoteOff(m_iMidiNoteChannelHigh, m_aiNotesHigh[i] + TRANSPOSE, 0);
				if(SEND_EXTRA_NOTE_OFF)
				{
					m_oMidiOut.sendNoteOff(m_iMidiNoteChannelHigh, m_aiNotesHigh[i] + TRANSPOSE, 0);
					m_oMidiOut.sendNoteOff(m_iMidiNoteChannelHigh, m_aiNotesHigh[i] + TRANSPOSE, 0);
					m_oMidiOut.sendNoteOff(m_iMidiNoteChannelHigh, m_aiNotesHigh[i] + TRANSPOSE, 0);
					m_oMidiOut.sendNoteOff(m_iMidiNoteChannelHigh, m_aiNotesHigh[i] + TRANSPOSE, 0);
				}
			}
			m_iNumNotesHigh = 0;

			// Turn off low notes
			m_oMidiOut.sendNoteOff(m_iMidiNoteChannelLow, g_aiNoteSet[m_iFundamentalIndex]    - OCTAVE + TRANSPOSE, 0);
			m_oMidiOut.sendNoteOff(m_iMidiNoteChannelLow, g_aiCordSet[m_iFundamentalIndex][1] - OCTAVE + TRANSPOSE, 0);
			m_oMidiOut.sendNoteOff(m_iMidiNoteChannelLow, g_aiCordSet[m_iFundamentalIndex][2] - OCTAVE + TRANSPOSE, 0);
			if(SEND_EXTRA_NOTE_OFF)
			{
				m_oMidiOut.sendNoteOff(m_iMidiNoteChannelLow, g_aiNoteSet[m_iFundamentalIndex]    - OCTAVE + TRANSPOSE, 0);
				m_oMidiOut.sendNoteOff(m_iMidiNoteChannelLow, g_aiCordSet[m_iFundamentalIndex][1] - OCTAVE + TRANSPOSE, 0);
				m_oMidiOut.sendNoteOff(m_iMidiNoteChannelLow, g_aiCordSet[m_iFundamentalIndex][2] - OCTAVE + TRANSPOSE, 0);
				m_oMidiOut.sendNoteOff(m_iMidiNoteChannelLow, g_aiNoteSet[m_iFundamentalIndex]    - OCTAVE + TRANSPOSE, 0);
				m_oMidiOut.sendNoteOff(m_iMidiNoteChannelLow, g_aiCordSet[m_iFundamentalIndex][1] - OCTAVE + TRANSPOSE, 0);
				m_oMidiOut.sendNoteOff(m_iMidiNoteChannelLow, g_aiCordSet[m_iFundamentalIndex][2] - OCTAVE + TRANSPOSE, 0);
				m_oMidiOut.sendNoteOff(m_iMidiNoteChannelLow, g_aiNoteSet[m_iFundamentalIndex]    - OCTAVE + TRANSPOSE, 0);
				m_oMidiOut.sendNoteOff(m_iMidiNoteChannelLow, g_aiCordSet[m_iFundamentalIndex][1] - OCTAVE + TRANSPOSE, 0);
				m_oMidiOut.sendNoteOff(m_iMidiNoteChannelLow, g_aiCordSet[m_iFundamentalIndex][2] - OCTAVE + TRANSPOSE, 0);
				m_oMidiOut.sendNoteOff(m_iMidiNoteChannelLow, g_aiNoteSet[m_iFundamentalIndex]    - OCTAVE + TRANSPOSE, 0);
				m_oMidiOut.sendNoteOff(m_iMidiNoteChannelLow, g_aiCordSet[m_iFundamentalIndex][1] - OCTAVE + TRANSPOSE, 0);
				m_oMidiOut.sendNoteOff(m_iMidiNoteChannelLow, g_aiCordSet[m_iFundamentalIndex][2] - OCTAVE + TRANSPOSE, 0);
			}

			// Turn down the controller
			m_oMidiOut.sendController(m_iMidiControllerChannel, m_iMidiControllerIndex, 0);
		}

		// TEMP_CL if(bNoteOffEvent && m_iNumNotesHigh > 0)
		if(bNoteOffEvent && m_iNumNotesHigh > 1)
		{
			println("TEMP_CL - note off");

			// Use the note off event to turn off cord notes but not the fundamental
			m_iNumNotesHigh--;
			m_oMidiOut.sendNoteOff(m_iMidiNoteChannelHigh, m_aiNotesHigh[m_iNumNotesHigh] + TRANSPOSE, 0);
			if(SEND_EXTRA_NOTE_OFF)
			{
				m_oMidiOut.sendNoteOff(m_iMidiNoteChannelHigh, m_aiNotesHigh[m_iNumNotesHigh] + TRANSPOSE, 0);
				m_oMidiOut.sendNoteOff(m_iMidiNoteChannelHigh, m_aiNotesHigh[m_iNumNotesHigh] + TRANSPOSE, 0);
				m_oMidiOut.sendNoteOff(m_iMidiNoteChannelHigh, m_aiNotesHigh[m_iNumNotesHigh] + TRANSPOSE, 0);
				m_oMidiOut.sendNoteOff(m_iMidiNoteChannelHigh, m_aiNotesHigh[m_iNumNotesHigh] + TRANSPOSE, 0);
			}

			// If we aren't playing any notes, turn off the cord
			if(m_iNumNotesHigh < 1)
			{
				// Turn off low notes
				m_oMidiOut.sendNoteOff(m_iMidiNoteChannelLow, g_aiNoteSet[m_iFundamentalIndex]    - OCTAVE + TRANSPOSE, 0);
				m_oMidiOut.sendNoteOff(m_iMidiNoteChannelLow, g_aiCordSet[m_iFundamentalIndex][1] - OCTAVE + TRANSPOSE, 0);
				m_oMidiOut.sendNoteOff(m_iMidiNoteChannelLow, g_aiCordSet[m_iFundamentalIndex][2] - OCTAVE + TRANSPOSE, 0);
				if(SEND_EXTRA_NOTE_OFF)
				{
					m_oMidiOut.sendNoteOff(m_iMidiNoteChannelLow, g_aiNoteSet[m_iFundamentalIndex]    - OCTAVE + TRANSPOSE, 0);
					m_oMidiOut.sendNoteOff(m_iMidiNoteChannelLow, g_aiCordSet[m_iFundamentalIndex][1] - OCTAVE + TRANSPOSE, 0);
					m_oMidiOut.sendNoteOff(m_iMidiNoteChannelLow, g_aiCordSet[m_iFundamentalIndex][2] - OCTAVE + TRANSPOSE, 0);
					m_oMidiOut.sendNoteOff(m_iMidiNoteChannelLow, g_aiNoteSet[m_iFundamentalIndex]    - OCTAVE + TRANSPOSE, 0);
					m_oMidiOut.sendNoteOff(m_iMidiNoteChannelLow, g_aiCordSet[m_iFundamentalIndex][1] - OCTAVE + TRANSPOSE, 0);
					m_oMidiOut.sendNoteOff(m_iMidiNoteChannelLow, g_aiCordSet[m_iFundamentalIndex][2] - OCTAVE + TRANSPOSE, 0);
					m_oMidiOut.sendNoteOff(m_iMidiNoteChannelLow, g_aiNoteSet[m_iFundamentalIndex]    - OCTAVE + TRANSPOSE, 0);
					m_oMidiOut.sendNoteOff(m_iMidiNoteChannelLow, g_aiCordSet[m_iFundamentalIndex][1] - OCTAVE + TRANSPOSE, 0);
					m_oMidiOut.sendNoteOff(m_iMidiNoteChannelLow, g_aiCordSet[m_iFundamentalIndex][2] - OCTAVE + TRANSPOSE, 0);
					m_oMidiOut.sendNoteOff(m_iMidiNoteChannelLow, g_aiNoteSet[m_iFundamentalIndex]    - OCTAVE + TRANSPOSE, 0);
					m_oMidiOut.sendNoteOff(m_iMidiNoteChannelLow, g_aiCordSet[m_iFundamentalIndex][1] - OCTAVE + TRANSPOSE, 0);
					m_oMidiOut.sendNoteOff(m_iMidiNoteChannelLow, g_aiCordSet[m_iFundamentalIndex][2] - OCTAVE + TRANSPOSE, 0);
				}
			}
		}

		// NOTE: Not an "else if"
		if(bNoteOnEvent && m_iNumNotesHigh < MAX_NOTES)
		{
			m_iNumNotesHigh++;

			// If we weren't playing any notes, pick a fundamental note
			if(m_iNumNotesHigh == 1)
			{
				// Don't pick the same fundamental note twice in row
				if(m_iFundamentalIndex == 0)
				{
					m_iFundamentalIndex += (int)random(2) + 1;
				}
				else if(m_iFundamentalIndex == g_iNumSetNotes-1)
				{
					m_iFundamentalIndex += (int)random(2) - 2;
				}
				else
				{
					m_iFundamentalIndex += (int)random(2) == 0 ? -1 : 1;
				}

				m_aiNotesHigh[m_iNumNotesHigh-1] = g_aiNoteSet[m_iFundamentalIndex];

				println("TEMP_CL - cord on");

				// Setup a low cord
				m_oMidiOut.sendNoteOn(m_iMidiNoteChannelLow, g_aiNoteSet[m_iFundamentalIndex]    - OCTAVE + TRANSPOSE, iNoteVelocity);
				m_oMidiOut.sendNoteOn(m_iMidiNoteChannelLow, g_aiCordSet[m_iFundamentalIndex][1] - OCTAVE + TRANSPOSE, iNoteVelocity);
				m_oMidiOut.sendNoteOn(m_iMidiNoteChannelLow, g_aiCordSet[m_iFundamentalIndex][2] - OCTAVE + TRANSPOSE, iNoteVelocity);
			}

			// If we have fundamental, pick random other notes in the cord
			else
			{
				// Pick a random interval in the cord
				m_aiCordIndices[m_iNumNotesHigh-2] = int(random(g_iNumCordNotes));

				//// Pick the next note in the cord
				//m_aiCordIndices[m_iNumNotesHigh-2] = ClampI(m_iNumNotesHigh-2, 0, g_iNumCordNotes-1);

				// Based on the fundamental note, pick a note based on the cord interval
				m_aiNotesHigh[m_iNumNotesHigh-1] = g_aiCordSet[m_iFundamentalIndex][m_aiCordIndices[m_iNumNotesHigh-2]];
			}

			println("TEMP_CL - note on");

			//println("Note ON node=" + i + " MidiNote=" + MidiNote + " MidiNoteChannel=" + MidiNoteChannel);
			m_oMidiOut.sendNoteOn(m_iMidiNoteChannelHigh, m_aiNotesHigh[m_iNumNotesHigh-1] + TRANSPOSE, iNoteVelocity);

			// TEMP_CL - trigger drum kit to test timing
			//m_oMidiOut.sendNoteOn(m_iMidiNoteChannelHigh, 44, iNoteVelocity);
		}

		// If we have notes going, adjust the controller
		if(m_iNumNotesHigh > 0)
		{
			// Adjust controller range
			float fSmoothingAmount = 0.9;
			m_fVerySmoothedInput = fInput * (1.0 - fSmoothingAmount) + m_fVerySmoothedInput * fSmoothingAmount;

			// Deal with min
			// If the current value is less than the current min, reset the min
			if(m_fVerySmoothedInput < fMinInput)
			{
				fMinInput = m_fVerySmoothedInput;
				m_iLastMinTimeMS = iCurTimeMS;
			}
			// If enough time as past, try moving the min up (but only if we are at least below the max input by a margin of error)
			else if(iCurTimeMS - m_iLastMinTimeMS >= 250 && fMinInput < fMaxInput - 0.1)
			{
				fMinInput += 0.05;
				m_iLastMinTimeMS = iCurTimeMS;
			}

			// Deal with max
			// If the current value is greater than the current max, reset the max
			if(m_fVerySmoothedInput > fMaxInput)
			{
				fMaxInput = m_fVerySmoothedInput;
				m_iLastMaxTimeMS = iCurTimeMS;
			}
			// If enough time as past, try moving the max down (but only if we are at least above the min input by a margin of error)
			else if(iCurTimeMS - m_iLastMaxTimeMS >= 500 && fMaxInput > fMinInput + 0.1)
			{
				fMaxInput -= 0.01;
				m_iLastMaxTimeMS = iCurTimeMS;
			}

			// Use min and max to reset input
			float fAdjustedInput = (m_fVerySmoothedInput - fMinInput) / (fMaxInput - fMinInput);
			//println("fAdjustedInput pre pow: " + fAdjustedInput);
			fAdjustedInput = pow(fAdjustedInput, 2.0);
			//println("fAdjustedInput post pow: " + fAdjustedInput);

			float fControllerSmoothing = 0.9;
			m_fSmoothedController = fAdjustedInput * (1.0 - fControllerSmoothing) + m_fSmoothedController * fControllerSmoothing;
			//println("m_fSmoothedController=" + m_fSmoothedController);

			int iControllerValue = ClampI(int(m_fSmoothedController * 80) + 10, 10, 90);
			m_oMidiOut.sendController(m_iMidiControllerChannel, m_iMidiControllerIndex, iControllerValue);
		}
	}

	// If the device isn't initialized, it won't output any MIDI
	boolean m_bInitialized;

	// MIDI output object
	MidiOutput m_oMidiOut;

	int m_iMidiNoteChannelHigh;
	int m_iNumNotesHigh;
	int[] m_aiNotesHigh;

	int m_iMidiNoteChannelLow;
	int m_iNumNotesLow;
	int[] m_aiNotesLow;

	int m_iMidiControllerChannel;
	int m_iMidiControllerIndex;

	int m_iFundamentalIndex;
	int[] m_aiCordIndices;

	float fMaxInput;
	float fMinInput;
	long m_iLastMinTimeMS;
	long m_iLastMaxTimeMS;
	float m_fVerySmoothedInput;
	float m_fSmoothedController;
}
