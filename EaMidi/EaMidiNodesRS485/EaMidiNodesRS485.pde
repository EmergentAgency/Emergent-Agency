/**
 * File: EaMidiNodesRS485.pde
 *
 * Description: Arduino program for each node that collects motion signals and transmits them to the PC.
 * This version is focused on using the RS485 protocol to communicate along a shared pair of wires.
 * The same logic can also be used for the XBees.
 *
 * Copyright: 2013 Chris Linder
 */

#include <EEPROM.h>

// Use Serial to print out debug statements
static bool USE_SERIAL_FOR_DEBUGGING = false;

// Number of nodes
static int NUM_NODES = 3;

// The index for this node.  If it is < 0, use the node index stored in
// EEPROM.  If >= 0, store that node index in the EEPROM of this node.
int iNodeIndex = -1;

// The address in EEPROM for various saved values
static const int EEPROM_ADDR_NODE_INDEX = 0;
static const int EEPROM_ADDR_DATA_VERSION = 1;
static const int EEPROM_ADDR_MIN_SPEED = 2;
static const int EEPROM_ADDR_MAX_SPEED = 4;
static const int EEPROM_ADDR_NEW_SPEED_WEIGHT = 6;

// The current data version of the EPROM data.  Each time the format changes, increment this
static const int CUR_EPROM_DATA_VERSION = 1;

// If this is true, we use Xbee to send signals.
static const bool USE_XBEE = true;

// If this is true, we use RS-485 to send signals.
static const bool USE_RS485 = false;

// If this is true, we are a remote node only communicating to send its current sensor
// reading along.
// If false, we are attached to a laptop via USB and are listening for all the other
// remote signals and the sending those signals to the laptop via USB serial
static const bool IS_REMOTE = true;

// The numbers of LEDs this node has
static const int NUM_LEDS_PER_NODE = 5;

// Output pin mapping
//static const int pins[NUM_LEDS_PER_NODE] = {16, 15, 14, 26, 25};
static const int pins[NUM_LEDS_PER_NODE] = {14, 15, 26, 25, 16};

// Interupt pin for motion sensor
static const int INT_PIN = 19; // INT7

// This is the base amount of time to delay (in ms) after each loop.
// This value is helpful for not sending data constantly and filling
// up the recieve buffer on the PC.
static const int LOOP_DELAY_BASE_MS = 12;

// An additional delay is added to the base delay to ensure that all the
// nodes don't end up repeatedy sending at the same time.  The XBee can
// handle packet collisions but this trys to avoid it in the first place.
static const int LOOP_DELAY_MAX_EXTRA_MS = 3;

// Because packets sometimes get dropped, we should periodically resend out current value
// even if it didn't charge.  This is the time for that forced resend.
static int NODE_FORCE_UPDATE_TIME_MS = 1000;

// This is the last time data was sent in ms.
unsigned long iLastUpdateTime = 0;

// This is the number of init loops to code does to test LEDs
static const int NUM_INIT_LOOPS = 2;

// Recieve data start byte and end byte
static const int START_RCV_BYTE = 240; // Binary = 11110000 (this also translates to node 7 sending 16 (out of 32) as a motion value).
static const int END_RCV_BYTE = 241; // Binary = 11110001 (this also translates to node 7 sending 17 (out of 32) as a motion value).

// Recieve data timeout counter.  Use this to avoid getting stuck waiting for data.
static const int RCV_TIMEOUT_MAX_COUNT = 1000;

// Lookup map used to linearize LED brightness
static const unsigned char exp_map[256]={
  0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,3,
  3,3,3,3,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,4,4,4,5,5,5,
  5,5,5,5,5,6,6,6,6,6,6,6,7,7,7,7,7,7,8,8,8,8,8,8,9,9,
  9,9,10,10,10,10,10,11,11,11,11,12,12,12,13,13,13,13,
  14,14,14,15,15,15,16,16,16,17,17,18,18,18,19,19,20,
  20,20,21,21,22,22,23,23,24,24,25,26,26,27,27,28,29,
  29,30,31,31,32,33,33,34,35,36,36,37,38,39,40,41,42,
  42,43,44,45,46,47,48,50,51,52,53,54,55,57,58,59,60,
  62,63,64,66,67,69,70,72,74,75,77,79,80,82,84,86,88,
  90,91,94,96,98,100,102,104,107,109,111,114,116,119,
  122,124,127,130,133,136,139,142,145,148,151,155,158,
  161,165,169,172,176,180,184,188,192,196,201,205,210,
  214,219,224,229,234,239,244,250,255
};

