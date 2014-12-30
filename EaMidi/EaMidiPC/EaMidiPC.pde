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
import controlP5.*;

// Number of nodes
static int NUM_NODES = 7;

// If this much time passes, without hearing from a node, we assume it is broken and set its output to 0
static int NODE_UPDATE_TIMEOUT_MS = 2000;

// The serial port used to talk to node connected directly to the PC
Serial g_port;

// Baud rate of g_port
static int COM_BAUD_RATE = 9600;
//static int COM_BAUD_RATE = 4800;
//static int COM_BAUD_RATE = 28800; // Didn't seem to work but didn't test a ton

// Sometimes, we want to test with just the PC and not read from the sensors.
// If no serial interface is found, this will be set to false and no communication
// will be read from the serial port (because it doesn't exist).
boolean g_bUseSerial = true;
 
// MIDI output object
MidiOutput g_midiOut;

// The midi channel used to set master volume to make the piece silent if no one has used it for a while
static int MASTER_VOLUME_CHANNEL = 7;

// The midi controller number used to set master volume to make the piece silent if no one has used it for a while
static int MASTER_VOLUME_CONTROLLER = 0;

// The midi note number used to set master volume to make the piece silent if no one has used it for a while
static int MASTER_VOLUME_NOTE = 60;

// Min movement speed (0-255) to trigger note on and off
static int MIN_SPEED_FOR_NOTE = 40;

// If this number time passes, the piece goes silent (aka standby mode)
static int NO_MOTION_STANDBY_TIME_MS = 20000;

// The current time in MS.  Saved between frames and used to calc delta time
int g_iCurTimeMS = 0;

// If too much time passes, the piece goes silent (aka standby mode)
boolean g_bStandbyMode = false;

// The last time motion was detected
int g_aiLastMotionTime = 0;

// The node that the max motion was last detected on.  This is used to see which node "blipping on"
// is preventing standby mode from kicking in.
int g_iMaxMotionNodeIndex = 0;

// Standby activity.  Ranges from 0 to 1.  Each time there is movement, this activity is incremented based on STANDYBY_FULL_ON_SECONDS.
// Each time this is no movement it is decremented based on STANDYBY_FULL_OFF_SECONDS.  This is compared to STANDBY_ON_THRESHOLD and
// STANDBY_OFF_THRESHOLD to determine when to enter and exit standby mode.
float g_fStandyActivityAmount = 0.0;

// The number of seconds it takes the system to go from 0 activity to full on if a node's movement is full on.
static float STANDYBY_FULL_ON_SECONDS = 0.5;

// The number of seconds it takes the system to go from full activity to 0 if no motion is seen on any nodes.
static float STANDYBY_FULL_OFF_SECONDS = 20.0;

// When g_fStandyActivityAmount falls below this the sytem enters standby mode. 
static float STANDBY_ON_THRESHOLD = 0.4;

// When g_fStandyActivityAmount goes above this the sytem exits standby mode. 
static float STANDBY_OFF_THRESHOLD = 0.6;

// Used to fade master volume in and out with standby mode.
int g_iMasterVolume = 0;
int g_iTargetMasterVolume = 0;

// "Full" volume for when standby is off
static int STANDBY_OFF_VOLUME = 100;

// Standby volume fade out rate - TODO - make this based on delta time.  Just per tick now.
static int STANDBY_VOLUME_FADE_OUT_RATE = 1;

// Standby volume fade int rate - TODO - make this based on delta time.  Just per tick now.
static int STANDBY_VOLUME_FADE_IN_RATE = 5;


// The lastest motion reading from the nodes
int[] g_aiLastestMotion = new int[NUM_NODES];

// True if we got different motion reading for this node
boolean[] g_abNewMotionData = new boolean[NUM_NODES];

// The last time in MS that we got a new value.  Used to zero out nodes that have stopped communicating.
int[] g_aiLastMotionUpdateTime = new int[NUM_NODES];

// If this is true, the "note" for the node is on
boolean[] g_abNoteOn = new boolean[NUM_NODES];

// This is used for testing and lets the PC override the latest motion data
int[] g_aiPCOverrideMotion = new int[NUM_NODES];

// Send data start byte
static int START_SEND_BYTE = 240; // Binary = 11110000 (this also translates to node 7 sending 16 (out of 32) as a motion value).
static int END_SEND_BYTE = 241; // Binary = 11110001 (this also translates to node 7 sending 17 (out of 32) as a motion value).

// The filename of the settings file.  If the file doesn't exist, the values below are used.
static String SETTINGS_FILENAME = "NodeSettings.txt";

// The number of tuning / config vars there are per node.  Some of these the vars are sent to the different nodes live.
static int NUM_TUNING_VARS = 10;

// Tuning - The min speed in meters per second to respond to.  Any motion at or below this will be 
// considered no motion at all.
float[] afMinSpeed = {0.02,0.02,0.02,0.02,0.02,0.02,0.02};

// Tuning - The max speed in meters per second.  All motion above this speed will be treated like this speed
float[] afMaxSpeed = {0.22,0.22,0.22,0.22,0.22,0.22,0.22};

// Tuning - This is the speed smoothing factor (0, 1.0].  Low values mean more smoothing while a value of 1 means 
// no smoothing at all.  This value depends on the loop speed so if anything changes the loop speed,
// the amount of smoothing will change (see LOOP_DELAY_MS).
float[] afNewSpeedWeight = {0.05,0.05,0.05,0.05,0.05,0.05,0.05};

