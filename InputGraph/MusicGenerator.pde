/**
 * File: MusicGenerator.pde
 *
 * Description: Takes touch input and generates MIDI
 *
 * Copyright: 2015 Chris Linder
 */

import rwmidi.*;

int g_aiNoteSet[] = {53,55,57,60,62,65,67,69,72};
int g_iNumSetNotes = 9;

static float NOTE_OFF_THRESHOLD = 0.02;


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
		m_bNoteOn = false;
		m_iMidiNoteChannel = 1;
		m_iMidiNote = 60;
		m_iCurNoteIndex = 0;
	}

	void Update(long iCurTimeMS, float fInput, boolean bNoteOnEvent, boolean bNoteOffEvent)
	{
		if(!m_bInitialized)
		{
			return;
		}

		// If we ever fall below this threshold, turn off all notes
		if(fInput < NOTE_OFF_THRESHOLD)
		{
			m_oMidiOut.sendNoteOff(m_iMidiNoteChannel, m_iMidiNote, 0);
		}

		if(bNoteOnEvent)
		{
			// TEMP_CL
			if(m_bNoteOn)
			{
				m_oMidiOut.sendNoteOff(m_iMidiNoteChannel, m_iMidiNote, 0);
			}

			// No repeated notes
			if(m_iCurNoteIndex == 0)
			{
				m_iCurNoteIndex += (int)random(2) + 1;
			}
			else if(m_iCurNoteIndex == g_iNumSetNotes-1)
			{
				m_iCurNoteIndex += (int)random(2) - 2;
			}
			else
			{
				m_iCurNoteIndex += (int)random(2) == 0 ? -1 : 1;
			}

			m_iMidiNote = g_aiNoteSet[m_iCurNoteIndex];

			m_iMidiNote -= 6;

			//println("Note ON node=" + i + " MidiNote=" + MidiNote + " MidiNoteChannel=" + MidiNoteChannel);
			m_oMidiOut.sendNoteOn(m_iMidiNoteChannel, m_iMidiNote, 127); // default to full velocity
			m_bNoteOn = true;
		}
	}

	// If the device isn't initialized, it won't output any MIDI
	boolean m_bInitialized;

	// MIDI output object
	MidiOutput m_oMidiOut;

	boolean m_bNoteOn;
	int m_iMidiNoteChannel;
	int m_iMidiNote;
	int m_iCurNoteIndex;
}
