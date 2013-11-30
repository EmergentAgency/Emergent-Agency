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

// TEMP_CL
ControlP5 cp5;

// Number of nodes
static int NUM_NODES = 7;

// If this much time passes, without hearing from a node, we assume it is broken and set its output to 0
static int NODE_UPDATE_TIMEOUT_MS = 2000;

// The serial port used to talk to node connected directly to the PC
Serial g_port;

// Sometimes, we want to test with just the PC and not read from the sensors.
// If no serial interface is found, this will be set to false and no communication
// will be read from the serial port (because it doesn't exist).
boolean g_bUseSerial = true;
 
// MIDI output object
MidiOutput g_midiOut;

// The midi controller number used to set master volume to make the piece silent if no one has used it for a while
static int MASTER_VOLUME_MIDI = 20;

// If this number time passes, the piece goes silent (aka standby mode)
static int NO_MOTION_STANDBY_TIME_MS = 20000;

// If too much time passes, the piece goes silent (aka standby mode)
boolean bStandbyMode = false;

// The last time motion was detected
int g_aiLastMotionTime = 0;

// The lastest motion reading from the nodes
int[] g_aiLastestMotion = new int[NUM_NODES];

// True if we got different motion reading for this node
boolean[] g_abNewMotionData = new boolean[NUM_NODES];

// The last time in MS that we got a new value.  Used to zero out nodes that have stopped communicating.
int[] g_aiLastMotionUpdateTime = new int[NUM_NODES];

// This is used for testing and lets the PC override the latest motion data
int[] g_aiPCOverrideMotion = new int[NUM_NODES];

// Send data start byte
static int START_SEND_BYTE = 240; // Binary = 11110000 (this also translates to node 7 sending 16 (out of 32) as a motion value).
static int END_SEND_BYTE = 241; // Binary = 11110001 (this also translates to node 7 sending 17 (out of 32) as a motion value).

// The filename of the settings file.  If the file doesn't exist, the values below are used.
static String SETTINGS_FILENAME = "NodeSettings.txt";

// The number of config vars there are per node
static int NUM_CONFIG_VARS = 3;

// The min speed in meters per second to respond to.  Any motion at or below this will be 
// considered no motion at all.
float[] afMinSpeed = {0.02,0.02,0.02,0.02,0.02,0.02,0.02};

// The max speed in meters per second.  All motion above this speed will be treated like this speed
float[] afMaxSpeed = {0.22,0.22,0.22,0.22,0.22,0.22,0.22};

// This is the speed smoothing factor (0, 1.0].  Low values mean more smoothing while a value of 1 means 
// no smoothing at all.  This value depends on the loop speed so if anything changes the loop speed,
// the amount of smoothing will change (see LOOP_DELAY_MS).
float[] afNewSpeedWeight = {0.05,0.05,0.05,0.05,0.05,0.05,0.05};

// TEMP_CL
boolean[] g_abNodeHadNewValues = new boolean[NUM_NODES];
boolean	g_bPushNewValuesToNodes = false;
int g_iNextExpectedNodeIndex = 0;
int g_iLastReceiveTime = 0;
static int NODE_COM_TIMEOUT_MS = 2000; // This is much too large but it should be a good test
static int LIGHT_BARS_INPUT_HEIGHT = 350;
CheckBox checkbox;
static int LINK_ALL_NODES_CHECKBOX_INDEX = 0;
boolean g_bLinkAllNodes = false;
Textfield currentFocusedTf;



