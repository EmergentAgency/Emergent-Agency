// logic node
Node logicNode;

// The index for this node.  Started setup for dip switches.
int NODE_INDEX;
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
int INPUT_SENSOR_PIN = 38;
int SENSOR_DELAY_TIME_IN_MILLS = 2000;
unsigned long lastSensorTime = 0;
boolean bSensorOn = false;
boolean readWireless = true;
float totalSensorOnTime;
float timeSinceLastSensor = -1;

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
    NODE_INDEX = 1;
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

    // read sensor -- need to add Joel's code that smooths out input!
    int rawSensorValue = analogRead(INPUT_SENSOR_PIN);
    //Serial.println(rawSensorValue);
    if(rawSensorValue > 100)
    {
//        bSensorOn = true;
//        lastSensorTime = millis();
    }
    else
    {
//        if(millis() - lastSensorTime > SENSOR_DELAY_TIME_IN_MILLS)
//        bSensorOn = false;
    }

    // insert Joel's boolean sensor function here:
    // bSensorOn = ...
    if (bSensorOn) {
        totalSensorOnTime += deltaSeconds;
        timeSinceLastSensor = 0;
    }
    else {
        totalSensorOnTime = 0;
        timeSinceLastSensor += deltaSeconds;
    }
    
    char incomingByte;
    if(readWireless)
    {
        if (Uart.available() > 0) {
            incomingByte = Uart.read();
            //readWireless = false;
            parse_incoming(incomingByte);    // sets global vars bounceNode and rotation with appropriate values
            Serial.println(bounceNode);
            Serial.println(rotation);
            logicNode.receiveMessage(bounceNode, rotation);
        }
    }

    // update logic node
    logicNode.update(deltaSeconds, bSensorOn);

    // hack to test print and receive
    if (NODE_INDEX == 6 && (timeSinceLastSensor > 40 || timeSinceLastSensor < 0)) 
    {
        parse_outgoing(4, true);               // fake a bounce from node 4, in clockwise direction
        Uart.print(bounceChar);                // tell other nodes about the bounce
        logicNode.receiveMessage(4, true);     // tell this node about the bounce
        //Serial.println(logicNode.lastBounceIdx);
        
        // temp
        timeSinceLastSensor = 0;
    }

    for(int i = 0; i < NUM_LEDS_PER_NODE; i++)
    {
        int LEDbrightness = logicNode.getLED(i) * 255;

        if(LEDbrightness > 0)
        {
            //Serial.println(LEDbrightness); // TEMP_CL
            analogWrite(pins[i], LEDbrightness);   // set the LED brightness
        }
        else
        {
            digitalWrite(pins[i], LOW);   // set the LED off
        }
    }
}