// Tuning - The exponent to apply to the linear 0.0 to 1.0 value from the sensor.  This allows the sensitivity curve
// to be adjusted in a non-linear fashion.
float[] afInputExponent = {1.0,1.0,1.0,1.0,1.0,1.0,1.0};

// Tuning - The max MIDI value this sensor will return.  The sensor's full range is scaled between the min and max MIDI values.
float[] afMaxMIDIValue = {127,127,127,127,127,127,127};

// Tuning - The min MIDI value this sensor will return.  The sensor's full range is scaled between the min and max MIDI values.
float[] afMinMIDIValue = {0,0,0,0,0,0,0};

// Tuning - MIDI controller to adjust with the sensor
float[] afMIDIController = {10,11,12,13,14,15,16};

// Tuning - MIDI controler channel index
float[] afMIDIControllerChannel = {0,0,0,0,0,0,0};

// Tuning - MIDI note to trigger (60 is middle C)
float[] afMIDINote = {60,60,60,60,60,60,60};

// Tuning - MIDI note channel index
float[] afMIDINoteChannel = {1,2,3,4,5,6,7};


// If this is true, we wait our turn on the com bus and then send out the tuning updates
boolean  g_bPushNewValuesToNodes = false;

// At startup do repeated pushes to nodes to try very hard to make sure they have the proper tuning values.
// This is how many times to push the tuning values.
int g_iStartupPushValuesToNodesTimes = 3;

// This var tracks how long to wait between startup tuning value pushes in MS.  It should be initialized to 0.
float g_fCurTimeTillNextStartupPushMS = 0;

// This is the next node index expected on the com bus.  Each node takes its turn on the com bus.
int g_iNextExpectedNodeIndex = 0;

// But sometimes a node drops out so we need to time out.
int g_iLastReceiveTime = 0;
static int NODE_COM_TIMEOUT_MS = 30; // This NEEDS to be the same as the setting in the node file!

// Layout constants
static int WINDOW_WIDTH = 700;
static int WINDOW_HEIGHT = 850;
static int LIGHT_BARS_INPUT_HEIGHT = 350;

// Controller for things like text fields and check boxes.
ControlP5 cp5;

// The checkbox container that hold our binary settings
CheckBox checkbox;

// One of our binary settings is if we want to link all the tuning settings together so we only need to type
// in one box as opposed to each node.  When this is true, we only accept input from node one and then
// copy it to all other other nodes.
static int LINK_ALL_NODES_CHECKBOX_INDEX = 0;
boolean g_bLinkAllNodes = false;

// We want to track focus and focus loss on text boxes so we don't have to press enter each time we change something.
Textfield currentFocusedTf;



// Setup function for processing
void setup()
{
  // Draw a blank rectangle
  size(WINDOW_WIDTH, WINDOW_HEIGHT); 
  noStroke();
  rectMode(CORNER);

  // Load our tuning setting from a file.  If the file doesn't exist or has problems, use the defaults specified
  // in the var declaration above.
  LoadSettingsFromFile();

  // Create the new controller for text boxes and check boxes.
  cp5 = new ControlP5(this);

  // Create a bigger than default font for the text boxes
  PFont font = createFont("arial",20);

  // Create all the per node text boxes
  int iOffsetY = 0;
  for(int i = 0; i < NUM_NODES; i++)
  {
    int iOffsetX = i * width / NUM_NODES + 3;
	iOffsetY = 10;
    cp5.addTextfield("Max_MIDI_" + i,        iOffsetX, LIGHT_BARS_INPUT_HEIGHT + iOffsetY, 70, 20)
      .setAutoClear(false)
      .setFont(font)
      .setText(str(afMaxMIDIValue[i]));
	iOffsetY += 40;
    cp5.addTextfield("Min_MIDI_" + i,        iOffsetX, LIGHT_BARS_INPUT_HEIGHT + iOffsetY, 70, 20)
      .setAutoClear(false)
      .setFont(font)
      .setText(str(afMinMIDIValue[i]));
	iOffsetY += 40;
    cp5.addTextfield("MIDI_Controller_" + i,        iOffsetX, LIGHT_BARS_INPUT_HEIGHT + iOffsetY, 70, 20)
      .setAutoClear(false)
      .setFont(font)
      .setText(str(afMIDIController[i]));
	iOffsetY += 40;
    cp5.addTextfield("Ctrl_Channel_" + i,        iOffsetX, LIGHT_BARS_INPUT_HEIGHT + iOffsetY, 70, 20)
      .setAutoClear(false)
      .setFont(font)
      .setText(str(afMIDIControllerChannel[i]));
	iOffsetY += 40;
    cp5.addTextfield("MIDI_Note_" + i,        iOffsetX, LIGHT_BARS_INPUT_HEIGHT + iOffsetY, 70, 20)
      .setAutoClear(false)
      .setFont(font)
      .setText(str(afMIDINote[i]));
	iOffsetY += 40;
    cp5.addTextfield("Note_Channel_" + i,        iOffsetX, LIGHT_BARS_INPUT_HEIGHT + iOffsetY, 70, 20)
      .setAutoClear(false)
      .setFont(font)
      .setText(str(afMIDINoteChannel[i]));
	iOffsetY += 40;
    cp5.addTextfield("Min_Speed_" + i,        iOffsetX, LIGHT_BARS_INPUT_HEIGHT + iOffsetY, 70, 20)
      .setAutoClear(false)
      .setFont(font)
      .setText(str(afMinSpeed[i]));
	iOffsetY += 40;
    cp5.addTextfield("Max_Speed_" + i,        iOffsetX, LIGHT_BARS_INPUT_HEIGHT + iOffsetY, 70, 20)
      .setAutoClear(false)
      .setFont(font)
      .setText(str(afMaxSpeed[i]));
	iOffsetY += 40;
    cp5.addTextfield("New_Speed_Weight_" + i, iOffsetX, LIGHT_BARS_INPUT_HEIGHT + iOffsetY, 70, 20)
      .setAutoClear(false)
      .setFont(font)
      .setText(str(afNewSpeedWeight[i]));
	iOffsetY += 40;
    cp5.addTextfield("Input_Exponent_" + i, iOffsetX, LIGHT_BARS_INPUT_HEIGHT + iOffsetY, 70, 20)
      .setAutoClear(false)
      .setFont(font)
      .setText(str(afInputExponent[i]));
	iOffsetY += 40;
  }

  // Create a check box container for all our binary settings
  iOffsetY += 30;
  checkbox = cp5.addCheckBox("ToggleSettings")
    .setPosition(3, LIGHT_BARS_INPUT_HEIGHT + iOffsetY)
    .setSize(25, 25)
    .addItem("Link_All_Nodes", LINK_ALL_NODES_CHECKBOX_INDEX);

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
  g_midiOut = RWMidi.getOutputDevices()[4].createOutput();
 
  // List available serial ports
  println("Available serial ports: " + Serial.list().length);
  println(Serial.list());

  // f there aren't any serial ports
  if(Serial.list().length == 0)
  {
    g_bUseSerial = false;
  }

  // Pick the correct serial port.  It is almost always the last device.
  if(g_bUseSerial)
  {
    g_port = new Serial(this, Serial.list()[Serial.list().length - 1], COM_BAUD_RATE);
  }
}