// The hardware serial port used for communication (either RS-485 or XBee)
HardwareSerial Uart = HardwareSerial();

// Hardware serial settings
static const int COM_BAUD_RATE = 9600;
//static const int COM_BAUD_RATE = 28800; // Didn't seem to work but didn't test a ton

// RS-485 specific settings
static const int MICRO_SECOND_DELAY_POST_WRITE_RS485 = 1000000 * 1 / (COM_BAUD_RATE/10) * 2; // formula from http://www.gammon.com.au/forum/?id=11428
static const int RS485_ENABLE_WRITE_PIN = 5; // TEMP_CL - actually make this a real value

// This is the offset for the IC2 (Wire) address which will be combined with the node index.
static const int WIRE_NODE_ADDR_OFFSET = 2;

// The motion sensor indicates the speed of the motion detected by oscillating its output pin.
// The faster the pulses, the faster the motion.  The detect the speed, we are counting the
// number of microseconds between rising pulses using a hardware interupt pin.  The interupt
// is called each time a rising pulse is detect and we save off the between the new pulse and
// the last pulse.  All the other code for calculating speed is done outside of teh interupt
// because the interupt code needs to be as light weight at possible so it returns execution
// to the main code as quickly as possible.

// This is the last period detected
volatile unsigned long g_iLastPeriodMicro = 0;

// This is the last time the interupt was called
volatile unsigned long g_iLastTimeMicro = 0;

// The min speed in meters per second to respond to.  Any motion at or below this will be 
// considered no motion at all.
static float fMinSpeed = 0.02;

// The max speed in meters per second.  All motion above this speed will be treated like this speed
static float fMaxSpeed = 0.22;

// This is the speed smoothing factor (0, 1.0].  Low values mean more smoothing while a value of 1 means 
// no smoothing at all.  This value depends on the loop speed so if anything changes the loop speed,
// the amount of smoothing will change (see LOOP_DELAY_MS).
//static float fNewSpeedWeight = 0.15;
static float fNewSpeedWeight = 0.05;

// This is the outout speed ratio [0, 1.0].  It is based on the speed read from the motion detector
// and fMinSpeed, fMaxSpeed, fNewSpeedWeight.
float g_fSpeedRatio;

// Settings needed for timing out nodes in case some disconnect, break, or a signal is lost.
static const int NODE_COM_TIMEOUT_MS = 30;
int g_iNextNodeIndex = 0;
unsigned long g_iLastReceiveTime = 0;



