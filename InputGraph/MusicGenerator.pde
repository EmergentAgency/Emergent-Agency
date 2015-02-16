/**
 * File: MusicGenerator.pde
 *
 * Description: Takes touch input and generates MIDI
 *
 * Copyright: 2015 Chris Linder
 */

import rwmidi.*;

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

static float NOTE_OFF_THRESHOLD = 0.02;
static int MAX_NOTES = 5;
static int OCTAVE = 12;



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
			if(RWMidi.getOutputDeviceNames()[i].startsWith("LoopBe"))
			{
				iLoopBeMidiIndex = i;
				println("Found LoopBe!");
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

		// TEMP_CL 
		m_iMidiNoteChannelHigh = 1;
		m_iMidiNoteChannelLow = 2;
		m_aiNotesHigh = new int[MAX_NOTES];
		m_aiCordIndices = new int[MAX_NOTES];
	}

	void Update(long iCurTimeMS, float fInput, boolean bNoteOnEvent, boolean bNoteOffEvent)
	{
		if(!m_bInitialized)
		{
			return;
		}

		// If we ever fall below this threshold, turn off all notes
		if(fInput < NOTE_OFF_THRESHOLD && m_iNumNotesHigh > 0)
		{
			// Turn off high notes
			for(int i = 0; i <  m_iNumNotesHigh; ++i)
			{
				m_oMidiOut.sendNoteOff(m_iMidiNoteChannelHigh, m_aiNotesHigh[i], 0);
			}
			m_iNumNotesHigh = 0;

			// Turn off low notes
			m_oMidiOut.sendNoteOff(m_iMidiNoteChannelLow, g_aiNoteSet[m_iFundamentalIndex]    - OCTAVE, 127); // default to full velocity
			m_oMidiOut.sendNoteOff(m_iMidiNoteChannelLow, g_aiCordSet[m_iFundamentalIndex][1] - OCTAVE, 127); // default to full velocity
			m_oMidiOut.sendNoteOff(m_iMidiNoteChannelLow, g_aiCordSet[m_iFundamentalIndex][2] - OCTAVE, 127); // default to full velocity

		}


		if(bNoteOffEvent && m_iNumNotesHigh > 0)
		//if(bNoteOffEvent && m_iNumNotesHigh > 1)
		{
			// Use the note off event to turn off cord notes but not the fundamental
			m_iNumNotesHigh--;
			m_oMidiOut.sendNoteOff(m_iMidiNoteChannelHigh, m_aiNotesHigh[m_iNumNotesHigh], 0);

			// If we would be left with no notes, treat this like a new note which will trigger a cord change
			if(m_iNumNotesHigh < 1 && fInput >= NOTE_OFF_THRESHOLD)
			{
				// Turn off low notes
				m_oMidiOut.sendNoteOff(m_iMidiNoteChannelLow, g_aiNoteSet[m_iFundamentalIndex]    - OCTAVE, 127); // default to full velocity
				m_oMidiOut.sendNoteOff(m_iMidiNoteChannelLow, g_aiCordSet[m_iFundamentalIndex][1] - OCTAVE, 127); // default to full velocity
				m_oMidiOut.sendNoteOff(m_iMidiNoteChannelLow, g_aiCordSet[m_iFundamentalIndex][2] - OCTAVE, 127); // default to full velocity

				// Trigger a new cord and fundamental
				//bNoteOnEvent = true;
			}
		}

		// NOTE: Not an else if
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

				// Setup a low cord
				m_oMidiOut.sendNoteOn(m_iMidiNoteChannelLow, g_aiNoteSet[m_iFundamentalIndex]    - OCTAVE, 127); // default to full velocity
				m_oMidiOut.sendNoteOn(m_iMidiNoteChannelLow, g_aiCordSet[m_iFundamentalIndex][1] - OCTAVE, 127); // default to full velocity
				m_oMidiOut.sendNoteOn(m_iMidiNoteChannelLow, g_aiCordSet[m_iFundamentalIndex][2] - OCTAVE, 127); // default to full velocity
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

			//println("Note ON node=" + i + " MidiNote=" + MidiNote + " MidiNoteChannel=" + MidiNoteChannel);
			m_oMidiOut.sendNoteOn(m_iMidiNoteChannelHigh, m_aiNotesHigh[m_iNumNotesHigh-1], 127); // default to full velocity
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

	int m_iFundamentalIndex;
	int[] m_aiCordIndices;
}