// Update min speed, max speed, and smoothing for all nodes
void SendNewValuesToNodes()
{
	println("SendNewValuesToNodes");

	if(g_bUseSerial)
	{
		g_port.write(START_SEND_BYTE);

		for(int i = 0; i < NUM_NODES; i++)
		{
			// convert fMinSpeed to two bytes of data to send. This has a range from 0.0 to 2.0 stored 2 bytes
			int iNewMinSpeed = int(afMinSpeed[i] * 65535.0 / 2.0);
			int iNewMinSpeedU = iNewMinSpeed >> 8;
			int iNewMinSpeedL = iNewMinSpeed & 255;

			// convert fMaxSpeed to two bytes of data to send. This has a range from 0.0 to 2.0 stored 2 bytes
			int iNewMaxSpeed = int(afMaxSpeed[i] * 65535.0 / 2.0);
			int iNewMaxSpeedU = iNewMaxSpeed >> 8;
			int iNewMaxSpeedL = iNewMaxSpeed & 255;

			// convert fNewSpeedWeight to two bytes of data to send. This has a range from 0.0 to 1.0 stored 2 bytes
			int iNewWeight = int(afNewSpeedWeight[i] * 65535.0);
			int iNewWeightU = iNewWeight >> 8;
			int iNewWeightL = iNewWeight & 255;

			// convert fNewSpeedWeight to two bytes of data to send. This has a range from 0.0 to 5.0 stored 2 bytes
			int iNewExponent = int(afInputExponent[i] * 65535.0 / 5.0);
			int iNewExponentU = iNewExponent >> 8;
			int iNewExponentL = iNewExponent & 255;

			g_port.write(iNewMinSpeedU);
			g_port.write(iNewMinSpeedL);
			g_port.write(iNewMaxSpeedU);
			g_port.write(iNewMaxSpeedL);
			g_port.write(iNewWeightU);
			g_port.write(iNewWeightL);
			g_port.write(iNewExponentU);
			g_port.write(iNewExponentL);

			println("sent iNewMinSpeedU=" + iNewMinSpeedU);
			println("sent iNewMinSpeedL=" + iNewMinSpeedL);
			println("sent iNewMaxSpeedU=" + iNewMaxSpeedU);
			println("sent iNewMaxSpeedL=" + iNewMaxSpeedL);
			println("sent iNewWeightU=" + iNewWeightU);
			println("sent iNewWeightL=" + iNewWeightL);
			println("sent iNewExponentU=" + iNewExponentU);
			println("sent iNewExponentL=" + iNewExponentL);
		}

		g_port.write(END_SEND_BYTE);
		println("### New settings sent!");
	}

	g_bPushNewValuesToNodes = false;
}



void SaveSettingsToFile()
{
  int iNumLinesPerNode = NUM_TUNING_VARS + 1;

  String[] asData = new String[iNumLinesPerNode * NUM_NODES];
  for(int i = 0; i < NUM_NODES; i++)
  {
    int iDataOffet = i * iNumLinesPerNode;
    asData[iDataOffet++] = str(i);
    asData[iDataOffet++] = str(afMinSpeed[i]);
    asData[iDataOffet++] = str(afMaxSpeed[i]);
    asData[iDataOffet++] = str(afNewSpeedWeight[i]);
    asData[iDataOffet++] = str(afInputExponent[i]);
    asData[iDataOffet++] = str(afMaxMIDIValue[i]);
    asData[iDataOffet++] = str(afMinMIDIValue[i]);
    asData[iDataOffet++] = str(afMIDIController[i]);
    asData[iDataOffet++] = str(afMIDIControllerChannel[i]);
    asData[iDataOffet++] = str(afMIDINote[i]);
    asData[iDataOffet++] = str(afMIDINoteChannel[i]);
  }
  saveStrings(SETTINGS_FILENAME, asData);
}



