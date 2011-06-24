// logic node
Node logicNode;

// The index for this node.  Should come from dip switches eventually.
int NODE_INDEX = 0;

// test LED pin number
int START_TEST_LED_PIN = 8;

// The communication link for this node that takes messages from the logic nodes and turns them into serial messages
CommunicationLink comLink;

// last time in milliseconds
int lastMillis;

// TEMP_CL - testing vars
boolean bHasFakedBounce = false;

void setup()
{
    Serial.begin(9600);           // set up Serial library at 9600 bps
    Serial.println("EAHardware - setup");

    // initialize the logic node
    logicNode.init(NODE_INDEX, comLink);

    for(int i = 0; i < NUM_LEDS_PER_NODE; i++)
    {
        // turn on pins for output to test an LED
        pinMode(START_TEST_LED_PIN + i, OUTPUT);   
    }
}



void loop()
{
    // calc delta seconds;
    int curMillis = millis();
    int deltaMillis = curMillis - lastMillis;
    lastMillis = curMillis;
    float deltaSeconds = deltaMillis / 1000.0;

    // update logic node
    logicNode.update(deltaSeconds, false); // TEMP_CL - get actual sensor data

    // TEMP_CL - fake a bounce on the far node for testing
    if(!bHasFakedBounce)
    {
        int tempIdx = 4;
        float tempRadPos = (float)tempIdx / (float)NUM_NODES * 2 * PI;
        logicNode.receiveMessage(tempIdx, tempRadPos, false);
        bHasFakedBounce = true;
    }

    Serial.println("Post update:"); // TEMP_CL

    for(int i = 0; i < NUM_LEDS_PER_NODE; i++)
    {
        int LEDbrightness = logicNode.getLED(i) * 255;

        if(LEDbrightness > 0)
        {
            Serial.println(LEDbrightness); // TEMP_CL
            analogWrite(START_TEST_LED_PIN + i, LEDbrightness);   // set the LED brightness
        }
        else
        {
            digitalWrite(START_TEST_LED_PIN + i, LOW);   // set the LED off
        }
    }
}