// Setup function for processing
void setup()
{
	// Draw a 400x400 blank rectangle
	size(700, 600); 
	noStroke();
	//rectMode(CENTER);
	rectMode(CORNER);

	LoadSettingsFromFile();

	// TEMP_CL
	cp5 = new ControlP5(this);
	PFont font = createFont("arial",20);

	for(int i = 0; i < NUM_NODES; i++)
	{
		int iOffsetX = i * width / NUM_NODES + 3;
		cp5.addTextfield("Min_Speed_" + i,        iOffsetX, LIGHT_BARS_INPUT_HEIGHT + 10, 70, 25)
			.setAutoClear(false)
			.setFont(font)
			.setText(str(afMinSpeed[i]));
		cp5.addTextfield("Max_Speed_" + i,        iOffsetX, LIGHT_BARS_INPUT_HEIGHT + 50, 70, 25)
			.setAutoClear(false)
			.setFont(font)
			.setText(str(afMaxSpeed[i]));
		cp5.addTextfield("New_Speed_Weight_" + i, iOffsetX, LIGHT_BARS_INPUT_HEIGHT + 90, 70, 25)
			.setAutoClear(false)
			.setFont(font)
			.setText(str(afNewSpeedWeight[i]));
	}

	checkbox = cp5.addCheckBox("ToggleSettings")
		.setPosition(3, LIGHT_BARS_INPUT_HEIGHT + 150)
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
	g_midiOut = RWMidi.getOutputDevices()[3].createOutput();
 
	// List available serial ports
	println("Available serial ports: " + Serial.list().length);
	println(Serial.list());

	// f there aren't any serial ports
	if(Serial.list().length == 0)
	{
		g_bUseSerial = false;
	}

	// Pick the correct serial port.  It is almost always the first device.
	if(g_bUseSerial)
	{
		g_port = new Serial(this, Serial.list()[0], 9600);
	}

	// When we start, update the nodes so everyone is working with the same values
	g_bPushNewValuesToNodes = true;
}



// Update min speed, max speed, and smoothing for all nodes
void SendNewValuesToNodes()
{
	println("TEMP_CL SendNewValuesToNodes");

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
			int nNewWeightL = iNewWeight & 255;

			g_port.write(iNewMinSpeedU);
			g_port.write(iNewMinSpeedL);
			g_port.write(iNewMaxSpeedU);
			g_port.write(iNewMaxSpeedL);
			g_port.write(iNewWeightU);
			g_port.write(nNewWeightL);
		}

		g_port.write(END_SEND_BYTE);
	}

	g_bPushNewValuesToNodes = false;
}



void SaveSettingsToFile()
{
	int iNumLinesPerNode = NUM_CONFIG_VARS + 1;

	String[] asData = new String[iNumLinesPerNode * NUM_NODES];
	for(int i = 0; i < NUM_NODES; i++)
	{
		int iDataOffet = i * iNumLinesPerNode;
		asData[iDataOffet + 0] = str(i);
		asData[iDataOffet + 1] = str(afMinSpeed[i]);
		asData[iDataOffet + 2] = str(afMaxSpeed[i]);
		asData[iDataOffet + 3] = str(afNewSpeedWeight[i]);
	}
	saveStrings(SETTINGS_FILENAME, asData);
}