void LoadSettingsFromFile()
{
  int iNumLinesPerNode = NUM_TUNING_VARS + 1;

  String[] asData = loadStrings(SETTINGS_FILENAME);
  if(asData == null)
  {
    println("Settings file doesn't exist.");
  }
  else if(asData.length != iNumLinesPerNode * NUM_NODES)
  {
    println("Invalid settings file format!");
  }
  else
  {
    try
    {
      for(int i = 0; i < NUM_NODES; i++)
      {
        int iDataOffet = i * iNumLinesPerNode; 
        // First entry is node index. We ignore it while reading but it helps make the file more human readable.
        afMinSpeed[i] =              Float.parseFloat(asData[++iDataOffet]);
        afMaxSpeed[i] =              Float.parseFloat(asData[++iDataOffet]);
        afNewSpeedWeight[i] =        Float.parseFloat(asData[++iDataOffet]);
        afInputExponent[i] =         Float.parseFloat(asData[++iDataOffet]);
        afMaxMIDIValue[i] =          Float.parseFloat(asData[++iDataOffet]);
        afMinMIDIValue[i] =          Float.parseFloat(asData[++iDataOffet]);
        afMIDIController[i] =        Float.parseFloat(asData[++iDataOffet]);
        afMIDIControllerChannel[i] = Float.parseFloat(asData[++iDataOffet]);
        afMIDINote[i] =              Float.parseFloat(asData[++iDataOffet]);
        afMIDINoteChannel[i] =       Float.parseFloat(asData[++iDataOffet]);
      }
    }
    catch (NumberFormatException e)
    {
      println("Error contents of settings file!");
    }
  }
}



