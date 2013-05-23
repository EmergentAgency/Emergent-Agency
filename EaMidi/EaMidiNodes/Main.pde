// logic node
Node logicNode;

// If this is true, we are a remote node only communicating via the XBee.
// If false, we are attached to a laptop via USB and are listening for all the other
// remote XBee signals and the sending those signals to the laptop via USB serial
static bool bRemote = true;

// The index for this node.  Started setup for dip switches.
int NODE_INDEX = 1;

// Dip switches
int NODE_PIN_0 = 5;		// Note: dip switch numbers start from 1, 
int NODE_PIN_1 = 7;     //       while node indices start from 0
int NODE_PIN_2 = 8;
int NODE_PIN_3 = 9;
int NODE_PIN_4 = 10;
int NODE_PIN_5 = 11;
int NODE_PIN_6 = 12;
int NODE_PIN_7 = 13;
int nodePins[8];

// test LED pin number
int LED_PIN_0 = 16;
int LED_PIN_1 = 15;
int LED_PIN_2 = 14;
int LED_PIN_3 = 26;
int LED_PIN_4 = 25;
int pins[5];

// interupt pin
static const int INT_PIN = 19; // INT7

// TEMP_CL
volatile int g_iPulseCount = 0;
volatile unsigned long g_iLastPeriodMicro = 0;
volatile unsigned long g_iLastTimeMicro = 0;

float g_fSpeedRatio;

// sensor vars
boolean bSensorOn = false;

// wireless vars
boolean readWireless = true;

// last time in milliseconds
unsigned long lastMillis;

// TEMP_CL - testing vars
boolean bHasFakedBounce = true;