void setup()
{
	if(USE_SERIAL_FOR_DEBUGGING)
	{
		// setup debugging serial port (over USB cable) at 9600 bps
		Serial.begin(9600);
		Serial.println("EAMidiNodes - Setup");
	}

	// setup Uart
	Uart.begin(COM_BAUD_RATE);

	// Deal with the index for this node.
	// If iNodeIndex is < 0, use the node index stored in
	// EEPROM.  If >= 0, store that node index in the EEPROM of this node.
	if(iNodeIndex < 0)
	{
		iNodeIndex = EEPROM.read(EEPROM_ADDR_NODE_INDEX);
		if(iNodeIndex == 255)
		{
			if(USE_SERIAL_FOR_DEBUGGING)
			{
				Serial.println("EAMidiNodes - tried to read node index from EEPROM but failed to get valid value.  Using 0.");
			}
			iNodeIndex = 0;
		}
	}
	else
	{
		EEPROM.write(EEPROM_ADDR_NODE_INDEX, iNodeIndex);
	}

	// Read in settings from EPROM
	int ySavedDataVersion = EEPROM.read(EEPROM_ADDR_DATA_VERSION);
	int ySavedMinSpeedU = EEPROM.read(EEPROM_ADDR_MIN_SPEED);
	int ySavedMinSpeedL = EEPROM.read(EEPROM_ADDR_MIN_SPEED+1);
	int ySavedMaxSpeedU = EEPROM.read(EEPROM_ADDR_MAX_SPEED);
	int ySavedMaxSpeedL = EEPROM.read(EEPROM_ADDR_MAX_SPEED+1);
	int ySavedNewSpeedWeightU = EEPROM.read(EEPROM_ADDR_NEW_SPEED_WEIGHT);
	int ySavedNewSpeedWeightL = EEPROM.read(EEPROM_ADDR_NEW_SPEED_WEIGHT+1);
	if( ySavedDataVersion != CUR_EPROM_DATA_VERSION )
	{
		// Invalid saved settings
		if(USE_SERIAL_FOR_DEBUGGING)
		{
			Serial.print("EAMidiNodes - tried to read settings from EEPROM but failed.  Read in data version ");
			Serial.println(ySavedDataVersion);
		}
	}
	else
	{
		if(USE_SERIAL_FOR_DEBUGGING)
		{
			Serial.print("EAMidiNodes - Read in data version ");
			Serial.println(ySavedDataVersion);
			Serial.println(ySavedMinSpeedU);
			Serial.println(ySavedMinSpeedL);
			Serial.println(ySavedMaxSpeedU);
			Serial.println(ySavedMaxSpeedL);
			Serial.println(ySavedNewSpeedWeightU);
			Serial.println(ySavedNewSpeedWeightL);
		}

		// Update fMinSpeed. This has a range from 0.0 to 2.0 stored 2 bytes
		int iNewMinSpeed = (ySavedMinSpeedU << 8) + ySavedMinSpeedL;
		fMinSpeed = iNewMinSpeed / 65535.0 * 2.0;

		// Update fMaxSpeed. This has a range from 0.0 to 2.0 stored 2 bytes
		int iNewMaxSpeed = (ySavedMaxSpeedU << 8) + ySavedMaxSpeedL;
		fMaxSpeed = iNewMaxSpeed / 65535.0 * 2.0;

		// Update fNewSpeedWeight with new value. This has a range from 0.0 to 1.0 stored 2 bytes 
		int iNewWeight = (ySavedNewSpeedWeightU << 8) + ySavedNewSpeedWeightL;
		fNewSpeedWeight = iNewWeight / 65535.0;
	}

    // setup LED pins for output
    for(int i = 0; i < NUM_LEDS_PER_NODE; i++)
    {
        // turn on pins for output to test an LED
        pinMode(pins[i], OUTPUT);   
    }

 	// Do a simple startup sequence to test LEDs
	for(int nTest = 0; nTest < NUM_INIT_LOOPS; nTest++)
	{
		// Blink binary value of node index.
		// For some reason "for" loops here weren't compiling so I just wrote it all out...

		digitalWrite(pins[0], HIGH); 
		digitalWrite(pins[1], HIGH); 
		digitalWrite(pins[2], HIGH); 
		digitalWrite(pins[3], HIGH); 
		digitalWrite(pins[4], HIGH);
		delay(500);

		bool bPin0 = (iNodeIndex >> 0) & 1;
		bool bPin1 = (iNodeIndex >> 1) & 1;
		bool bPin2 = (iNodeIndex >> 2) & 1;
		bool bPin3 = (iNodeIndex >> 3) & 1;
		bool bPin4 = (iNodeIndex >> 4) & 1;
		digitalWrite(pins[0], bPin0 ? HIGH : LOW);
		digitalWrite(pins[1], bPin1 ? HIGH : LOW);
		digitalWrite(pins[2], bPin2 ? HIGH : LOW);
		digitalWrite(pins[3], bPin3 ? HIGH : LOW);
		digitalWrite(pins[4], bPin4 ? HIGH : LOW);
		delay(1500);

		digitalWrite(pins[0], LOW); 
		digitalWrite(pins[1], LOW); 
		digitalWrite(pins[2], LOW); 
		digitalWrite(pins[3], LOW); 
		digitalWrite(pins[4], LOW); 
	}

	// Setup interupt
	pinMode(INT_PIN, INPUT);
	attachInterrupt(INT_PIN, MotionDetectorPulse, RISING);   // Attach an Interupt to INT_PIN for timing period of motion detector input
}



// Interupt called to get time between pulses
void MotionDetectorPulse()
{
	unsigned long iCurTimeMicro = micros();
	//unsigned long iCurTimeMicro = millis();
	g_iLastPeriodMicro = iCurTimeMicro - g_iLastTimeMicro;
	g_iLastTimeMicro = iCurTimeMicro;
}



