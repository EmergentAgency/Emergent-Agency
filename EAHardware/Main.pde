// logic node
Node logicNode;

// The index for this node.  Should come from dip switches eventually.
int NODE_INDEX = 0;

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

// The communication link for this node that takes messages from the logic nodes and turns them into serial messages
CommunicationLink comLink;

// last time in milliseconds
unsigned long lastMillis;

// TEMP_CL - testing vars
boolean bHasFakedBounce = false;

void setup()
{
    // setup debugging serial code
    Serial.begin(9600);           // set up Serial library at 9600 bps
    Serial.println("EAHardware - setup");

    // initialize the logic node
    logicNode.init(NODE_INDEX, comLink);

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

    // read sensor
    int rawSensorValue = analogRead(INPUT_SENSOR_PIN);
    if(rawSensorValue > 20)
    {
        bSensorOn = true;
        lastSensorTime = millis();
    }
    else
    {
        if(millis() - lastSensorTime > SENSOR_DELAY_TIME_IN_MILLS)
            bSensorOn = false;
    }

    // update logic node
    logicNode.update(deltaSeconds, bSensorOn);

    //// TEMP_CL - fake a bounce on the far node for testing
    //if(!bHasFakedBounce)
    //{
    //    int tempIdx = 4;
    //    float tempRadPos = (float)tempIdx / (float)NUM_NODES * 2 * PI;
    //    logicNode.receiveMessage(tempIdx, tempRadPos, false);
    //    bHasFakedBounce = true;
    //}

    Serial.println("Post update:"); // TEMP_CL

    for(int i = 0; i < NUM_LEDS_PER_NODE; i++)
    {
        int LEDbrightness = logicNode.getLED(i) * 255;

        if(LEDbrightness > 0)
        {
            Serial.println(LEDbrightness); // TEMP_CL
            analogWrite(pins[i], LEDbrightness);   // set the LED brightness
        }
        else
        {
            digitalWrite(pins[i], LOW);   // set the LED off
        }
    }
}