void LoadSettingsFromFile()
{
	int iNumLinesPerNode = NUM_CONFIG_VARS + 1;

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
				afMinSpeed[i] =       Float.parseFloat(asData[iDataOffet + 1]);
				afMaxSpeed[i] =       Float.parseFloat(asData[iDataOffet + 2]);
				afNewSpeedWeight[i] = Float.parseFloat(asData[iDataOffet + 3]);
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
	// Get current time.  Used for checking if nodes haven't updated a value in a while
	int iCurTimeMS = millis();

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
			g_aiLastMotionUpdateTime[i] = iCurTimeMS;
		}

		fill(g_aiLastestMotion[i], 255);
		rect(iLeft, 0, width/NUM_NODES - 2, 255);
	}
	if(bStandbyMode)
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
		int iNodeIndex = iReadByte >> 5;
		int iMotion = (iReadByte & 31) << 3;

		println("Read value iNodeIndex=" + iNodeIndex + " iMotion=" + iMotion);

		// Save off new value.  Since we can get multiple values from the same node in this loop, it is import
		// to remember if any of the values are new, not just the last one.
		g_abNewMotionData[iNodeIndex] = g_abNewMotionData[iNodeIndex] || g_aiLastestMotion[iNodeIndex] != iMotion;
		g_aiLastestMotion[iNodeIndex] = iMotion;
		g_aiLastMotionUpdateTime[iNodeIndex] = iCurTimeMS;

		// Save off the last node index and the time of the the last value so that we 
		// can know when to talk on the common bus.
		g_iNextExpectedNodeIndex = (iNodeIndex + 1) % (NUM_NODES + 1); // The PC gets a chance to talk also and has a node index of NUM_NODES
		g_iLastReceiveTime = iCurTimeMS;
	}

	// Check for a com timeout and then increment the next expected node.
	// This is slightly different from the node timeout below.  We might consider it ok
	// to occationally miss an update in the com cycle but not consider that a node
	// that needs to be zeroed out.
	if(iCurTimeMS - g_iLastReceiveTime > NODE_COM_TIMEOUT_MS)
	{
		println("Timedout waiting for node " + g_iNextExpectedNodeIndex); 

		g_iNextExpectedNodeIndex = (g_iNextExpectedNodeIndex + 1) % (NUM_NODES + 1); // The PC gets a chance to talk also and has a node index of NUM_NODES
		g_iLastReceiveTime = iCurTimeMS;
	}

	// If we have new values to push to the nodes, check to see if it is our turn to talk
	if(g_bPushNewValuesToNodes)
	{
		// If it is our turn to talk, do that
		if(g_iNextExpectedNodeIndex == NUM_NODES)
		{
			SendNewValuesToNodes();
			g_iNextExpectedNodeIndex = 0;
		}
	}

	// Check for nodes timing out
	for(int i = 0; i < NUM_NODES; i++)
	{
		if(g_aiLastestMotion[i] > 0 && iCurTimeMS - g_aiLastMotionUpdateTime[i] > NODE_UPDATE_TIMEOUT_MS)
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
		}
	}
	if(iMaxMotion > 0)
	{
		g_aiLastMotionTime = iCurTimeMS;
		if(bStandbyMode)
		{
			println("Write MIDI i=" + MASTER_VOLUME_MIDI + " iMidiValue=127");
			g_midiOut.sendController(0, MASTER_VOLUME_MIDI, 127);
			bStandbyMode = false;
		}
	}
	else if(!bStandbyMode && iCurTimeMS - g_aiLastMotionTime > NO_MOTION_STANDBY_TIME_MS)
	{
		println("Write MIDI i=" + MASTER_VOLUME_MIDI + " iMidiValue=0");
		g_midiOut.sendController(0, MASTER_VOLUME_MIDI, 0);
		bStandbyMode = true;
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
			println("Write MIDI i=" + i + " iMidiValue=" + iMidiValue);
			g_midiOut.sendController(0, 10 + i, iMidiValue);
		}
	}
}



// Updates the given setting and text box.  Accounts for g_bLinkAllNodes and handles 
// invalid numbers.
void Update_Setting(int iNodeIndex, float[] afValues, String textFieldPrefix, String sValue)
{
	float fNewValue = 0;
	try
	{
		fNewValue = Float.parseFloat(sValue);
		if(g_bLinkAllNodes)
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

	if(g_bLinkAllNodes)
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
	println(textFieldPrefix + iNodeIndex + " = " + afValues[iNodeIndex]);

	g_bPushNewValuesToNodes = true;
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
	Update_Setting(iNodeIndex, afMinSpeed, "Min_Speed_", sValue);
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
	Update_Setting(iNodeIndex, afMaxSpeed, "Max_Speed_", sValue);
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
	Update_Setting(iNodeIndex, afNewSpeedWeight, "New_Speed_Weight_", sValue);
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
					cp5.get(Textfield.class,"Min_Speed_"        + iNodeIndex).setLock(g_bLinkAllNodes);
					cp5.get(Textfield.class,"Max_Speed_"        + iNodeIndex).setLock(g_bLinkAllNodes);
					cp5.get(Textfield.class,"New_Speed_Weight_" + iNodeIndex).setLock(g_bLinkAllNodes);

					int iColor = g_bLinkAllNodes ? 0 : cp5.get(Textfield.class,"Min_Speed_0").getColor().getBackground();
					cp5.get(Textfield.class,"Min_Speed_"        + iNodeIndex).setColorBackground(iColor);
					cp5.get(Textfield.class,"Max_Speed_"        + iNodeIndex).setColorBackground(iColor);
					cp5.get(Textfield.class,"New_Speed_Weight_" + iNodeIndex).setColorBackground(iColor);
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