// Returns the number of micro seconds for the last period (time between pulses)
// for the motion sensor.  If the time between this call and
// the last inpterupt call is greater than the last period , we return the time
// between this call and the the last interupt.  This is to ensure that if we
// abruptly go from fast motion to slow motion, that this function will not smoothly
// transition to slow motion.
unsigned long GetLastPeriodMicro()
{
	unsigned long iCurTimeMicro = micros();
	//unsigned long iCurTimeMicro = millis();
	unsigned long iThisPeriodMicro = iCurTimeMicro - g_iLastTimeMicro;
	if(iThisPeriodMicro < g_iLastPeriodMicro)
		return g_iLastPeriodMicro;
	else
		return iThisPeriodMicro;
}



// Returns the current speed of the detected motion in meters per second.
// It is calculated with the following formula (pulled from the X-Band website)
// http://www.parallax.com/Portals/0/Downloads/docs/prod/sens/32213-X-BandMotionDetector-v1.1.pdf
//
// Resulting frequency for speed detection:
//
// Fd = 2V(Ft/c)cos(theta)
//
// Where: 
// Fd = Difference frequency (sometimes referred to as Doppler frequency) 
// V = Velocity of the target 
// Ft = Transmit frequency (10.525 GHz)
// c = Speed of light at 3 x 10^8 m/s
// theta = Motion direction angle deviation from perpendicular to the antenna PCB (Figure 1)
//
// Thus and object moving at 0.2 ms/2 =
// 2*0.2*(10.525*10^9/(3*10^8)) = 14.0333
float GetCurSpeed()
{
	float iLastPeriod = GetLastPeriodMicro();
	float fFreq = 1.0 / (iLastPeriod / 1000000.0);
	//float fFreq = 1.0 / (iLastPeriod / 1000.0);
	float fSpeed = fFreq / 70.1666; // 70.1666 = (2 * Ft/c) - Ignore angle (cos(theta)) because we have no way of knowing it
	return fSpeed;
}



// Clamp function
float Clamp(float fVal, float fMin, float fMax)
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



