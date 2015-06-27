/**
 * File: MusicGenerator.pde
 *
 * Description: Takes touch input and generates MIDI
 *
 * Copyright: 2015 Chris Linder
 */

import rwmidi.*;

//// Synth
//static String MIDI_OUT_DEVICE_NAME = "LoopBe";
//static String MIDI_IN_DEVICE_NAME = "2- Legacy";
//static int CHANNEL_HIGH = 1;
//static int CHANNEL_LOW = 2;
//static int CONTROLLER_CHANNEL = 10;
//static int CONTROLLER_INDEX = 1;
//static boolean SEND_EXTRA_NOTE_OFF = false;

// Velocity
//static String MIDI_OUT_DEVICE_NAME = "LoopBe";
static String MIDI_OUT_DEVICE_NAME = "loopMIDI";
//static String MIDI_IN_DEVICE_NAME = "Legacy";
static String MIDI_IN_DEVICE_NAME = "SV1";
static int CHANNEL_HIGH = 1;
static int CHANNEL_LOW = 2;
static int CONTROLLER_CHANNEL = 10;
static int CONTROLLER_INDEX = 1;
static boolean SEND_EXTRA_NOTE_OFF = false;

//// Xiao Xiao's piano
//static String MIDI_OUT_DEVICE_NAME = "E-MU";
//static String MIDI_IN_DEVICE_NAME = "E-MU";
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

int g_iBaseNote = 60;
int[] g_aiPossibleBaseNotes = {60, 60, 60, 62, 65, 57, 55};
static int CHANGE_KEY_NO_INPUT_TIME_MS = 2000;
int[] g_aiBaseScaleIntervals = {0,2,4,5,7,9,11};
int[][] g_aiCordsScaleIndexOffsets = {
	//{0,2,4}, // This is more commonly called a 1,3,5 cord
	//{0,3,4}, // This is more commonly called a 1,4,5 cord
	{0,2,4,6}, // This is more commonly called a 1,3,5 cord
	{0,3,4}, // This is more commonly called a 1,4,5 cord
};
//int[] g_iOctaveOffsets = {-1,0,1};
int[] g_iOctaveOffsets = {-2,-1,0};
//int[] g_iOctaveOffsets = {-1,0};
//int[] g_iOctaveOffsets = {-1};
//int[] g_iOctaveOffsets = {0};

// TEMP_CL static float NOTE_OFF_THRESHOLD = 0.02;
static float NOTE_OFF_THRESHOLD = 0.1;
static int MAX_NOTES = 5;
static int OCTAVE = 12;
static int TRANSPOSE = -12;
static int MAX_RECORDED_NOTES = 256;

static float NOTE_INTENSITY_MAX = 50.0;
static float NOTE_INTENSITY_DROP_PER_SECOND = 50.0;


