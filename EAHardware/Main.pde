// logic node
Node logicNode;

// The index for this node.  Started setup for dip switches.
int NODE_INDEX = 3;

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

// sensor vars
boolean bSensorOn = false;

// wireless vars
boolean readWireless = true;

// last time in milliseconds
unsigned long lastMillis;

// TEMP_CL - testing vars
boolean bHasFakedBounce = true;

void setup()
{
    // setup debugging serial code
    Serial.begin(9600);           // set up Serial library at 9600 bps
    Serial.println("EAHardware - setup");
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
    tone1.begin(18);
}



void loop()
{
    // calc delta seconds;
    int curMillis = millis();
    int deltaMillis = curMillis - lastMillis;
    lastMillis = curMillis;
    float deltaSeconds = deltaMillis / 1000.0;

    // use fancy sensor code
    bSensorOn = check_motion_state();

    // Debugging - print sensor
    Serial.println(bSensorOn ? "Sensor: on" : "Sensor: off");

    // check for wireless messages
    char incomingByte;
    if(readWireless)
    {
        if (Uart.available() > 0) {
            incomingByte = Uart.read();
            //readWireless = false;
            parse_incoming(incomingByte);    // sets global vars bounceNode and rotation with appropriate values
            Serial.println(bounceNode);
            Serial.println(rotation);
            logicNode.receiveMessage(bounceNode, lociIdx, rotation);

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
}