bool ReceiveTuningMessage()
{
	// Once we have the start of a message, delay until we have it all
	int iTimeoutCounter = 0;
	int iNumExpectedBytes = 3 * 2 * NUM_NODES + 1; // 3 tuning vars * 2 bytes per var * number of nodes + the end byte
	while(Uart.available() < iNumExpectedBytes && iTimeoutCounter < RCV_TIMEOUT_MAX_COUNT)
	{
		iTimeoutCounter++;
		delay(1);
	}

	// Return false if we hit the timeout max
	if(iTimeoutCounter >= RCV_TIMEOUT_MAX_COUNT)
	{
		return false;
	}

	// Read data
	int iNewMinSpeedU = -1;
	int iNewMinSpeedL = -1;
	int iNewMaxSpeedU = -1;
	int iNewMaxSpeedL = -1;
	int iNewWeightU = -1;
	int iNewWeightL = -1;
	for(int i = 0; i < NUM_NODES; i++)
	{
		// If this is our node, read in the values and save them
		if(i == iNodeIndex)
		{
			iNewMinSpeedU = Uart.read();
			iNewMinSpeedL = Uart.read();
			iNewMaxSpeedU = Uart.read();
			iNewMaxSpeedL = Uart.read();
			iNewWeightU = Uart.read();
			iNewWeightL = Uart.read();
		}
		// Otherwise, just read and ignore the results
		else
		{
			Uart.read();
			Uart.read();
			Uart.read();
			Uart.read();
			Uart.read();
			Uart.read();
		}
	}

	// Return false if we don't hit the correct end byte
	int iEndByte = Uart.read();
	if(iEndByte != END_RCV_BYTE)
	{
		return false;
	}

	// Update fMinSpeed. This has a range from 0.0 to 2.0 stored 2 bytes
	int iNewMinSpeed = (iNewMinSpeedU << 8) + iNewMinSpeedL;
	fMinSpeed = iNewMinSpeed / 65535.0 * 2.0;

	// Update fMaxSpeed. This has a range from 0.0 to 2.0 stored 2 bytes
	int iNewMaxSpeed = (iNewMaxSpeedU << 8) + iNewMaxSpeedL;
	fMaxSpeed = iNewMaxSpeed / 65535.0 * 2.0;

	// Update fNewSpeedWeight with new value. This has a range from 0.0 to 1.0 stored 2 bytes 
	int iNewWeight = (iNewWeightU << 8) + iNewWeightL;
	fNewSpeedWeight = iNewWeight / 65535.0;

	if(USE_SERIAL_FOR_DEBUGGING)
	{
		Serial.print("TEMP_CL Got new settings");
		Serial.print(" fMinSpeed=");
		Serial.print(fMinSpeed);
		Serial.print(" fMaxSpeed=");
		Serial.print(fMaxSpeed);
		Serial.print(" fNewSpeedWeight*10=");
		Serial.print(fNewSpeedWeight*10);
		Serial.println("");
	}

	// Save out settings to EPROM
	EEPROM.write(EEPROM_ADDR_DATA_VERSION, CUR_EPROM_DATA_VERSION);
	EEPROM.write(EEPROM_ADDR_MIN_SPEED,   iNewMinSpeedU);
	EEPROM.write(EEPROM_ADDR_MIN_SPEED+1, iNewMinSpeedL);
	EEPROM.write(EEPROM_ADDR_MAX_SPEED,   iNewMaxSpeedU);
	EEPROM.write(EEPROM_ADDR_MIN_SPEED+1, iNewMaxSpeedL);
	EEPROM.write(EEPROM_ADDR_NEW_SPEED_WEIGHT,   iNewWeightU);
	EEPROM.write(EEPROM_ADDR_NEW_SPEED_WEIGHT+1, iNewWeightL);

	// Debug saying we got a valid message
	for(int i = 0; i < 4; i++)
	{
		analogWrite(pins[4], 255);
		delay(100);
		digitalWrite(pins[4], LOW);
		delay(200);
	}

	if(USE_SERIAL_FOR_DEBUGGING)
	{
		Serial.print("TEMP_CL Got new remote values! fMinSpeed=");
		Serial.print(fMinSpeed);
		Serial.print(" fMaxSpeed=");
		Serial.print(fMaxSpeed);
		Serial.print(" fNewSpeedWeight=");
		Serial.print(fNewSpeedWeight);
		Serial.println("");
	}

	return true;
}