//static unsigned char exp_map[256]={
//  0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
//  1,1,1,1,1,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,3,
//  3,3,3,3,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,4,4,4,5,5,5,
//  5,5,5,5,5,6,6,6,6,6,6,6,7,7,7,7,7,7,8,8,8,8,8,8,9,9,
//  9,9,10,10,10,10,10,11,11,11,11,12,12,12,13,13,13,13,
//  14,14,14,15,15,15,16,16,16,17,17,18,18,18,19,19,20,
//  20,20,21,21,22,22,23,23,24,24,25,26,26,27,27,28,29,
//  29,30,31,31,32,33,33,34,35,36,36,37,38,39,40,41,42,
//  42,43,44,45,46,47,48,50,51,52,53,54,55,57,58,59,60,
//  62,63,64,66,67,69,70,72,74,75,77,79,80,82,84,86,88,
//  90,91,94,96,98,100,102,104,107,109,111,114,116,119,
//  122,124,127,130,133,136,139,142,145,148,151,155,158,
//  161,165,169,172,176,180,184,188,192,196,201,205,210,
//  214,219,224,229,234,239,244,250,255
//};
static unsigned char exp_map[256]={
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

void setup()
{
    // setup debugging serial code
    Serial.begin(9600);           // set up Serial library at 9600 bps
    Serial.println("EAHardware - setup TEMP");
    Uart.begin(9600);

	// define node pins
	nodePins[0] = 5;
	nodePins[1] = 7;
	nodePins[2] = 8;
	nodePins[3] = 9;
	nodePins[4] = 10;
	nodePins[5] = 11;
	nodePins[6] = 12;
	nodePins[7] = 13;

	// determine which node we are -- not working, not worth it
//    for(int i = 0; i < 8; i++)
//    {
//        pinMode(nodePins[i], INPUT);   
//		int whichNode = 0;
//                whichNode = digitalRead(nodePins[i]);
//                //Serial.println(nodePins[i]);
//		if (whichNode == 1) {
//			NODE_INDEX = i;
//			Serial.println(i);
//		}
//    }

    // initialize the logic node
    logicNode.init(NODE_INDEX);

    // define LED pins
    pins[0] = LED_PIN_0;
    pins[1] = LED_PIN_1;
    pins[2] = LED_PIN_2;
    pins[3] = LED_PIN_3;
    pins[4] = LED_PIN_4;

    // setup LED pins for output
    for(int i = 0; i < NUM_LEDS_PER_NODE; i++)
    {
        // turn on pins for output to test an LED
        pinMode(pins[i], OUTPUT);   
    }

    // start tones
    //tone1.begin(18);

	// TEMP_CL - figure out LED non linearily
	for(int nTest = 0; nTest < 2; nTest++)
	{
		for(int brightness = 0; brightness < 256; brightness++)
		{
			//float normBright = (float)brightness / 255.0;
			//int transBright = (int)(normBright * normBright * normBright * normBright * 255);
			//analogWrite(pins[0], transBright);   // set the LED brightness

			analogWrite(pins[0], exp_map[brightness]);   // set the LED brightness
			delay(10);
		}
	}
	analogWrite(pins[0], exp_map[0]);   // set the LED brightness

	// Setup interupt
	pinMode(INT_PIN, INPUT);
	attachInterrupt(INT_PIN, MotionDetectorPulse, RISING);   // Attach an Interupt to INT_PIN for timing period of motion detector input
}

// Interupt called to get time between pulses
void MotionDetectorPulse()
{
	g_iPulseCount++;

	//unsigned long iCurTimeMicro = micros();
	unsigned long iCurTimeMicro = millis();
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
	//unsigned long iCurTimeMicro = micros();
	unsigned long iCurTimeMicro = millis();
	unsigned long iThisPeriodMicro = iCurTimeMicro - g_iLastTimeMicro;
	if(iThisPeriodMicro < g_iLastPeriodMicro)
		return g_iLastPeriodMicro;
	else
		return iThisPeriodMicro;
}

// Returns the current speed of the detected motion in meters per second.
float GetCurSpeed()
{
	float iLastPeriod = GetLastPeriodMicro();
	//float fFreq = 1.0 / (iLastPeriod / 1000000.0);
	float fFreq = 1.0 / (iLastPeriod / 1000.0);
	float fSpeed = fFreq / 70.1666; // 70.1666 = (2 * Ft/c)
	return fSpeed;
}

void loop()
{
	float fCurSpeed = GetCurSpeed();

	//delay(50);
	//Serial.print("Pulse count = ");
	//Serial.print(g_iPulseCount);
	//Serial.print(" Current speed = ");
	//Serial.println(fCurSpeed);
	//g_iPulseCount = 0;

	static const float fMinSpeed = 0.02;
	static const float fMaxSpeed = 0.4;
	static const float fNewSpeedWeight = 0.15;
	if(fCurSpeed < fMinSpeed)
	{
		fCurSpeed = fMinSpeed;
	}
	else if(fCurSpeed > fMaxSpeed)
	{
		fCurSpeed = fMaxSpeed;
	}
	float fNewSpeedRatio = (fCurSpeed - fMinSpeed) / (fMaxSpeed - fMinSpeed);

	g_fSpeedRatio = fNewSpeedRatio * fNewSpeedWeight + g_fSpeedRatio * (1.0 - fNewSpeedWeight);


    int iLEDbrightness = g_fSpeedRatio * 255;
    if(iLEDbrightness > 0)
    {
        //Serial.println(LEDbrightness);
        //analogWrite(pins[0], LEDbrightness);   // set the LED brightness
        analogWrite(pins[0], exp_map[iLEDbrightness]);   // set the LED brightness
    }
    else
    {
        digitalWrite(pins[0], LOW);   // set the LED off
    }

	byte sendByte = NODE_INDEX << 5;
	sendByte = sendByte | (iLEDbrightness >> 3);

	// if we are a remote sensor...
	if(bRemote)
	{
		// Listen for any requests for this node
		//if(Uart.available() > 0)
		while(Uart.available() > 0)
		{
			// read byte and then parse out node and motion value
			int iReadByte = Uart.read();
			int iNodeIndex = iReadByte >> 5;

			// If this message is for us, send back our current motion value
			if(iNodeIndex == NODE_INDEX)
			{
				Uart.write(sendByte);
				break;
			}
		}
	}
	else
	{
		// write our our local value
		Serial.write(sendByte);

		// Listen for other values on the XBee and then send them along
        //if (Uart.available() > 0)
        while (Uart.available() > 0)
		{
            byte incomingByte = Uart.read();
			Serial.write(incomingByte);
		}
	}

	// TEMP_CL - try a bit of delay to avoid filling the recieve buffer
	//delay(5); // ok, response time... PC sometimes times out with a timeout of 8ms
	delay(3);

	return;


	/*

    // calc delta seconds;
    int curMillis = millis();
    int deltaMillis = curMillis - lastMillis;
    lastMillis = curMillis;
    float deltaSeconds = deltaMillis / 1000.0;

    // use fancy sensor code
    //bSensorOn = check_motion_state();
    // Debugging - print sensor
    //Serial.println(bSensorOn ? "Sensor: on" : "Sensor: off");

	// This sensor changes state the more motion it detects
	// Count the number of high low cycle in a loop.
	int iNumChanges = 0;
	int iRawSensor = 0;
	bool bSensorLastHigh = false;
	bool bSensorHigh = false;
	for(int i = 0; i < 4000; i++)
	{
		bSensorHigh = get_raw_motion_state() > 100;
		if( bSensorHigh && !bSensorLastHigh ||
			!bSensorHigh && bSensorLastHigh )
		{
			iNumChanges++;
		}

		bSensorLastHigh = bSensorHigh;
	}

	// Send that as debug info
	//Serial.print("iNumChanges = ");
	//Serial.println(iNumChanges);

	// Turn those changes into and LED brightness value
    int LEDbrightness = iNumChanges * 16;
	if(LEDbrightness > 255)
		LEDbrightness = 255;
    if(LEDbrightness > 0)
    {
        //Serial.println(LEDbrightness);
        //analogWrite(pins[0], LEDbrightness);   // set the LED brightness
        analogWrite(pins[0], exp_map[LEDbrightness]);   // set the LED brightness
    }
    else
    {
        digitalWrite(pins[0], LOW);   // set the LED off
    }

	// Send a single byte that is both our node number (3 bits) and our brightness (brightness is scaled to only use 5 bits aka 32 values)
	byte sendByte = NODE_INDEX << 5;
	sendByte = sendByte | (LEDbrightness >> 3);

	// if we are a remote sensor, just sent ourbrightness of the XBee
	if(bRemote)
	{
		Uart.write(sendByte);
		//Uart.print(sendByte);
	}
	else
	{
		// write our our local value
		Serial.write(sendByte);

		// Listen for other values on the XBee and then send them along
        //if (Uart.available() > 0)
        while (Uart.available() > 0)
		{
            byte incomingByte = Uart.read();
			Serial.write(incomingByte);
		}
	}


	/*
    // check for wireless messages
    char incomingByte;
    if(readWireless)
    {
        if (Uart.available() > 0) {
            incomingByte = Uart.read();
            //readWireless = false;
            parse_incoming(incomingByte);    // sets global vars bounceNode and rotation with appropriate values
            Serial.println("Got message (incomingByte, bounce, rotation, lociIdxMsg");
            Serial.println(incomingByte);
            Serial.println(bounceNode);
            Serial.println(rotation ? "CW" : "CCW");
            Serial.println(lociIdxMsg);
            logicNode.receiveMessage(bounceNode, lociIdxMsg, rotation);

        }
    }

    // update logic node
    logicNode.update(deltaSeconds, bSensorOn);

    // set actual LED pins based on logic node state
    for(int i = 0; i < NUM_LEDS_PER_NODE; i++)
    {
        int LEDbrightness = logicNode.getLED(i) * 255;

        if(LEDbrightness > 0)
        {
            //Serial.println(LEDbrightness);
            analogWrite(pins[i], LEDbrightness);   // set the LED brightness
        }
        else
        {
            digitalWrite(pins[i], LOW);   // set the LED off
        }
    }
	*/
}
