/**
 * File: EaMidiPC.pde
 *
 * Description: Processing program that collects motion signals from many remote nodes via XBee wireless
 * and then sends it to a virtual MIDI instrument
 *
 * Copyright: 2013 Chris Linder
 */

import rwmidi.*;
import processing.serial.*;

// Constants
static int NUM_NODES = 8;
static int NODE_RESPONSE_TIMEOUT_MS = 5;

// The serial port used to talk to node connected directly to the PC
Serial g_port;
 
// MIDI output object
MidiOutput g_midiOut;

// The lastest motion reading from the nodes
int[] g_aiLastestMotion = new int[NUM_NODES];

// True if we got different motion reading for this node
boolean[] g_abNewMotionData = new boolean[NUM_NODES];

// Send data start byte
static int START_SEND_BYTE = 240; // Binary = 11110000 (this also translates to node 7 sending 16 (out of 32) as a motion value).
static int END_SEND_BYTE = 4; // Binary = 00000100 (this also translates to node 0 sending 4 (out of 32) as a motion value).

// Every time we start this program, update all nodes with
// the speed and smoothing values from here.  This makes
// tuning all the nodes MUCH easier.  All nodes need to 
// be on for this to work though.

// The min speed in meters per second to respond to.  Any motion at or below this will be 
// considered no motion at all.
static float fMinSpeed = 0.02;

// The max speed in meters per second.  All motion above this speed will be treated like this speed
static float fMaxSpeed = 0.3;

// This is the speed smoothing factor (0, 1.0].  Low values mean more smoothing while a value of 1 means 
// no smoothing at all.  This value depends on the loop speed so if anything changes the loop speed,
// the amount of smoothing will change (see LOOP_DELAY_MS).
static float fNewSpeedWeight = 0.04;

// Setup function for processing
void setup()
{
	// Draw a 400x400 blank rectangle
	size(400, 400); 
	noStroke();
	rectMode(CENTER);

	// List valid MIDI output devices.
	println("Available MIDI ouput devices:");
	for(int i = 0; i < RWMidi.getOutputDeviceNames().length; i++)
	{
		println("MIDI output device name " + i + " = " + RWMidi.getOutputDeviceNames()[i]);
	}

	// Bail if there aren't any MIDI outputs
	if(RWMidi.getOutputDeviceNames().length == 0)
	{
		exit();
	}

	// Pick the correct MIDI device.
	// TEMP_CL - See if there is a way to avoid hardcoding this...
	g_midiOut = RWMidi.getOutputDevices()[3].createOutput();
 
	// List available serial ports
	println("Available serial ports: " + Serial.list().length);
	println(Serial.list());

	// Bail if there aren't any serial ports
	if(Serial.list().length == 0)
	{
		exit();
	}

	// Pick the correct serial port.  It is almost always the first device.
	g_port = new Serial(this, Serial.list()[0], 9600);


	// Update min speed, max speed, and smoothing for all nodes

	// convert fMinSpeed to two bytes of data to send. This has a range from 0.0 to 2.0 stored 2 bytes
	int iNewMinSpeed = int(fMinSpeed * 65535.0 / 2.0);
	int iNewMinSpeedU = iNewMinSpeed >> 8;
	int iNewMinSpeedL = iNewMinSpeed & 255;

	// convert fMaxSpeed to two bytes of data to send. This has a range from 0.0 to 2.0 stored 2 bytes
	int iNewMaxSpeed = int(fMaxSpeed * 65535.0 / 2.0);
	int iNewMaxSpeedU = iNewMaxSpeed >> 8;
	int iNewMaxSpeedL = iNewMaxSpeed & 255;

	// convert fNewSpeedWeight to two bytes of data to send. This has a range from 0.0 to 1.0 stored 2 bytes
	int iNewWeight = int(fNewSpeedWeight * 65535.0);
	int iNewWeightU = iNewWeight >> 8;
	int nNewWeightL = iNewWeight & 255;

	g_port.write(START_SEND_BYTE);
	g_port.write(iNewMinSpeedU);
	g_port.write(iNewMinSpeedL);
	g_port.write(iNewMaxSpeedU);
	g_port.write(iNewMaxSpeedL);
	g_port.write(iNewWeightU);
	g_port.write(nNewWeightL);
	g_port.write(END_SEND_BYTE);
}



// Draw / loop function for processing
// Unless there is too much going on this runs at 60 FPS (about 16 or 17 ms).
void draw()
{
	// Draw some partial alpha squares just to have something visually entertaining
	// and to tell if the app becomes unresponsive.
	background(51); 
	fill(255, 204);
	rect(mouseX, height/2, mouseY/2+10, mouseY/2+10);
	fill(255, 204);
	int inverseX = width-mouseX;
	int inverseY = height-mouseY;
	rect(inverseX, height/2, (inverseY/2)+10, (inverseY/2)+10);
  
	// DEBUG - Keep this code around to test MIDI output without the nodes
	//int controlOutX = int((float)mouseX / width * 127);
	//int controlOutY = int((float)mouseY / width * 127);
	//println("Controllers controlOutX=" + controlOutX + " controlOutY=" + controlOutY);
	//g_midiOut.sendController(0, 10, controlOutX);
	//g_midiOut.sendController(0, 11, controlOutY);

	// Reset g_abNewMotionData before we read new values
	for(int i = 0; i < NUM_NODES; i++)
	{
		g_abNewMotionData[i] = false;
	}

	// Read all the incoming messages and save them off into g_aiLastestMotion.
	// It is important to read all the messages or else the buffer of backlog message
	// will grow they will be lag between the new signal and it getting sent to MIDI
	while(g_port.available() > 0)
	{
		// read byte and then parse out node and motion value
		int iReadByte = g_port.read();
		int iNodeIndex = iReadByte >> 5;
		int iMotion = (iReadByte & 31) << 3;

		println("Read value iNodeIndex=" + iNodeIndex + " iMotion=" + iMotion);

		// Save off new value.  Since we can get multiple values from the same node in this loop, it is import
		// to remember if any of the values are new, not just the last one.
		g_abNewMotionData[iNodeIndex] = g_abNewMotionData[iNodeIndex] || g_aiLastestMotion[iNodeIndex] != iMotion;
		g_aiLastestMotion[iNodeIndex] = iMotion;
	}

	// Go through all the motion values and if we got a new one, send it as MIDI
	for(int i = 0; i < NUM_NODES; i++)
	{
		// If we have a new motion value, send it to MIDI.
		// If we send the same signal over and over again 
		// the MIDI mapper (loopbe1 in this case) can freak out
		// and think there is a feedback loop.
		if(g_abNewMotionData[i])
		{
			int iMidiValue = g_aiLastestMotion[i] / 2;

			// TEMP_CL - clamp values to 100 right now just to avoid clipping since we're mapping to volume as a test
			if(iMidiValue > 100)
			{
				iMidiValue = 100;
			}

			println("Write MIDI i=" + i + " iMidiValue=" + iMidiValue);

			g_midiOut.sendController(0, 10 + i, iMidiValue);
		}
	}
}