// Draw / loop function for processing
// Unless there is too much going on this runs at 60 FPS (about 16 or 17 ms).
void draw()
{
	// Get current time and delta time
	int iNewTimeMS = millis();
	int iDeltaTimeMS = iNewTimeMS - g_iCurTimeMS;
	g_iCurTimeMS = iNewTimeMS;

	// Reset g_abNewMotionData before we read new values
	for(int i = 0; i < NUM_NODES; i++)
	{
		g_abNewMotionData[i] = false;
	}

	// Draw midi input from mouse (and also from sensors if pc override input is 0)
	background(51); 
	for(int i = 0; i < NUM_NODES; i++)
	{
		// Get left and right bounds of this vertical rect
		int iLeft = i * width/NUM_NODES;
		int iRight = (i+1) * width/NUM_NODES;

		// Make it possible to instantly set to 0
		boolean bForceUpdate = false;

		// If the mouse is down, update the pc override values
		if(mousePressed && mouseX >= iLeft && mouseX < iRight && mouseY < LIGHT_BARS_INPUT_HEIGHT)
		{
			g_aiPCOverrideMotion[i] = 255 - mouseY;
			if(g_aiPCOverrideMotion[i] < 0)
			{
				g_aiPCOverrideMotion[i] = 0;
				bForceUpdate = true;
			}
			else if(g_aiPCOverrideMotion[i] > 255)
			{
				g_aiPCOverrideMotion[i] = 255;
			}
		}

		// If we have overriden to a non-zero value, force that value into the motion data.
		if(bForceUpdate || g_aiPCOverrideMotion[i] > 0)
		{
			g_abNewMotionData[i] = g_abNewMotionData[i] || g_aiLastestMotion[i] != g_aiPCOverrideMotion[i];
			g_aiLastestMotion[i] = g_aiPCOverrideMotion[i];
			g_aiLastMotionUpdateTime[i] = g_iCurTimeMS;
		}

		fill(g_aiLastestMotion[i], 255);
		rect(iLeft, 0, width/NUM_NODES - 2, 255);

		// Draw mark for last max motion
		if(g_iMaxMotionNodeIndex == i)
		{
			fill(200,0,0,255);
			rect(iLeft, 235, width/NUM_NODES - 2, 20);
		}
	}

	if(g_bStandbyMode)
	{
		fill(125, 255);
		rect(0, 0, width, height/4);
	}
  
	// DEBUG - Keep this code around to test MIDI output without the nodes
	//int controlOutX = int((float)mouseX / width * 127);
	//int controlOutY = int((float)mouseY / width * 127);
	//println("Controllers controlOutX=" + controlOutX + " controlOutY=" + controlOutY);
	//g_midiOut.sendController(0, 10, controlOutX);
	//g_midiOut.sendController(0, 11, controlOutY);

	// Read all the incoming messages and save them off into g_aiLastestMotion.
	// It is important to read all the messages or else the buffer of backlog message
	// will grow they will be lag between the new signal and it getting sent to MIDI
	while(g_bUseSerial && g_port.available() > 0)
	{
		// read byte and then parse out node and motion value
		int iReadByte = g_port.read();
		if(iReadByte == 0)
		{
			println("Got 0 byte!  Invalid! #########################################");
			continue;
		}

		// Get node index
		int iNodeIndex = iReadByte >> 5;

		// We are compressing our speed ratio
		// to 5 bits (32 values) so that it fits in a single byte.
		// We are also never sending a byte of 0 so make speed run from 1 to 31, not 0 to 31
		int iMotion = ((iReadByte & 31) - 1) * 255 / 30;

		println("Read value iNodeIndex=" + iNodeIndex + " iMotion=" + iMotion + " at time " + g_iCurTimeMS);

		if(iNodeIndex >= NUM_NODES)
		{
			println("Got invalid node index!");
			continue;
		}

		// Only update the motion value if we aren't overriding it
		if(g_aiPCOverrideMotion[iNodeIndex] == 0)
		{
			// Save off new value.  Since we can get multiple values from the same node in this loop, it is import
			// to remember if any of the values are new, not just the last one.
			g_abNewMotionData[iNodeIndex] = g_abNewMotionData[iNodeIndex] || g_aiLastestMotion[iNodeIndex] != iMotion;
			g_aiLastestMotion[iNodeIndex] = iMotion;
			g_aiLastMotionUpdateTime[iNodeIndex] = g_iCurTimeMS;
		}

		// Save off the last node index and the time of the the last value so that we 
		// can know when to talk on the common bus.
		g_iNextExpectedNodeIndex = (iNodeIndex + 1) % (NUM_NODES + 1); // The PC gets a chance to talk also and has a node index of NUM_NODES
		g_iLastReceiveTime = g_iCurTimeMS;
	}

	// Check for a com timeout and then increment the next expected node.
	// This is slightly different from the node timeout below.  We might consider it ok
	// to occationally miss an update in the com cycle but not consider that a node
	// that needs to be zeroed out.
	while(g_iCurTimeMS - g_iLastReceiveTime > NODE_COM_TIMEOUT_MS)
	{
		println(g_iCurTimeMS + " Timeout waiting for node " + g_iNextExpectedNodeIndex); 

		g_iNextExpectedNodeIndex = (g_iNextExpectedNodeIndex + 1) % (NUM_NODES + 1); // The PC gets a chance to talk also and has a node index of NUM_NODES
		g_iLastReceiveTime += NODE_COM_TIMEOUT_MS;
	}

	// TEMP_CL This is causing more problems than it seems to be fixing right now.  For the current run, the plan is to not have auto program switching.
	//// At startup do repeated pushes to nodes to try very hard to make sure they have the proper tuning values
	//if(!g_bPushNewValuesToNodes && g_iStartupPushValuesToNodesTimes > 0)
	//{
	//	g_fCurTimeTillNextStartupPushMS -= iDeltaTimeMS;
	//	if(g_fCurTimeTillNextStartupPushMS < 0)
	//	{
	//		g_bPushNewValuesToNodes = true;
	//		g_iStartupPushValuesToNodesTimes--;
	//		g_fCurTimeTillNextStartupPushMS = 2000;
	//	}
	//}

	// If it is our turn to talk, do that
	if(g_iNextExpectedNodeIndex == NUM_NODES)
	{
		// If we have new values to push to the nodes, check to see if it is our turn to talk
		if(g_bPushNewValuesToNodes)
		{
			SendNewValuesToNodes();
		}
		// Otherwise, just send a fake message with our "node" index to keep com flow going
		else if(g_bUseSerial)
		{
			int iSendByte = (NUM_NODES << 5) | 1;

			delay(3); // This delay matches the delay on the node side.  Without it the PC can send its message too earlyf or the nodes to be listening.
			g_port.write(iSendByte);
			println(g_iCurTimeMS + " Just sent PC message"); 
		}

		// Move along on the next expected node index and timeout time.
		g_iNextExpectedNodeIndex = 0;
		g_iLastReceiveTime = g_iCurTimeMS;
	}

	// Check for nodes timing out
	for(int i = 0; i < NUM_NODES; i++)
	{
		if(g_aiLastestMotion[i] > 0 && g_iCurTimeMS - g_aiLastMotionUpdateTime[i] > NODE_UPDATE_TIMEOUT_MS)
		{
			println("Node " + i + " timed out.");
			g_abNewMotionData[i] = true;
			g_aiLastestMotion[i] = 0;
		}
	}

	// Calc max motion and use it to kill master volume (standby mode) if all nodes read 0 for too long
	int iMaxMotion = 0;
	for(int i = 0; i < NUM_NODES; i++)
	{
		if(g_aiLastestMotion[i] > iMaxMotion)
		{
			iMaxMotion = g_aiLastestMotion[i];
			g_iMaxMotionNodeIndex = i;
		}
	}
	if(iMaxMotion > 0)
	{
		g_aiLastMotionTime = g_iCurTimeMS;

		// Quickly increase activty if there is motion.  Increase faster based on level of activity
		g_fStandyActivityAmount += (1/STANDYBY_FULL_ON_SECONDS) * (iMaxMotion / 255.0) * (iDeltaTimeMS / 1000.0);
		if(g_fStandyActivityAmount > 1.0)
		{
			g_fStandyActivityAmount = 1.0;
		}
	}
	else
	{
		// Slowly reduce activity if there no motion at this time
		g_fStandyActivityAmount -= (1/STANDYBY_FULL_OFF_SECONDS) * (iDeltaTimeMS / 1000.0);
		if(g_fStandyActivityAmount < 0.0)
		{
			g_fStandyActivityAmount = 0.0;
		}
	}

	// Standby mode based on amount of activity
	if(g_bStandbyMode && g_fStandyActivityAmount > STANDBY_OFF_THRESHOLD)
	{
		println("Standby OFF");

		g_midiOut.sendNoteOn(MASTER_VOLUME_CHANNEL, MASTER_VOLUME_NOTE, 127); // default to full velocity
		g_iTargetMasterVolume = STANDBY_OFF_VOLUME;

		g_bStandbyMode = false;

		// TEMP_CL This is causing more problems than it seems to be fixing right now.  For the current run, the plan is to not have auto program switching.
		//// Try pushing new nodes values every time it turns back on.  Also will grab people's attention.
		//g_bPushNewValuesToNodes = true;
	}
	else if(!g_bStandbyMode && g_fStandyActivityAmount < STANDBY_ON_THRESHOLD)
	{
		println("Standby ON");

		g_midiOut.sendNoteOff(MASTER_VOLUME_CHANNEL, MASTER_VOLUME_NOTE, 0);
		g_iTargetMasterVolume = 0;

		g_bStandbyMode = true;
	}

	// Fade volume on and off
	if(g_iTargetMasterVolume != g_iMasterVolume)
	{
		if(g_iMasterVolume > g_iTargetMasterVolume)
		{
			g_iMasterVolume -= STANDBY_VOLUME_FADE_OUT_RATE;
			if(g_iMasterVolume < g_iTargetMasterVolume)
			{
				g_iMasterVolume = g_iTargetMasterVolume;
			}
		}
		else
		{
			g_iMasterVolume += STANDBY_VOLUME_FADE_IN_RATE;
			if(g_iMasterVolume > g_iTargetMasterVolume)
			{
				g_iMasterVolume = g_iTargetMasterVolume;
			}
		}
		g_midiOut.sendController(MASTER_VOLUME_CHANNEL, MASTER_VOLUME_CONTROLLER, g_iMasterVolume);
	}

	// Go through all the motion values and if we got a new one, send it as MIDI
	for(int i = 0; i < NUM_NODES; i++)
	{
		// If we have a new motion value, send it to MIDI.
		// If we send the same signal over and over again 
		// the MIDI mapper (loopbe1 in this case) can freak out
		// and think there is a feedback loop.
		// Also, send a note on event if the previous value was 0 and a note off if it is now 0 but was previously on
		if(g_abNewMotionData[i])
		{
			int iMidiControllerValue = int(afMinMIDIValue[i] + (afMaxMIDIValue[i] - afMinMIDIValue[i]) * (g_aiLastestMotion[i] / 255.0));
			int iMidiControllerIndex = int(afMIDIController[i]);
			int iMidiControllerChannel = int(afMIDIControllerChannel[i]);
			int MidiNote = int(afMIDINote[i]);
			int MidiNoteChannel = int(afMIDINoteChannel[i]);

			if(!g_abNoteOn[i] && g_aiLastestMotion[i] > MIN_SPEED_FOR_NOTE)
			{
				println("Note ON node=" + i + " MidiNote=" + MidiNote + " MidiNoteChannel=" + MidiNoteChannel);
				g_midiOut.sendNoteOn(MidiNoteChannel, MidiNote, 127); // default to full velocity
				g_abNoteOn[i] = true;
			}
			else if(g_abNoteOn[i] && g_aiLastestMotion[i] <= MIN_SPEED_FOR_NOTE)
			{
				println("Note OFF node=" + i + " MidiNote=" + MidiNote + " MidiChannel=" + MidiNoteChannel);
				g_midiOut.sendNoteOff(MidiNoteChannel, MidiNote, 0);
				g_abNoteOn[i] = false;
			}

			// TEMP_CL println("Write controller node=" + i + " iMidiControllerValue=" + iMidiControllerValue + " iMidiControllerIndex=" + iMidiControllerIndex + " iMidiControllerChannel=" + iMidiControllerChannel);
			g_midiOut.sendController(iMidiControllerChannel, iMidiControllerIndex, iMidiControllerValue);
		}
	}
}



