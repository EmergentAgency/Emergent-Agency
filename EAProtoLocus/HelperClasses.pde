/**
 * Emergent Agency processing sim helper classes
 */

// Class for each person in the sim
class Person
{
    float x;
    float y;
    float size = 50;
    boolean bOver = false;
    boolean locked = false;
    float bdifx = 0.0; 
    float bdify = 0.0;
}

// Class for the sensor.  The state of the sensor is then fed into the node
class Sensor
{
    int index;
    float x;
    float y;
    float radPos; // was distAlongCircle, actually in radians
    float size = SENSOR_SIZE; 
    boolean bActive = false;

    void init(int inIndex)
    {
        index = inIndex;
        radPos = (float)index / (float)NUM_NODES * 2 * PI;

        x = cos(radPos) * width / 3 + width / 2;
        y = sin(radPos) * height / 3 + height / 2;
    }

    void update()
    {
        bActive = false;
        for(int i = 0; i < NUM_PEOPLE; i++)
        {
            if(dist(x, y, people[i].x, people[i].y) < size/2)
            {
                bActive = true;
                break;
            }
        }
    }
}

// class that represents one LED
class LED 
{ 
  int index;
  int sensIndex;  // index of parent sensor
  float x;
  float y;
  float radPos;
  float size = LED_SIZE;  
  boolean bLit = false;
  float litRatio = 0;  // 0 = totally off, 0.5 = half on, 1 = totally on

  void init(int inSensIndex, int LEDindex, float sensRadPos)
  {
    index = LEDindex; 
    sensIndex = inSensIndex;
    
    // get LEDs aligned within sensor boundaries
    float radOffset = 1 / (2*(float)NUM_NODES) * 2 * PI; // 1/2 of sensor radians
    float LEDoffset = 1 / (2*(float)NUM_LEDS) * 2 * PI;  // 1/2 of LED radians
    radPos = (float)index / (float)NUM_LEDS * 2 * PI - radOffset + LEDoffset;
    //println("node " + sensIndex + ", LED " + index + ": " + radPos);
    
    x = cos(radPos) * width / 3 + width / 2;
    y = sin(radPos) * height / 3 + height / 2;
  }
}

// Class the represents a bundled node on the sim.  This
// contains a sensor, some LED's, and a Node that does the actual logic.
class SimNode
{
    Sensor nodeSensor;
    LED[] LEDs = new LED[NUM_LEDS_PER_NODE]; 
    Node logicNode;
    float lastUpdate;

    void init(int nodeIndex, CommunicationLink inComLink)
    {
        nodeSensor = new Sensor();
        nodeSensor.init(nodeIndex);
        lastUpdate = millis();

        for(int i = 0; i < NUM_LEDS_PER_NODE; i++)
        {
            LEDs[i] = new LED();
            LEDs[i].init(nodeIndex, nodeIndex * NUM_LEDS_PER_NODE + i, nodeSensor.radPos);
        }
        
        logicNode = new Node();
        logicNode.init(nodeIndex, inComLink, nodeSensor.radPos);
        for(int i = 0; i < NUM_LEDS_PER_NODE; i++)
        {
            LEDs[i].litRatio = logicNode.getLED(i);
        }
    }

    void update()
    {
        float offset;
        nodeSensor.update();
        float thisUpdate = millis();    // more accurately calculated time
        float deltaSeconds = (thisUpdate - lastUpdate) / 1000; 
        lastUpdate = thisUpdate;
        logicNode.update(deltaSeconds, nodeSensor.bActive); 
        for(int i = 0; i < NUM_LEDS_PER_NODE; i++)
        {
            LEDs[i].litRatio = logicNode.getLED(i);
        }
    }

    void drawSensor()
    {
        stroke(0); 
        if(nodeSensor.bActive)
            fill(150, 64, 64);
        else 
            fill(100, 64, 64);
        ellipse(nodeSensor.x, nodeSensor.y, nodeSensor.size, nodeSensor.size);
    }

    void drawLEDs()
    {
        for(int i = 0; i < NUM_LEDS_PER_NODE; i++)
        {
            float fillRatio = LEDs[i].litRatio * 255;
            fill(0,(int) fillRatio,0);
            ellipse(LEDs[i].x, LEDs[i].y, LEDs[i].size, LEDs[i].size); 
        }
    }
}
