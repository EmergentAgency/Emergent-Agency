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

	// Loop through all the nodes and send a request for info and then wait for an answer.
	// If now answer comes in a certain period of time, just go to the next node.
	// TEMP_CL for(int nNode = 0; nNode < NUM_NODES; nNode++)
	for(int nNode = 0; nNode < 2; nNode++)
	{
		// Create request for data message.  It is just a byte that has the 3 highest bits
		// set to the node number.  The idea is that no node but itself will ever send a byte
		// like that so if it hears it, it must be from the PC
		int iSendByte = nNode << 5;
		g_port.write(iSendByte);

		// Wait for the response.
		int iStartWaitTimeMS = millis();
		int iDebugLoopCount = 0; // TEMP_CL
		while(millis() - iStartWaitTimeMS < NODE_RESPONSE_TIMEOUT_MS)
		{
			// Wait a little just to give the node time to respond
			delay(1);

			// Check for responses.  There is a chance there will be more than one
			// if something gets out of sync so read and process all returned values
			// to keep things as up to date as possible.
			boolean bGotResponse = false;
			while(g_port.available() > 0)
			{
				// read byte and then parse out node and motion value
				int iReadByte = g_port.read();
				int iNodeIndex = iReadByte >> 5;
				int iMotion = (iReadByte & 31) << 3;

				println("Read value iNodeIndex=" + iNodeIndex + " iMotion=" + iMotion);

				g_abNewMotionData[iNodeIndex] = g_aiLastestMotion[iNodeIndex] != iMotion;
				g_aiLastestMotion[iNodeIndex] = iMotion;

				bGotResponse = true;
			}

			// Bail out of our timeout loop if we got a response
			if(bGotResponse)
			{
				println("TEMP_CL - got response iDebugLoopCount=" + iDebugLoopCount);
				break;
			}

			iDebugLoopCount++; // TEMP_CL
		}
	}
		
	//// Read all the incoming messages and save them off into g_aiLastestMotion.
	//// It is important to read all the messages or else the buffer of backlog message
	//// will grow they will be lag between the new signal and it getting sent to MIDI
	//while(g_port.available() > 0)
	//{
	//	// read byte and then parse out node and motion value
	//	int iReadByte = g_port.read();
	//	int iNodeIndex = iReadByte >> 5;
	//	int iMotion = (iReadByte & 31) << 3;

	//	println("Read value iNodeIndex=" + iNodeIndex + " iMotion=" + iMotion);

	//	g_abNewMotionData[iNodeIndex] = g_aiLastestMotion[iNodeIndex] != iMotion;
	//	g_aiLastestMotion[iNodeIndex] = iMotion;
	//}

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
			if(iMidiValue > 100)
			{
				iMidiValue = 100;
			}

			println("Write MIDI i=" + i + " iMidiValue=" + iMidiValue);

			g_midiOut.sendController(0, 10 + i, iMidiValue);
		}
	}
}