// Updates the given setting and text box.  Accounts for g_bLinkAllNodes and handles 
// invalid numbers.
void Update_Setting(int iNodeIndex, float[] afValues, String textFieldPrefix, String sValue, boolean bPushNewSettingsToNodes)
{
  float fNewValue = 0;
  boolean bLinkThisField = g_bLinkAllNodes && textFieldPrefix != "MIDI_Controller_" && textFieldPrefix != "Ctrl_Channel_" && textFieldPrefix != "MIDI_Note_" && textFieldPrefix != "Note_Channel_";
  try
  {
    fNewValue = Float.parseFloat(sValue);
    if(bLinkThisField)
    {
      for(int i = 0; i < NUM_NODES; i++)
      {
        afValues[i] = fNewValue;
      }
    }
    else
    {
      afValues[iNodeIndex] = fNewValue;
    }

  }
  catch (NumberFormatException e)
  {
    println("Invalid number!");
  }

  if(bLinkThisField)
  {
    for(int i = 0; i < NUM_NODES; i++)
    {
      cp5.get(Textfield.class, textFieldPrefix + i).setText(str(afValues[i]));
    }
  }
  else
  {
    cp5.get(Textfield.class, textFieldPrefix + iNodeIndex).setText(str(afValues[iNodeIndex]));
  }
  println("Update setting" + textFieldPrefix + iNodeIndex + " = " + afValues[iNodeIndex]);

  if(bPushNewSettingsToNodes)
  {
	g_bPushNewValuesToNodes = true;
	println("### New settings queued up to be sent!");
  }

  SaveSettingsToFile();
}

public void Min_Speed_0(String sValue) { Update_Min_Speed(0, sValue); }
public void Min_Speed_1(String sValue) { Update_Min_Speed(1, sValue); }
public void Min_Speed_2(String sValue) { Update_Min_Speed(2, sValue); }
public void Min_Speed_3(String sValue) { Update_Min_Speed(3, sValue); }
public void Min_Speed_4(String sValue) { Update_Min_Speed(4, sValue); }
public void Min_Speed_5(String sValue) { Update_Min_Speed(5, sValue); }
public void Min_Speed_6(String sValue) { Update_Min_Speed(6, sValue); }
public void Update_Min_Speed(int iNodeIndex, String sValue)
{
  Update_Setting(iNodeIndex, afMinSpeed, "Min_Speed_", sValue, true);
}