public class MusicGenerator
{
	MusicGenerator()
	{
		// Check bounds
		for(int i = 0; i < g_iOctaveOffsets.length; ++i)
		{
			if((g_iBaseNote + g_iOctaveOffsets[i] * OCTAVE) < 0)
			{
				println("g_iBaseNote and g_iOctaveOffsets will create note with index < 0.  FIX THIS!");
				delay(999999);
				return;
			}
		}

		// List valid MIDI output devices and look for LoopBe.
		int iMidiOutIndex = -1;
		println("Available MIDI ouput devices:");
		for(int i = 0; i < RWMidi.getOutputDeviceNames().length; i++)
		{
			println("MIDI output device name " + i + " = " + RWMidi.getOutputDeviceNames()[i]);
			if(RWMidi.getOutputDeviceNames()[i].indexOf(MIDI_OUT_DEVICE_NAME) >= 0)
			{
				iMidiOutIndex = i;
				println("Found device!");
			}
		}

		// Check to see if we initialized out MIDI device
		m_bInitializedOutput = iMidiOutIndex >= 0;

		if(!m_bInitializedOutput)
		{
			println("ERROR - Couldn't find " + MIDI_OUT_DEVICE_NAME + " MIDI device. No MIDI will be generated.");
		}
		else
		{
			// Pick the correct MIDI device.
			m_oMidiOut = RWMidi.getOutputDevices()[iMidiOutIndex].createOutput();
		}


		// List valid MIDI input devices and look for LoopBe.
		int iMidiInIndex = -1;
		println("Available MIDI input devices:");
		for(int i = 0; i < RWMidi.getInputDeviceNames().length; i++)
		{
			println("MIDI input device name " + i + " = " + RWMidi.getInputDeviceNames()[i]);
			if(RWMidi.getInputDeviceNames()[i].indexOf(MIDI_IN_DEVICE_NAME) >= 0)
			{
				iMidiInIndex = i;
				println("Found device!");
			}
		}

		// Check to see if we initialized out MIDI device
		m_bInitializedInput = iMidiInIndex >= 0;

		if(!m_bInitializedInput)
		{
			println("ERROR - Couldn't find " + MIDI_IN_DEVICE_NAME + " MIDI device. No MIDI will be generated.");
		}
		else
		{
			// Pick the correct MIDI device.
			m_oMidiIn = RWMidi.getInputDevices()[iMidiInIndex].createInput(this);
		}


		// Channel is 1 indexed in Reason and 0 index here so channel 0 = channel 1 there.
		// Controllers are 0 indexed in both places.

		m_iCurTimeMS = 0;
		m_iLastInputEvent = 0;

		// Reason
		m_iMidiNoteChannelHigh = CHANNEL_HIGH;
		m_iMidiNoteChannelLow = CHANNEL_LOW;
		m_iMidiControllerChannel = CONTROLLER_CHANNEL;
		m_iMidiControllerIndex = CONTROLLER_INDEX;

		m_aiNotesHigh = new int[MAX_NOTES];
		m_aiCordIndices = new int[MAX_NOTES];

		fMaxInput = 0.25;
		fMinInput = 0.75;

		// Setup recorded notes and default it to a single note with middle C so you don't need to record anything to test
		m_aiRecordedNotes = new int[MAX_RECORDED_NOTES];
		m_aiRecordedNotes[0] = 60;
		m_iNumRecordedNotes = 1;
		m_bRecording = false;
		m_iRecordingPlaybackIndex = 0;

		m_bRecordingEnabled = true;

		m_bNoteOn = false;

		m_aiAllCords = new int[g_aiCordsScaleIndexOffsets.length * g_aiBaseScaleIntervals.length][3];
		m_iNumCords = 0;
		BuildAllCords();

		m_iMaxActiveNotes = 3 * g_iOctaveOffsets.length;
		m_aiCurNotes = new int[m_iMaxActiveNotes];
		m_iNumCurNotes = 0;

		m_abActiveMidiNotes = new boolean[120]; // This is sort of arbitrary but 108 is the top of piano keyboard so we should be safe

		m_fNoteIntensity = 0.0;
		m_bNoteEventThisFrame = false;
	}


	public void SendNoteOn(int iChannel, int iNote, int iVelocity)
	{
		println("TEMP_CL SendNoteOn iNote=" + iNote + "  m_fNoteIntensity="+m_fNoteIntensity);
		if(!m_bNoteEventThisFrame)
		{
			m_fNoteIntensity += 1.0;
			m_bNoteEventThisFrame = true;
		}
		if(m_fNoteIntensity < NOTE_INTENSITY_MAX)
		{
			m_oMidiOut.sendNoteOn(iChannel, iNote, iVelocity);
		}
		else
		{
			println("TEMP_CL Skipping sendNoteOn");
		}
	}


	public void SendNoteOff(int iChannel, int iNote, int iVelocity)
	{
		println("TEMP_CL SendNoteOff m_fNoteIntensity="+m_fNoteIntensity);
		if(!m_bNoteEventThisFrame)
		{
			m_fNoteIntensity += 1.0;
			m_bNoteEventThisFrame = true;
		}
		if(m_fNoteIntensity < NOTE_INTENSITY_MAX)
		{
			int iNumOffs = SEND_EXTRA_NOTE_OFF ? 5 : 1;
			for(int i = 0; i < iNumOffs; i++)
			{
				m_oMidiOut.sendNoteOff(iChannel, iNote, iVelocity);
			}
		}
		else
		{
			println("TEMP_CL Skipping sendNoteOff");
		}
	}

	public void BuildAllCords()
	{
		for(int iCord = 0; iCord < g_aiCordsScaleIndexOffsets.length; ++iCord)
		{
			for(int iFirstNote = 0; iFirstNote < g_aiBaseScaleIntervals.length; ++iFirstNote)
			{
				AddCord(g_aiBaseScaleIntervals[(g_aiCordsScaleIndexOffsets[iCord][0] + iFirstNote) % g_aiBaseScaleIntervals.length],
					    g_aiBaseScaleIntervals[(g_aiCordsScaleIndexOffsets[iCord][1] + iFirstNote) % g_aiBaseScaleIntervals.length],
						g_aiBaseScaleIntervals[(g_aiCordsScaleIndexOffsets[iCord][2] + iFirstNote) % g_aiBaseScaleIntervals.length]);
			}
		}
	}