void loop()
{
	// Get the cur
	float fCurSpeed = GetCurSpeed();

	// Debug the speed returned
	//delay(50);
	//Serial.print("TEMP_CL Current speed = ");
	//Serial.println(fCurSpeed);

	// Clamp the range of the speed
	fCurSpeed = Clamp(fCurSpeed, fMinSpeed, fMaxSpeed);

	// Calculate the new speed ratio
	float fNewSpeedRatio = (fCurSpeed - fMinSpeed) / (fMaxSpeed - fMinSpeed);

	// Calculate the smoothed speed ratio
	g_fSpeedRatio = fNewSpeedRatio * fNewSpeedWeight + g_fSpeedRatio * (1.0 - fNewSpeedWeight);

	// Use the speed ratio to set the brightness of the LEDs
    if(g_fSpeedRatio > 0)
    {
		for(int nLED = 0; nLED < NUM_LEDS_PER_NODE; nLED++)
		{
			float fSubRangeMin = nLED / 5.0;
			float fSubRangeMax = (nLED + 1.0) / 5.0;
			float fSubRangeRatio = (g_fSpeedRatio - fSubRangeMin) / (fSubRangeMax - fSubRangeMin);
			fSubRangeRatio = Clamp(fSubRangeRatio, 0.0, 1.0);
			int iLEDbrightness = fSubRangeRatio * 255;

			analogWrite(pins[nLED], exp_map[iLEDbrightness]);   // set the LED brightness
		}
     }
    else
    {
		for(int nLEDOff = 0; nLEDOff < NUM_LEDS_PER_NODE; nLEDOff++)
		{
			digitalWrite(pins[nLEDOff], LOW); // Turn off LED
		}
    }

	// Construct the byte to send that contains our node ID
	// and the current speed.  We are compressing our speed ratio
	// to 5 bits (32 values) so that it fits in a single byte.
	byte ySendByte = iNodeIndex << 5;
	ySendByte = ySendByte | (int(g_fSpeedRatio * 255) >> 3);

	// We don't do this check anymore because we only have 7 nodes 
	// and the node index of the START_RCV_BYTE and END_RCV_BYTE
	// refer to the 8th node so they will never get sent.
	//// Make sure send byte isn't one of our signal chars
	//if(ySendByte == START_RCV_BYTE || ySendByte == END_RCV_BYTE)
	//{
	//	ySendByte++;
	//}

	// Get the current time (after all the sensor and LED stuff)
	unsigned long iCurTime = millis();

	// Listen for other nodes talking
	int iNumReadBytes = 0;
	while(Uart.available() > 0)
	{
		// Get the byte 
		int iReadByte = Uart.read();
		iNumReadBytes++;

		// Really, I don't think we should be reading more than one each loop... but maybe not?
		if(iNumReadBytes > 1)
		{
			if(USE_SERIAL_FOR_DEBUGGING)
			{
				Serial.print("TEMP_CL Got more more than a single byte in a loop:");
				Serial.println(iNumReadBytes);
			}
		}

		// If we got a new start signal, start trying to read the packet
		if(iReadByte == START_RCV_BYTE)
		{
			ReceiveTuningMessage();
		}
		else
		{
			// Get the node index of the sender
			int iIncomingNodeIndex = iReadByte >> 5;

			// Do some error checking that can likely be commented out in a final version
			if( iIncomingNodeIndex == iNodeIndex ||
				iIncomingNodeIndex >= NUM_NODES )
			{
				if(USE_SERIAL_FOR_DEBUGGING)
				{
					Serial.println("TEMP_CL Got invlaid node index!");
				}
			}

			// Save the next node index
			g_iNextNodeIndex = (iIncomingNodeIndex + 1) % (NUM_NODES + 1); // The PC gets a chance to talk also and has a node index of NUM_NODES
		
			// Save off the time we got this message so we can time out if a node isn't sending.
			g_iLastReceiveTime = iCurTime;

			if(USE_SERIAL_FOR_DEBUGGING)
			{
				Serial.print(iCurTime);
				Serial.print(" TEMP_CL - Got data from node ");
				Serial.println(iIncomingNodeIndex);
			}
		}
	}

	if(iNumReadBytes == 0)
	{
		// Check for a timeout.
		if(iCurTime - g_iLastReceiveTime > NODE_COM_TIMEOUT_MS)
		{
			if(USE_SERIAL_FOR_DEBUGGING)
			{
				Serial.print("TEMP_CL - Timeout! g_iNextNodeIndex=");
				Serial.print(g_iNextNodeIndex);
				Serial.print(" iCurTime=");
				Serial.print(iCurTime);
				Serial.print(" g_iLastReceiveTime=");
				Serial.print(g_iLastReceiveTime);
				Serial.println("");
			}

			g_iNextNodeIndex = (g_iNextNodeIndex + 1) % (NUM_NODES + 1); // The PC gets a chance to talk also and has a node index of NUM_NODES
			g_iLastReceiveTime = iCurTime;
		}
	}

	// If we are supposed to send next, do that
	if(iNodeIndex == g_iNextNodeIndex)
	{
		if(USE_SERIAL_FOR_DEBUGGING)
		{
			Serial.print(iCurTime);
			Serial.print(" TEMP_CL - about to write data iNodeIndex=");
			Serial.println(iNodeIndex);
		}

		if(USE_RS485)
		{
			digitalWrite (RS485_ENABLE_WRITE_PIN, HIGH);  // enable sending
		}
		Uart.write(ySendByte);
		if(USE_RS485)
		{
			delayMicroseconds (MICRO_SECOND_DELAY_POST_WRITE_RS485); // wait to make sure the message went out before turning off sending
			digitalWrite (RS485_ENABLE_WRITE_PIN, LOW);  // disable sending  
		}

		// Update next node now that we have sent
		g_iNextNodeIndex = (g_iNextNodeIndex + 1) % (NUM_NODES + 1); // The PC gets a chance to talk also and has a node index of NUM_NODES

		if(USE_SERIAL_FOR_DEBUGGING)
		{
			Serial.print(iCurTime);
			Serial.print(" TEMP_CL - just wrote data iNodeIndex=");
			Serial.println(iNodeIndex);
		}
	}
}