public void Max_Speed_0(String sValue) { Update_Max_Speed(0, sValue); }
public void Max_Speed_1(String sValue) { Update_Max_Speed(1, sValue); }
public void Max_Speed_2(String sValue) { Update_Max_Speed(2, sValue); }
public void Max_Speed_3(String sValue) { Update_Max_Speed(3, sValue); }
public void Max_Speed_4(String sValue) { Update_Max_Speed(4, sValue); }
public void Max_Speed_5(String sValue) { Update_Max_Speed(5, sValue); }
public void Max_Speed_6(String sValue) { Update_Max_Speed(6, sValue); }
public void Update_Max_Speed(int iNodeIndex, String sValue)
{
  Update_Setting(iNodeIndex, afMaxSpeed, "Max_Speed_", sValue, true);
}

public void New_Speed_Weight_0(String sValue) { Update_New_Speed_Weight(0, sValue); }
public void New_Speed_Weight_1(String sValue) { Update_New_Speed_Weight(1, sValue); }
public void New_Speed_Weight_2(String sValue) { Update_New_Speed_Weight(2, sValue); }
public void New_Speed_Weight_3(String sValue) { Update_New_Speed_Weight(3, sValue); }
public void New_Speed_Weight_4(String sValue) { Update_New_Speed_Weight(4, sValue); }
public void New_Speed_Weight_5(String sValue) { Update_New_Speed_Weight(5, sValue); }
public void New_Speed_Weight_6(String sValue) { Update_New_Speed_Weight(6, sValue); }
public void Update_New_Speed_Weight(int iNodeIndex, String sValue)
{
  Update_Setting(iNodeIndex, afNewSpeedWeight, "New_Speed_Weight_", sValue, true);
}

public void Input_Exponent_0(String sValue) { Update_Input_Exponent(0, sValue); }
public void Input_Exponent_1(String sValue) { Update_Input_Exponent(1, sValue); }
public void Input_Exponent_2(String sValue) { Update_Input_Exponent(2, sValue); }
public void Input_Exponent_3(String sValue) { Update_Input_Exponent(3, sValue); }
public void Input_Exponent_4(String sValue) { Update_Input_Exponent(4, sValue); }
public void Input_Exponent_5(String sValue) { Update_Input_Exponent(5, sValue); }
public void Input_Exponent_6(String sValue) { Update_Input_Exponent(6, sValue); }
public void Update_Input_Exponent(int iNodeIndex, String sValue)
{
  Update_Setting(iNodeIndex, afInputExponent, "Input_Exponent_", sValue, true);
}

public void Max_MIDI_0(String sValue) { Update_Max_MIDI(0, sValue); }
public void Max_MIDI_1(String sValue) { Update_Max_MIDI(1, sValue); }
public void Max_MIDI_2(String sValue) { Update_Max_MIDI(2, sValue); }
public void Max_MIDI_3(String sValue) { Update_Max_MIDI(3, sValue); }
public void Max_MIDI_4(String sValue) { Update_Max_MIDI(4, sValue); }
public void Max_MIDI_5(String sValue) { Update_Max_MIDI(5, sValue); }
public void Max_MIDI_6(String sValue) { Update_Max_MIDI(6, sValue); }
public void Update_Max_MIDI(int iNodeIndex, String sValue)
{
  Update_Setting(iNodeIndex, afMaxMIDIValue, "Max_MIDI_", sValue, false);
}

public void Min_MIDI_0(String sValue) { Update_Min_MIDI(0, sValue); }
public void Min_MIDI_1(String sValue) { Update_Min_MIDI(1, sValue); }
public void Min_MIDI_2(String sValue) { Update_Min_MIDI(2, sValue); }
public void Min_MIDI_3(String sValue) { Update_Min_MIDI(3, sValue); }
public void Min_MIDI_4(String sValue) { Update_Min_MIDI(4, sValue); }
public void Min_MIDI_5(String sValue) { Update_Min_MIDI(5, sValue); }
public void Min_MIDI_6(String sValue) { Update_Min_MIDI(6, sValue); }
public void Update_Min_MIDI(int iNodeIndex, String sValue)
{
  Update_Setting(iNodeIndex, afMinMIDIValue, "Min_MIDI_", sValue, false);
}

public void MIDI_Controller_0(String sValue) { Update_MIDI_Controller(0, sValue); }
public void MIDI_Controller_1(String sValue) { Update_MIDI_Controller(1, sValue); }
public void MIDI_Controller_2(String sValue) { Update_MIDI_Controller(2, sValue); }
public void MIDI_Controller_3(String sValue) { Update_MIDI_Controller(3, sValue); }
public void MIDI_Controller_4(String sValue) { Update_MIDI_Controller(4, sValue); }
public void MIDI_Controller_5(String sValue) { Update_MIDI_Controller(5, sValue); }
public void MIDI_Controller_6(String sValue) { Update_MIDI_Controller(6, sValue); }
public void Update_MIDI_Controller(int iNodeIndex, String sValue)
{
  Update_Setting(iNodeIndex, afMIDIController, "MIDI_Controller_", sValue, false);
}