	public void AddCord(int iNote1, int iNote2, int iNote3)
	{
		m_aiAllCords[m_iNumCords][0] = iNote1 % OCTAVE;
		m_aiAllCords[m_iNumCords][1] = iNote2 % OCTAVE;
		m_aiAllCords[m_iNumCords][2] = iNote3 % OCTAVE;

		println("TEMP_CL added cord " + m_aiAllCords[m_iNumCords][0] + "," +
			                            m_aiAllCords[m_iNumCords][1] + "," +
										m_aiAllCords[m_iNumCords][2]);

		++m_iNumCords;
	}

	public int FindNewNoteInCord()
	{
		int[] aiValidCordIndices = new int[m_aiAllCords.length];
		int iNumMatchingCords = 0;

		for(int iCordIndex = 0; iCordIndex < m_aiAllCords.length; ++iCordIndex)
		{
			boolean bCordContainsAllActiveNotes = true;
			for(int iNoteIndex = 0; iNoteIndex < m_iNumCurNotes; ++iNoteIndex)
			{
				// Check to see if this note is contained in this cord
				boolean bCordContainsThisNote = false;
				for(int iTriadIndex = 0; iTriadIndex < 3; iTriadIndex++)
				{
					if(m_aiCurNotes[iNoteIndex] == m_aiAllCords[iCordIndex][iTriadIndex])
					{
						bCordContainsThisNote = true;
						break;
					}
				}
				
				if(!bCordContainsThisNote)
				{
					bCordContainsAllActiveNotes = false;
					break;
				}
			}

			if(bCordContainsAllActiveNotes)
			{
				aiValidCordIndices[iNumMatchingCords++] = iCordIndex;
			}
		}

		// Pick a random cord from the set of one matches
		int iNewCordIndex = int(random(iNumMatchingCords));
		println("TEMP_CL picked cord with notes " + m_aiAllCords[aiValidCordIndices[iNewCordIndex]][0] + "," +
			                                        m_aiAllCords[aiValidCordIndices[iNewCordIndex]][1] + "," +
													m_aiAllCords[aiValidCordIndices[iNewCordIndex]][2] + ",");

		// Create list of all possible notes across multiple octaves that could be used to voice this cord.
		// Exclude notes that are already being played
		int iNumPossibleNotes = 0;
		int[] aiPossibleNotes = new int[3 * g_iOctaveOffsets.length];
		for(int iOctave = 0; iOctave < g_iOctaveOffsets.length; ++iOctave)
		{
			for(int iCordNote = 0; iCordNote < 3; ++iCordNote)
			{
				int iNote = m_aiAllCords[aiValidCordIndices[iNewCordIndex]][iCordNote] + g_iOctaveOffsets[iOctave] * OCTAVE;
				if(!m_abActiveMidiNotes[g_iBaseNote + iNote])
				{
					println("TEMP_CL adding possible note " + iNote);
					aiPossibleNotes[iNumPossibleNotes++] = iNote;
				}
			}
		}

		// Pick a random note
		int iNewNote = aiPossibleNotes[int(random(iNumPossibleNotes))];
		println("iNewNote=" + iNewNote);
		return iNewNote;
	}

	public void AddNoteToCord()
	{
		if(m_iNumCurNotes >= m_iMaxActiveNotes)
		{
			println("AddNoteToCord called when m_iNumCurNotes=" + m_iNumCurNotes + ".  Full up on notes.  Bailing.");
			return;
		}

		int iNewNote = FindNewNoteInCord();

		PlayCordNote(iNewNote);
	}

	public void RemoveNoteFromCord()
	{
		// Check to see if we actually have any current notes.  If not, bail
		if(m_iNumCurNotes <= 0)
		{
			return;
		}

		int iNoteToRemove = m_aiCurNotes[int(random(m_iNumCurNotes))];
		ReleaseCordNote(iNoteToRemove);
	}

	public void PlayCordNote(int iNote)
	{
		println("TEMP_CL PlayCordNote note=" + iNote + " MIDI=" + (g_iBaseNote + iNote));

		// Save current note info
		m_aiCurNotes[m_iNumCurNotes++] = iNote;
		m_abActiveMidiNotes[g_iBaseNote + iNote] = true;

		// Actually send the midi note
		SendNoteOn(m_iMidiNoteChannelLow, g_iBaseNote + iNote, 127);

		println("TEMP_CL Post PlayCordNote");
	}

