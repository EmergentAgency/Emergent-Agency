// logic node
Node logicNode;

// The index for this node.  Should come from dip switches eventually.
int NODE_INDEX = 0;

// test LED pin number
int START_TEST_LED_PIN = 1;

// The communication link for this node that takes messages from the logic nodes and turns them into serial messages
CommunicationLink comLink;

// last time in milliseconds
int lastMillis;



void setup()
{
    // initialize the logic node
    logicNode.init(NODE_INDEX, comLink);

    for(int i = 0; i < NUM_LEDS_PER_NODE; i++)
    {
        // turn on pins for output to test an LED
        pinMode(START_TEST_LED_PIN + i, OUTPUT);   
    }

    // debugging
    //Serial.begin(9600);
}



void loop()
{
    // calc delta seconds;
    int curMillis = millis();
    int deltaMillis = curMillis - lastMillis;
    lastMillis = curMillis;
    float deltaSeconds = deltaMillis / 1000.0;

    // update logic node
    //logicNode.update(deltaSeconds, random(2) == 0);
    logicNode.update(deltaSeconds, true);

    for(int i = 0; i < NUM_LEDS_PER_NODE; i++)
    {
        int LEDbrightness = logicNode.getLED(i) * 255;

        //Serial.println(LEDbrightness);

        if(LEDbrightness > 0)
        {
           //analogWrite(START_TEST_LED_PIN + i, LEDbrightness);   // set the LED brightness
           analogWrite(START_TEST_LED_PIN + i, HIGH);   // set the LED brightness
        }
        else
        {
            digitalWrite(START_TEST_LED_PIN + i, LOW);   // set the LED off
        }
    }

    // wait half a second
    //delay(deltaSeconds * 1000);
}