public void Ctrl_Channel_0(String sValue) { Update_Ctrl_Channel(0, sValue); }
public void Ctrl_Channel_1(String sValue) { Update_Ctrl_Channel(1, sValue); }
public void Ctrl_Channel_2(String sValue) { Update_Ctrl_Channel(2, sValue); }
public void Ctrl_Channel_3(String sValue) { Update_Ctrl_Channel(3, sValue); }
public void Ctrl_Channel_4(String sValue) { Update_Ctrl_Channel(4, sValue); }
public void Ctrl_Channel_5(String sValue) { Update_Ctrl_Channel(5, sValue); }
public void Ctrl_Channel_6(String sValue) { Update_Ctrl_Channel(6, sValue); }
public void Update_Ctrl_Channel(int iNodeIndex, String sValue)
{
  Update_Setting(iNodeIndex, afMIDIControllerChannel, "Ctrl_Channel_", sValue, false);
}

public void MIDI_Note_0(String sValue) { Update_MIDI_Note(0, sValue); }
public void MIDI_Note_1(String sValue) { Update_MIDI_Note(1, sValue); }
public void MIDI_Note_2(String sValue) { Update_MIDI_Note(2, sValue); }
public void MIDI_Note_3(String sValue) { Update_MIDI_Note(3, sValue); }
public void MIDI_Note_4(String sValue) { Update_MIDI_Note(4, sValue); }
public void MIDI_Note_5(String sValue) { Update_MIDI_Note(5, sValue); }
public void MIDI_Note_6(String sValue) { Update_MIDI_Note(6, sValue); }
public void Update_MIDI_Note(int iNodeIndex, String sValue)
{
  Update_Setting(iNodeIndex, afMIDINote, "MIDI_Note_", sValue, false);
}

public void Note_Channel_0(String sValue) { Update_Note_Channel(0, sValue); }
public void Note_Channel_1(String sValue) { Update_Note_Channel(1, sValue); }
public void Note_Channel_2(String sValue) { Update_Note_Channel(2, sValue); }
public void Note_Channel_3(String sValue) { Update_Note_Channel(3, sValue); }
public void Note_Channel_4(String sValue) { Update_Note_Channel(4, sValue); }
public void Note_Channel_5(String sValue) { Update_Note_Channel(5, sValue); }
public void Note_Channel_6(String sValue) { Update_Note_Channel(6, sValue); }
public void Update_Note_Channel(int iNodeIndex, String sValue)
{
  Update_Setting(iNodeIndex, afMIDINoteChannel, "Note_Channel_", sValue, false);
}



// This is called anytime a cp5 controller changes
void controlEvent(ControlEvent theEvent)
{
  if(theEvent.isFrom(checkbox))
  {
    for(int i = 0; i < checkbox.getArrayValue().length; i++)
    {
      boolean bIsChecked = ((int)checkbox.getArrayValue()[i] == 0) ? false : true;
      if(i == LINK_ALL_NODES_CHECKBOX_INDEX)
      {
        g_bLinkAllNodes = bIsChecked;

        // Enable / Disable all the text boxes except for the first node.
        for(int iNodeIndex = 1; iNodeIndex < NUM_NODES; iNodeIndex++)
        {
          cp5.get(Textfield.class,"Max_MIDI_"        + iNodeIndex).setLock(g_bLinkAllNodes);
          cp5.get(Textfield.class,"Min_MIDI_"        + iNodeIndex).setLock(g_bLinkAllNodes);
          cp5.get(Textfield.class,"Min_Speed_"        + iNodeIndex).setLock(g_bLinkAllNodes);
          cp5.get(Textfield.class,"Max_Speed_"        + iNodeIndex).setLock(g_bLinkAllNodes);
          cp5.get(Textfield.class,"New_Speed_Weight_" + iNodeIndex).setLock(g_bLinkAllNodes);
          cp5.get(Textfield.class,"Input_Exponent_" + iNodeIndex).setLock(g_bLinkAllNodes);

          int iColor = g_bLinkAllNodes ? 0 : cp5.get(Textfield.class,"Min_Speed_0").getColor().getBackground();
          cp5.get(Textfield.class,"Max_MIDI_"        + iNodeIndex).setColorBackground(iColor);
          cp5.get(Textfield.class,"Min_MIDI_"        + iNodeIndex).setColorBackground(iColor);
          cp5.get(Textfield.class,"Min_Speed_"        + iNodeIndex).setColorBackground(iColor);
          cp5.get(Textfield.class,"Max_Speed_"        + iNodeIndex).setColorBackground(iColor);
          cp5.get(Textfield.class,"New_Speed_Weight_" + iNodeIndex).setColorBackground(iColor);
          cp5.get(Textfield.class,"Input_Exponent_" + iNodeIndex).setColorBackground(iColor);
        }
      }
    }
  }
}



void mouseReleased()
{
  // Loop through all the text fields and see if we've just clicked on one.
  // If so, that is our currently focused text field so save it off.
  ControllerInterface[] ctrl = cp5.getControllerList();
  currentFocusedTf = null;
  for(int i=0; i < ctrl.length; ++i)
  {
    ControllerInterface ct = ctrl[i];
    if(ct instanceof Textfield)
    {
      Textfield tf = (Textfield) ct;
      if(tf.isFocus())
      {
        currentFocusedTf = tf;
        break;
      }
    }
  }
}

void mousePressed()
{
  // If we have a currently focused text field and we click somewhere, 
  // chances are we're about to lose focus so submit the contents of 
  // the field.  Even if we click in the field, we'll just submit the
  // current value so that is fine.
    if(currentFocusedTf != null)
    {
        currentFocusedTf.submit();
    currentFocusedTf = null;
    }
}