	public void ReleaseCordNote(int iNote)
	{
		// Check to see if we actually have any current notes.  If not, bail
		if(m_iNumCurNotes <= 0)
		{
			return;
		}

		// Remove note from current note info
		boolean bFoundNote = false;
		for(int i = 0; i < m_iNumCurNotes-1; ++i)
		{
			if(iNote == m_aiCurNotes[i])
			{
				bFoundNote = true;
			}

			// Overwrite this note with the next one
			if(bFoundNote)
			{
				m_aiCurNotes[i] = m_aiCurNotes[i+1];
			}
		}
		--m_iNumCurNotes;
		m_abActiveMidiNotes[g_iBaseNote + iNote] = false;

		// Actually send the midi note off
		SendNoteOff(m_iMidiNoteChannelLow, g_iBaseNote + iNote, 0);
	}

	public void PlayIndividualNote(int iVelocity)
	{
		int iNote = FindNewNoteInCord();

		// TEMP_CL - introduce random in key note that isn't in the cord
		if(random(4) <= 1.0)
		{
			iNote = g_aiBaseScaleIntervals[int(random(g_aiBaseScaleIntervals.length))];
			println("OUT OF CORD NOTE!");
		}
		// Only use one octave here
		else
		{
			iNote = iNote % OCTAVE;
		}

		println("TEMP_CL PlayIndividualNote iNote=" + iNote + " MIDI=" + (g_iBaseNote + iNote + OCTAVE));

		// Send a note off followed by a note off
		SendNoteOn(m_iMidiNoteChannelHigh, g_iBaseNote + iNote + OCTAVE, iVelocity);
		delay(20); // Doesn't work without a delay
		SendNoteOff(m_iMidiNoteChannelHigh, g_iBaseNote + iNote + OCTAVE, 0);
	}

	public void noteOnReceived(Note oNote)
	{
		if(!m_bRecordingEnabled)
		{
			return;
		}

		// TEMP_CL - Right now bail on all notes above middle C
		if(oNote.getPitch() < 60)
		{
			return;
		}

		println("Note on: " + oNote.getPitch() + ", velocity: " + oNote.getVelocity());
		SendNoteOn(m_iMidiNoteChannelHigh, oNote.getPitch(), oNote.getVelocity());

		// If this is a note on event, record the note
		if(oNote.getVelocity() > 0)
		{
			if(!m_bRecording)
			{
				m_bRecording = true;
				m_iNumRecordedNotes = 0;
			}

			m_aiRecordedNotes[m_iNumRecordedNotes] = oNote.getPitch();
			if(m_iNumRecordedNotes < MAX_RECORDED_NOTES)
			{
				m_iNumRecordedNotes++;
			}
			else
			{
				println("Record buffer full!");
			}
		}
	}

	public void noteOffReceived(Note oNote)
	{
		// This never seems to be called...
		println("Note off: " + oNote.getPitch());
		SendNoteOff(m_iMidiNoteChannelHigh, oNote.getPitch(), 0);
	}

