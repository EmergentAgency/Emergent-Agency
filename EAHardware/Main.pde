// logic node
Node logicNode;

// The index for this node.  Should come from dip switches eventually.
int NODE_INDEX = 0;

// test LED pin number
int TEST_LED_PIN = 2;

// The communication link for this node that takes messages from the logic nodes and turns them into serial messages
CommunicationLink comLink;

void setup()
{
    // initialize the logic node
    logicNode.init(NODE_INDEX, comLink);

    // turn on pin 2 for output to test an LED
    pinMode(TEST_LED_PIN, OUTPUT);     
}



void loop()
{
    float deltaSeconds = 0.03; // TEMP_CL - do this for real

    // update logic node
    logicNode.update(deltaSeconds, random(2) == 0);

    if(logicNode.getLED(0))
    {
        digitalWrite(TEST_LED_PIN, HIGH);   // set the LED on
    }
    else
    {
        digitalWrite(TEST_LED_PIN, LOW);   // set the LED off
    }

    // wait half a second
    delay(deltaSeconds * 1000);
}