	void Update(long iCurTimeMS, float fInput, boolean bNoteOnEvent, boolean bNoteOffEvent, int iNoteVelocity)
	{
		if(!m_bInitializedOutput)
		{
			return;
		}

		// Get delta time
		long iLastTimeMS = m_iCurTimeMS;
		m_iCurTimeMS = iCurTimeMS;
		long iDeltaTimeMS = m_iCurTimeMS - iLastTimeMS;

		// Deal with NoteIntensity.  This prevents too many notes being sent to the midi system which sometimes causes it to shutdown
		m_bNoteEventThisFrame = false;
		if(m_fNoteIntensity > NOTE_INTENSITY_MAX * 1.5)
		{
			m_fNoteIntensity = NOTE_INTENSITY_MAX * 1.5;
		}
		if(m_fNoteIntensity > 0)
		{
			m_fNoteIntensity -= NOTE_INTENSITY_DROP_PER_SECOND * iDeltaTimeMS / 1000.0;
		}

		// Change key if we haven't gotten any input recently
		//println("TEMP_CL bNoteOnEvent=" + bNoteOnEvent + " bNoteOffEvent=" + bNoteOffEvent + " m_iNumCurNotes=" + m_iNumCurNotes);
		if(fInput > NOTE_OFF_THRESHOLD)
		{
			//println("TEMP_CL ======================================== Maybe new base note");
			if(m_iCurTimeMS - m_iLastInputEvent > CHANGE_KEY_NO_INPUT_TIME_MS)
			{
				g_iBaseNote = g_aiPossibleBaseNotes[int(random(g_aiPossibleBaseNotes.length))];
				println("\n\n\n\n============================New base note = " + g_iBaseNote);
			}
			m_iLastInputEvent = m_iCurTimeMS;
		}

		if(bNoteOnEvent && m_bRecording)
		{
			m_bRecording = false;
			m_iRecordingPlaybackIndex = m_iNumRecordedNotes - 1;
		}



		// Cord system shifting system

		int iMaxNumNotes = 5;
		int iExpectedNumNotes = int(fInput * iMaxNumNotes) + 1;
		if(fInput < NOTE_OFF_THRESHOLD)
		{
			--iExpectedNumNotes;
		}

		if(m_iNumCurNotes != iExpectedNumNotes)
		{
			while(m_iNumCurNotes < iExpectedNumNotes)
			{
				println("TEMP_CL m_iNumCurNotes=" + m_iNumCurNotes + " iExpectedNumNotes=" + iExpectedNumNotes);
				AddNoteToCord();
			}
			while(m_iNumCurNotes > iExpectedNumNotes)
			{
				RemoveNoteFromCord();
			}

			// Print current notes
			//for(int i = 0; i < g_aiBaseScaleIntervals.length; ++i)
			//{
			//	print(m_abActiveMidiNotes[g_iBaseNote + g_aiBaseScaleIntervals[i]] ? "X" : ".");
			//}
			for(int i = 21; i < 107; ++i)
			{
				int iRelativeNote = (i-21) % 12;
				//switch(iRelativeNote)
				//{
				//case 0:
				//case 2:
				//case 3:
				//case 5:
				//case 7:
				//case 8:
				//case 10:
				//	print("|");
				//	break;
				//default:
				//	print("#");
				//	break;
				//}
				switch(iRelativeNote)
				{
				case 0:
					print("A");
					break;
				case 2:
					print("B");
					break;
				case 3:
					print("C");
					break;
				case 5:
					print("D");
					break;
				case 7:
					print("E");
					break;
				case 8:
					print("F");
					break;
				case 10:
					print("G");
					break;
				default:
					print("#");
					break;
				}
				//print(char(iRelativeNote+65));
			}
			println("");
			for(int i = 21; i < 107; ++i)
			{
				print(m_abActiveMidiNotes[i] ? "X" : ".");
			}
			println("");
		}

		int iTestControllerValue1 = ClampI(int(fInput * 80) + 30, 0, 127);
		m_oMidiOut.sendController(m_iMidiControllerChannel, m_iMidiControllerIndex, iTestControllerValue1);

		// Play individual notes of touch events.
		if(bNoteOnEvent)
		{
			PlayIndividualNote(iNoteVelocity);
		}

		// Apparently we can't just bail here in a simple way...  : (
		if(iCurTimeMS >= 0)
		{
			return;
		}

		// end - Cord system shifting system



		// Playback recording

		int iTestControllerValue = ClampI(int(fInput * 127), 0, 127);
		m_oMidiOut.sendController(m_iMidiControllerChannel, m_iMidiControllerIndex, iTestControllerValue);

		if(m_iNumRecordedNotes == 0)
		{
			return;
		}

		if(bNoteOnEvent)
		{
			SendNoteOff(m_iMidiNoteChannelHigh, m_aiRecordedNotes[m_iRecordingPlaybackIndex], 0);
			m_iRecordingPlaybackIndex = (m_iRecordingPlaybackIndex + 1) % m_iNumRecordedNotes;
			SendNoteOn(m_iMidiNoteChannelHigh, m_aiRecordedNotes[m_iRecordingPlaybackIndex], iNoteVelocity);
			m_bNoteOn = true;
		}
		//else if(bNoteOffEvent) // TEMP_CL - skip note off for now and just use low input state
		else if(m_bNoteOn && fInput < 0.1)
		{
			SendNoteOff(m_iMidiNoteChannelHigh, m_aiRecordedNotes[m_iRecordingPlaybackIndex], 0);
			m_bNoteOn = false;
		}	

		// Apparently we can't just bail here in a simple way...  : (
		if(iCurTimeMS >= 0)
		{
			return;
		}

		// end - Playback recording



		// If we ever fall below this threshold, turn off all notes and turn off controller
		//println("TEMP_CL fInput=" + fInput + " m_iNumNotesHigh=" + m_iNumNotesHigh + " iNoteVelocity=" + iNoteVelocity);
		if(fInput < NOTE_OFF_THRESHOLD && m_iNumNotesHigh > 0)
		{
			println("TEMP_CL - kill all input");

			// Turn off high notes
			for(int i = 0; i <  m_iNumNotesHigh; ++i)
			{
				SendNoteOff(m_iMidiNoteChannelHigh, m_aiNotesHigh[i] + TRANSPOSE, 0);
			}
			m_iNumNotesHigh = 0;

			// Turn off low notes
			SendNoteOff(m_iMidiNoteChannelLow, g_aiNoteSet[m_iFundamentalIndex]    - OCTAVE + TRANSPOSE, 0);
			SendNoteOff(m_iMidiNoteChannelLow, g_aiCordSet[m_iFundamentalIndex][1] - OCTAVE + TRANSPOSE, 0);
			SendNoteOff(m_iMidiNoteChannelLow, g_aiCordSet[m_iFundamentalIndex][2] - OCTAVE + TRANSPOSE, 0);

			// Turn down the controller
			m_oMidiOut.sendController(m_iMidiControllerChannel, m_iMidiControllerIndex, 0);
		}

		// TEMP_CL if(bNoteOffEvent && m_iNumNotesHigh > 0)
		if(bNoteOffEvent && m_iNumNotesHigh > 1)
		{
			println("TEMP_CL - note off");

			// Use the note off event to turn off cord notes but not the fundamental
			m_iNumNotesHigh--;
			SendNoteOff(m_iMidiNoteChannelHigh, m_aiNotesHigh[m_iNumNotesHigh] + TRANSPOSE, 0);

			// If we aren't playing any notes, turn off the cord
			if(m_iNumNotesHigh < 1)
			{
				// Turn off low notes
				SendNoteOff(m_iMidiNoteChannelLow, g_aiNoteSet[m_iFundamentalIndex]    - OCTAVE + TRANSPOSE, 0);
				SendNoteOff(m_iMidiNoteChannelLow, g_aiCordSet[m_iFundamentalIndex][1] - OCTAVE + TRANSPOSE, 0);
				SendNoteOff(m_iMidiNoteChannelLow, g_aiCordSet[m_iFundamentalIndex][2] - OCTAVE + TRANSPOSE, 0);
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
				SendNoteOn(m_iMidiNoteChannelLow, g_aiNoteSet[m_iFundamentalIndex]    - OCTAVE + TRANSPOSE, iNoteVelocity);
				SendNoteOn(m_iMidiNoteChannelLow, g_aiCordSet[m_iFundamentalIndex][1] - OCTAVE + TRANSPOSE, iNoteVelocity);
				SendNoteOn(m_iMidiNoteChannelLow, g_aiCordSet[m_iFundamentalIndex][2] - OCTAVE + TRANSPOSE, iNoteVelocity);
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
			SendNoteOn(m_iMidiNoteChannelHigh, m_aiNotesHigh[m_iNumNotesHigh-1] + TRANSPOSE, iNoteVelocity);

			// TEMP_CL - trigger drum kit to test timing
			//SendNoteOn(m_iMidiNoteChannelHigh, 44, iNoteVelocity);
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

	void ToggleRecording()
	{
		m_bRecordingEnabled = !m_bRecordingEnabled;
		println("m_bRecordingEnabled=" + m_bRecordingEnabled);
	}

	// Tracking time (used to get delta time)
	long m_iCurTimeMS;
	long m_iLastInputEvent;

	// If the device isn't initialized, it won't output/input any MIDI
	boolean m_bInitializedOutput;
	boolean m_bInitializedInput;

	// MIDI output object
	MidiOutput m_oMidiOut;

	// MIDI input object
	MidiInput m_oMidiIn;

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

	// Recording
	int[] m_aiRecordedNotes;
	int m_iNumRecordedNotes;
	boolean m_bRecording;
	int m_iRecordingPlaybackIndex;
	boolean m_bRecordingEnabled;

	boolean m_bNoteOn;

	int[][] m_aiAllCords;
	int m_iNumCords;
	int m_iMaxActiveNotes;
	int[] m_aiCurNotes;
	int m_iNumCurNotes;
	boolean[] m_abActiveMidiNotes;

	float m_fNoteIntensity;
	boolean m_bNoteEventThisFrame;
}
